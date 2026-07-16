// src/apps/minesweeper/MinesweeperApp.cpp — see MinesweeperApp.h.
#include "apps/minesweeper/MinesweeperApp.h"

#include <Arduino.h>
#include <esp_system.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Preset {
  uint8_t rows, cols;
  uint16_t mines;
  const char* bestKey;
};
constexpr Preset kEasy{9, 9, 18, "mines_best_easy"};
constexpr Preset kHard{13, 9, 33, "mines_best_hard"};

constexpr lv_coord_t kHudH = 32;

constexpr uint8_t kMinRC = 5, kMaxRows = 14, kMaxCols = 12;

// 35% cap: keeps first-tap safety satisfiable (5x5: 8 <= 25-9) and admits
// both presets (18/81, 33/117).
constexpr uint16_t maxMines(uint8_t rows, uint8_t cols) {
  return static_cast<uint16_t>(rows * cols * 35U / 100U);
}

uint16_t clampU(uint32_t v, uint16_t lo, uint16_t hi) {
  return v < lo ? lo : (v > hi ? hi : static_cast<uint16_t>(v));
}

// The played config records a best time only when it exactly matches a
// preset — a manually dialed 9x9/18 is the same game as Fácil.
const char* bestKeyFor(const MinesBoard& g) {
  for (const Preset* p : {&kEasy, &kHard})
    if (g.rows() == p->rows && g.cols() == p->cols && g.mineCount() == p->mines)
      return p->bestKey;
  return nullptr;
}

// Classic per-number colors.
lv_color_t numColor(uint8_t n) {
  switch (n) {
    case 1: return lv_color_hex(0x1976D2);   // blue
    case 2: return lv_color_hex(0x388E3C);   // green
    case 3: return lv_color_hex(0xD32F2F);   // red
    case 4: return lv_color_hex(0x303F9F);   // navy
    case 5: return lv_color_hex(0x795548);   // maroon-ish
    case 6: return lv_color_hex(0x00796B);   // teal
    case 7: return lv_color_hex(0x212121);   // black
    default: return lv_color_hex(0x616161);  // grey (8)
  }
}

void fmtTime(char* buf, size_t n, uint32_t secs) {
  snprintf(buf, n, "%u:%02u", static_cast<unsigned>(secs / 60),
           static_cast<unsigned>(secs % 60));
}

// Load an LVGL v8 .bin (4-byte lv_img_header_t + pixel data) from the SD into
// a malloc'd buffer so onGridDraw can blit it per cell without touching the
// card. Returns the buffer (caller frees) and fills *dsc, or nullptr if the
// file is missing/short/unreadable — callers fall back to the text glyphs.
uint8_t* loadImgBin(const char* path, lv_img_dsc_t* dsc) {
  lv_fs_file_t f;
  if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return nullptr;
  uint32_t size = 0;
  lv_fs_seek(&f, 0, LV_FS_SEEK_END);
  lv_fs_tell(&f, &size);
  lv_fs_seek(&f, 0, LV_FS_SEEK_SET);
  uint8_t* buf = nullptr;
  uint32_t rd = 0;
  if (size > sizeof(lv_img_header_t))
    buf = static_cast<uint8_t*>(malloc(size));
  if (buf != nullptr) lv_fs_read(&f, buf, size, &rd);
  lv_fs_close(&f);
  if (buf == nullptr || rd != size) {
    free(buf);
    return nullptr;
  }
  memcpy(&dsc->header, buf, sizeof(lv_img_header_t));
  dsc->data = buf + sizeof(lv_img_header_t);
  dsc->data_size = size - sizeof(lv_img_header_t);
  return buf;
}

}  // namespace

void MinesweeperApp::setDeps(ISettingsStore& store) { store_ = &store; }

void MinesweeperApp::buildUI(lv_obj_t* parent) {
  root_ = parent;
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  showStart();
}

void MinesweeperApp::onExit() {
  pauseGame();
  if (game_ && (game_->state() == GameState::Won || game_->state() == GameState::Lost)) {
    game_.reset();  // finished boards are not resumable
    accumMs_ = 0;
  }
  screen_ = Screen::Start;  // re-enter lands on the start screen
  root_ = grid_ = minesLbl_ = timeLbl_ = flagBtn_ = nullptr;
  setupLbl_[0] = setupLbl_[1] = setupLbl_[2] = nullptr;
  free(flagBuf_);
  free(mineBuf_);
  flagBuf_ = mineBuf_ = nullptr;
}

