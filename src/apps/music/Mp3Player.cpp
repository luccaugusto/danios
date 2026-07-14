#include "apps/music/Mp3Player.h"

namespace {
// RAM knobs (global constraint: no PSRAM, ~90-150 KB free with BT up — see
// the F5 heap figures in docs/hardware.md). If allocation ever fails on
// device, halve kRingSamples first.
constexpr size_t kRingSamples = 16 * 1024;   // int16 samples = 32 KB ~ 186 ms
constexpr size_t kMp3ChunkBytes = 128;       // <= 2 decoded frames per write,
                                             // even at the lowest CBR bitrates
constexpr size_t kMinFreeToFeed = 2 * 2304;  // room for 2 worst-case stereo
                                             // MP3 frames (1152 samples x 2 ch)
constexpr int kMaxChunksPerFeed = 8;         // bounds tick() time; ~1 KB/tick
                                             // of MP3 easily outpaces playback
}  // namespace

Mp3Player::Mp3Player() : ring_(kRingSamples) {
  decoder_.setDataCallback(pcmCallback);
  decoder_.setReference(this);
}

Mp3Player::~Mp3Player() { close(); }

bool Mp3Player::open(const char* path, bool flushRing) {
  close();
  file_ = SD.open(path, FILE_READ);
  if (!file_ || file_.isDirectory()) {
    if (file_) file_.close();
    Serial.printf("[music] open FAILED: %s\n", path);
    return false;
  }
  badFormat_ = false;
  decoder_.begin();  // fresh sync state per track
  if (flushRing) ring_.requestClear();
  Serial.printf("[music] open: %s (%u bytes)\n", path,
                static_cast<unsigned>(file_.size()));
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
                            void* ref) {
  auto* self = static_cast<Mp3Player*>(ref);
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
