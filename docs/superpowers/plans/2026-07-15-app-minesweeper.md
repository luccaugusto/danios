# Minesweeper (Campo Minado) App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Classic minesweeper with two difficulties (Fácil 9×9/10, Difícil 9×13/25), first-tap-safe placement, flood fill, chording, flag-mode toggle, NVS best times, and mid-game pause/resume, per `docs/superpowers/specs/2026-07-15-minesweeper-design.md`.

**Architecture:** House two-unit pattern: `lib/mines_model/` is a pure-C++17 board (injected RNG — seeded LCG in native tests, `esp_random()` on device), and `src/apps/minesweeper/MinesweeperApp` is the LVGL 8.4 UI. The grid is **one custom-drawn widget** (draw-event hooks; rects + glyphs per cell) — no per-cell LVGL objects (117 widgets would strain the ~20 K LVGL pool; a 234×338 canvas buffer ≈ 158 KB RGB565 is out of budget).

**Tech Stack:** C++17, LVGL 8.4.0, Unity native tests (`pio test -e native`), PlatformIO env `cyd` for device builds.

## Global Constraints

- `lib/` code must compile in the `native` env: **zero Arduino/LVGL includes** in `lib/mines_model/` (std headers incl. `<functional>`/`<vector>` are fine).
- LVGL is pinned at **8.4.0** — use v8 APIs only (`lv_draw_ctx_t`, `lv_event_get_draw_ctx`, `lv_draw_rect`, `lv_draw_label`).
- UI copy is **PT-BR**; default font `montserrat_pt_14` (covers accents: "Fácil", "Difícil"). No emoji — the fonts don't have them; the flag-mode button is a checkable text button "Bandeira".
- App id is `"mines"` — an NVS/navigation key; never change it. Existing ids must not change either.
- Launcher/app contract: content area is 240×288 (32 px top bar). `tick()` runs only while active. Launcher deletes widgets after `onExit()`. `handleBack()` returning true consumes the back tap.
- Titles/icons live only in `src/apps/app_catalog.h`; icon path stays `nullptr` until art exists.
- Settings must remain the **last** icon in the launcher grid.
- NVS keys: `mines_best_easy`, `mines_best_hard` (u32 seconds, 0 = none).
- Geometry: HUD row 32 px; Fácil 9 cols × 9 rows @ 26 px (234×234); Difícil 9 cols × 13 rows @ 19 px (171×247, centered). Board area below the HUD is 256 px tall — both fit.
- Run native tests with `pio test -e native --filter test_mines_model`; device build with `pio run -e cyd`.

---

### Task 1: `lib/mines_model` — board core: placement, reveal, flood fill, win/lose (TDD)

**Files:**
- Create: `lib/mines_model/mines_model.h`
- Create: `lib/mines_model/mines_model.cpp`
- Test: `test/test_mines_model/test_main.cpp`

**Interfaces:**
- Consumes: nothing (pure logic).
- Produces (used by Tasks 2–3):
  - `enum class CellView : uint8_t { Hidden, Flagged, Revealed }`
  - `enum class GameState : uint8_t { Fresh, Playing, Won, Lost }`
  - `class MinesBoard` — `MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines, std::function<uint32_t()> rng)`; `GameState state() const`; `void reveal(uint8_t r, uint8_t c)`; `void toggleFlag(uint8_t r, uint8_t c)`; `CellView view(r,c) const`; `uint8_t adjacent(r,c) const`; `bool isMine(r,c) const`; `uint16_t flagsPlaced() const`; `uint16_t mineCount() const`; `uint8_t rows() const`; `uint8_t cols() const`.

**Determinism trick used throughout the tests:** placement candidates are enumerated row-major, excluding the 3×3 zone around the first tap; a partial Fisher–Yates then picks `mines` of them with `j = i + rng() % (n - i)`. With an rng that always returns 0, the mines land on the **first `mines` candidates in row-major order** — fully predictable. Small boards + zero-rng give exact layouts; 9×9/9×13 property tests use a seeded LCG.

