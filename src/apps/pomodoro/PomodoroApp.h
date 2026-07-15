// src/apps/pomodoro/PomodoroApp.h — Pomodoro timer (spec 2026-07-15). Thin
// LVGL wrapper over lib/pomodoro_model: status art (120x120, colored
// placeholder until S:/art/pomo/*.bin exist), 48 px mm:ss countdown, one
// Start/Stop button, work/break steppers (NVS, editable only while Idle),
// full-screen flash on a phase flip. The timer lives in this boot-time
// static instance, so it keeps counting while the app is closed; main.cpp
// polls badgeOn(millis()) ~1 Hz for the launcher badge. No radio.
#pragma once

#include <lvgl.h>

#include <vector>

#include <pomodoro_model.h>
#include <settings_store.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class StorageService;

class PomodoroApp : public App {
 public:
  // Call once from main.cpp before registerApp (loads config from NVS).
  void setDeps(ISettingsStore& store, StorageService& storage);

  const char* id() const override { return "pomodoro"; }
  const char* title() const override { return catalog::kPomodoro.title; }
  const char* iconPath() const override { return catalog::kPomodoro.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;

  // main.cpp badge poll; also resolves transitions that fell due while the
  // app was closed (keeps the wrap-safety invariant of pomodoro_model).
  bool badgeOn(uint32_t now_ms) {
    timer_.phase(now_ms);
    return timer_.running();
  }

 private:
  void buildStepperRow(lv_obj_t* parent, lv_coord_t y, const char* name,
                       lv_obj_t** valLbl, lv_event_cb_t minusCb,
                       lv_event_cb_t plusCb);
  void syncAll(uint32_t now_ms);  // art + countdown + button + steppers
  void updateCountdown(uint32_t now_ms);
  void applyPhaseArt(PomoPhase p);
  void adjust(int dWork, int dBreak);  // stepper deltas, clamps + saves
  void saveConfig();
  void flash();      // 3 quick white blinks on lv_layer_top()
  void stopFlash();

  // LVGL C callbacks (each gets `this` via user_data).
  static void onStartStop(lv_event_t* e);
  static void onWorkMinus(lv_event_t* e);
  static void onWorkPlus(lv_event_t* e);
  static void onBreakMinus(lv_event_t* e);
  static void onBreakPlus(lv_event_t* e);
  static void onFlashTimer(lv_timer_t* t);

  ISettingsStore* store_ = nullptr;
  StorageService* storage_ = nullptr;

  PomoTimer timer_;
  PomoPhase shownPhase_ = PomoPhase::Idle;
  uint32_t lastLblMs_ = 0;

  lv_obj_t* box_ = nullptr;        // placeholder art (no SD file)
  lv_obj_t* boxLbl_ = nullptr;
  lv_obj_t* img_ = nullptr;        // real art (SD file present)
  lv_obj_t* countdown_ = nullptr;
  lv_obj_t* btnLbl_ = nullptr;
  lv_obj_t* workVal_ = nullptr;
  lv_obj_t* breakVal_ = nullptr;
  std::vector<lv_obj_t*> stepperBtns_;
  lv_obj_t* flashOv_ = nullptr;
  lv_timer_t* flashTimer_ = nullptr;
  uint8_t flashCount_ = 0;
};