bool MinesweeperApp::handleBack() {
  if (screen_ != Screen::Board) return false;
  pauseGame();
  if (game_ && (game_->state() == GameState::Won || game_->state() == GameState::Lost)) {
    game_.reset();
    accumMs_ = 0;
  }
  showStart();
  return true;
}

bool MinesweeperApp::resumable() const {
  return game_ != nullptr &&
         (game_->state() == GameState::Fresh || game_->state() == GameState::Playing);
}

uint32_t MinesweeperApp::elapsedMs(uint32_t now_ms) const {
  return timing_ ? accumMs_ + (now_ms - resumeMs_) : accumMs_;
}

void MinesweeperApp::pauseGame() {
  if (timing_) {
    accumMs_ += millis() - resumeMs_;
    timing_ = false;
  }
}

void MinesweeperApp::showStart() {
  screen_ = Screen::Start;
  lv_obj_clean(root_);
  grid_ = minesLbl_ = timeLbl_ = flagBtn_ = nullptr;
  loadSetup();

  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, "Minas");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  lv_coord_t y = 26;
  if (resumable()) {
    lv_obj_t* btn = lv_btn_create(root_);
    lv_obj_set_size(btn, 224, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    y += 46;
    char t[16];
    fmtTime(t, sizeof t, accumMs_ / 1000);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text_fmt(l, "Continuar (%s)", t);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, onResume, LV_EVENT_CLICKED, this);
  }

  static const char* kNames[] = {"Linhas", "Colunas", "Minas"};
  for (uint8_t f = 0; f < 3; ++f) {
    lv_obj_t* name = lv_label_create(root_);
    lv_label_set_text(name, kNames[f]);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 8, y + 8);

    stepCtx_[f * 2] = {this, f, -1};
    stepCtx_[f * 2 + 1] = {this, f, +1};
    makeStepBtn("-", 104, y, &stepCtx_[f * 2]);
    setupLbl_[f] = lv_label_create(root_);
    lv_obj_set_width(setupLbl_[f], 40);
    lv_obj_set_style_text_align(setupLbl_[f], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(setupLbl_[f], LV_ALIGN_TOP_LEFT, 146, y + 8);
    makeStepBtn("+", 192, y, &stepCtx_[f * 2 + 1]);
    y += 36;
  }
  refreshSetupLabels();

  struct PresetBtn {
    const Preset& p;
    const char* name;
    lv_event_cb_t cb;
  };
  const PresetBtn pb[] = {{kEasy, "Fácil", onPresetEasy},
                          {kHard, "Difícil", onPresetHard}};
  for (uint8_t i = 0; i < 2; ++i) {
    lv_obj_t* btn = lv_btn_create(root_);
    lv_obj_set_size(btn, 108, 52);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 8 + i * 116, y);
    char best[16] = "-";
    const uint32_t b = store_->getU32(pb[i].p.bestKey, 0);
    if (b != 0) fmtTime(best, sizeof best, b);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text_fmt(l, "%s\nmelhor: %s", pb[i].name, best);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, pb[i].cb, LV_EVENT_CLICKED, this);
  }
  y += 60;

  lv_obj_t* play = lv_btn_create(root_);
  lv_obj_set_size(play, 224, 40);
  lv_obj_align(play, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_t* pl = lv_label_create(play);
  lv_label_set_text(pl, "Jogar");
  lv_obj_center(pl);
  lv_obj_add_event_cb(play, onPlay, LV_EVENT_CLICKED, this);
}

void MinesweeperApp::loadSetup() {
  setupRows_ = static_cast<uint8_t>(
      clampU(store_->getU32("mines_cfg_rows", kEasy.rows), kMinRC, kMaxRows));
  setupCols_ = static_cast<uint8_t>(
      clampU(store_->getU32("mines_cfg_cols", kEasy.cols), kMinRC, kMaxCols));
  setupMines_ = clampU(store_->getU32("mines_cfg_mines", kEasy.mines), 1,
                       maxMines(setupRows_, setupCols_));
}