- [ ] **Step 1: Write the failing tests**

Create `test/test_mines_model/test_main.cpp`:

```cpp
// Host-side tests for MinesBoard (pio test -e native).
//
// Layout used by most zero-rng tests: 3x3 board, 2 mines, first tap (2,2).
// Exclusion zone around (2,2) = rows 1-2 x cols 1-2, so candidates row-major
// are (0,0),(0,1),(0,2),(1,0),(2,0) and zero-rng places mines at (0,0),(0,1):
//
//   M M 1        adj: (0,2)=1  (1,0)=2 (1,1)=2 (1,2)=1
//   2 2 1             (2,0)=0  (2,1)=0 (2,2)=0
//   0 0 0
//
// First tap (2,2) flood-reveals the bottom-left 6 cells and stops at the
// numbered border; (0,2) stays hidden (its only path is through numbers).
#include <unity.h>

#include "mines_model.h"

void setUp() {}
void tearDown() {}

static uint32_t g_seed = 1;
static uint32_t lcg() {
  g_seed = g_seed * 1664525u + 1013904223u;
  return g_seed;
}
static uint32_t zero() { return 0; }

static MinesBoard rigged3x3() {  // the layout documented above
  MinesBoard b(3, 3, 2, zero);
  b.reveal(2, 2);
  return b;
}

static void test_fresh_board_all_hidden() {
  MinesBoard b(9, 9, 10, lcg);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Fresh), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL_UINT16(10, b.mineCount());
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  for (uint8_t r = 0; r < 9; ++r)
    for (uint8_t c = 0; c < 9; ++c)
      TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden),
                        static_cast<int>(b.view(r, c)));
}

static void test_first_reveal_places_mines_outside_safe_zone() {
  g_seed = 42;
  MinesBoard b(9, 9, 10, lcg);
  b.reveal(4, 4);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  uint16_t mines = 0;
  for (uint8_t r = 0; r < 9; ++r)
    for (uint8_t c = 0; c < 9; ++c) {
      if (b.isMine(r, c)) ++mines;
      if (r >= 3 && r <= 5 && c >= 3 && c <= 5)
        TEST_ASSERT_FALSE(b.isMine(r, c));  // 3x3 zone around the tap is clean
    }
  TEST_ASSERT_EQUAL_UINT16(10, mines);
}

static void test_first_reveal_corner_safe_zone_clips() {
  g_seed = 7;
  MinesBoard b(9, 9, 10, lcg);
  b.reveal(0, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  TEST_ASSERT_FALSE(b.isMine(0, 0));
  TEST_ASSERT_FALSE(b.isMine(0, 1));
  TEST_ASSERT_FALSE(b.isMine(1, 0));
  TEST_ASSERT_FALSE(b.isMine(1, 1));
}

static void test_zero_rng_layout_is_the_documented_one() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_TRUE(b.isMine(0, 0));
  TEST_ASSERT_TRUE(b.isMine(0, 1));
  TEST_ASSERT_FALSE(b.isMine(0, 2));
}

static void test_flood_fill_stops_at_numbers() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  // Revealed: the tapped zero region plus its numbered border.
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 2)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 0)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 1)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(1, 2)));
  // Beyond the numbered border: untouched.
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 2)));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 0)));
}

static void test_adjacency_counts() {
  MinesBoard b = rigged3x3();
  TEST_ASSERT_EQUAL_UINT8(2, b.adjacent(1, 0));
  TEST_ASSERT_EQUAL_UINT8(2, b.adjacent(1, 1));
  TEST_ASSERT_EQUAL_UINT8(1, b.adjacent(1, 2));
  TEST_ASSERT_EQUAL_UINT8(0, b.adjacent(2, 2));
}

static void test_reveal_last_safe_cell_wins() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 2);  // the 7th and last safe cell
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Won), static_cast<int>(b.state()));
}

static void test_reveal_mine_loses() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 0);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Lost), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(0, 0)));
}

static void test_no_ops_after_game_over() {
  MinesBoard b = rigged3x3();
  b.reveal(0, 0);  // Lost
  b.reveal(0, 2);
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 2)));
  b.toggleFlag(0, 2);
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Lost), static_cast<int>(b.state()));
}

static void test_out_of_bounds_are_no_ops() {
  MinesBoard b(3, 3, 2, zero);
  b.reveal(3, 0);
  b.reveal(0, 3);
  b.toggleFlag(200, 200);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Fresh), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(200, 200)));
  TEST_ASSERT_FALSE(b.isMine(200, 200));
  TEST_ASSERT_EQUAL_UINT8(0, b.adjacent(200, 200));
}

static void test_hard_preset_geometry() {  // 9x13 board = 13 rows x 9 cols
  g_seed = 99;
  MinesBoard b(13, 9, 25, lcg);
  b.reveal(6, 4);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  uint16_t mines = 0;
  for (uint8_t r = 0; r < 13; ++r)
    for (uint8_t c = 0; c < 9; ++c)
      if (b.isMine(r, c)) ++mines;
  TEST_ASSERT_EQUAL_UINT16(25, mines);
  TEST_ASSERT_EQUAL_UINT8(13, b.rows());
  TEST_ASSERT_EQUAL_UINT8(9, b.cols());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_board_all_hidden);
  RUN_TEST(test_first_reveal_places_mines_outside_safe_zone);
  RUN_TEST(test_first_reveal_corner_safe_zone_clips);
  RUN_TEST(test_zero_rng_layout_is_the_documented_one);
  RUN_TEST(test_flood_fill_stops_at_numbers);
  RUN_TEST(test_adjacency_counts);
  RUN_TEST(test_reveal_last_safe_cell_wins);
  RUN_TEST(test_reveal_mine_loses);
  RUN_TEST(test_no_ops_after_game_over);
  RUN_TEST(test_out_of_bounds_are_no_ops);
  RUN_TEST(test_hard_preset_geometry);
  return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `pio test -e native --filter test_mines_model`
Expected: build FAILURE — `mines_model.h: No such file or directory`.

- [ ] **Step 3: Write the model**

Create `lib/mines_model/mines_model.h`:

```cpp
// lib/mines_model/mines_model.h — minesweeper board (spec
// 2026-07-15-minesweeper-design). Pure C++17, no Arduino/LVGL; randomness is
// an injected rng (seeded LCG in native tests, esp_random() on device).
// Mines are placed on the FIRST reveal, excluding the tapped cell and its 8
// neighbors, so the first tap always opens an area. reveal() on an
// already-revealed number chords (Task 2). All coordinates are bounds-checked
// no-ops when invalid.
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

