#include "core/StatusBar.h"

#include "core/Layout.h"

void StatusBar::build(lv_obj_t* parent) {
  bar_ = lv_obj_create(parent);
  lv_obj_remove_style_all(bar_);
  lv_obj_set_size(bar_, layout::kScreenW, kHeight);
  lv_obj_set_pos(bar_, 0, 0);
  lv_obj_set_style_bg_color(bar_, lv_color_hex(0x1B2026), 0);
  lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

  clockLabel_ = lv_label_create(bar_);
  lv_label_set_text(clockLabel_, "--:--");  // F4 replaces via setClockText
  lv_obj_align(clockLabel_, LV_ALIGN_LEFT_MID, 6, 0);

  radioLabel_ = lv_label_create(bar_);
  lv_label_set_text(radioLabel_, "");  // RadioMode::None → blank
  lv_obj_align(radioLabel_, LV_ALIGN_RIGHT_MID, -32, 0);

  batteryLabel_ = lv_label_create(bar_);
  lv_label_set_text(batteryLabel_, "");  // §5.1: stays empty on this board
  lv_obj_align(batteryLabel_, LV_ALIGN_RIGHT_MID, -6, 0);
}

void StatusBar::setClockText(const char* text) {
  if (clockLabel_) lv_label_set_text(clockLabel_, text);
}

void StatusBar::setRadio(RadioMode mode) {
  if (!radioLabel_) return;
  switch (mode) {
    case RadioMode::WiFi:
      lv_label_set_text(radioLabel_, LV_SYMBOL_WIFI);
      break;
    case RadioMode::Bluetooth:
      lv_label_set_text(radioLabel_, LV_SYMBOL_BLUETOOTH);
      break;
    case RadioMode::None:
      lv_label_set_text(radioLabel_, "");
      break;
  }
}

void StatusBar::setBatteryText(const char* text) {
  if (batteryLabel_) lv_label_set_text(batteryLabel_, text);
}
