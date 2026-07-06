#include "apps/settings/SettingsApp.h"

void SettingsApp::buildUI(lv_obj_t* parent) {
  lv_obj_t* list = lv_list_create(parent);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));

  // ================= SECTION REGISTRATION POINT =================
  // Later plans append their section here, one file per section
  // (roadmap §4.11), each exposing:
  //   void buildSection(lv_obj_t* parent, /* deps by reference */);
  //
  //   DisplaySection.cpp, UnitsSection.cpp, AboutSection.cpp   (F3)
  //   WifiSection.cpp, ClockSection.cpp                        (F4)
  //   BluetoothSection.cpp                                     (F5)
  //   WeatherLocationSection.cpp                               (A3)
  //
  // Delete the placeholder row below when the first real section lands.
  // ===============================================================
  lv_list_add_text(list, "Nenhuma configuração ainda");
}