void MinesweeperApp::stepSetup(uint8_t field, int8_t delta) {
  if (field == 0)
    setupRows_ = static_cast<uint8_t>(
        clampU(setupRows_ + delta, kMinRC, kMaxRows));
  else if (field == 1)
    setupCols_ = static_cast<uint8_t>(
        clampU(setupCols_ + delta, kMinRC, kMaxCols));
  else
    setupMines_ = static_cast<uint16_t>(setupMines_ + delta);
  // Any change can shrink the mines ceiling; keep the config always valid.
  setupMines_ = clampU(setupMines_, 1, maxMines(setupRows_, setupCols_));
  refreshSetupLabels();
}

void MinesweeperApp::applyPreset(uint8_t rows, uint8_t cols, uint16_t mines) {
  setupRows_ = rows;
  setupCols_ = cols;
  setupMines_ = mines;
  refreshSetupLabels();
}

void MinesweeperApp::refreshSetupLabels() {
  if (setupLbl_[0] == nullptr) return;
  lv_label_set_text_fmt(setupLbl_[0], "%u", setupRows_);
  lv_label_set_text_fmt(setupLbl_[1], "%u", setupCols_);
  lv_label_set_text_fmt(setupLbl_[2], "%u", setupMines_);
}

lv_obj_t* MinesweeperApp::makeStepBtn(const char* txt, lv_coord_t x,
                                      lv_coord_t y, StepCtx* ctx) {
  lv_obj_t* btn = lv_btn_create(root_);
  lv_obj_set_size(btn, 36, 32);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  // SHORT_CLICKED + LONG_PRESSED_REPEAT = tap steps once, holding repeats
  // (plain CLICKED would add a spurious step on release after a hold).
  lv_obj_add_event_cb(btn, onStep, LV_EVENT_SHORT_CLICKED, ctx);
  lv_obj_add_event_cb(btn, onStep, LV_EVENT_LONG_PRESSED_REPEAT, ctx);
  return btn;
}

void MinesweeperApp::newGame(uint8_t rows, uint8_t cols, uint16_t mines) {
  lv_obj_update_layout(root_);
  const lv_coord_t w = lv_obj_get_content_width(root_);
  const lv_coord_t h = lv_obj_get_content_height(root_) - kHudH - 2;
  cellPx_ = LV_MIN(w / cols, h / rows);
  if (cellPx_ > 32) cellPx_ = 32;  // small grids: don't balloon the cells
  accumMs_ = 0;
  timing_ = false;  // starts on the first reveal
  game_.reset(new MinesBoard(rows, cols, mines,
                             []() -> uint32_t { return esp_random(); }));
  showBoard();
}

