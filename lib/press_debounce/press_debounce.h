// Debounce for a polled touch signal (pure std-C++ — native-testable).
//
// A press is reported immediately; a release is reported only after more
// than `hold_polls` consecutive empty polls. Rationale: the XPT2046 driver's
// median/pressure filter can reject one sample mid-press, and reporting that
// to LVGL as release+repress fires CLICKED twice per physical tap. At the
// ~30 ms LVGL indev period the default (1 held poll) adds at most ~60 ms of
// release latency.
#pragma once

#include <cstdint>

class PressDebounce {
public:
  explicit PressDebounce(uint8_t hold_polls = 1) : hold_(hold_polls) {}

  // Feed one raw poll result; returns the debounced pressed state.
  bool update(bool raw_pressed);

private:
  uint8_t hold_;
  uint8_t misses_ = 0;
  bool pressed_ = false;
};
