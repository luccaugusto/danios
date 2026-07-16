# Minesweeper custom setup — design

**Date:** 2026-07-16
**Status:** approved
**Context:** replaces the two hardcoded difficulty buttons on the Campo
Minado start screen (see `2026-07-15-minesweeper-design.md`, whose
"no custom board sizes" non-goal this supersedes) with a configurable
setup: stepper controls for board size and mine count, plus preset
buttons that fill in today's Fácil/Difícil values.

## Goal

Let the player pick rows, columns, and mine count freely (within
screen-fit and playability limits) while keeping the two presets one tap
away and keeping best-time records for preset games.

## Non-goals

- Best-time records for non-preset configs.
- Changes to `lib/mines_model` — `MinesBoard` already takes arbitrary
  `rows, cols, mines`.
- Changes to the board screen, drawing, pause/resume, or timer.

## UI — start screen (rewritten)

Top to bottom:

1. Title "Minas".
2. **Continuar (m:ss)** — unchanged, shown only when a paused game exists.
3. Three **stepper rows**, each: name label, current value, `-` and `+`
   buttons. Steppers, not sliders — exact integers are fiddly on the
   resistive digitizer, and buttons are cheap on the LVGL pool. `+`/`-`
   auto-repeat on long press (`LV_EVENT_LONG_PRESSED_REPEAT`).
   - **Linhas** 5–14
   - **Colunas** 5–12
   - **Minas** 1–⌊0.35 × linhas × colunas⌋ (see Rules below)
4. **Preset row** — two half-width buttons, **Fácil** and **Difícil**,
   each with its NVS best time underneath ("melhor: 1:23" / "melhor: -").
   Tapping one only sets the three steppers (Fácil → 9×9, 18; Difícil →
   13 rows × 9 cols, 33); it does **not** start a game.
5. Full-width **Jogar** button — starts a new game with the stepper
   values (discarding any paused game, as today).

The layout must fit 320 px minus the top bar with Continuar visible;
compress paddings / stepper row height as needed rather than scrolling.

## Rules and derived values

- **Mines clamp:** whenever rows or cols change, the mines maximum is
  recomputed and the current value clamped down if needed, so the shown
  config is always valid. The 35 % cap keeps first-tap safety trivially
  satisfiable (worst case 5×5: 8 mines ≤ 25 − 9 = 16 candidate cells)
  and accommodates both presets (18/81 ≈ 22 %, 33/117 ≈ 28 %).
- **Cell size:** the per-preset `cellPx` constant goes away. At
  `newGame(rows, cols, mines)`:
  `cellPx_ = min(rootW / cols, (rootH − kHudH − 2) / rows)`, capped at
  32 px so small grids don't balloon. Range limits above guarantee
  ≥ ~18 px cells (tappable, numbers fit `LV_FONT_DEFAULT`).
- **Range limits rationale:** cols ≤ 12 keeps cells ≥ 20 px wide
  (240/12); rows ≤ 14 keeps cells ≥ ~18 px tall in the area below the
  HUD. Minimum 5×5 keeps the game meaningful.

## Code changes (all in `src/apps/minesweeper/`)

- `Difficulty` enum, `onEasy`/`onHard`, and `Preset::cellPx` are removed.
  `newGame(uint8_t rows, uint8_t cols, uint16_t mines)` replaces
  `newGame(Difficulty)`. Presets shrink to
  `{rows, cols, mines, bestKey}`.
- `showStart()` builds the steppers/presets/Jogar UI; stepper values live
  in member fields (`setupRows_`, `setupCols_`, `setupMines_`).
- `endGame()` picks the best-time NVS key by comparing the **played**
  game's `(rows, cols, mines)` — taken from the `MinesBoard`, not the
  current stepper values — against the two presets; no match → no record.
  A manually dialed 9×9/18 counts as Fácil (values reached either way
  are the same game).
- Header comment updated (start screen description).

## Persistence

Existing: `mines_best_easy`, `mines_best_hard` (unchanged, u32 seconds).
New: `mines_cfg_rows`, `mines_cfg_cols`, `mines_cfg_mines` (u32) — the
last-used stepper values, written when Jogar is tapped, read (and
clamped to the valid ranges) when the start screen builds. First run
defaults to Fácil.

## Error handling

Steppers clamp at their bounds (buttons no-op past the edge); mines
re-clamps on any rows/cols change; NVS reads clamp out-of-range stored
values. No other I/O.

## Testing

- **Native:** none — `lib/mines_model` is untouched and the stepper
  clamp logic is trivial UI state. (If clamping grows hairy in review,
  extract and test it; not expected.)
- **On-device:** steppers step and long-press-repeat within bounds;
  mines max re-clamps when shrinking the board; presets fill correct
  values and show best times; Jogar starts the dialed config with sane
  cell sizes at the extremes (5×5, 14×12); best time recorded only for
  preset configs (including manually dialed ones); last-used config
  survives app exit and reboot; Continuar still works.
