// src/core/Launcher.h — home grid + app lifecycle (roadmap §4.5).
// Public API is the roadmap contract verbatim; do not rename methods.
#pragma once

#include <lvgl.h>

#include <functional>
#include <memory>
#include <vector>

#include "core/App.h"
#include "core/Layout.h"
#include "core/StatusBar.h"
#include "launcher_model.h"

class Launcher {
 public:
  explicit Launcher(StatusBar& statusBar);

  void registerApp(App* app);                     // call order = grid order; every app gets a grid icon
  void show();                                    // build/refresh home screen
  void openApp(const char* id);                   // radio switch + lifecycle
  void goHome();                                  // apps call this for back/home
  void setBadge(const char* appId, bool on);      // red dot on an app icon
  void setAppEnabled(const char* appId, bool en); // greyed icon; tap → hint msgbox
  void setRadioRequest(std::function<bool(RadioMode)> fn);
                                                  // unset → treated as always-true;
                                                  // F4 wires RadioManager::request
  void tick(uint32_t now_ms);                     // forwards to active app

 private:
  struct IconCtx {
    Launcher* self;
    const char* appId;
  };

  void buildHomeScreen();
  void rebuildGrid();
  void buildAppScreen();
  void resetAppContainer();
  void showDisabledHint(const char* title);
  static void onIconClicked(lv_event_t* e);
  static void onBackClicked(lv_event_t* e);

  StatusBar& statusBar_;
  LauncherModel model_{layout::kGridCols};
  std::vector<App*> apps_;                 // index == model_ registration index
  App* active_ = nullptr;
  std::function<bool(RadioMode)> radioRequest_;

  lv_obj_t* homeScreen_ = nullptr;
  lv_obj_t* gridContainer_ = nullptr;
  lv_obj_t* appScreen_ = nullptr;
  lv_obj_t* appTitleLabel_ = nullptr;
  lv_obj_t* appContainer_ = nullptr;       // parent passed to App::buildUI
  std::vector<lv_obj_t*> cells_;           // index = grid index
  std::vector<lv_obj_t*> badges_;          // index = grid index
  std::vector<std::unique_ptr<IconCtx>> iconCtxs_;  // stable addrs for LVGL cbs
};
