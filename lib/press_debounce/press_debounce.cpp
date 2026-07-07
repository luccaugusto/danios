#include "press_debounce.h"

bool PressDebounce::update(bool raw_pressed) {
  if (raw_pressed) {
    misses_ = 0;
    // Require `press_settle` consecutive hits before latching pressed, so the
    // offset first-contact sample is discarded (see header).
    if (!pressed_ && ++hits_ >= press_settle_) {
      pressed_ = true;
    }
  } else {
    hits_ = 0;
    if (pressed_ && ++misses_ > release_hold_) {
      misses_ = 0;
      pressed_ = false;
    }
  }
  return pressed_;
}
