// Settings -> Local do clima (A3 spec): auto (default — ip-api geolocation on
// the next weather fetch) vs. manual override (city/lat/lon typed in).
// Owns no radio and does no network itself — it only edits the loc.* keys the
// weather fetch pipeline reads.
#include <lvgl.h>

#include <cstdio>
#include <cstdlib>

#include "Sections.h"

namespace {
struct LocUi {
  ISettingsStore* store;
  lv_obj_t* modeSwitch;
  lv_obj_t* manualBox;  // hidden while auto
  lv_obj_t* city;
  lv_obj_t* lat;
  lv_obj_t* lon;
  lv_obj_t* status;
  lv_obj_t* kb;  // top-layer keyboard bound to a manualBox textarea, if open
};
LocUi ui;  // one Settings screen at a time (single LVGL task) — safe

void refreshVisibility() {
  if (ui.store->getU32("loc.mode", 0) == 1) {
    lv_obj_clear_flag(ui.manualBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui.manualBox, LV_OBJ_FLAG_HIDDEN);
  }
}

void modeChanged(lv_event_t*) {
  const bool autoMode = lv_obj_has_state(ui.modeSwitch, LV_STATE_CHECKED);
  ui.store->setU32("loc.mode", autoMode ? 0 : 1);
  if (autoMode) {
    // Drop the stored place so the next weather fetch geolocates afresh.
    ui.store->remove("loc.lat");
    ui.store->remove("loc.lon");
    ui.store->remove("loc.city");
    lv_label_set_text(ui.status, "Local automático (pela internet)");
  } else {
    lv_label_set_text(ui.status, "Digite o local e salve");
  }
  refreshVisibility();
}

void kbEvent(lv_event_t* e) {
  // READY (checkmark) or CANCEL: done typing — the textarea keeps its text.
  lv_obj_del_async(lv_event_get_current_target(e));
}

void kbDeleted(lv_event_t*) {
  // Fires on every deletion path (READY/CANCEL async delete, or teardown).
  ui.kb = nullptr;
}

void manualBoxDeleted(lv_event_t*) {
  // Section body torn down while the keyboard was open — its textarea
  // binding is about to go stale, so close it now (kb lives on the
  // unrelated top layer, so this is safe to do synchronously).
  if (ui.kb != nullptr) lv_obj_del(ui.kb);
}

void taClicked(lv_event_t* e) {
  lv_obj_t* ta = lv_event_get_current_target(e);
  if (ui.kb != nullptr) lv_obj_del(ui.kb);
  lv_obj_t* kb = lv_keyboard_create(lv_layer_top());
  lv_keyboard_set_textarea(kb, ta);
  if (ta != ui.city) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_add_event_cb(kb, kbEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, kbEvent, LV_EVENT_CANCEL, nullptr);
  lv_obj_add_event_cb(kb, kbDeleted, LV_EVENT_DELETE, nullptr);
  ui.kb = kb;
}

lv_obj_t* makeEntry(lv_obj_t* parent, const char* placeholder,
                    const char* value) {
  lv_obj_t* ta = lv_textarea_create(parent);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_textarea_set_text(ta, value);
  lv_obj_add_event_cb(ta, taClicked, LV_EVENT_CLICKED, nullptr);
  return ta;
}

void saveClicked(lv_event_t*) {
  const char* latTxt = lv_textarea_get_text(ui.lat);
  const char* lonTxt = lv_textarea_get_text(ui.lon);
  char* latEnd = nullptr;
  char* lonEnd = nullptr;
  const float lat = strtof(latTxt, &latEnd);
  const float lon = strtof(lonTxt, &lonEnd);
  // strtof() accepts empty/partial input and silently returns 0.0f — reject
  // anything that didn't fully parse as a number, not just out-of-range.
  if (latEnd == latTxt || *latEnd != '\0' || lonEnd == lonTxt ||
      *lonEnd != '\0' || lat < -90.0f || lat > 90.0f || lon < -180.0f ||
      lon > 180.0f) {
    lv_label_set_text(ui.status, "Coordenadas inválidas");
    return;
  }
  ui.store->setU32("loc.mode", 1);
  ui.store->setFloat("loc.lat", lat);
  ui.store->setFloat("loc.lon", lon);
  ui.store->setString("loc.city", lv_textarea_get_text(ui.city));
  ui.store->remove("wx.day");  // next fetch is fresh for the new place
  lv_label_set_text(ui.status, "Local salvo " LV_SYMBOL_OK);
}
}  // namespace

void buildWeatherLocationSection(lv_obj_t* parent, ISettingsStore& store) {
  ui = {};
  ui.store = &store;

  const bool manual = store.getU32("loc.mode", 0) == 1;

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, "Local automático");
  ui.modeSwitch = lv_switch_create(row);
  if (!manual) lv_obj_add_state(ui.modeSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(ui.modeSwitch, modeChanged, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  ui.manualBox = lv_obj_create(parent);
  lv_obj_add_event_cb(ui.manualBox, manualBoxDeleted, LV_EVENT_DELETE,
                      nullptr);
  lv_obj_set_width(ui.manualBox, LV_PCT(100));
  lv_obj_set_height(ui.manualBox, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ui.manualBox, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(ui.manualBox, 8, 0);

  const float lat = store.getFloat("loc.lat", 999.0f);
  const float lon = store.getFloat("loc.lon", 999.0f);
  char num[16];
  ui.city = makeEntry(ui.manualBox, "Cidade",
                      store.getString("loc.city", "").c_str());
  snprintf(num, sizeof(num), "%.4f", static_cast<double>(lat));
  ui.lat = makeEntry(ui.manualBox, "Latitude", lat > 900.0f ? "" : num);
  snprintf(num, sizeof(num), "%.4f", static_cast<double>(lon));
  ui.lon = makeEntry(ui.manualBox, "Longitude", lon > 900.0f ? "" : num);

  lv_obj_t* saveBtn = lv_btn_create(ui.manualBox);
  lv_obj_t* saveLbl = lv_label_create(saveBtn);
  lv_label_set_text(saveLbl, LV_SYMBOL_OK " Salvar");
  lv_obj_add_event_cb(saveBtn, saveClicked, LV_EVENT_CLICKED, nullptr);

  ui.status = lv_label_create(parent);
  lv_label_set_text(ui.status, manual ? "Local manual"
                                      : "Local automático (pela internet)");

  refreshVisibility();
}
