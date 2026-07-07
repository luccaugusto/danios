#include "DisplayService.h"

void DisplayService::begin() {
  tft_.init();
  tft_.setRotation(7);    // portrait, USB-C down (240x320) — docs/DISPLAY.md
  // Touch calibration measured on THIS unit with LovyanGFX calibrateTouch at
  // rotation 7 (docs/DISPLAY.md). Supersedes the earlier hand-measured min/max
  // box constants, which mis-scaled the horizontal axis so right-of-centre keys
  // registered one key too far right. 8 values = raw ADC at the 4 screen
  // corners; setTouchCalibrate builds the affine and MUST run after setRotation.
  static uint16_t kTouchCal[8] = {3830, 319, 3876, 3631, 724, 181, 521, 3482};
  tft_.setTouchCalibrate(kTouchCal);
  tft_.setBrightness(160);

  lv_init();
  lv_disp_draw_buf_init(&drawBuf_, buf1_, nullptr, kBufPixels);
  lv_disp_drv_init(&dispDrv_);
  dispDrv_.hor_res = kHorRes;
  dispDrv_.ver_res = kVerRes;
  dispDrv_.flush_cb = flushCb;
  dispDrv_.draw_buf = &drawBuf_;
  dispDrv_.user_data = this;
  lv_disp_drv_register(&dispDrv_);
}

void DisplayService::setBrightness(uint8_t level) {
  tft_.setBrightness(level);
}

void DisplayService::tick() {
  lv_timer_handler();
}

// Byte order: LV_COLOR_16_SWAP is 0 (lv_conf.h), so LVGL renders
// native-endian RGB565. Casting the buffer to lgfx::rgb565_t* makes
// LovyanGFX convert to the panel's big-endian order during the SPI write —
// the swap-free glue option from docs/hardware.md, pinned in this plan and
// documented in docs/DISPLAY.md "LVGL glue".
void DisplayService::flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                             lv_color_t* pixels) {
  auto* self = static_cast<DisplayService*>(drv->user_data);
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  self->tft_.startWrite();
  self->tft_.setAddrWindow(area->x1, area->y1, w, h);
  self->tft_.writePixels(reinterpret_cast<lgfx::rgb565_t*>(&pixels->full),
                         static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
  self->tft_.endWrite();
  lv_disp_flush_ready(drv);
}
