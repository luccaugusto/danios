// XPT2046 resistive touch -> LVGL pointer indev.
// (This board turned out to be the RESISTIVE 2432S024 variant — no CST820;
// see docs/VENDOR-NOTES.md and the Task 6 ledger for the discovery trail.)
// The XPT2046 shares the display's SPI bus; all bus handling, raw reads and
// raw->screen mapping live in LovyanGFX (LGFX_ESP32_2432S024C.hpp holds the
// measured calibration). This service polls getTouchRaw + convertRawXY —
// equivalent to getTouch(), split to keep raw coords for the serial
// verification contract — and feeds LVGL.
#pragma once

#include <lvgl.h>

#include "LGFX_ESP32_2432S024C.hpp"

class TouchService {
public:
  // Registers the LVGL pointer indev. `gfx` must outlive this service and
  // already be initialized (DisplayService::begin() first).
  void begin(LGFX* gfx);

private:
  static void readCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  // true while pressed; outputs both raw (for corner verification over
  // serial) and screen coords (clamped to 240x320 portrait).
  bool readTouch(int16_t& screen_x, int16_t& screen_y,
                 int16_t& raw_x, int16_t& raw_y);

  LGFX* gfx_ = nullptr;
  lv_indev_drv_t indevDrv_{};
  int16_t lastX_ = 0;  // LVGL wants the last point retained on release
  int16_t lastY_ = 0;
  bool wasPressed_ = false;
};
