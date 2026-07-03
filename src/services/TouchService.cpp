#include "TouchService.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kAddr = 0x15;
constexpr int kPinSda = 33;
constexpr int kPinScl = 32;
constexpr int kPinRst = 25;
constexpr uint32_t kI2cHz = 400000;

// CST820 register map (register-compatible with CST816S basic reads —
// docs/hardware.md). We burst-read 6 bytes starting at 0x01:
//   0x01 GestureID   (unused — we poll raw points, no gesture handling)
//   0x02 FingerNum   (0 = no touch)
//   0x03 XposH       (bits 3:0 = X[11:8])
//   0x04 XposL       (X[7:0])
//   0x05 YposH       (bits 3:0 = Y[11:8])
//   0x06 YposL       (Y[7:0])
constexpr uint8_t kRegGesture = 0x01;
constexpr uint8_t kRegDisAutoSleep = 0xFE;  // non-zero disables auto-sleep
}  // namespace

void TouchService::begin() {
  // Raw space is landscape-native like the panel (320x240); the display runs
  // rotation 7 = swap + mirror both, so touch starts with the same flags.
  // Task 6 verifies these against on-screen targets and flips them if the
  // corner test disagrees (docs/DISPLAY.md gotcha).
  transform_.raw_w = 320;
  transform_.raw_h = 240;
  transform_.swap_xy = true;
  transform_.mirror_x = true;
  transform_.mirror_y = true;

  // The chip sleeps: pulse RST low->high, then wait ~300 ms before the first
  // I2C access (docs/hardware.md) or it simply won't ACK.
  pinMode(kPinRst, OUTPUT);
  digitalWrite(kPinRst, LOW);
  delay(10);
  digitalWrite(kPinRst, HIGH);
  delay(300);

  Wire.begin(kPinSda, kPinScl, kI2cHz);
  writeRegister(kRegDisAutoSleep, 0x01);  // keep it awake for polling

  lv_indev_drv_init(&indevDrv_);
  indevDrv_.type = LV_INDEV_TYPE_POINTER;
  indevDrv_.read_cb = readCb;
  indevDrv_.user_data = this;
  indev_ = lv_indev_drv_register(&indevDrv_);

  Serial.println("[danios] touch: CST820 polling indev registered");
}

void TouchService::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool TouchService::readRaw(int16_t& raw_x, int16_t& raw_y) {
  Wire.beginTransmission(kAddr);
  Wire.write(kRegGesture);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(kAddr, static_cast<uint8_t>(6)) != 6) return false;

  (void)Wire.read();                                   // 0x01 gesture (unused)
  const uint8_t fingers = Wire.read();                 // 0x02
  const uint8_t xh = Wire.read();                      // 0x03
  const uint8_t xl = Wire.read();                      // 0x04
  const uint8_t yh = Wire.read();                      // 0x05
  const uint8_t yl = Wire.read();                      // 0x06

  if (fingers == 0) return false;
  raw_x = static_cast<int16_t>(((xh & 0x0F) << 8) | xl);
  raw_y = static_cast<int16_t>(((yh & 0x0F) << 8) | yl);
  return true;
}

void TouchService::readCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  auto* self = static_cast<TouchService*>(drv->user_data);
  int16_t rawX = 0;
  int16_t rawY = 0;
  const bool pressed = self->readRaw(rawX, rawY);
  if (pressed) {
    const TouchPoint p = transformTouch(self->transform_, rawX, rawY);
    self->lastX_ = p.x;
    self->lastY_ = p.y;
    if (!self->wasPressed_) {
      // One line per touch-down. Task 6's four-corner verification reads
      // exactly this output over the serial monitor.
      Serial.printf("[touch] raw=(%d,%d) screen=(%d,%d)\n",
                    rawX, rawY, p.x, p.y);
    }
  }
  self->wasPressed_ = pressed;
  data->point.x = self->lastX_;  // keep last point on release (LVGL v8 rule)
  data->point.y = self->lastY_;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
