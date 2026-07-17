# Landscape UI Spike — Design

**Date:** 2026-07-17
**Branch:** `landscape-ui`
**Status:** Approved

## Goal

Evaluate the device in landscape orientation (rotated 90° so the USB-C port
sits on the **right** instead of below) and decide whether landscape should
become the permanent orientation. Both orientations stay buildable via a
compile-time flag so portrait/landscape builds can be flashed A/B on the same
device.

This is an **evaluation spike**: the launcher, status bar, and two
representative apps (Weather, Minesweeper) get real landscape layouts. The
remaining apps run but are known-broken in landscape (bottom clip) — see
Out of scope.

## Build flag

- New PlatformIO env `[env:cyd-landscape]` extending `[env:cyd]` with
  `-DDANIOS_LANDSCAPE=1`.
- `pio run -e cyd` → portrait (unchanged); `pio run -e cyd-landscape` →
  landscape.
- `[env:native]` builds without the flag → portrait constants; existing unit
  tests unaffected.

## Layout constants — `src/core/Layout.h`

The only place the flag is read besides `DisplayService`:

```cpp
namespace layout {
#ifdef DANIOS_LANDSCAPE
constexpr bool kLandscape = true;
#else
constexpr bool kLandscape = false;
#endif
constexpr lv_coord_t kScreenW = kLandscape ? 320 : 240;
constexpr lv_coord_t kScreenH = kLandscape ? 240 : 320;
// kAppH derives from Launcher::kTopBarH (32): 288 portrait, 208 landscape.
constexpr int kGridCols = kLandscape ? 4 : 3;
}
```

All other code branches with plain `if (layout::kLandscape)` — **no
`#ifdef` outside Layout.h and DisplayService** — so both orientation paths
compile in every build and can't silently rot. Hardcoded 240/320/288 in
touched files are replaced by these constants (a cleanup that benefits main
even if landscape loses).

## Step 0 — hardware probe (before any UI work)

A throwaway flash that:

1. Draws labeled text/arrows at candidate landscape rotations — try **6**
   first (the 90° neighbor of portrait's rotation 7 within LovyanGFX's
   mirrored 4–7 family), fall back to **4** — to pin the exact rotation
   value with USB-C right and non-mirrored text.
2. Draws touch-corner markers to check whether the rotation-7 calibration
   array (`DisplayService::begin()`) still lands corners correctly after
   the rotation change. If not, re-run `calibrateTouch` once at the
   landscape rotation and store a second, flag-selected 8-value array
   (per docs/DISPLAY.md: never hand-edit or add manual swap/mirror code).

UI work starts only after the rotation value and calibration story are
pinned. The probe is throwaway code — not merged.

## Display service

- `setRotation(layout::kLandscape ? <probed value> : 7)`.
- `kHorRes/kVerRes` come from `layout::kScreenW/kScreenH`.
- Draw buffer keeps the same ~14.4 KB budget: same pixel count,
  `kScreenW` wide × correspondingly fewer lines.
- `TouchService` already reads post-rotation dimensions from LovyanGFX —
  no changes beyond what the probe dictates for calibration.

## Launcher & StatusBar

- Replace hardcoded 240/320 with layout constants (home screen, grid
  container, app-screen top bar, app container, status bar width).
- Grid: `LauncherModel model_{layout::kGridCols}` — 4 columns × 80 px
  fills 320 exactly; 2 rows of 98 px cells fit the 216 px grid area.
- App container in landscape: **320×208**.

## Weather — landscape layout (split screen)

Portrait code path untouched. Landscape:

- **Left half (160×208), pure art panel — no text over it:** background
  art (240×288) center-cropped; character stack (188×222 base + outfit +
  overlay, sharing one anchor as today) zoomed to fit.
- **Right half, clean data panel:** temp + condition, city, hi/lo,
  3-day forecast as a compact list, stale-status line.

## Minesweeper — landscape layout

- **Game screen — slim top HUD, wide board:** HUD compressed to one thin
  row (~28 px: mines · timer · flag toggle); board gets the full 320 width
  below — up to ~12×6 at the current 26 px cells. Preset and custom-setup
  clamps adjusted to what fits.
- **Setup screen — two columns:** rows/cols/mines steppers left,
  difficulty presets + play button right (the portrait vertical stack
  needs ~288 px; landscape has 208).

## Out of scope (known-broken in landscape build)

- Calculator, Oracle, Pet, Pomodoro, Settings run in the 320×208 container
  and clip their bottom ~80 px. Not fixed in the spike.
- Boot splash logo (240×288) is centered with bottom crop.
- No art is redrawn for landscape; portrait art is cropped/zoomed.

## Verification

- Both envs compile: `pio run -e cyd -e cyd-landscape`; native tests pass.
- On-device landscape: orientation/USB side correct, touch corners land,
  launcher taps open the right apps, weather renders split-screen, a full
  minesweeper game is playable.
- Re-flash portrait env: zero visual/behavioral regression.

## Decision criteria

After living with the landscape build: if it wins, the flag flips to
default (or portrait is removed) in a follow-up; if not, the branch's
constant cleanup can still be cherry-picked to main.
