// Settings section builders (roadmap 4.11). Each builds its widgets into
// `parent` (a flex-column container provided by SettingsApp::showSection) and
// takes its dependencies by reference. F4/F5/A3 append their declarations here.
#pragma once
#include <lvgl.h>
#include <settings_store.h>

class DisplayService;
class StorageService;
class RadioManager;   // Task 3
class WiFiService;    // Task 4
class TimeService;    // Task 5
class BluetoothAudioService;  // F5

void buildDisplaySection(lv_obj_t* parent, ISettingsStore& store, DisplayService& display);
void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store);      // Task 8
void buildAboutSection(lv_obj_t* parent, StorageService& storage);    // Task 9
void buildWifiSection(lv_obj_t* parent, RadioManager& radio, WiFiService& wifi);
void buildClockSection(lv_obj_t* parent, TimeService& time);  // Task 7
void buildBluetoothSection(lv_obj_t* parent, RadioManager& radio,
                           BluetoothAudioService& bt);
