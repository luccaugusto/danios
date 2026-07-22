# Landscape UI Spike — Design

**Date:** 2026-07-17
**Branch:** `landscape-ui`
**Status:** Approved

## Goal

Evaluate the device in landscape orientation (rotated 90° so the USB-C port
sits on the **left** instead of below — revised from "right" during the
Task 1 hardware probe) and decide whether landscape should
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

**COMPLETED 2026-07-17. Measured facts:**

- **Landscape rotation = 6** — readable, non-mirrored text with the USB-C
  port on the **left** (the user revised the goal from USB-right to
  USB-left mid-probe; rotation 4 is the same image rotated 180°, USB-right).
- **Touch calibration does NOT survive the rotation change**, and a plain
  `calibrateTouch` re-capture at rotation 6 does not fix it either: the
  capture internally cancels rotation to 0, and this clone panel's hardware
  mirror makes LovyanGFX's `convertRawXY` apply the wrong mirror-family
  transform at rotation 6 (an x-flip where the panel needs a y-flip),
  yielding touches point-reflected 180°. Verified with per-tap raw/converted
  serial logging: the four corners came back exactly diagonal-swapped.
- **Fix (data layer, verified on device):** keep the measured corner data
  but reorder the pairs 180° (TL↔BR, BL↔TR) so the solved affine bakes in
  the reflection. Landscape array (measured at capture, then reordered):
  `{652, 3313, 762, 181, 3841, 3586, 3837, 285}`. Portrait rotation-7
  array is untouched. No manual swap/mirror code in the input path — the
  correction lives in the calibration data, selected by `layout::kLandscape`.
  ⚠️ If this array is ever re-captured, the raw `calibrateTouch` output must
  be reordered the same way before use.

The probe was throwaway code — not merged.

## Display service

- `setRotation(layout::kLandscape ? 6 : 7)` (probed).
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

## Outcome (2026-07-22)

The on-device landscape checklist passed in full after three
verification-driven fixes: the character sprites were invisible because
LVGL 8 silently draws nothing when zooming file-backed images (fixed by
dropping the zoom, then by pre-scaled 90% `ls/` art variants on the SD
card), and the forecast list was re-flowed to stacked day-over-temps
entries. Native suite 246/246, both envs build. **Verdict: landscape is
promising — the follow-up phase ports the remaining five apps
(spec 2026-07-22-landscape-all-apps-design.md). The portrait regression
flash rolls into that phase's verification.**
