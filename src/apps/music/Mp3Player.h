// src/apps/music/Mp3Player.h — the SD-file -> MP3-decode -> PCM-ring pipeline
// (A4, spec §4.2). Decoder wiring lives with the app, not the service: the
// service only moves PCM frames (spec architecture rule).
//
// Threading: feed() decodes on the Arduino loop task (called from
// MusicApp::tick); sourceCallback() drains the ring on the Bluetooth stack
// task. PcmRing is the only object both touch; playing_/volumePct_ are atomics.
//
// Format contract (spec RAM constraint): 44.1 kHz CBR MP3s, mono or stereo.
// Mono is upmixed to stereo here; any other sample rate marks the track bad
// (no resampler on a no-PSRAM board) and feed() reports Error -> skip.
#pragma once

#include <MP3DecoderHelix.h>
#include <SD.h>
#include <pcm_ring.h>

#include <atomic>
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
  static void pcmCallback(MP3FrameInfo& info, short* pcm, size_t len, void* ref);

  File file_;
  libhelix::MP3DecoderHelix decoder_;
  PcmRing ring_;
  std::atomic<bool> playing_{false};
  std::atomic<uint8_t> volumePct_{60};  // session-only: A4 owns no NVS keys
  bool badFormat_ = false;  // set by pcmCallback on unsupported sample rate
};
