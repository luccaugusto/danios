// src/apps/weather/WeatherApp.h — Weather app (A3, spec §4.1). Thin LVGL
// wrapper: renders the cached forecast instantly, refreshes over WiFi from
// tick() (first ~400 ms after entry, then every 20 min while open). All
// mapping lives in lib/weather_model, parsing in lib/weather_parse, fetching
// in WeatherFetch — this file only draws.
#pragma once

#include <settings_store.h>
#include <weather_parse.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class WiFiService;
class TimeService;
class StorageService;

class WeatherApp : public App {
 public:
  // Call once from main.cpp before registerApp.
  void setDeps(ISettingsStore& store, WiFiService& wifi, TimeService& time,
               StorageService& storage);

  const char* id() const override { return "weather"; }
  const char* title() const override { return catalog::kWeather.title; }
  const char* iconPath() const override { return catalog::kWeather.icon; }
  RadioMode requiredRadio() const override { return RadioMode::WiFi; }
  void onEnter() override;
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;

 private:
  void render(const ForecastWx& f, bool stale);  // full rebuild of root_
  void renderEmpty();          // no data at all: friendly hint (spec §6.5)
  bool renderCached();         // true if a parseable cache was rendered
  void refreshNow(uint32_t now_ms);  // blocking: connect + fetch + re-render
  void setStatus(const char* msg);   // top-right marker (atualizando/stale)

  ISettingsStore* store_ = nullptr;
  WiFiService* wifi_ = nullptr;
  TimeService* time_ = nullptr;
  StorageService* storage_ = nullptr;

  lv_obj_t* root_ = nullptr;       // launcher-owned 240x288 container
  lv_obj_t* statusLbl_ = nullptr;  // child of root_, recreated per render

  bool pendingRefresh_ = false;
  uint32_t enteredMs_ = 0;
  uint32_t lastFetchMs_ = 0;
};
