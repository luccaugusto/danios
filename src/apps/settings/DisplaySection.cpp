#include "apps/settings/Sections.h"

#include "services/DisplayService.h"
#include <esp_system.h>

namespace {

// Valid while the section is on screen (sections are rebuilt on every visit;
// the deps are process-lifetime singletons owned by main.cpp).
struct Ctx {
  ISettingsStore* store = nullptr;
  DisplayService* display = nullptr;
};
Ctx ctx;

const uint32_t kSleepSeconds[] = {0, 30, 60, 120, 300};
const int kSleepCount = 5;

void brightnessChanged(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  const int32_t v = lv_slider_get_value(slider);
  ctx.display->setBrightness(static_cast<uint8_t>(v));  // live preview
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    ctx.store->setU32("disp.bright", static_cast<uint32_t>(v));  // persist once
  }
}

void sleepChanged(lv_event_t* e) {
  lv_obj_t* dd = lv_event_get_target(e);
  const uint16_t idx = lv_dropdown_get_selected(dd);
  if (idx < kSleepCount) {
    ctx.store->setU32("disp.sleep_s", kSleepSeconds[idx]);
  }
}

void orientationMsgboxCb(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  const uint16_t btn = lv_msgbox_get_active_btn(mbox);
  lv_msgbox_close(mbox);
  if (btn == 0) esp_restart();  // "Reiniciar"
}

void orientationChanged(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  const bool landscape = lv_obj_has_state(sw, LV_STATE_CHECKED);
  ctx.store->setBool("disp.landscape", landscape);
  static const char* kBtns[] = {"Reiniciar", "Depois", ""};
  lv_obj_t* m = lv_msgbox_create(NULL, "Orientação",
                                 "Reinicie para aplicar.", kBtns, true);
  lv_obj_add_event_cb(m, orientationMsgboxCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_center(m);
}

}  // namespace

void buildDisplaySection(lv_obj_t* parent, ISettingsStore& store, DisplayService& display) {
  ctx.store = &store;
  ctx.display = &display;

  lv_obj_t* brightLbl = lv_label_create(parent);
  lv_label_set_text(brightLbl, "Brilho");

  lv_obj_t* slider = lv_slider_create(parent);
  lv_obj_set_width(slider, LV_PCT(90));
  lv_slider_set_range(slider, 10, 255);  // 10 = floor: never fully dark
  lv_slider_set_value(slider, static_cast<int32_t>(store.getU32("disp.bright", 160)),
                      LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightnessChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(slider, brightnessChanged, LV_EVENT_RELEASED, nullptr);

  lv_obj_t* sleepLbl = lv_label_create(parent);
  lv_label_set_text(sleepLbl, "Suspender tela");

  lv_obj_t* dd = lv_dropdown_create(parent);
  lv_obj_set_width(dd, LV_PCT(90));
  lv_dropdown_set_options(dd, "Nunca\n30 segundos\n1 minuto\n2 minutos\n5 minutos");
  const uint32_t cur = store.getU32("disp.sleep_s", 60);
  uint16_t sel = 2;  // "1 minuto" — matches the 60 s default
  for (int i = 0; i < kSleepCount; ++i) {
    if (kSleepSeconds[i] == cur) { sel = static_cast<uint16_t>(i); break; }
  }
  lv_dropdown_set_selected(dd, sel);
  lv_obj_add_event_cb(dd, sleepChanged, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* orientLbl = lv_label_create(parent);
  lv_label_set_text(orientLbl, "Deitado (USB à esquerda)");

  lv_obj_t* sw = lv_switch_create(parent);
  if (store.getBool("disp.landscape", false))
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, orientationChanged, LV_EVENT_VALUE_CHANGED, nullptr);
}
