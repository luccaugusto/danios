// CST820 capacitive touch -> LVGL pointer indev.
// POLLED over I2C: the INT line is GPIO 21 on some units and 22 on others
// (docs/hardware.md) — do not use it. Raw coordinates are landscape-native
// like the panel and are mapped to portrait rotation-7 screen space by
// lib/touch_transform. The swap/mirror flags below are VERIFIED ON-SCREEN in
// this plan's final task (docs/DISPLAY.md: never trust board-def touch flags).
#pragma once

#include <lvgl.h>
#include <touch_transform.h>

class TouchService {
public:
  // CST820 reset pulse + ~300 ms wake wait, I2C init (SDA 33, SCL 32,
  // 400 kHz), auto-sleep disable, LVGL pointer-indev registration.
  // Call exactly once, after DisplayService::begin().
  void begin();

private:
  static void readCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  bool readRaw(int16_t& raw_x, int16_t& raw_y);  // true while a finger is down
  void writeRegister(uint8_t reg, uint8_t value);

  TouchTransform transform_{};
  lv_indev_drv_t indevDrv_{};
  lv_indev_t* indev_ = nullptr;
  int16_t lastX_ = 0;  // LVGL wants the last point retained on release
  int16_t lastY_ = 0;
  bool wasPressed_ = false;
};
