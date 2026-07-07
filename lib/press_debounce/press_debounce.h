// Debounce for a polled touch signal (pure std-C++ — native-testable).
//
// Two edges, two guards:
//  - release_hold: a release is reported only after more than `release_hold`
//    consecutive empty polls. The XPT2046 driver's median/pressure filter can
//    reject one sample mid-press; reporting that to LVGL as release+repress
//    fires CLICKED twice per physical tap.
//  - press_settle: a press is reported only after `press_settle` consecutive
//    touched polls. The resistive panel's FIRST contact sample reads offset
//    (down-and-right) while pressure is still building, then settles over the
//    next poll or two; requiring 2 hits discards that offset sample so LVGL
//    only ever sees a settled coordinate. Default 1 = report immediately.
//
// At the ~30 ms LVGL indev period each held poll adds ~30 ms of latency.
#pragma once

#include <cstdint>

class PressDebounce {
public:
  explicit PressDebounce(uint8_t release_hold = 1, uint8_t press_settle = 1)
      : release_hold_(release_hold), press_settle_(press_settle) {}

  // Feed one raw poll result; returns the debounced pressed state.
  bool update(bool raw_pressed);

private:
  uint8_t release_hold_;
  uint8_t press_settle_;
  uint8_t misses_ = 0;
  uint8_t hits_ = 0;
  bool pressed_ = false;
};
