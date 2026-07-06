#include "core/StatusBar.h"

void StatusBar::build(lv_obj_t* parent, std::function<void()> onGear) {
  onGear_ = std::move(onGear);

  bar_ = lv_obj_create(parent);
  lv_obj_remove_style_all(bar_);
  lv_obj_set_size(bar_, 240, kHeight);
  lv_obj_set_pos(bar_, 0, 0);
  lv_obj_set_style_bg_color(bar_, lv_color_hex(0x1B2026), 0);
  lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

  clockLabel_ = lv_label_create(bar_);
  lv_label_set_text(clockLabel_, "--:--");  // F4 replaces via setClockText
  lv_obj_align(clockLabel_, LV_ALIGN_LEFT_MID, 6, 0);

  radioLabel_ = lv_label_create(bar_);
  lv_label_set_text(radioLabel_, "");  // RadioMode::None → blank
  lv_obj_align(radioLabel_, LV_ALIGN_RIGHT_MID, -64, 0);

  batteryLabel_ = lv_label_create(bar_);
  lv_label_set_text(batteryLabel_, "");  // §5.1: stays empty on this board
  lv_obj_align(batteryLabel_, LV_ALIGN_RIGHT_MID, -38, 0);

  lv_obj_t* gearBtn = lv_btn_create(bar_);
  lv_obj_set_size(gearBtn, 28, 20);
  lv_obj_align(gearBtn, LV_ALIGN_RIGHT_MID, -2, 0);
  lv_obj_set_ext_click_area(gearBtn, 8);  // 24 px bar → widen the touch target
  lv_obj_t* gearLabel = lv_label_create(gearBtn);
  lv_label_set_text(gearLabel, LV_SYMBOL_SETTINGS);
  lv_obj_center(gearLabel);
  lv_obj_add_event_cb(gearBtn, onGearClicked, LV_EVENT_CLICKED, this);
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

void StatusBar::onGearClicked(lv_event_t* e) {
  auto* self = static_cast<StatusBar*>(lv_event_get_user_data(e));
  if (self->onGear_) self->onGear_();
}
