// src/apps/minesweeper/MinesweeperApp.cpp — see MinesweeperApp.h.
#include "apps/minesweeper/MinesweeperApp.h"

#include <Arduino.h>
#include <esp_system.h>

#include <cstdio>

namespace {

struct Preset {
  uint8_t rows, cols;
  uint16_t mines;
  uint16_t cellPx;
  const char* bestKey;
};
constexpr Preset kEasy{9, 9, 18, 26, "mines_best_easy"};
constexpr Preset kHard{13, 9, 33, 19, "mines_best_hard"};

constexpr lv_coord_t kHudH = 32;

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

  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, "Campo Minado");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

  lv_coord_t y = 52;
  if (resumable()) {
    lv_obj_t* btn = lv_btn_create(root_);
    lv_obj_set_size(btn, 204, 48);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    y += 62;
    char t[16];
    fmtTime(t, sizeof t, accumMs_ / 1000);
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text_fmt(l, "Continuar (%s)", t);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, onResume, LV_EVENT_CLICKED, this);
  }

  struct Row {
    const Preset& p;
    const char* name;
    lv_event_cb_t cb;
  };
  const Row rows[] = {{kEasy, "Fácil - 9x9, 18 minas", onEasy},
                      {kHard, "Difícil - 9x13, 33 minas", onHard}};
  for (const Row& row : rows) {
    lv_obj_t* btn = lv_btn_create(root_);
    lv_obj_set_size(btn, 204, 62);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    y += 76;
    char best[32];
    const uint32_t b = store_->getU32(row.p.bestKey, 0);
    if (b != 0) {
      char t[16];
      fmtTime(t, sizeof t, b);
      snprintf(best, sizeof best, "melhor: %s", t);
    } else {
      snprintf(best, sizeof best, "melhor: -");
    }
    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text_fmt(l, "%s\n%s", row.name, best);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(btn, row.cb, LV_EVENT_CLICKED, this);
  }
}

void MinesweeperApp::newGame(Difficulty d) {
  diff_ = d;
  const Preset& p = (d == Difficulty::Easy) ? kEasy : kHard;
  cellPx_ = p.cellPx;
  accumMs_ = 0;
  timing_ = false;  // starts on the first reveal
  game_.reset(new MinesBoard(p.rows, p.cols, p.mines,
                             []() -> uint32_t { return esp_random(); }));
  showBoard();
}

void MinesweeperApp::showBoard() {
  screen_ = Screen::Board;
  lv_obj_clean(root_);

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
  if (won) {
    const char* key = (diff_ == Difficulty::Easy) ? kEasy.bestKey : kHard.bestKey;
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

void MinesweeperApp::onEasy(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))->newGame(Difficulty::Easy);
}
void MinesweeperApp::onHard(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))->newGame(Difficulty::Hard);
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
      if (v == CellView::Flagged) {
        txt = "F";
        tc = lv_color_hex(0xD32F2F);
      } else if (lost && g->isMine(r, c)) {
        txt = "*";  // post-loss: show every unflagged mine
      } else if (v == CellView::Revealed && g->adjacent(r, c) > 0) {
        num[0] = static_cast<char>('0' + g->adjacent(r, c));
        txt = num;
        tc = numColor(g->adjacent(r, c));
      }
      if (txt != nullptr) {
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
