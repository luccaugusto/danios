#include "DisplayService.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "core/Layout.h"

void DisplayService::begin() {
  tft_.init();
  // Portrait = rotation 7, USB-C down. Landscape = rotation 6, USB-C left —
  // pinned empirically by the Task 1 hardware probe (docs/DISPLAY.md).
  tft_.setRotation(layout::kLandscape ? 6 : 7);
  // Touch calibration measured on THIS unit with LovyanGFX calibrateTouch at
  // rotation 7 (docs/DISPLAY.md). Supersedes the earlier hand-measured min/max
  // box constants, which mis-scaled the horizontal axis so right-of-centre keys
  // registered one key too far right. 8 values = raw ADC at the 4 screen
  // corners; setTouchCalibrate builds the affine and MUST run after setRotation.
  // The landscape array is the rotation-6 calibrateTouch capture with its
  // corner pairs reordered 180° (TL<->BR, BL<->TR): this clone's hardware
  // mirror makes convertRawXY apply the wrong mirror-family transform at
  // rotation 6, and the reorder bakes the correction into the affine
  // (docs spec 2026-07-17). Re-captures must be reordered the same way.
  static uint16_t kTouchCalPortrait[8] = {3830, 319, 3876, 3631, 724, 181, 521, 3482};
  static uint16_t kTouchCalLandscape[8] = {652, 3313, 762, 181, 3841, 3586, 3837, 285};
  tft_.setTouchCalibrate(layout::kLandscape ? kTouchCalLandscape : kTouchCalPortrait);
  tft_.setBrightness(160);

  lv_init();
  // DMA-capable internal RAM (no PSRAM on this board). Allocated here rather
  // than as a static member so the 14.4 KB stays out of dram0_0_seg (see .h).
  buf1_ = static_cast<lv_color_t*>(
      heap_caps_malloc(kBufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA));
  if (buf1_ == nullptr) {
    Serial.println("[display] FATAL: draw buffer alloc failed");
    while (true) { delay(1000); }  // boot-critical: cannot run without it
  }
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
