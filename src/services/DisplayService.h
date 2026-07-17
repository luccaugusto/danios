// LVGL <-> LovyanGFX glue for the danios display. Owns the LGFX instance and
// the LVGL display driver. Read docs/DISPLAY.md before touching this file —
// the panel is a landscape-native 320x240 clone; orientation comes from layout::.
#pragma once

#include <lvgl.h>

#include "core/Layout.h"
#include "LGFX_ESP32_2432S024.hpp"

class DisplayService {
public:
  // Panel init (orientation from layout::, brightness 160), lv_init(),
  // one 7200 px draw buffer, flush-callback registration.
  // Call exactly once from setup(), before any other LVGL call.
  void begin();

  // Backlight PWM, 0-255. F3's Settings->Display drives this from NVS.
  void setBrightness(uint8_t level);

  // Pumps LVGL (lv_timer_handler). Call every loop() iteration, from the
  // Arduino loop task only — LVGL is not thread-safe (roadmap §2).
  void tick();

  // The LGFX instance also owns the XPT2046 touch driver (shared SPI bus) —
  // TouchService::begin() needs it. Valid only after begin().
  LGFX& gfx() { return tft_; }

private:
  static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                      lv_color_t* pixels);

  static constexpr int16_t kHorRes = layout::kScreenW;
  static constexpr int16_t kVerRes = layout::kScreenH;
  // Single buffer (14,400 bytes = 7200 px), same budget both orientations:
  // flushCb is synchronous, so a second buffer would never overlap render
  // with flush — it only earns its RAM if the flush ever goes async.
  static constexpr size_t kBufPixels = 7200;

  LGFX tft_;
  lv_disp_draw_buf_t drawBuf_{};
  lv_disp_drv_t dispDrv_{};
  // Heap-allocated once in begin() (never freed — DisplayService lives the whole
  // run). Keeping this 14.4 KB off the static dram0_0_seg segment is what leaves
  // room for the Bluetooth host stack's static buffers to link (F5).
  lv_color_t* buf1_ = nullptr;
};
