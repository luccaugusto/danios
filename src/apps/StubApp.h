// src/apps/StubApp.h — header-only placeholder app. Registered five times in
// main.cpp ("weather", "music", "calc", "oracle", "pet"); each real app plan
// (A1–A5) replaces its stub registration with the real App.
#pragma once

#include "apps/app_catalog.h"
#include "core/App.h"

class StubApp : public App {
 public:
  StubApp(const char* id, const AppInfo& info) : id_(id), info_(info) {}

  const char* id() const override { return id_; }
  const char* title() const override { return info_.title; }
  const char* iconPath() const override { return info_.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text_fmt(label, "%s\nem breve", info_.title);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
  }
  void onExit() override {}

 private:
  const char* id_;
  const AppInfo& info_;
};
