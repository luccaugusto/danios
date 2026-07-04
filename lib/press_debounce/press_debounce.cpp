#include "press_debounce.h"

bool PressDebounce::update(bool raw_pressed) {
  if (raw_pressed) {
    misses_ = 0;
    pressed_ = true;
  } else if (pressed_ && ++misses_ > hold_) {
    misses_ = 0;
    pressed_ = false;
  }
  return pressed_;
}
