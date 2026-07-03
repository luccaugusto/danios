// LVGL <-> LovyanGFX glue for the danios display. Owns the LGFX instance and
// the LVGL display driver. Read docs/DISPLAY.md before touching this file —
// the panel is a landscape-native 320x240 clone; portrait UI = rotation 7.
#pragma once

#include <lvgl.h>

#include "LGFX_ESP32_2432S024C.hpp"

class DisplayService {
public:
  // Panel init (rotation 7 portrait 240x320, brightness 160), lv_init(),
  // two 240x30 draw buffers, flush-callback registration.
  // Call exactly once from setup(), before any other LVGL call.
  void begin();

  // Backlight PWM, 0-255. F3's Settings->Display drives this from NVS.
  void setBrightness(uint8_t level);

  // Pumps LVGL (lv_timer_handler). Call every loop() iteration, from the
  // Arduino loop task only — LVGL is not thread-safe (roadmap §2).
  void tick();

private:
  static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                      lv_color_t* pixels);

  static constexpr int16_t kHorRes = 240;
  static constexpr int16_t kVerRes = 320;
  static constexpr size_t kBufPixels = 240 * 30;  // per buffer; x2 ~= 28.8 KB

  LGFX tft_;
  lv_disp_draw_buf_t drawBuf_{};
  lv_disp_drv_t dispDrv_{};
  lv_color_t buf1_[kBufPixels];
  lv_color_t buf2_[kBufPixels];
};