enum class CellView : uint8_t { Hidden, Flagged, Revealed };
enum class GameState : uint8_t { Fresh, Playing, Won, Lost };

class MinesBoard {
 public:
  MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines,
             std::function<uint32_t()> rng);

  GameState state() const { return state_; }
  void reveal(uint8_t r, uint8_t c);
  void toggleFlag(uint8_t r, uint8_t c);

  CellView view(uint8_t r, uint8_t c) const;
  uint8_t adjacent(uint8_t r, uint8_t c) const;  // meaningful when Revealed
  bool isMine(uint8_t r, uint8_t c) const;       // UI uses it post-loss
  uint16_t flagsPlaced() const { return flags_; }
  uint16_t mineCount() const { return mines_; }
  uint8_t rows() const { return rows_; }
  uint8_t cols() const { return cols_; }

 private:
  struct Cell {
    bool mine = false;
    bool flagged = false;
    bool revealed = false;
    uint8_t adj = 0;
  };

  size_t idx(int r, int c) const { return static_cast<size_t>(r) * cols_ + c; }
  bool inBounds(int r, int c) const {
    return r >= 0 && r < rows_ && c >= 0 && c < cols_;
  }
  void placeMines(uint8_t safeR, uint8_t safeC);
  void floodReveal(uint8_t r, uint8_t c);
  void chord(uint8_t r, uint8_t c);
  void checkWin();

  uint8_t rows_;
  uint8_t cols_;
  uint16_t mines_;
  std::function<uint32_t()> rng_;
  GameState state_ = GameState::Fresh;
  uint16_t flags_ = 0;
  uint16_t revealedCount_ = 0;
  std::vector<Cell> cells_;
};
```

Create `lib/mines_model/mines_model.cpp` (chord() lands in Task 2 — ship a stub now so this task's tests link):

```cpp
#include "mines_model.h"

