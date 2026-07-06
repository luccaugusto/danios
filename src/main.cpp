// src/main.cpp — danios boot flow + main loop (spec §3.4/§3.5, F2 slice).
// F3 adds storage/settings init; F4 adds radio/time init + setRadioRequest;
// each app plan (A1–A5) replaces its StubApp registration with the real app.
#include <Arduino.h>
#include <lvgl.h>

#include "apps/StubApp.h"
#include "apps/app_catalog.h"
#include "apps/settings/SettingsApp.h"
#include "core/Launcher.h"
#include "core/StatusBar.h"
#include "services/DisplayService.h"  // F1 API
#include "services/TouchService.h"    // F1 API

static DisplayService displayService;
static TouchService touchService;
static StatusBar statusBar;
static Launcher launcher(statusBar);

// Grid order = registration order (roadmap §4.5); ids pinned by App::id() docs.
// Names + icons are edited in src/apps/app_catalog.h, never here.
static StubApp weatherStub("weather", catalog::kWeather);
static StubApp musicStub("music", catalog::kMusic);
static StubApp calcStub("calc", catalog::kCalc);
static StubApp oracleStub("oracle", catalog::kOracle);
static StubApp petStub("pet", catalog::kPet);
static SettingsApp settingsApp;

void setup() {
  Serial.begin(115200);

  displayService.begin();  // F1 API: panel + LVGL + flush binding
  touchService.begin(&displayService.gfx());  // F1 API: XPT2046 -> LVGL indev

  launcher.registerApp(&weatherStub);
  launcher.registerApp(&musicStub);
  launcher.registerApp(&calcStub);
  launcher.registerApp(&oracleStub);
  launcher.registerApp(&petStub);
  launcher.registerApp(&settingsApp);  // sixth grid icon

  launcher.show();
  Serial.println("danios: launcher up");
}

void loop() {
  displayService.tick();     // F1 API: LVGL tick + lv_timer_handler
  launcher.tick(millis());   // forwards to the active app
  delay(5);
}
