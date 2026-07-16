// src/apps/minesweeper/MinesweeperApp.h — Campo Minado (spec 2026-07-15).
// Thin LVGL UI over lib/mines_model. Two internal screens: start (difficulty
// buttons + best times + resume) and board (HUD + grid). The grid is ONE
// custom-drawn widget — per-cell rects/glyphs drawn in a DRAW_MAIN handler,
// taps mapped point->cell — because 117 per-cell objects would strain the
// LVGL pool and a full canvas buffer is out of RAM budget. Flag mode is a
// checkable "Bandeira" button (the only flagging mechanism). Leaving
// mid-game pauses (game + accumulated time survive in this boot-time static
// instance); best times per difficulty in NVS. No radio.
#pragma once

#include <lvgl.h>

#include <memory>

#include <mines_model.h>
#include <settings_store.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class MinesweeperApp : public App {
 public:
  // Call once from main.cpp before registerApp.
  void setDeps(ISettingsStore& store);

  const char* id() const override { return "mines"; }
  const char* title() const override { return catalog::kMines.title; }
  const char* iconPath() const override { return catalog::kMines.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;
  bool handleBack() override;  // board -> start screen (pauses), else exit

 private:
  enum class Screen : uint8_t { Start, Board };
  enum class Difficulty : uint8_t { Easy, Hard };

  void showStart();
  void showBoard();
  void newGame(Difficulty d);
  void pauseGame();  // fold running time into accumMs_
  void handleCellTap(int row, int col);
  void updateHud();
  void endGame();  // overlay + best-time write; game stays for mine display
  bool resumable() const;
  uint32_t elapsedMs(uint32_t now_ms) const;

  static void onEasy(lv_event_t* e);
  static void onHard(lv_event_t* e);
  static void onResume(lv_event_t* e);
  static void onGridDraw(lv_event_t* e);
  static void onGridClicked(lv_event_t* e);
  static void onOverlayClicked(lv_event_t* e);

  ISettingsStore* store_ = nullptr;

  std::unique_ptr<MinesBoard> game_;
  Difficulty diff_ = Difficulty::Easy;
  Screen screen_ = Screen::Start;
  uint16_t cellPx_ = 26;
  bool timing_ = false;    // Playing + board screen on display
  uint32_t accumMs_ = 0;   // elapsed play time before the last resume
  uint32_t resumeMs_ = 0;  // millis() when timing_ last became true
  uint32_t lastHudMs_ = 0;

  lv_obj_t* root_ = nullptr;
  lv_obj_t* grid_ = nullptr;
  lv_obj_t* minesLbl_ = nullptr;
  lv_obj_t* timeLbl_ = nullptr;
  lv_obj_t* flagBtn_ = nullptr;  // checkable; checked = flag mode
};
