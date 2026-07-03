// Raw capacitive-touch → screen coordinate mapping. PURE LOGIC: std C++ only,
// zero Arduino/LVGL includes (roadmap §2 — native-tested with pio test -e native).
//
// The danios CST820 reports raw coordinates in the panel's landscape-native
// 320x240 space; the UI is portrait 240x320 at LovyanGFX rotation 7
// (swap + mirror both). The swap/mirror flags are runtime parameters because
// docs/DISPLAY.md requires verifying them against on-screen targets — a
// mismatch is fixed by flipping a flag in TouchService, not by editing logic.
#pragma once
#include <cstdint>

struct TouchPoint {
  int16_t x;
  int16_t y;
};

struct TouchTransform {
  int16_t raw_w = 0;      // raw coordinate-space width  (danios CST820: 320)
  int16_t raw_h = 0;      // raw coordinate-space height (danios CST820: 240)
  bool swap_xy = false;   // swap axes first (landscape-native -> portrait)
  bool mirror_x = false;  // then mirror across the OUTPUT-space width
  bool mirror_y = false;  // then mirror across the OUTPUT-space height
};

// Clamps (raw_x, raw_y) into [0, raw_w) x [0, raw_h), applies swap then
// mirrors. Output space is raw_h x raw_w when swap_xy is set, else
// raw_w x raw_h — so the result is always a valid on-screen point.
TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y);
