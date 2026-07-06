#include "apps/settings/SettingsApp.h"

#include <cstdint>

#include "apps/settings/Sections.h"

namespace {
SettingsApp* g_self = nullptr;  // single instance, owned by main.cpp

// Tasks 8 and 9 extend this array and the switch in showSection.
const char* kSectionNames[] = {"Tela", "Unidades"};
constexpr int kSectionCount =
    static_cast<int>(sizeof(kSectionNames) / sizeof(kSectionNames[0]));
}  // namespace

void SettingsApp::setDeps(ISettingsStore& store, DisplayService& display,
                          StorageService& storage) {
  store_ = &store;
  display_ = &display;
  storage_ = &storage;
}

void SettingsApp::buildUI(lv_obj_t* parent) {
  g_self = this;
  root_ = parent;
  showMenu();
}

void SettingsApp::showMenu() {
  lv_obj_clean(root_);
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

void SettingsApp::backClicked(lv_event_t* /*e*/) {
  g_self->showMenu();
}

void SettingsApp::showSection(int idx) {
  lv_obj_clean(root_);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* back = lv_btn_create(root_);
  lv_obj_t* backLbl = lv_label_create(back);
  lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Voltar");
  lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, nullptr);

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
    default:
      break;
  }
}