void MinesweeperApp::showBoard() {
  screen_ = Screen::Board;
  lv_obj_clean(root_);
  setupLbl_[0] = setupLbl_[1] = setupLbl_[2] = nullptr;

  // Cell sprites, loaded once per visit (freed in onExit). Missing files just
  // leave the buffers null and the draw handler keeps its text glyphs.
  if (flagBuf_ == nullptr) flagBuf_ = loadImgBin("S:/art/mines/flag.bin", &flagDsc_);
  if (mineBuf_ == nullptr) mineBuf_ = loadImgBin("S:/art/mines/mine.bin", &mineDsc_);

  minesLbl_ = lv_label_create(root_);
  lv_obj_align(minesLbl_, LV_ALIGN_TOP_LEFT, 8, 8);
  timeLbl_ = lv_label_create(root_);
  lv_obj_align(timeLbl_, LV_ALIGN_TOP_MID, -20, 8);
  flagBtn_ = lv_btn_create(root_);
  lv_obj_add_flag(flagBtn_, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_size(flagBtn_, 90, 26);
  lv_obj_align(flagBtn_, LV_ALIGN_TOP_RIGHT, -4, 3);
  lv_obj_t* fl = lv_label_create(flagBtn_);
  lv_label_set_text(fl, "Bandeira");
  lv_obj_center(fl);

  grid_ = lv_obj_create(root_);
  lv_obj_remove_style_all(grid_);
  lv_obj_set_size(grid_, game_->cols() * cellPx_, game_->rows() * cellPx_);
  lv_obj_align(grid_, LV_ALIGN_TOP_MID, 0, kHudH + 2);
  lv_obj_add_flag(grid_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(grid_, onGridDraw, LV_EVENT_DRAW_MAIN, this);
  lv_obj_add_event_cb(grid_, onGridClicked, LV_EVENT_CLICKED, this);

  if (game_->state() == GameState::Playing) {  // resuming a paused game
    timing_ = true;
    resumeMs_ = millis();
  }
  updateHud();
}

void MinesweeperApp::updateHud() {
  if (minesLbl_ == nullptr || game_ == nullptr) return;
  lv_label_set_text_fmt(minesLbl_, "%d",
                        static_cast<int>(game_->mineCount()) -
                            static_cast<int>(game_->flagsPlaced()));
  char t[16];
  fmtTime(t, sizeof t, elapsedMs(millis()) / 1000);
  lv_label_set_text(timeLbl_, t);
}

void MinesweeperApp::tick(uint32_t now_ms) {
  if (screen_ != Screen::Board || !timing_) return;
  if (now_ms - lastHudMs_ < 500) return;
  lastHudMs_ = now_ms;
  updateHud();
}

void MinesweeperApp::handleCellTap(int row, int col) {
  if (game_ == nullptr) return;
  if (game_->state() != GameState::Fresh && game_->state() != GameState::Playing)
    return;
  if (row < 0 || col < 0 || row >= game_->rows() || col >= game_->cols()) return;

  const bool flagMode = lv_obj_has_state(flagBtn_, LV_STATE_CHECKED);
  const GameState before = game_->state();
  if (flagMode)
    game_->toggleFlag(static_cast<uint8_t>(row), static_cast<uint8_t>(col));
  else
    game_->reveal(static_cast<uint8_t>(row), static_cast<uint8_t>(col));

  if (before == GameState::Fresh && game_->state() == GameState::Playing) {
    timing_ = true;  // the timer starts on the first reveal
    resumeMs_ = millis();
  }
  lv_obj_invalidate(grid_);
  updateHud();
  if (game_->state() == GameState::Won || game_->state() == GameState::Lost)
    endGame();
}

void MinesweeperApp::endGame() {
  pauseGame();
  const bool won = game_->state() == GameState::Won;
  const uint32_t secs = (accumMs_ + 999) / 1000;
  bool newBest = false;
  const char* key = won ? bestKeyFor(*game_) : nullptr;
  if (key != nullptr) {
    const uint32_t best = store_->getU32(key, 0);
    if (best == 0 || secs < best) {
      store_->setU32(key, secs);
      newBest = true;
    }
  }

  lv_obj_t* ov = lv_obj_create(root_);
  lv_obj_set_size(ov, 220, 100);
  lv_obj_center(ov);
  lv_obj_set_style_bg_color(
      ov, won ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ov, onOverlayClicked, LV_EVENT_CLICKED, this);
  lv_obj_t* l = lv_label_create(ov);
  lv_obj_set_style_text_color(l, lv_color_white(), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  if (won) {
    char t[16];
    fmtTime(t, sizeof t, secs);
    lv_label_set_text_fmt(l, "Venceu! %s%s\nToque para voltar", t,
                          newBest ? "\nNovo recorde!" : "");
  } else {
    lv_label_set_text(l, "Perdeu!\nToque para voltar");
  }
  lv_obj_center(l);
}

void MinesweeperApp::onStep(lv_event_t* e) {
  auto* ctx = static_cast<StepCtx*>(lv_event_get_user_data(e));
  ctx->app->stepSetup(ctx->field, ctx->delta);
}
void MinesweeperApp::onPresetEasy(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))
      ->applyPreset(kEasy.rows, kEasy.cols, kEasy.mines);
}
void MinesweeperApp::onPresetHard(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))
      ->applyPreset(kHard.rows, kHard.cols, kHard.mines);
}
void MinesweeperApp::onPlay(lv_event_t* e) {
  auto* self = static_cast<MinesweeperApp*>(lv_event_get_user_data(e));
  self->store_->setU32("mines_cfg_rows", self->setupRows_);
  self->store_->setU32("mines_cfg_cols", self->setupCols_);
  self->store_->setU32("mines_cfg_mines", self->setupMines_);
  self->newGame(self->setupRows_, self->setupCols_, self->setupMines_);
}
void MinesweeperApp::onResume(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))->showBoard();
}

void MinesweeperApp::onOverlayClicked(lv_event_t* e) {
  auto* self = static_cast<MinesweeperApp*>(lv_event_get_user_data(e));
  self->game_.reset();
  self->accumMs_ = 0;
  self->showStart();
}

