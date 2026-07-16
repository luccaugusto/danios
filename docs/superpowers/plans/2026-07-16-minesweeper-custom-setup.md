# Minesweeper Custom Setup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the two hardcoded difficulty buttons on the Campo Minado start screen with stepper controls for rows/cols/mines plus Fácil/Difícil preset buttons, per spec `docs/superpowers/specs/2026-07-16-minesweeper-custom-setup-design.md`.

**Architecture:** All changes live in `src/apps/minesweeper/MinesweeperApp.{h,cpp}`. `lib/mines_model` already takes arbitrary `rows, cols, mines` and is untouched. Task 1 is a behavior-preserving refactor (parameterized `newGame`, computed `cellPx_`, config-matched best-time keys); Task 2 rewrites the start screen UI and adds NVS persistence of the last-used config; Task 3 is on-device verification.

**Tech Stack:** C++17, LVGL 8.4.0, PlatformIO env `cyd` for device builds (`pio run -e cyd`), NVS via `ISettingsStore`.

## Global Constraints

- All user-facing strings in PT-BR: "Linhas", "Colunas", "Minas", "Fácil", "Difícil", "Jogar", "Continuar", "melhor: -".
- No changes to `lib/mines_model/` or the board screen / drawing / pause-resume code.
- No native tests for this feature (UI-only; spec's Testing section) — but run the full native suite once to prove no regressions.
- LVGL pool is tight (~20 K floor): the start screen must stay flat widgets on `root_` (~27 objects), no nested containers.
- Screen is 240×320 portrait minus the launcher top bar; the start-screen layout must fit without scrolling even when Continuar is shown.
- Ranges (spec): rows 5–14, cols 5–12, mines 1–⌊rows·cols·35/100⌋. Presets: Fácil 9×9/18, Difícil 13 rows × 9 cols / 33.
- Device build check: `pio run -e cyd`, expected `SUCCESS`.

---

### Task 1: Parameterized newGame, computed cell size, config-matched best keys

Behavior-preserving refactor: the start screen keeps its two current buttons, but they now call `newGame(rows, cols, mines)`; `cellPx_` is computed from the root size instead of stored per preset; `endGame()` picks the best-time NVS key by comparing the played board against the presets. The `Difficulty` enum dies.

**Files:**
- Modify: `src/apps/minesweeper/MinesweeperApp.h`
- Modify: `src/apps/minesweeper/MinesweeperApp.cpp`

**Interfaces:**
- Consumes: `MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines, rng)`, `MinesBoard::rows()/cols()/mineCount()` (existing, unchanged).
- Produces: `void newGame(uint8_t rows, uint8_t cols, uint16_t mines)` — Task 2's Jogar button calls this. `Preset {uint8_t rows, cols; uint16_t mines; const char* bestKey;}` with `kEasy`/`kHard` constants — Task 2's preset buttons read these.

- [ ] **Step 1: Update the header**

In `src/apps/minesweeper/MinesweeperApp.h`:

Remove the line:

```cpp
  enum class Difficulty : uint8_t { Easy, Hard };
```

Remove the member:

```cpp
  Difficulty diff_ = Difficulty::Easy;
```

Replace the declaration `void newGame(Difficulty d);` with:

```cpp
  void newGame(uint8_t rows, uint8_t cols, uint16_t mines);
```

Keep `onEasy`/`onHard` for now (Task 2 removes them).

- [ ] **Step 2: Update the implementation**

In `src/apps/minesweeper/MinesweeperApp.cpp`:

Replace the `Preset` struct and constants (drop `cellPx`):

```cpp
struct Preset {
  uint8_t rows, cols;
  uint16_t mines;
  const char* bestKey;
};
constexpr Preset kEasy{9, 9, 18, "mines_best_easy"};
constexpr Preset kHard{13, 9, 33, "mines_best_hard"};
```

Add to the anonymous namespace (after `kHudH`):

```cpp
// The played config records a best time only when it exactly matches a
// preset — a manually dialed 9x9/18 is the same game as Fácil.
const char* bestKeyFor(const MinesBoard& g) {
  for (const Preset* p : {&kEasy, &kHard})
    if (g.rows() == p->rows && g.cols() == p->cols && g.mineCount() == p->mines)
      return p->bestKey;
  return nullptr;
}
```

Replace `newGame` entirely:

```cpp
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
```

In `endGame()`, replace the best-time block:

```cpp
  bool newBest = false;
  const char* key = won ? bestKeyFor(*game_) : nullptr;
  if (key != nullptr) {
    const uint32_t best = store_->getU32(key, 0);
    if (best == 0 || secs < best) {
      store_->setU32(key, secs);
      newBest = true;
    }
  }
```

Replace the `onEasy`/`onHard` bodies:

```cpp
void MinesweeperApp::onEasy(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))
      ->newGame(kEasy.rows, kEasy.cols, kEasy.mines);
}
void MinesweeperApp::onHard(lv_event_t* e) {
  static_cast<MinesweeperApp*>(lv_event_get_user_data(e))
      ->newGame(kHard.rows, kHard.cols, kHard.mines);
}
```

`showStart()` is untouched in this task (its `Row` struct only uses `p.bestKey`, which survives).

- [ ] **Step 3: Verify the device build compiles**

Run: `pio run -e cyd`
Expected: `SUCCESS`. (Sanity check: `240/9 = 26` and `(rootH−34)/13 = 19` reproduce today's preset cell sizes.)

- [ ] **Step 4: Commit**

```bash
git add src/apps/minesweeper/MinesweeperApp.h src/apps/minesweeper/MinesweeperApp.cpp
git commit -m "refactor: campo minado newGame takes explicit config, computed cellPx"
```

---

### Task 2: Setup UI — steppers, presets, Jogar, NVS config

Rewrite `showStart()`: three stepper rows (Linhas/Colunas/Minas with −/+ buttons, long-press auto-repeat), two half-width preset buttons showing best times that only fill the steppers, a full-width Jogar button, and last-used-config persistence in NVS.

**Files:**
- Modify: `src/apps/minesweeper/MinesweeperApp.h`
- Modify: `src/apps/minesweeper/MinesweeperApp.cpp`

**Interfaces:**
- Consumes: `newGame(uint8_t, uint8_t, uint16_t)` and `kEasy`/`kHard` from Task 1; `ISettingsStore::getU32/setU32`.
- Produces: nothing consumed later. NVS keys written: `mines_cfg_rows`, `mines_cfg_cols`, `mines_cfg_mines` (u32).

- [ ] **Step 1: Update the header**

In `src/apps/minesweeper/MinesweeperApp.h`:

Update the top-of-file comment's start-screen description — replace the sentence fragment `start (difficulty buttons + best times + resume)` with `start (linhas/colunas/minas steppers, Fácil/Difícil preset buttons with best times, Jogar, resume)` and append to the comment: last-used config in NVS; best times recorded only for preset-matching configs.

In the private section, remove:

```cpp
  static void onEasy(lv_event_t* e);
  static void onHard(lv_event_t* e);
```

Add declarations (after `elapsedMs`):

```cpp
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
```

Add members (near `cellPx_`):

```cpp
  uint8_t setupRows_ = 9, setupCols_ = 9;  // start-screen stepper values
  uint16_t setupMines_ = 18;               // defaults = Fácil until loadSetup
  StepCtx stepCtx_[6];                     // {rows,cols,mines} x {-,+}
  lv_obj_t* setupLbl_[3] = {nullptr, nullptr, nullptr};
```

- [ ] **Step 2: Rewrite showStart and add the setup logic**

In `src/apps/minesweeper/MinesweeperApp.cpp`:

Add to the anonymous namespace (after `kHudH`):

```cpp
constexpr uint8_t kMinRC = 5, kMaxRows = 14, kMaxCols = 12;

// 35% cap: keeps first-tap safety satisfiable (5x5: 8 <= 25-9) and admits
// both presets (18/81, 33/117).
constexpr uint16_t maxMines(uint8_t rows, uint8_t cols) {
  return static_cast<uint16_t>(rows * cols * 35U / 100U);
}

uint16_t clampU(uint32_t v, uint16_t lo, uint16_t hi) {
  return v < lo ? lo : (v > hi ? hi : static_cast<uint16_t>(v));
}
```

Replace `showStart()` entirely:

```cpp
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
```

(Layout budget, Continuar visible: title 4–20, Continuar 26–66, steppers 72/108/144, presets 180–232, Jogar 240–280 — fits the ~292 px root. Without Continuar everything sits 46 px higher. Widget count ≈ 27, fine for the pool.)

Add the new methods (place after `showStart`):

```cpp
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
    setupMines_ = clampU(setupMines_ + delta, 1, 0xFFFF);
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
```

Replace the `onEasy`/`onHard` callbacks with:

```cpp
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
```

Null the stale label pointers where the start screen is torn down — in `showBoard()` right after `lv_obj_clean(root_);` and in `onExit()` alongside the other widget nulls, add:

```cpp
  setupLbl_[0] = setupLbl_[1] = setupLbl_[2] = nullptr;
```

- [ ] **Step 3: Verify the device build compiles**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Run the full native suite (no regressions)**

Run: `pio test -e native`
Expected: all tests PASS (nothing in `lib/` changed; this is a guard).

- [ ] **Step 5: Commit**

```bash
git add src/apps/minesweeper/MinesweeperApp.h src/apps/minesweeper/MinesweeperApp.cpp
git commit -m "feat: campo minado configurable setup screen (steppers + presets)"
```

---

### Task 3: On-device verification

**Files:**
- None (flash + manual test; a `test:` commit records the outcome).

**Interfaces:**
- Consumes: the full feature from Tasks 1–2.

- [ ] **Step 1: Flash**

Free the serial port if a reader is attached, then:
Run: `pio run -e cyd -t upload` (CYD on `/dev/ttyUSB0`)
Expected: upload `SUCCESS`, device reboots into the launcher.

- [ ] **Step 2: Manual checklist (needs the human — device in hand)**

Ask the user to verify, and wait for their report:

1. Start screen fits without scrolling; steppers show 9 / 9 / 18 on first run.
2. `+`/`-` step by one and stop at the bounds (linhas 5–14, colunas 5–12); holding a button auto-repeats.
3. Minas max re-clamps: set a big board, raise minas near max, shrink linhas/colunas → minas drops to the new ceiling; minas never goes below 1.
4. Fácil / Difícil buttons fill 9/9/18 and 13/9/33 (and show "melhor: …" from existing records) without starting a game.
5. Jogar starts the dialed config; extremes look sane and are tappable (5×5 → 32 px capped cells; 14 linhas × 12 colunas → ~18 px cells).
6. Win a manually dialed 9×9/18 game → counts toward the Fácil best time.
7. Win a custom (non-preset) game → no record written (presets' "melhor" unchanged).
8. Set a custom config, Jogar, exit the app, power-cycle → steppers restore the custom values.
9. Continuar still appears for a paused game and restores it.

- [ ] **Step 3: Commit the verification note**

```bash
git commit --allow-empty -m "test: campo minado custom setup on-device verification passed"
```

(Only after the user confirms the checklist; if anything fails, fix first — no green-washing.)
