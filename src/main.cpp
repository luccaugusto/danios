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
#include "services/BluetoothAudioService.h"
#include "services/DisplayService.h"  // F1 API
#include "services/LvglFs.h"
#include "services/RadioManager.h"
#include "services/SettingsService.h"
#include "services/StorageService.h"
#include "services/TimeService.h"
#include "services/TouchService.h"    // F1 API
#include "services/WiFiService.h"

static DisplayService displayService;
static TouchService touchService;
static StatusBar statusBar;
static Launcher launcher(statusBar);

SettingsService settings;   // sole owner (roadmap 4.4); pass as ISettingsStore&
StorageService storage;     // sole owner (roadmap 4.9)
static RadioManager radioManager;  // sole owner of radio power (roadmap 4.6)
static WiFiService wifiService(settings);
static TimeService timeService(radioManager, wifiService, settings);
static BluetoothAudioService btAudio(settings);

// Grid order = registration order (roadmap §4.5); ids pinned by App::id() docs.
// Names + icons are edited in src/apps/app_catalog.h, never here.
static StubApp weatherStub("weather", catalog::kWeather);
static StubApp musicStub("music", catalog::kMusic);
static StubApp calcStub("calc", catalog::kCalc);
static StubApp oracleStub("oracle", catalog::kOracle);
static StubApp petStub("pet", catalog::kPet);
static SettingsApp settingsApp;

// --- Screen sleep (spec 6.4): backlight off after disp.sleep_s of inactivity;
// --- any touch wakes; the waking tap is swallowed by a full-screen shield.
static lv_obj_t* g_sleepShield = nullptr;
static bool g_screenAsleep = false;

static void sleepShieldEvent(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    // Light up while the finger is still down.
    displayService.setBrightness(
        static_cast<uint8_t>(settings.getU32("disp.bright", 160)));
  } else if (code == LV_EVENT_RELEASED) {
    lv_obj_del_async(g_sleepShield);  // safe self-delete from own handler
    g_sleepShield = nullptr;
    g_screenAsleep = false;
  }
}

static void sleepTick() {
  static uint32_t lastCheck = 0;
  const uint32_t now = millis();
  if (now - lastCheck < 500) return;  // throttle: check twice a second
  lastCheck = now;

  if (g_screenAsleep) return;
  const uint32_t timeoutS = settings.getU32("disp.sleep_s", 60);
  if (timeoutS == 0) return;  // 0 = never sleep

  if (lv_disp_get_inactive_time(NULL) >= timeoutS * 1000u) {
    g_sleepShield = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_sleepShield);  // fully transparent
    lv_obj_set_size(g_sleepShield, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(g_sleepShield, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_sleepShield, sleepShieldEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_sleepShield, sleepShieldEvent, LV_EVENT_RELEASED, nullptr);
    displayService.setBrightness(0);  // backlight off — display asleep
    g_screenAsleep = true;
  }
}

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
  timeService.begin();  // apply persisted TZ before anything reads the clock

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
  settingsApp.setDeps(settings, displayService, storage, radioManager,
                      wifiService, timeService, btAudio);
  launcher.registerApp(&settingsApp);  // sixth grid icon
  launcher.setRadioRequest([](RadioMode m) { return radioManager.request(m); });
  radioManager.setBluetoothService(&btAudio);

  if (!sdOk) {
    // Spec 6.5: SD-dependent apps disabled. Pet is the exception (state in
    // NVS, placeholder art); Calculator and Settings never depend on the
    // card.
    launcher.setAppEnabled("weather", false);
    launcher.setAppEnabled("music", false);
    launcher.setAppEnabled("oracle", false);
  }

  // Boot flow step 4 (spec §3.4): if a network is saved, bring WiFi up briefly
  // to sync time, prefetch weather, then drop the radio. Bounded ~23 s worst
  // case (8 s connect + 15 s NTP); a splash label keeps the screen honest.
  if (wifiService.hasCredentials()) {
    lv_obj_t* splash = lv_label_create(lv_scr_act());
    lv_label_set_text(splash, "Conectando" LV_SYMBOL_WIFI);
    lv_obj_center(splash);
    lv_refr_now(nullptr);

    if (radioManager.request(RadioMode::WiFi) && wifiService.connect(8000)) {
      timeService.syncNow();  // already WiFiOn -> no radio dance inside
      // A3: weather boot prefetch hook
    }
    radioManager.request(RadioMode::None);
    lv_obj_del(splash);
  }

  launcher.show();
  if (!sdOk) showSdMissingError();  // modal on top of the launcher, dismissable
  Serial.println("danios: launcher up");
}

void loop() {
  displayService.tick();     // F1 API: LVGL tick + lv_timer_handler
  launcher.tick(millis());   // forwards to the active app
  sleepTick();

  // Status bar: clock + radio glyph, once per second (spec §3.3).
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus >= 1000) {
    lastStatus = millis();
    char clock[6];
    timeService.hhmm(clock);
    statusBar.setClockText(clock);
    switch (radioManager.current()) {
      case RadioState::WiFiOn: statusBar.setRadio(RadioMode::WiFi); break;
      case RadioState::BtOn:   statusBar.setRadio(RadioMode::Bluetooth); break;
      default:                 statusBar.setRadio(RadioMode::None); break;
    }
  }

  delay(5);
}