#include <cstdlib>
#include <utility>

MinesBoard::MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines,
                       std::function<uint32_t()> rng)
    : rows_(rows),
      cols_(cols),
      mines_(mines),
      rng_(std::move(rng)),
      cells_(static_cast<size_t>(rows) * cols) {}

void MinesBoard::placeMines(uint8_t safeR, uint8_t safeC) {
  // Candidates: every cell outside the 3x3 zone around the first tap,
  // row-major. Partial Fisher-Yates picks mines_ distinct ones.
  std::vector<uint16_t> cand;
  cand.reserve(cells_.size());
  for (int r = 0; r < rows_; ++r)
    for (int c = 0; c < cols_; ++c) {
      const bool nearTap = std::abs(r - static_cast<int>(safeR)) <= 1 &&
                           std::abs(c - static_cast<int>(safeC)) <= 1;
      if (!nearTap) cand.push_back(static_cast<uint16_t>(idx(r, c)));
    }
  for (uint16_t i = 0; i < mines_; ++i) {
    const uint16_t j =
        i + static_cast<uint16_t>(rng_() % static_cast<uint32_t>(cand.size() - i));
    std::swap(cand[i], cand[j]);
    cells_[cand[i]].mine = true;
  }
  for (int r = 0; r < rows_; ++r)
    for (int c = 0; c < cols_; ++c) {
      if (cells_[idx(r, c)].mine) continue;
      uint8_t n = 0;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc)
          if (inBounds(r + dr, c + dc) && cells_[idx(r + dr, c + dc)].mine) ++n;
      cells_[idx(r, c)].adj = n;
    }
}

void MinesBoard::reveal(uint8_t r, uint8_t c) {
  if (!inBounds(r, c)) return;
  if (state_ == GameState::Won || state_ == GameState::Lost) return;
  Cell& cell = cells_[idx(r, c)];
  if (cell.flagged) return;
  if (state_ == GameState::Fresh) {
    placeMines(r, c);
    state_ = GameState::Playing;
  }
  if (cell.revealed) {
    chord(r, c);
    return;
  }
  if (cell.mine) {
    cell.revealed = true;
    state_ = GameState::Lost;
    return;
  }
  floodReveal(r, c);
  checkWin();
}

// Iterative flood fill (bounded explicit stack, no recursion): reveal the
// tapped safe cell; a 0-adjacency cell also queues its neighbors. Flagged
// cells are never auto-revealed (classic rule).
void MinesBoard::floodReveal(uint8_t r0, uint8_t c0) {
  std::vector<uint16_t> stack{static_cast<uint16_t>(idx(r0, c0))};
  while (!stack.empty()) {
    const uint16_t i = stack.back();
    stack.pop_back();
    Cell& cell = cells_[i];
    if (cell.revealed || cell.flagged || cell.mine) continue;
    cell.revealed = true;
    ++revealedCount_;
    if (cell.adj != 0) continue;
    const int r = i / cols_;
    const int c = i % cols_;
    for (int dr = -1; dr <= 1; ++dr)
      for (int dc = -1; dc <= 1; ++dc)
        if (inBounds(r + dr, c + dc))
          stack.push_back(static_cast<uint16_t>(idx(r + dr, c + dc)));
  }
}

void MinesBoard::chord(uint8_t, uint8_t) {}  // Task 2

