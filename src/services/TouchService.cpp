#include "TouchService.h"

#include <Arduino.h>

namespace {
int16_t clampTo(int32_t v, int32_t max_exclusive) {
  if (v < 0) return 0;
  if (v >= max_exclusive) return max_exclusive - 1;
  return static_cast<int16_t>(v);
}
}  // namespace

void TouchService::begin(LGFX* gfx) {
  gfx_ = gfx;

  lv_indev_drv_init(&indevDrv_);
  indevDrv_.type = LV_INDEV_TYPE_POINTER;
  indevDrv_.read_cb = readCb;
  indevDrv_.user_data = this;
  lv_indev_drv_register(&indevDrv_);

  Serial.println("[danios] touch: XPT2046 polling indev registered");
}

bool TouchService::readTouch(int16_t& screen_x, int16_t& screen_y,
                             int16_t& raw_x, int16_t& raw_y) {
  lgfx::touch_point_t tp;
  // getTouchRaw checks PENIRQ first (no SPI traffic when idle) and applies
  // the driver's median-of-7 + pressure filtering.
  if (gfx_->getTouchRaw(&tp, 1) == 0) return false;
  raw_x = static_cast<int16_t>(tp.x);
  raw_y = static_cast<int16_t>(tp.y);
  gfx_->convertRawXY(&tp, 1);  // calibration + rotation from the LGFX config
  // Post-rotation dimensions from LovyanGFX itself — the single source of
  // truth; convertRawXY does no bounds check, so clamp its output.
  screen_x = clampTo(tp.x, gfx_->width());
  screen_y = clampTo(tp.y, gfx_->height());
  return true;
}

void TouchService::readCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  auto* self = static_cast<TouchService*>(drv->user_data);
  int16_t sx = 0, sy = 0, rx = 0, ry = 0;
  const bool sampled = self->readTouch(sx, sy, rx, ry);
  if (sampled) {
    self->lastX_ = sx;
    self->lastY_ = sy;
    if (!self->wasPressed_) {
      // One line per touch-down. Task 6's four-corner verification reads
      // exactly this output over the serial monitor.
      Serial.printf("[touch] raw=(%d,%d) screen=(%d,%d)\n", rx, ry, sx, sy);
    }
  }
  const bool pressed = self->debounce_.update(sampled);
  self->wasPressed_ = pressed;
  data->point.x = self->lastX_;  // keep last point on release (LVGL v8 rule)
  data->point.y = self->lastY_;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
