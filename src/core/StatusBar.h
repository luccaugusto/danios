// src/core/StatusBar.h — 24 px top strip on the home screen (roadmap §4.5).
// Clock text ("--:--" until F4 wires TimeService), radio glyph, gear → Settings.
// Battery: roadmap deviation §5.1 — no battery ADC on this board; the
// setBatteryText hook renders nothing while its text is empty.
#pragma once

#include <lvgl.h>

#include <functional>

#include "App.h"  // RadioMode

class StatusBar {
 public:
  static constexpr lv_coord_t kHeight = 24;

  // Builds the bar as a child of `parent` (the launcher's home screen).
  // `onGear` is invoked when the gear button is tapped.
  void build(lv_obj_t* parent, std::function<void()> onGear);

  void setClockText(const char* text);    // F4 wires TimeService::hhmm here
  void setRadio(RadioMode mode);          // None → blank, WiFi/BT → LVGL symbol
  void setBatteryText(const char* text);  // "" (default) renders nothing — §5.1

 private:
  static void onGearClicked(lv_event_t* e);

  lv_obj_t* bar_ = nullptr;
  lv_obj_t* clockLabel_ = nullptr;
  lv_obj_t* radioLabel_ = nullptr;
  lv_obj_t* batteryLabel_ = nullptr;
  std::function<void()> onGear_;
};