void MinesweeperApp::onGridClicked(lv_event_t* e) {
  auto* self = static_cast<MinesweeperApp*>(lv_event_get_user_data(e));
  lv_indev_t* indev = lv_indev_get_act();
  if (indev == nullptr || self->grid_ == nullptr) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  lv_area_t a;
  lv_obj_get_coords(self->grid_, &a);
  if (p.x < a.x1 || p.y < a.y1 || p.x > a.x2 || p.y > a.y2) return;
  self->handleCellTap((p.y - a.y1) / self->cellPx_, (p.x - a.x1) / self->cellPx_);
}

// The whole board, drawn cell by cell into the object's area. Cheap: runs
// only on invalidation (taps, screen build), not per frame.
void MinesweeperApp::onGridDraw(lv_event_t* e) {
  auto* self = static_cast<MinesweeperApp*>(lv_event_get_user_data(e));
  MinesBoard* g = self->game_.get();
  if (g == nullptr) return;
  lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
  lv_area_t obj;
  lv_obj_get_coords(lv_event_get_target(e), &obj);
  const lv_coord_t px = self->cellPx_;
  const bool lost = g->state() == GameState::Lost;

  lv_draw_rect_dsc_t rd;
  lv_draw_label_dsc_t ld;

  for (uint8_t r = 0; r < g->rows(); ++r) {
    for (uint8_t c = 0; c < g->cols(); ++c) {
      lv_area_t cell;
      cell.x1 = obj.x1 + c * px;
      cell.y1 = obj.y1 + r * px;
      cell.x2 = cell.x1 + px - 1;
      cell.y2 = cell.y1 + px - 1;

      const CellView v = g->view(r, c);
      lv_draw_rect_dsc_init(&rd);
      rd.bg_opa = LV_OPA_COVER;
      rd.border_width = 1;
      rd.border_color = lv_color_hex(0x808080);
      if (v == CellView::Revealed && g->isMine(r, c))
        rd.bg_color = lv_color_hex(0xE57373);  // the tripped mine
      else if (v == CellView::Revealed)
        rd.bg_color = lv_color_hex(0xE8E8E8);  // sunken
      else
        rd.bg_color = lv_color_hex(0xAFAFAF);  // raised (hidden / flagged)
      lv_draw_rect(ctx, &rd, &cell);

      const char* txt = nullptr;
      char num[2] = {0, 0};
      lv_color_t tc = lv_color_black();
      const lv_img_dsc_t* sprite = nullptr;
      if (v == CellView::Flagged) {
        if (self->flagBuf_ != nullptr) sprite = &self->flagDsc_;
        txt = "F";
        tc = lv_color_hex(0xD32F2F);
      } else if (lost && g->isMine(r, c)) {
        if (self->mineBuf_ != nullptr) sprite = &self->mineDsc_;
        txt = "*";  // post-loss: show every unflagged mine
      } else if (v == CellView::Revealed && g->adjacent(r, c) > 0) {
        num[0] = static_cast<char>('0' + g->adjacent(r, c));
        txt = num;
        tc = numColor(g->adjacent(r, c));
      }
      if (sprite != nullptr) {
        // Blit the sprite scaled to the cell: zoom is relative to the native
        // size around pivot (0,0) — dsc_init zeroes the pivot — so the drawn
        // area is exactly cell.x1/y1 .. +px.
        lv_draw_img_dsc_t id;
        lv_draw_img_dsc_init(&id);
        id.zoom = static_cast<uint16_t>(px * 256 / sprite->header.w);
        id.antialias = 1;
        lv_area_t ia;
        ia.x1 = cell.x1;
        ia.y1 = cell.y1;
        ia.x2 = cell.x1 + static_cast<lv_coord_t>(sprite->header.w) - 1;
        ia.y2 = cell.y1 + static_cast<lv_coord_t>(sprite->header.h) - 1;
        lv_draw_img(ctx, &id, &ia, sprite);
      } else if (txt != nullptr) {
        lv_draw_label_dsc_init(&ld);
        ld.color = tc;
        ld.font = LV_FONT_DEFAULT;
        ld.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t ta = cell;
        ta.y1 += (px - lv_font_get_line_height(ld.font)) / 2;
        lv_draw_label(ctx, &ld, &ta, txt, nullptr);
      }
    }
  }
}
