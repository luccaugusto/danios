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
#include "core/Version.h"
#include "services/DisplayService.h"  // F1 API
#include "services/LvglFs.h"
#include "services/SettingsService.h"
#include "services/StorageService.h"
#include "services/TouchService.h"    // F1 API

static DisplayService displayService;
static TouchService touchService;
static StatusBar statusBar;
static Launcher launcher(statusBar);

SettingsService settings;   // sole owner (roadmap 4.4); pass as ISettingsStore&
StorageService storage;     // sole owner (roadmap 4.9)

// Grid order = registration order (roadmap §4.5); ids pinned by App::id() docs.
// Names + icons are edited in src/apps/app_catalog.h, never here.
static StubApp weatherStub("weather", catalog::kWeather);
static StubApp musicStub("music", catalog::kMusic);
static StubApp calcStub("calc", catalog::kCalc);
static StubApp oracleStub("oracle", catalog::kOracle);
static StubApp petStub("pet", catalog::kPet);
static SettingsApp settingsApp;

// SD-missing error helper (LVGL v8 msgbox with a nullptr parent is created on
// the top layer and is modal, so it covers the launcher until dismissed).
static void sdErrorMsgboxCb(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  lv_msgbox_close(mbox);  // "OK" tapped -> launcher is already behind it
}

static void showSdMissingError() {
  static const char* kBtns[] = {"OK", ""};
  lv_obj_t* m = lv_msgbox_create(
      nullptr, "Sem cartão de memória",
      "Não encontrei o cartão microSD, então Clima, Música e\n"
      "Oráculo estão tirando uma soneca.\n\n"
      "Seu bichinho está bem - ele mora dentro de mim, não no cartão!\n\n"
      "Insira o cartão e reinicie para trazer tudo de volta.",
      kBtns, false);
  lv_obj_set_width(m, 230);  // near-full-width on the 240 px portrait screen
  lv_obj_center(m);
  lv_obj_add_event_cb(m, sdErrorMsgboxCb, LV_EVENT_VALUE_CHANGED, nullptr);
}

void setup() {
  Serial.begin(115200);

  // Spec 3.4 step 1: mount SD (VSPI, CS 5).
  const bool sdOk = storage.begin();
  // Spec 3.4 step 2: open settings (NVS namespace "danios").
  settings.begin();

  displayService.begin();  // F1 API: panel + LVGL + flush binding
  touchService.begin(&displayService.gfx());  // F1 API: XPT2046 -> LVGL indev

  // Apply persisted brightness (roadmap 4.2: disp.bright, default 160).
  displayService.setBrightness(static_cast<uint8_t>(settings.getU32("disp.bright", 160)));

  // Register the LVGL 'S:' drive so app icons/art load from SD (roadmap 4.1).
  if (sdOk) lvglFsRegisterSd();

  launcher.registerApp(&weatherStub);
  launcher.registerApp(&musicStub);
  launcher.registerApp(&calcStub);
  launcher.registerApp(&oracleStub);
  launcher.registerApp(&petStub);
  settingsApp.setDeps(settings, displayService, storage);
  launcher.registerApp(&settingsApp);  // sixth grid icon

  if (!sdOk) {
    // Spec 6.5: SD-dependent apps disabled. Pet is the exception (state in
    // NVS, placeholder art); Calculator and Settings never depend on the
    // card.
    launcher.setAppEnabled("weather", false);
    launcher.setAppEnabled("music", false);
    launcher.setAppEnabled("oracle", false);
  }

  launcher.show();
  if (!sdOk) showSdMissingError();  // modal on top of the launcher, dismissable
  Serial.println("danios: launcher up");
}

void loop() {
  displayService.tick();     // F1 API: LVGL tick + lv_timer_handler
  launcher.tick(millis());   // forwards to the active app
  delay(5);
}
