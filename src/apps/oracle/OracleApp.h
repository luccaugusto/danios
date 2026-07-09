// src/apps/oracle/OracleApp.h — Oracle app (A2, spec §4.4). Thin LVGL
// wrapper: reload /oracle/wisdom.txt on every open, typeset one entry over
// the oracle frame art (placeholder box when missing). The daily pick lives
// in lib/oracle_picker (native-tested); while the clock is unknown the entry
// is random, re-rolled per open. No radio, no NVS.
#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "apps/app_catalog.h"
#include "core/App.h"

class StorageService;
class TimeService;

class OracleApp : public App {
 public:
  void setDeps(StorageService& storage, TimeService& time) {
    storage_ = &storage;
    time_ = &time;
  }

  const char* id() const override { return "oracle"; }
  const char* title() const override { return catalog::kOracle.title; }
  const char* iconPath() const override { return catalog::kOracle.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override;
  void buildUI(lv_obj_t* parent) override;
  void onExit() override { label_ = nullptr; }  // launcher deletes widgets
  void tick(uint32_t now_ms) override;

 private:
  void showEntry();

  StorageService* storage_ = nullptr;
  TimeService* time_ = nullptr;
  std::vector<std::string> lines_;
  lv_obj_t* label_ = nullptr;
  uint32_t shownKey_ = 0;   // dateKey of the shown entry; 0 = random fallback
  uint32_t lastCheck_ = 0;  // tick() throttle
};
