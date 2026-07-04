// danios — F1 smoke screen: LVGL + touch, end to end.
// A centered "tap me" button and a counter label that increments per tap.
// Proves: DisplayService flush path, TouchService -> LVGL pointer indev,
// LVGL event dispatch, and the loop-task tick. Replaced by the Launcher in F2.
//
// (The milestone-1 display diagnostic this file replaces lives in the initial
// git commit if it's ever needed again.)
#include <Arduino.h>
#include <lvgl.h>

#include "services/DisplayService.h"
#include "services/TouchService.h"

DisplayService displayService;
TouchService touchService;

namespace {

lv_obj_t* counterLabel = nullptr;
uint32_t tapCount = 0;

void onTap(lv_event_t*) {
  ++tapCount;
  lv_label_set_text_fmt(counterLabel, "taps: %u",
                        static_cast<unsigned>(tapCount));
  Serial.printf("[danios] tap %u\n", static_cast<unsigned>(tapCount));
}

void buildSmokeScreen() {
  lv_obj_t* scr = lv_scr_act();

  lv_obj_t* btn = lv_btn_create(scr);
  lv_obj_set_size(btn, 120, 60);
  lv_obj_center(btn);
  lv_obj_add_event_cb(btn, onTap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* btnLabel = lv_label_create(btn);
  lv_label_set_text(btnLabel, "tap me");
  lv_obj_center(btnLabel);

  counterLabel = lv_label_create(scr);
  lv_label_set_text(counterLabel, "taps: 0");
  lv_obj_align(counterLabel, LV_ALIGN_CENTER, 0, 70);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("\n[danios] F1 smoke screen starting");

  displayService.begin();  // panel + LVGL + flush (must be first)
  touchService.begin(&displayService.gfx());  // XPT2046 -> LVGL indev
  buildSmokeScreen();

  Serial.println("[danios] UI up — tap the button");
}

void loop() {
  displayService.tick();  // lv_timer_handler()
  delay(5);
}
