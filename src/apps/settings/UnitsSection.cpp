#include "apps/settings/Sections.h"

namespace {
ISettingsStore* g_store = nullptr;  // valid while the section is on screen

void unitsChanged(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  g_store->setBool("units.f", lv_obj_has_state(sw, LV_STATE_CHECKED));
}
}  // namespace

void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store) {
  g_store = &store;

  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, "Unidade de temperatura\ndesligado = Celsius, ligado = Fahrenheit");

  lv_obj_t* sw = lv_switch_create(parent);
  if (store.getBool("units.f", false)) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw, unitsChanged, LV_EVENT_VALUE_CHANGED, nullptr);
}
