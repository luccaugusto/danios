// src/apps/music/Mp3Player.h — the SD-file -> MP3-decode -> PCM-ring pipeline
// (A4, spec §4.2). Decoder wiring lives with the app, not the service: the
// service only moves PCM frames (spec architecture rule).
//
// Decoder is minimp3 (vendored, lib/minimp3, CC0), replacing arduino-libhelix:
// helix needed ~30 KB of heap per open and its allocator hangs in while(true)
// on OOM — structurally unfittable beside BT Classic on this no-PSRAM board
// (measured 2026-07-14: 13-22 KB free once the A2DP link is up). minimp3 keeps
// all state in this object (~15.5 KB, allocated once with the ring before the
// link comes up) and never touches the heap while decoding.
//
// Threading: feed() decodes on the Arduino loop task (called from
// MusicApp::tick); sourceCallback() drains the ring on the Bluetooth stack
// task. PcmRing is the only object both touch; playing_/volumePct_ are atomics.
//
// Format contract (spec RAM constraint): 44.1 kHz CBR MP3s, mono or stereo.
// Mono is upmixed to stereo here; any other sample rate marks the track bad
// (no resampler on a no-PSRAM board) and feed() reports Error -> skip.
#pragma once

#include <SD.h>
#include <minimp3.h>
#include <pcm_ring.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

class Mp3Player {
 public:
  enum class Feed : uint8_t { Ok, End, Error };

  Mp3Player();
  ~Mp3Player();

  // Open "/music/<file>". flushRing=true (manual track change) drops the
  // previous track's buffered tail; false (auto-advance at end of track)
  // keeps it so playback rolls over seamlessly. false = unreadable file.
  bool open(const char* path, bool flushRing);
  void close();

  Feed feed();  // decode a little more into the ring; loop-task only
  void setPlaying(bool on) { playing_.store(on); }
  bool playing() const { return playing_.load(); }
  void setVolume(uint8_t pct) { volumePct_.store(pct > 100 ? 100 : pct); }
  uint8_t volume() const { return volumePct_.load(); }

  // roadmap §4.10 AudioSourceFn — register with bt.setSource(..., player).
  // Runs on the BT task: reads the ring, applies the volume gain, returns
  // frames written (0 while paused or on underrun = silence).
  static int32_t sourceCallback(int16_t* stereo_buf, int32_t frames, void* ctx);

 private:
  void writePcm(int samplesPerChannel, int channels);  // upmixes mono

  static constexpr size_t kInBufBytes = 4096;  // >= 2 max frames of lookahead

  File file_;
  mp3dec_t dec_;                // ~6.7 KB decoder state, no hidden allocs
  uint8_t inBuf_[kInBufBytes];  // raw MP3 bytes from SD, compacted after use
  size_t inLen_ = 0;
  int16_t pcm_[MINIMP3_MAX_SAMPLES_PER_FRAME];  // one decoded frame (4.6 KB)
  PcmRing ring_;
  std::atomic<bool> playing_{false};
  std::atomic<uint8_t> volumePct_{60};  // session-only: A4 owns no NVS keys
};
