// src/apps/pet/PetApp.h — Pet app (A5, spec §4.5). Thin LVGL wrapper over the
// fully-tested lib/pet_model state machine: onEnter loads state + runs one
// petTick/onAppOpen, buildUI draws the egg / alive / memorial screen, and each
// interaction calls one model entry point then re-saves and re-renders. No
// radio (requiredRadio()==None); state in NVS, art from S:/art/pet/.
#pragma once

#include <lvgl.h>

#include <string>

#include <pet_model.h>
#include <settings_store.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class TimeService;
class StorageService;

class PetApp : public App {
 public:
  // Call once from main.cpp before registerApp.
  void setDeps(ISettingsStore& store, TimeService& time, StorageService& storage);

  const char* id() const override { return "pet"; }
  const char* title() const override { return catalog::kPet.title; }
  const char* iconPath() const override { return catalog::kPet.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override;
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;

 private:
  // Screen builders (render() cleans root_ then dispatches on currentScreen).
  void render();
  void buildEgg();
  void buildAlive();
  void buildMemorial();

  // Landscape layout helper.
  static lv_obj_t* makeColumn(lv_obj_t* parent, lv_coord_t pctWidth);

  // Flows.
  void askName();               // egg -> LVGL keyboard naming modal
  void doHatch(const std::string& name);
  void openFoodTray();
  void doFeed(Food food);
  void doPlay();
  void doClean();
  void doScold();
  void doRebirth();

  void toast(const char* msg);  // transient message on the top layer
  void bounce();                // quick sprite hop on a happy interaction
  void save();                  // savePet(*store_, st_)
  uint32_t todayKey() const;    // 0 when the clock is unknown
  int nowMinutes() const;       // -1 when the clock is unknown

  // LVGL C callbacks (each gets `this` via user_data).
  static void onHatchClicked(lv_event_t* e);
  static void onNameKeyboard(lv_event_t* e);
  static void onFeedClicked(lv_event_t* e);
  static void onPlayClicked(lv_event_t* e);
  static void onCleanClicked(lv_event_t* e);
  static void onScoldClicked(lv_event_t* e);
  static void onFoodChosen(lv_event_t* e);
  static void onAdeusClicked(lv_event_t* e);
  static void onToastTimer(lv_timer_t* t);

  ISettingsStore* store_ = nullptr;
  TimeService* time_ = nullptr;
  StorageService* storage_ = nullptr;

  PetState st_{};
  bool misbehavesThisVisit_ = false;
  bool scoldedThisVisit_ = false;

  lv_obj_t* root_ = nullptr;         // launcher-owned 240x288 container
  lv_obj_t* petImg_ = nullptr;       // current stage sprite (for bounce)
  lv_obj_t* toastLbl_ = nullptr;     // transient toast on lv_layer_top()
  lv_timer_t* toastTimer_ = nullptr; // one-shot that dismisses toastLbl_
  lv_obj_t* modal_ = nullptr;  // active naming/food modal on lv_layer_top()
};
