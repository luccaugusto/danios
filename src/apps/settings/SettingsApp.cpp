#include "apps/settings/SettingsApp.h"

#include <cstdint>

#include "apps/settings/Sections.h"

namespace {
SettingsApp* g_self = nullptr;  // single instance, owned by main.cpp

// F4/F5/A3 sections append here and to the switch in showSection.
const char* kSectionNames[] = {"Tela", "Unidades", "Sobre", "WiFi", "Relógio"};
constexpr int kSectionCount =
    static_cast<int>(sizeof(kSectionNames) / sizeof(kSectionNames[0]));
}  // namespace

void SettingsApp::setDeps(ISettingsStore& store, DisplayService& display,
                          StorageService& storage, RadioManager& radio,
                          WiFiService& wifi, TimeService& time) {
  store_ = &store;
  display_ = &display;
  storage_ = &storage;
  radio_ = &radio;
  wifi_ = &wifi;
  time_ = &time;
}

void SettingsApp::buildUI(lv_obj_t* parent) {
  g_self = this;
  root_ = parent;
  showMenu();
}

void SettingsApp::showMenu() {
  inSection_ = false;
  lv_obj_clean(root_);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_t* list = lv_list_create(root_);
  lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
  for (int i = 0; i < kSectionCount; ++i) {
    lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_RIGHT, kSectionNames[i]);
    lv_obj_add_event_cb(btn, menuClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(i)));
  }
}

void SettingsApp::menuClicked(lv_event_t* e) {
  g_self->showSection(
      static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
}

bool SettingsApp::handleBack() {
  if (!inSection_) return false;  // at the menu -> let the Launcher go home
  showMenu();                     // in a section -> step up to the menu
  return true;
}

void SettingsApp::showSection(int idx) {
  inSection_ = true;
  lv_obj_clean(root_);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* body = lv_obj_create(root_);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, 12, 0);

  switch (idx) {
    case 0:
      buildDisplaySection(body, *store_, *display_);
      break;
    case 1:
      buildUnitsSection(body, *store_);
      break;
    case 2:
      buildAboutSection(body, *storage_);
      break;
    case 3:
      buildWifiSection(body, *radio_, *wifi_);
      break;
    case 4:
      buildClockSection(body, *time_);  // Task 7
      break;
    default:
      break;
  }
}
