#include "apps/music/Mp3Player.h"

namespace {
// RAM knobs. MEASURED budget (HW, 2026-07-14): ~33 KB of free heap with BT
// Classic up — the plan's "~90-150 KB" never existed. The whole pipeline
// (ring + ~23 KB helix decoder structs + ~7 KB helix pcm/frame buffers) must
// fit in that, so the ring is 8 KB and the MP3 chunk is 64 bytes: with <= 64
// new bytes per decoder_.write(), CommonHelix's drain loop (decodes while
// >= 1024 bytes are buffered) can complete at most ONE frame per write, so
// one worst-case stereo frame (1152 x 2 ch = 2304 samples) of ring headroom
// is enough to guarantee pcmCallback never overflows.
constexpr size_t kRingSamples = 4 * 1024;  // int16 samples = 8 KB ~ 46 ms
constexpr size_t kMp3ChunkBytes = 64;      // <= 1 decoded frame per write
constexpr size_t kMinFreeToFeed = 2304;    // 1 worst-case stereo MP3 frame
constexpr int kMaxChunksPerFeed = 8;       // bounds tick() time; 512 B/tick
                                           // of MP3 easily outpaces playback

// arduino-libhelix v0.8.5 never forwards setReference()'s pointer to the data
// callback: provideResult() passes p_caller_data, which the library never
// assigns (setReference fills the separate p_caller_ref, read only by the
// unused info callback). So `ref` below is always null and the instance is
// reached through this single-active-player global instead — same g_self
// idiom as the apps. Set/cleared on the loop task; pcmCallback runs
// synchronously inside decoder_.write() on that same task (never the BT task).
Mp3Player* g_activePlayer = nullptr;
}  // namespace

Mp3Player::Mp3Player() : ring_(kRingSamples) {
  decoder_.setDataCallback(pcmCallback);
  g_activePlayer = this;
}

Mp3Player::~Mp3Player() {
  close();
  if (g_activePlayer == this) g_activePlayer = nullptr;
}

bool Mp3Player::open(const char* path, bool flushRing) {
  close();
  file_ = SD.open(path, FILE_READ);
  if (!file_ || file_.isDirectory()) {
    if (file_) file_.close();
    Serial.printf("[music] open FAILED: %s\n", path);
    return false;
  }
  badFormat_ = false;
  // Heap gate for the ~30 KB decoder allocation (7 helix structs + pcm/frame
  // buffers; close() above already freed any previous instance). libhelix's
  // allocator hangs in while(true) on OOM instead of failing, so this check
  // is the ONLY thing standing between a tight session and a silent freeze —
  // gate generously and let the track fail cleanly.
  constexpr size_t kDecoderHeapNeed = 34 * 1024;
  if (esp_get_free_heap_size() < kDecoderHeapNeed) {
    file_.close();
    Serial.printf("[music] low heap (%u free) — refusing decoder alloc: %s\n",
                  static_cast<unsigned>(esp_get_free_heap_size()), path);
    return false;
  }
  if (!decoder_.begin()) {  // ~24 KB MP3InitDecoder alloc can fail (no PSRAM)
    file_.close();
    Serial.printf("[music] decoder alloc FAILED: %s\n", path);
    return false;
  }
  if (flushRing) ring_.requestClear();
  Serial.printf("[music] open: %s (%u bytes, heap %u)\n", path,
                static_cast<unsigned>(file_.size()),
                static_cast<unsigned>(esp_get_free_heap_size()));
  return true;
}

void Mp3Player::close() {
  decoder_.end();
  if (file_) file_.close();
}

Mp3Player::Feed Mp3Player::feed() {
  if (!file_ || !playing_.load()) return Feed::Ok;
  for (int i = 0; i < kMaxChunksPerFeed; ++i) {
    if (ring_.freeSpace() < kMinFreeToFeed) return Feed::Ok;  // ring is topped up
    uint8_t chunk[kMp3ChunkBytes];
    const int n = file_.read(chunk, sizeof(chunk));
    if (n <= 0) return Feed::End;
    decoder_.write(chunk, static_cast<size_t>(n));
    if (badFormat_) return Feed::Error;
  }
  return Feed::Ok;
}

void Mp3Player::pcmCallback(MP3FrameInfo& info, short* pcm, size_t len,
                            void* /*ref — always null, see g_activePlayer*/) {
  auto* self = g_activePlayer;
  if (self == nullptr) return;
  if (info.samprate != 44100 || info.nChans < 1 || info.nChans > 2) {
    self->badFormat_ = true;  // A2DP is pinned at 44100 Hz; no resampler
    return;
  }
  if (info.nChans == 2) {
    self->ring_.write(pcm, len);  // already interleaved stereo; gate in feed()
    return;                       // guarantees this never overflows
  }
  // Mono: duplicate each sample into L+R, in small stack chunks.
  int16_t stereo[128];
  size_t i = 0;
  while (i < len) {
    size_t j = 0;
    while (j < 64 && i < len) {
      stereo[j * 2] = pcm[i];
      stereo[j * 2 + 1] = pcm[i];
      ++j;
      ++i;
    }
    self->ring_.write(stereo, j * 2);
  }
}

int32_t Mp3Player::sourceCallback(int16_t* stereo_buf, int32_t frames,
                                  void* ctx) {
  auto* self = static_cast<Mp3Player*>(ctx);
  if (self == nullptr) return 0;  // setSource disable race: stale fn, nulled ctx
  if (!self->playing_.load()) return 0;  // paused = silence
  const size_t got =
      self->ring_.read(stereo_buf, static_cast<size_t>(frames) * 2);
  const int32_t gain = self->volumePct_.load() * 255 / 100;  // 0..255
  for (size_t i = 0; i < got; ++i) {
    stereo_buf[i] = static_cast<int16_t>(
        (static_cast<int32_t>(stereo_buf[i]) * gain) >> 8);
  }
  return static_cast<int32_t>(got / 2);  // samples -> stereo frames
}