void MinesBoard::checkWin() {
  if (state_ != GameState::Playing) return;
  const uint16_t safe = static_cast<uint16_t>(rows_) * cols_ - mines_;
  if (revealedCount_ >= safe) state_ = GameState::Won;
}

void MinesBoard::toggleFlag(uint8_t r, uint8_t c) {
  if (!inBounds(r, c)) return;
  if (state_ != GameState::Fresh && state_ != GameState::Playing) return;
  Cell& cell = cells_[idx(r, c)];
  if (cell.revealed) return;
  cell.flagged = !cell.flagged;
  flags_ = cell.flagged ? flags_ + 1 : flags_ - 1;
}

CellView MinesBoard::view(uint8_t r, uint8_t c) const {
  if (!inBounds(r, c)) return CellView::Hidden;
  const Cell& cell = cells_[idx(r, c)];
  if (cell.revealed) return CellView::Revealed;
  if (cell.flagged) return CellView::Flagged;
  return CellView::Hidden;
}

uint8_t MinesBoard::adjacent(uint8_t r, uint8_t c) const {
  return inBounds(r, c) ? cells_[idx(r, c)].adj : 0;
}

bool MinesBoard::isMine(uint8_t r, uint8_t c) const {
  return inBounds(r, c) && cells_[idx(r, c)].mine;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `pio test -e native --filter test_mines_model`
Expected: `11 Tests 0 Failures 0 Ignored` — PASSED.

- [ ] **Step 5: Commit**

```bash
git add lib/mines_model test/test_mines_model
git commit -m "feat: mines_model core — first-tap-safe placement, flood fill, win/lose"
```

---

### Task 2: `lib/mines_model` — flags blocking reveals + chording (TDD)

**Files:**
- Modify: `lib/mines_model/mines_model.cpp` (replace the `chord()` stub)
- Test: `test/test_mines_model/test_main.cpp` (append tests)

**Interfaces:**
- Consumes: Task 1's `MinesBoard`.
- Produces: complete `reveal()` semantics — chording on revealed numbers.

- [ ] **Step 1: Append the failing tests**

Append to `test/test_mines_model/test_main.cpp` (above `main`), and register each in `main` with `RUN_TEST`:

```cpp
static void test_flag_blocks_reveal_until_unflagged() {
  MinesBoard b = rigged3x3();
  b.toggleFlag(0, 2);
  TEST_ASSERT_EQUAL_UINT16(1, b.flagsPlaced());
  b.reveal(0, 2);  // blocked
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Flagged), static_cast<int>(b.view(0, 2)));
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  b.toggleFlag(0, 2);
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  b.reveal(0, 2);
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Won), static_cast<int>(b.state()));
}

static void test_flag_on_revealed_cell_is_a_no_op() {
  MinesBoard b = rigged3x3();
  b.toggleFlag(2, 2);
  TEST_ASSERT_EQUAL_UINT16(0, b.flagsPlaced());
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(2, 2)));
}

static void test_chord_with_correct_flags_reveals_neighbors() {
  MinesBoard b = rigged3x3();
  // (1,2) shows 1; its only mine neighbor is (0,1). Flag it and chord.
  b.toggleFlag(0, 1);
  b.reveal(1, 2);  // chord: reveals (0,2), the last safe cell
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Won), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Revealed), static_cast<int>(b.view(0, 2)));
}

static void test_chord_with_wrong_flag_loses() {
  MinesBoard b = rigged3x3();
  // Wrong guess: flag safe (0,2) instead of mine (0,1), chord (1,2).
  b.toggleFlag(0, 2);
  b.reveal(1, 2);  // reveals unflagged neighbor (0,1) — a mine
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Lost), static_cast<int>(b.state()));
}

static void test_chord_with_flag_count_mismatch_is_a_no_op() {
  MinesBoard b = rigged3x3();
  b.reveal(1, 2);  // no flags around: nothing happens
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
  TEST_ASSERT_EQUAL(static_cast<int>(CellView::Hidden), static_cast<int>(b.view(0, 2)));
}

static void test_chord_on_zero_cell_is_a_no_op() {
  MinesBoard b = rigged3x3();
  b.reveal(2, 0);  // revealed 0-cell: chord must not fire
  TEST_ASSERT_EQUAL(static_cast<int>(GameState::Playing), static_cast<int>(b.state()));
}
```

- [ ] **Step 2: Run the tests to verify the new ones fail**

Run: `pio test -e native --filter test_mines_model`
Expected: FAIL — `test_chord_with_correct_flags_reveals_neighbors` and `test_chord_with_wrong_flag_loses` fail (chord is a stub); the flag tests pass already.

- [ ] **Step 3: Implement chord()**

Replace the stub in `lib/mines_model/mines_model.cpp`:

```cpp
// Chording: tapping a revealed number whose adjacent-flag count matches its
// number reveals all its non-flagged hidden neighbors (mines included — a
// wrong flag loses, classic behavior).
void MinesBoard::chord(uint8_t r, uint8_t c) {
  const Cell& cell = cells_[idx(r, c)];
  if (cell.adj == 0) return;
  uint8_t flagsAround = 0;
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc)
      if (inBounds(r + dr, c + dc) && cells_[idx(r + dr, c + dc)].flagged)
        ++flagsAround;
  if (flagsAround != cell.adj) return;

  // First pass: any unflagged hidden mine neighbor loses immediately.
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc) {
      if (!inBounds(r + dr, c + dc)) continue;
      Cell& n = cells_[idx(r + dr, c + dc)];
      if (!n.revealed && !n.flagged && n.mine) {
        n.revealed = true;
        state_ = GameState::Lost;
        return;
      }
    }
  // Second pass: flood-reveal the safe ones.
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc)
      if (inBounds(r + dr, c + dc)) {
        const Cell& n = cells_[idx(r + dr, c + dc)];
        if (!n.revealed && !n.flagged)
          floodReveal(static_cast<uint8_t>(r + dr), static_cast<uint8_t>(c + dc));
      }
  checkWin();
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `pio test -e native --filter test_mines_model`
Expected: `17 Tests 0 Failures 0 Ignored` — PASSED.

- [ ] **Step 5: Run the full native suite (no regressions)**

Run: `pio test -e native`
Expected: all test dirs pass.

- [ ] **Step 6: Commit**

```bash
git add lib/mines_model test/test_mines_model
git commit -m "feat: mines_model — flag semantics and chording"
```

---

### Task 3: MinesweeperApp UI + catalog entry

**Files:**
- Modify: `src/apps/app_catalog.h` (add `kMines` after `kPet` — or after `kPomodoro` if the pomodoro plan already landed)
- Create: `src/apps/minesweeper/MinesweeperApp.h`
- Create: `src/apps/minesweeper/MinesweeperApp.cpp`

**Interfaces:**
- Consumes: `MinesBoard` (Tasks 1–2); `ISettingsStore` (`getU32`/`setU32`); `catalog::kMines`; `esp_random()`.
- Produces (used by Task 4): `class MinesweeperApp : public App` with `void setDeps(ISettingsStore&)`.

- [ ] **Step 1: Add the catalog entry**

In `src/apps/app_catalog.h` add:

```cpp
inline constexpr AppInfo kMines{"Campo Minado", nullptr};
```

- [ ] **Step 2: Write the app header**

Create `src/apps/minesweeper/MinesweeperApp.h`:

```cpp
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
```

- [ ] **Step 3: Write the app implementation**

Create `src/apps/minesweeper/MinesweeperApp.cpp`:

```cpp
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
constexpr Preset kEasy{9, 9, 10, 26, "mines_best_easy"};
constexpr Preset kHard{13, 9, 25, 19, "mines_best_hard"};

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
  const Row rows[] = {{kEasy, "Fácil - 9x9, 10 minas", onEasy},
                      {kHard, "Difícil - 9x13, 25 minas", onHard}};
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
```

- [ ] **Step 4: Verify the device build compiles**

Run: `pio run -e cyd`
Expected: SUCCESS (app not yet registered; compilation proof only).

- [ ] **Step 5: Commit**

```bash
git add src/apps/app_catalog.h src/apps/minesweeper
git commit -m "feat: MinesweeperApp UI — custom-drawn grid, HUD, flag mode, resume"
```

---

### Task 4: Register the app in main.cpp

**Files:**
- Modify: `src/main.cpp` (instance near the other `static XxxApp` lines ~44–48; registration in `setup()` before the `settingsApp` lines ~153)

**Interfaces:**
- Consumes: Task 3's `MinesweeperApp::setDeps(ISettingsStore&)`; `Launcher::registerApp`.
- Produces: the app on the home grid (before Settings). No badge, no SD dependency — the app stays enabled even when `sdOk` is false.

- [ ] **Step 1: Wire main.cpp**

Add the include next to the other app includes:

```cpp
#include "apps/minesweeper/MinesweeperApp.h"
```

Add the instance after the last `static XxxApp` app line:

```cpp
static MinesweeperApp minesApp;
```

In `setup()`, after the pet (or pomodoro, if present) registration and **before** the `settingsApp` lines:

```cpp
minesApp.setDeps(settings);
launcher.registerApp(&minesApp);
```

- [ ] **Step 2: Build**

Run: `pio run -e cyd`
Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: register Campo Minado app"
```

---

### Task 5: README art TODOs + on-device verification

**Files:**
- Modify: `README.md` (append to the `## TODO` section)

**Interfaces:**
- Consumes: nothing new.
- Produces: art work-items for the user; a verified app.

- [ ] **Step 1: Append to the README `## TODO` list**

Add after the pomodoro-art block (or the pet block if pomodoro hasn't landed), keeping the existing checkbox style:

```markdown
- [ ] draw the launcher icon for campo minado — wire in `app_catalog.h` like
      oracle's once drawn:
  - [ ] `sd/art/icons/mines.bin`
- [ ] (optional) campo minado cell sprites — v1 draws text glyphs (`F`, `*`)
      and is fully playable without art; if drawn, wire in
      `src/apps/minesweeper/MinesweeperApp.cpp` (drawn at cell size, 26/19 px):
  - [ ] `sd/art/mines/flag.bin`
  - [ ] `sd/art/mines/mine.bin`
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: campo minado sprite TODOs"
```

- [ ] **Step 3: Flash and verify on device**

Flash: `pio run -e cyd -t upload` (CYD on `/dev/ttyUSB0`; free the serial port first if a reader is attached).

Checklist (all must pass):
1. Launcher shows "Campo Minado" (colored-letter fallback icon) before Settings.
2. Start screen: Fácil/Difícil buttons with "melhor: -"; no Continuar yet.
3. Fácil: 9×9 grid, 26 px cells, centered; HUD shows `10` and `0:00`.
4. First tap opens an area (never a mine); timer starts.
5. "Bandeira" checked: taps place/remove red `F`; mines counter tracks; unchecked taps reveal again.
6. Flagged cell can't be revealed; tapping a satisfied number chords.
7. Back mid-game → start screen shows "Continuar (m:ss)"; Continuar restores the exact board, timer resumes on the paused value.
8. Exit the app mid-game, re-enter → Continuar still offered; starting a new game discards it.
9. Lose: all mines shown as `*`, red overlay "Perdeu!", tap → start screen, no Continuar.
10. Win (easy board): green overlay with time + "Novo recorde!"; best time shows on the start screen and survives a reboot.
11. Difícil: 9×13 grid at 19 px renders inside the content area; cells are tappable (stylus OK).
12. Timer while paused does not advance (pause 30 s on the start screen, resume, verify).

- [ ] **Step 4: Record the result**

Commit any fixes found during verification, then:

```bash
git commit --allow-empty -m "test: Campo Minado on-device verification passed"
```
