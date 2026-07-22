# Runtime Orientation Setting — Design

**Date:** 2026-07-22
**Branch:** `landscape-ui` (final phase before merge)
**Status:** Approved

## Goal

Replace the compile-time `DANIOS_LANDSCAPE` flag with a user-facing setting:
Settings → Tela gets a landscape toggle, persisted in NVS, **applied on
reboot** (with an optional immediate restart). One firmware binary serves
both orientations; the `cyd-landscape` build env goes away.

Decisions (user): single env; restart-now offered after toggling; default
is **portrait** when the NVS key is unset.

## How it works

- **NVS key:** `disp.landscape` (bool, default `false` = portrait), read
  once at boot.
- **`layout::init(bool landscape)`** runs in `setup()` immediately after
  `settings.begin()` and before `displayService.begin()` — every widget is
  built after this point, so all existing `layout::` consumers keep
  working unchanged.
- **`src/core/Layout.h` constants become boot-time globals** (new
  `Layout.cpp`), keeping their current names (`layout::kLandscape`,
  `kScreenW`, `kScreenH`, `kAppW`, `kAppH`, `kGridCols`) so the ~40 call
  sites don't change. `kTopBarH` stays `constexpr` (orientation-
  independent). The `k` prefix on now-mutable globals is a deliberate
  naming compromise to avoid touching every call site; they are set once
  at boot and documented as such.
- **`DisplayService::begin()`** picks rotation (6/7) and the calibration
  array (landscape-reordered / portrait) at runtime — no other change;
  the LVGL driver resolution comes from `layout::` as before, just no
  longer `constexpr`.
- **The `#ifdef DANIOS_LANDSCAPE` and `[env:cyd-landscape]` are deleted.**
  One env (`cyd`), one binary.

## Constexpr fallout (the only code that must restructure)

Everything that consumed `layout::` in a *constant expression* moves to
runtime evaluation — all of it is used only after boot, so first-use
initialization is safe:

- `DisplayService.h`: `kHorRes`/`kVerRes` members deleted; `begin()` uses
  `layout::kScreenW/kScreenH` directly. `kBufPixels = 7200` stays.
- `MinesweeperApp.cpp`: `kEasy`/`kHard` presets, `kHudH`, `kMaxRows`
  become functions (`easyPreset()`, `hardPreset()`, `hudH()`,
  `maxRows()`) returning orientation-correct values at call time.
- `WeatherApp.cpp`: function-local `constexpr kArtW/kArtH` → `const`.
- `main.cpp`: `kBootLogo` ternary → plain function-local `const char*`.
- `Launcher.h`: `model_{layout::kGridCols}` reads the value at static-init
  time (before NVS) — fixed by constructing with 3 and calling a new
  `LauncherModel::setColumns(int)` in `buildHomeScreen()` (columns only
  feed `slotOf` math, so late setting is safe; native-tested).

## Settings UI (Tela section)

Below the existing Brilho/Suspender controls:

- Row: label "Deitado (USB à esquerda)" + `lv_switch`, checked from
  `disp.landscape`.
- On toggle: `setBool("disp.landscape", v)`, then a msgbox — title
  "Orientação", text "Reinicie para aplicar.", buttons
  **"Reiniciar"** (calls `esp_restart()`) and **"Depois"** (closes).
  Strings use the PT font's codepoint range (no em dashes).

## Out of scope

- Live rotation without reboot.
- Removing the landscape layouts or art — everything shipped by the
  previous phases stays; this only changes how the orientation is chosen.

## Verification

- `pio run -e cyd` SUCCESS (and `cyd-landscape` no longer exists);
  `pio test -e native` all pass (including new `setColumns` coverage).
- Flash once. On device: boots portrait (fresh key) → Settings → Tela →
  toggle Deitado → Reiniciar → boots landscape (USB left, touch correct,
  apps in landscape layouts) → toggle back → Depois → manual power cycle
  → portrait again.

## Outcome (2026-07-22)

Verified on device: the full toggle round-trip passed (portrait boot with
key unset → Reiniciar into landscape → Depois + power cycle back to
portrait). 247/247 native tests, single `cyd` env, `DANIOS_LANDSCAPE`
gone from all code. Reviews confirmed static-init safety (no `layout::`
reads before `layout::init`) and that the msgbox close-X cannot trigger
the restart.
