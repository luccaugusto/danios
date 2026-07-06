// src/apps/settings/SettingsApp.h — Settings shell (roadmap §4.11).
// F2 ships only the shell; sections are appended by later plans.
#pragma once

#include <settings_store.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class DisplayService;
class StorageService;

class SettingsApp : public App {
 public:
  const char* id() const override { return "settings"; }
  const char* title() const override { return catalog::kSettings.title; }
  const char* iconPath() const override { return catalog::kSettings.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override {}

  // F3: dependency injection. Call once from main.cpp, before registerApp.
  void setDeps(ISettingsStore& store, DisplayService& display, StorageService& storage);

 private:
  void showMenu();          // the lv_list of sections
  void showSection(int idx);
  static void menuClicked(lv_event_t* e);
  static void backClicked(lv_event_t* e);

  ISettingsStore* store_ = nullptr;
  DisplayService* display_ = nullptr;
  StorageService* storage_ = nullptr;
  lv_obj_t* root_ = nullptr;
};
