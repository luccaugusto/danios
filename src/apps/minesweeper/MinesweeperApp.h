// src/apps/minesweeper/MinesweeperApp.h — Campo Minado (spec 2026-07-15).
// Thin LVGL UI over lib/mines_model. Two internal screens: start
// (linhas/colunas/minas steppers, Fácil/Difícil preset buttons with best
// times, Jogar, resume) and board (HUD + grid). The grid is ONE custom-drawn
// widget — per-cell rects/glyphs drawn in a DRAW_MAIN handler, taps mapped
// point->cell — because 117 per-cell objects would strain the LVGL pool and
// a full canvas buffer is out of RAM budget. Flag mode is a checkable
// "Bandeira" button (the only flagging mechanism). Leaving mid-game pauses
// (game + accumulated time survive in this boot-time static instance); best
// times per difficulty in NVS. No radio. Last-used config persists in NVS;
// best times are recorded only for preset-matching configs.
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

  void showStart();
  void showBoard();
  void newGame(uint8_t rows, uint8_t cols, uint16_t mines);
  void pauseGame();  // fold running time into accumMs_
  void handleCellTap(int row, int col);
  void updateHud();
  void endGame();  // overlay + best-time write; game stays for mine display
  bool resumable() const;
  uint32_t elapsedMs(uint32_t now_ms) const;

  struct StepCtx {
    MinesweeperApp* app;
    uint8_t field;  // 0 = rows, 1 = cols, 2 = mines
    int8_t delta;
  };

  void loadSetup();                                // NVS -> setup fields, clamped
  void stepSetup(uint8_t field, int8_t delta);     // +/- button action
  void applyPreset(uint8_t rows, uint8_t cols, uint16_t mines);
  void refreshSetupLabels();
  lv_obj_t* makeStepBtn(const char* txt, lv_coord_t x, lv_coord_t y,
                        StepCtx* ctx);

  static void onStep(lv_event_t* e);
  static void onPresetEasy(lv_event_t* e);
  static void onPresetHard(lv_event_t* e);
  static void onPlay(lv_event_t* e);
  static void onResume(lv_event_t* e);
  static void onGridDraw(lv_event_t* e);
  static void onGridClicked(lv_event_t* e);
  static void onOverlayClicked(lv_event_t* e);

  ISettingsStore* store_ = nullptr;

  std::unique_ptr<MinesBoard> game_;
  Screen screen_ = Screen::Start;
  uint16_t cellPx_ = 26;
  uint8_t setupRows_ = 9, setupCols_ = 9;  // start-screen stepper values
  uint16_t setupMines_ = 18;               // defaults = Fácil until loadSetup
  StepCtx stepCtx_[6];                     // {rows,cols,mines} x {-,+}
  lv_obj_t* setupLbl_[3] = {nullptr, nullptr, nullptr};
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
