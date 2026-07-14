#define MINIMP3_IMPLEMENTATION  // exactly one TU carries the decoder body
#define MINIMP3_ONLY_MP3        // no MP1/MP2 support: smaller flash

#include "apps/music/Mp3Player.h"

#include <cstring>

namespace {
// RAM knobs. MEASURED budget (HW, 2026-07-14): ~43-51 KB free after BT
// enable, and the A2DP link itself takes ~14 KB more once established. The
// whole pipeline (this object ~15.5 KB + the 8 KB ring below) is allocated
// by MusicApp BEFORE the link comes up, and decoding allocates nothing.
constexpr size_t kRingSamples = 4 * 1024;  // int16 samples = 8 KB ~ 46 ms
constexpr size_t kMinFreeSamples = MINIMP3_MAX_SAMPLES_PER_FRAME;  // 1 frame
constexpr size_t kRefillBelowBytes = 2048;  // keep >= 1 max frame of lookahead
constexpr int kMaxFramesPerFeed = 3;        // ~78 ms of audio per tick, bounds
                                            // tick() time and SD reads
}  // namespace

Mp3Player::Mp3Player() : ring_(kRingSamples) { mp3dec_init(&dec_); }

Mp3Player::~Mp3Player() { close(); }

bool Mp3Player::open(const char* path, bool flushRing) {
  close();
  file_ = SD.open(path, FILE_READ);
  if (!file_ || file_.isDirectory()) {
    if (file_) file_.close();
    Serial.printf("[music] open FAILED: %s\n", path);
    return false;
  }
  inLen_ = 0;
  mp3dec_init(&dec_);  // fresh sync state per track; no allocation
  if (flushRing) ring_.requestClear();
  Serial.printf("[music] open: %s (%u bytes, heap %u)\n", path,
                static_cast<unsigned>(file_.size()),
                static_cast<unsigned>(esp_get_free_heap_size()));
  return true;
}

void Mp3Player::close() {
  if (file_) file_.close();
}

Mp3Player::Feed Mp3Player::feed() {
  if (!file_ || !playing_.load()) return Feed::Ok;
  for (int i = 0; i < kMaxFramesPerFeed; ++i) {
    if (ring_.freeSpace() < kMinFreeSamples) return Feed::Ok;  // topped up
    if (inLen_ < kRefillBelowBytes) {
      const int n = file_.read(inBuf_ + inLen_, kInBufBytes - inLen_);
      if (n > 0) inLen_ += static_cast<size_t>(n);
    }
    if (inLen_ == 0) return Feed::End;
    mp3dec_frame_info_t info;
    const int samples = mp3dec_decode_frame(
        &dec_, inBuf_, static_cast<int>(inLen_), pcm_, &info);
    if (info.frame_bytes == 0) {
      // No complete frame in the buffer. Mid-file with a full buffer of
      // unsyncable bytes = a broken file; mid-file otherwise = wait for the
      // refill above; at EOF the residue is trailing junk = track over.
      if (file_.available() > 0) {
        return (inLen_ >= kInBufBytes) ? Feed::Error : Feed::Ok;
      }
      return Feed::End;
    }
    inLen_ -= static_cast<size_t>(info.frame_bytes);
    std::memmove(inBuf_, inBuf_ + info.frame_bytes, inLen_);
    if (samples > 0) {
      if (info.hz != 44100 || info.channels < 1 || info.channels > 2) {
        return Feed::Error;  // A2DP pinned at 44100 Hz; no resampler
      }
      writePcm(samples, info.channels);
    }
    // samples == 0 with frame_bytes > 0: junk skipped — keep going.
  }
  return Feed::Ok;
}

void Mp3Player::writePcm(int samplesPerChannel, int channels) {
  if (channels == 2) {
    // Already interleaved stereo; the kMinFreeSamples gate in feed()
    // guarantees this never overflows the ring.
    ring_.write(pcm_, static_cast<size_t>(samplesPerChannel) * 2);
    return;
  }
  // Mono: duplicate each sample into L+R, in small stack chunks.
  int16_t stereo[128];
  int i = 0;
  while (i < samplesPerChannel) {
    size_t j = 0;
    while (j < 64 && i < samplesPerChannel) {
      stereo[j * 2] = pcm_[i];
      stereo[j * 2 + 1] = pcm_[i];
      ++j;
      ++i;
    }
    ring_.write(stereo, j * 2);
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
