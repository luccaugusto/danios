// src/apps/calculator/CalculatorApp.h — Calculator app (A1, spec §4.3).
// Thin LVGL wrapper: btnmatrix keypad + result label; all behavior lives in
// lib/calc_engine (native-tested). No radio, no SD, no NVS.
#pragma once

#include <calc_engine.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class CalculatorApp : public App {
 public:
  const char* id() const override { return "calc"; }
  const char* title() const override { return catalog::kCalc.title; }
  const char* iconPath() const override { return catalog::kCalc.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override { engine_.clear(); }  // fresh calc on every open
  void buildUI(lv_obj_t* parent) override;
  void onExit() override { displayLabel_ = nullptr; }  // launcher deletes widgets

 private:
  static void keyPressed(lv_event_t* e);
  void handleKey(const char* txt);
  void refresh();

  CalcEngine engine_;
  lv_obj_t* displayLabel_ = nullptr;
};
