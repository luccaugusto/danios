# danios app spec — Calculator

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §4.3, §8.
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative — never rename its names/paths.
**Roadmap slot:** A1 (`lib/calc_engine/` reserved in roadmap §3). Depends only on F2 (already done) — no radio, no SD, no NVS.

---

## What it is

A basic touchscreen calculator. App id `"calc"` (pinned), replaces the
`calcStub` registration in `src/main.cpp`. `requiredRadio()` = `None`
(radios off = battery saving).

## Requirements

- LVGL keypad: `0–9  .  +  −  ×  ÷  =  C  ⌫  +/−  %`.
- Four-function arithmetic **with operation chaining** (e.g. `2 + 3 × 4 =`
  evaluates left-to-right as entered: `2+3=5`, `5×4=20` — classic calculator
  chaining, not precedence).
- Graceful divide-by-zero handling (show an error state, `C` recovers; never
  crash or show `inf`/`nan` raw).
- Sensible number formatting (no trailing `.000000`; fit the display width).
- Overflow handled without crashing.
- **No** history or memory keys — kept intentionally simple (spec non-goal).

## Architecture (roadmap conventions)

- **Pure logic:** `lib/calc_engine/` — std C++17 only, zero Arduino/LVGL
  includes. The whole engine (digit entry, chaining, `C`/`⌫`/`+/−`/`%`,
  divide-by-zero, formatting, overflow) lives here and is native-tested with
  `pio test -e native` (Unity), TDD, one test dir `test/test_calc_engine/`.
- **Thin UI wrapper:** `src/apps/calculator/CalculatorApp.{h,cpp}` — an `App`
  (roadmap §4.5) that builds an LVGL keypad (`lv_btnmatrix` is the natural
  fit) + display label, forwarding key presses to the engine and rendering
  `engine.display()`.
- UI renders portrait 240×320 below the launcher-provided top bar (back arrow
  is the launcher's; don't add your own). Read `docs/DISPLAY.md` before UI work.

## Name & icon

Launcher label and icon come from `catalog::kCalc` in
`src/apps/app_catalog.h` — the app's `title()`/`iconPath()` must return those
fields. Icon file (when drawn): `S:/art/icons/calc.bin`; `nullptr` until then.

## Testing (spec §8)

Calculator engine is the flagship native-TDD module: arithmetic, chaining,
divide-by-zero, formatting, overflow — all off-device. On-device verification
is manual: a handful of scripted key sequences with expected display values.

## E2E outcome (roadmap §1)

Working four-function calculator on device.
