// src/apps/settings/SettingsApp.h — Settings shell (roadmap §4.11).
// F2 ships only the shell; sections are appended by later plans.
#pragma once

#include "apps/app_catalog.h"
#include "core/App.h"

class SettingsApp : public App {
 public:
  const char* id() const override { return "settings"; }
  const char* title() const override { return catalog::kSettings.title; }
  const char* iconPath() const override { return catalog::kSettings.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override {}
};
