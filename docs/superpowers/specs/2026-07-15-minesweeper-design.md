# Minesweeper app — design

**Date:** 2026-07-15
**Status:** approved
**Context:** replaces the deferred Music app (A4) in the launcher lineup,
alongside Pomodoro (see `2026-07-15-pomodoro-design.md`).

## Goal

Classic minesweeper on the 240×320 touch screen: two difficulties
(Fácil 9×9 / 10 mines, Difícil 9×13 / 25 mines), first-tap-safe mine
placement, flood-fill reveal, flagging via an explicit reveal/flag mode
toggle, chording, per-difficulty best times in NVS, and pause/resume when
leaving mid-game.

## Non-goals

- Custom board sizes or mine counts beyond the two presets.
- Saving an in-progress game across reboot (RAM-only pause/resume).
- Cell art in v1 — text glyphs; sprites are an optional later upgrade.

## Architecture

House pattern, two units:

1. **`lib/mines_model/`** (pure C++17, zero Arduino/LVGL) — board state,
   mine placement, reveal/flag/chord rules, win/lose detection.
2. **`src/apps/minesweeper/MinesweeperApp`** — LVGL UI (start screen +
   board screen), game timer, NVS best times.

Catalog entry `catalog::kMines{"Campo Minado", nullptr}` (icon path becomes
`"S:/art/icons/mines.bin"` once the art exists). App id `"mines"`,
`requiredRadio() == RadioMode::None`. Registered in `main.cpp` before
Settings.

### MinesBoard public API

```cpp
enum class CellView : uint8_t { Hidden, Flagged, Revealed };
enum class GameState : uint8_t { Fresh, Playing, Won, Lost };

class MinesBoard {
 public:
  // rng: returns uniform uint32_t; injected (seeded LCG in native tests,
  // esp_random() on device).
  MinesBoard(uint8_t rows, uint8_t cols, uint16_t mines,
             std::function<uint32_t()> rng);

  GameState state() const;
  void      reveal(uint8_t r, uint8_t c);      // also chords, see rules
  void      toggleFlag(uint8_t r, uint8_t c);

  CellView  view(uint8_t r, uint8_t c) const;
  uint8_t   adjacent(uint8_t r, uint8_t c) const;  // valid when Revealed
  bool      isMine(uint8_t r, uint8_t c) const;    // for post-loss display
  uint16_t  flagsPlaced() const;
  uint16_t  mineCount() const;
  uint8_t   rows() const;  uint8_t cols() const;
};
```

## Game rules

- **First-tap safety:** the board starts `Fresh` with no mines. The first
  `reveal(r,c)` places mines uniformly (Fisher–Yates over candidate cells
  using the injected rng) excluding the tapped cell **and its 8 neighbors**,
  then transitions to `Playing`. Both presets leave enough candidates
  (72 ≥ 10, 108 ≥ 25).
- **Reveal:** revealing a 0-adjacency cell flood-fills its region (iterative,
  not recursive — bounded stack). Revealing a mine → `Lost`.
- **Flag:** `toggleFlag` cycles Hidden↔Flagged; flagged cells can't be
  revealed; flags are unlimited. No-op on revealed cells and outside
  `Playing`/`Fresh`.
- **Chording:** `reveal` on an already-Revealed cell with `adjacent > 0`
  whose count of adjacent flags equals its number reveals all its
  non-flagged hidden neighbors (which can hit a mine if flags were wrong —
  classic behavior). If flag count ≠ number, no-op.
- **Win:** all non-mine cells revealed → `Won` (flags irrelevant).
- After `Won`/`Lost`, `reveal`/`toggleFlag` are no-ops.

## UI

Portrait 240×320, standard top bar. Two internal screens; `handleBack()`
returns true on the board screen (steps back to the start screen), false on
the start screen (launcher exits the app).

### Start screen

- Title, then two big buttons, each with its NVS best time underneath
  ("melhor: 1:23" or "melhor: —"):
  - **Fácil** — 9×9, 10 minas
  - **Difícil** — 9×13, 25 minas
- When a paused game exists (see below), a third button on top:
  **Continuar (m:ss)** — returns to the board exactly as left.
  Starting a new game discards the paused one.

### Board screen

- **HUD row:** mines remaining (`mineCount − flagsPlaced`), elapsed timer
  `m:ss`, and the mode toggle button showing the active mode (reveal ⛏ /
  flag 🚩 — rendered as text glyphs). Every board tap performs the active
  mode's action; this is the only flagging mechanism (no long-press).
- **Grid:** cells are 26 px on Fácil (9 cols = 234 px) and ~19 px on
  Difícil (fits 13 rows in the remaining height; exact size computed from
  real top-bar/HUD heights at implementation). Rendering uses **one
  custom-drawn widget** (LVGL draw-event hooks on a single container:
  rects + text drawn per cell, touch point mapped back to row/col). No
  per-cell LVGL objects — 117 widgets would strain the ~20K LVGL pool
  floor, and a 234×338 canvas buffer (~158 KB RGB565) is out of budget.
- **Cell rendering:** hidden = raised tile; flagged = tile + red `F`;
  revealed 0 = sunken blank; numbers 1–8 in the classic per-number colors;
  mine = `*`. Fonts already on flash (montserrat).
- **Timer:** starts on the first reveal, driven by `millis()` in `tick()`,
  stops on win/lose.
- **Lose:** all mines shown, brief red flash, "Perdeu — toque para voltar"
  → start screen.
- **Win:** brief flash, "Venceu! m:ss" plus "novo recorde!" when applicable
  → tap → start screen.

### Pause/resume

Leaving the board screen — via back (→ start screen) or exiting the app —
pauses the game: the app accumulates elapsed ms and keeps the `MinesBoard`
in its (boot-time static) instance. Re-entering the app lands on the start
screen with the **Continuar** button. Won/Lost boards are not resumable.

## Persistence

Via `ISettingsStore` (NVS): `mines_best_easy`, `mines_best_hard` — u32
best time in seconds, 0 = none yet. Written only on a new best.

## Error handling

No radio, no I/O beyond NVS. Model guards all out-of-bounds coordinates as
no-ops (belt-and-braces; the UI only maps taps inside the grid).

## Art (append to README TODO)

- `sd/art/icons/mines.bin` — launcher icon, wired in `app_catalog.h`.
- Optional, later: `sd/art/mines/flag.bin` + `sd/art/mines/mine.bin` cell
  sprites; v1 uses text glyphs and is fully playable without art.

## Testing

- **Native (`lib/mines_model` tests, test-first, seeded LCG rng):**
  first-reveal exclusion zone (tapped cell + neighbors never mined; correct
  mine count placed); flood fill on zero regions incl. board edges;
  flag/unflag and reveal-blocked-by-flag; chording (correct flags →
  neighbors revealed; wrong flags → loss; count mismatch → no-op);
  win detection with and without flags; loss reveals state queryable;
  no-ops after Won/Lost and out-of-bounds; both preset geometries.
- **On-device:** both difficulties playable; mode toggle behavior; HUD
  counters; timer start-on-first-reveal; pause/resume via back and via app
  exit; best time persists across reboot; ~19 px Difícil cells acceptably
  tappable (stylus available).
