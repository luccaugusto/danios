# Landscape UI Spike Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the danios UI buildable in landscape (USB-C right, 320×240) behind a `-DDANIOS_LANDSCAPE` flag, with real landscape layouts for the launcher, status bar, Weather, and Minesweeper, so portrait/landscape can be flashed A/B and compared.

**Architecture:** A new `src/core/Layout.h` derives all orientation constants from the flag (`kLandscape`, `kScreenW/H`, `kAppW/H`, `kGridCols`); it and `DisplayService` are the ONLY places `#ifdef DANIOS_LANDSCAPE` (Layout.h) or orientation hardware values (DisplayService) appear. All other code branches with plain `if (layout::kLandscape)` so both paths compile in every build. Hardware facts (exact rotation value, touch-calibration survival) are pinned by a throwaway probe before any UI work.

**Tech Stack:** PlatformIO (espressif32), LVGL 8.4, LovyanGFX, ESP32-2432S024 ("CYD", resistive XPT2046, `/dev/ttyUSB0`).

**Spec:** `docs/superpowers/specs/2026-07-17-landscape-ui-design.md`

## Global Constraints

- Branch: `landscape-ui` (already created; spec committed).
- `#ifdef DANIOS_LANDSCAPE` appears in `src/core/Layout.h` ONLY. Everything else uses `if (layout::kLandscape)` / the layout constants — both orientation paths must compile in every build.
- Portrait behavior must not change: `pio run -e cyd` output is pixel-identical logic (constants replace literals 1:1).
- Draw buffer stays ~14.4 KB (`kBufPixels = 7200` both orientations).
- Never hand-edit touch calibration constants or add manual swap/mirror code (docs/DISPLAY.md); recalibrate with `calibrateTouch` only.
- `offset_rotation` in `LGFX_ESP32_2432S024.hpp` stays 0 — do not touch that file.
- Out of scope (known-broken in landscape): Calculator, Oracle, Pet, Pomodoro, Settings (bottom ~80 px clip), boot splash (bottom crop is acceptable per spec — `main.cpp` needs no change).
- **Testing reality:** LVGL/Arduino code is excluded from the native test env by design (host tests cover `lib/` only). The per-task gate is "both device envs compile"; behavior is verified on-device in Task 1 (hardware probe) and Task 6 (full checklist). No new host tests are required by this plan.
- Serial/flash workflow (from project memory): free the serial port before flashing; `pio device monitor` dies headless — capture serial with a background pyserial reader if needed.

---

### Task 1: Hardware probe — pin the landscape rotation + touch verdict

**⚠️ USER-IN-THE-LOOP:** the user must physically look at the device and report what they see. Do not guess the answers.

**Files:**
- Create (throwaway, deleted at end of task): `src/probe/probe_main.cpp`
- Modify (temporarily, reverted at end of task): `platformio.ini`

**Interfaces:**
- Produces: two facts consumed by Task 2 — `LANDSCAPE_ROTATION` (one of 6/4/2/0) and `CAL_VERDICT` (either "rotation-7 array works at the landscape rotation" or a fresh 8-value calibration array).

- [ ] **Step 1: Exclude the probe dir from the normal build and add the probe env**

In `platformio.ini`, change the `[env:cyd]` src filter line:

```ini
build_src_filter = +<*> -<probe/>
```

and append at the end of the file:

```ini
; THROWAWAY probe env (landscape spike Task 1) — deleted when the rotation
; and touch verdict are pinned. Pure LovyanGFX, no LVGL.
[env:probe]
extends = env:cyd
build_src_filter = +<probe/>
lib_deps = lovyan03/LovyanGFX@^1.2.0
```

- [ ] **Step 2: Write the probe**

Create `src/probe/probe_main.cpp`:

```cpp
// THROWAWAY landscape rotation/touch probe (plan Task 1). Cycles the four
// landscape-candidate rotations every 12 s AND on a tap in the NEXT box.
// For each rotation it draws the rotation number, an arrow that should point
// at the USB-C port, and corner circles; taps plot red dots so calibration
// accuracy is visible on the device itself. Everything is also serial-logged.
//
// Set kCalibrate = true (and re-flash) AFTER the right rotation is known to
// capture a fresh 8-value array at that rotation, if the old one mislands.
#include <Arduino.h>

#include "LGFX_ESP32_2432S024.hpp"

namespace {
LGFX tft;
// 6 first (the 90-degree neighbour of portrait 7 in the mirrored family),
// then its mirror-family sibling 4, then the unmirrored evens.
constexpr uint8_t kRots[] = {6, 4, 2, 0};
uint8_t idx = 0;
uint32_t lastCycleMs = 0;
// The rotation-7 portrait array from DisplayService::begin() — the probe
// answers whether LovyanGFX re-maps it correctly at other rotations.
uint16_t cal[8] = {3830, 319, 3876, 3631, 724, 181, 521, 3482};

constexpr bool kCalibrate = false;  // flip to true for the capture pass
constexpr uint8_t kCalibrateRotation = 6;  // set to the pinned rotation

void drawScreen() {
  tft.setRotation(kRots[idx]);
  tft.setTouchCalibrate(cal);  // MUST re-apply after every setRotation
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.printf("ROT %u  %dx%d", kRots[idx], tft.width(), tft.height());
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("USB deve estar ->");
  const int y = tft.height() / 2;
  tft.drawLine(tft.width() - 80, y, tft.width() - 10, y, TFT_YELLOW);
  tft.fillTriangle(tft.width() - 10, y, tft.width() - 26, y - 10,
                   tft.width() - 26, y + 10, TFT_YELLOW);
  for (int cx : {8, tft.width() - 8})
    for (int cy : {8, tft.height() - 8}) tft.drawCircle(cx, cy, 6, TFT_CYAN);
  // NEXT box, bottom-left (away from the corner markers).
  tft.drawRect(10, tft.height() - 40, 80, 32, TFT_GREEN);
  tft.setCursor(20, tft.height() - 32);
  tft.print("NEXT");
  lastCycleMs = millis();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setBrightness(160);
  if (kCalibrate) {
    tft.setRotation(kCalibrateRotation);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Toque as setas nos cantos");
    uint16_t fresh[8];
    tft.calibrateTouch(fresh, TFT_WHITE, TFT_BLACK, 20);
    Serial.printf("rotation %u calibration: {%u, %u, %u, %u, %u, %u, %u, %u}\n",
                  kCalibrateRotation, fresh[0], fresh[1], fresh[2], fresh[3],
                  fresh[4], fresh[5], fresh[6], fresh[7]);
    tft.setCursor(10, 40);
    tft.print("OK - array no serial");
    while (true) delay(1000);
  }
  drawScreen();
}

void loop() {
  if (millis() - lastCycleMs > 12000) {  // auto-advance survives broken touch
    idx = (idx + 1) % 4;
    drawScreen();
  }
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    Serial.printf("rot=%u touch x=%u y=%u\n", kRots[idx], x, y);
    if (x < 90 && y > tft.height() - 40) {
      idx = (idx + 1) % 4;
      drawScreen();
      delay(400);
    } else {
      tft.fillCircle(x, y, 3, TFT_RED);
      delay(150);
    }
  }
}
```

- [ ] **Step 3: Build and flash the probe**

Run: `pio run -e probe -t upload`
Expected: `SUCCESS`, upload to `/dev/ttyUSB0` completes. (Free the port first if a serial reader is running.)

- [ ] **Step 4: Ask the user to run the rotation check**

Ask the user to hold the device with the **USB-C port to the right** and report, for the rotation whose number is shown on screen (it cycles every 12 s, or tap NEXT):

1. Which `ROT n` shows the text **readable, not mirrored**, with the yellow arrow **pointing at the USB-C port**? → record as `LANDSCAPE_ROTATION`.
2. On that rotation, do red dots appear **under the finger** (try all four cyan corner circles)? → if yes, `CAL_VERDICT = rotation-7 array works`.

- [ ] **Step 5 (only if corner taps misland): capture a fresh calibration**

Edit the probe: set `kCalibrate = true` and `kCalibrateRotation = <LANDSCAPE_ROTATION>`. Re-flash (`pio run -e probe -t upload`), have the user tap the four corner arrows, then read the printed array from serial (background pyserial reader, e.g. `python3 -c "import serial; s=serial.Serial('/dev/ttyUSB0',115200); [print(s.readline().decode(),end='')  for _ in iter(int,1)]"` — Ctrl-C when the line appears). Record the 8 values as `CAL_VERDICT`.

- [ ] **Step 6: Record the facts and clean up**

1. Write `LANDSCAPE_ROTATION` and `CAL_VERDICT` into the spec's "Step 0" section (replace the "probed on device" language with the measured values) — `docs/superpowers/specs/2026-07-17-landscape-ui-design.md`.
2. Delete the probe: `rm -rf src/probe`, remove the `[env:probe]` block from `platformio.ini`, and restore the original `build_src_filter = +<*>` line in `[env:cyd]`.
3. Sanity build: `pio run -e cyd` → `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add docs/superpowers/specs/2026-07-17-landscape-ui-design.md platformio.ini
git commit -m "docs: record probed landscape rotation + touch verdict"
```

---

### Task 2: Layout.h, cyd-landscape env, DisplayService

**Files:**
- Create: `src/core/Layout.h`
- Modify: `platformio.ini` (new env at the end)
- Modify: `src/services/DisplayService.h:32-38`
- Modify: `src/services/DisplayService.cpp:6-16`

**Interfaces:**
- Consumes: `LANDSCAPE_ROTATION` and `CAL_VERDICT` from Task 1. **The code below assumes rotation 6 and a surviving calibration array — substitute the measured rotation, and add the second array only if Task 1 captured one.**
- Produces: `namespace layout` with `constexpr bool kLandscape`, `constexpr lv_coord_t kScreenW, kScreenH, kTopBarH, kAppW, kAppH`, `constexpr int kGridCols` — all later tasks include `"core/Layout.h"` and use exactly these names.

- [ ] **Step 1: Create `src/core/Layout.h`**

```cpp
// src/core/Layout.h — orientation-derived UI dimensions (landscape spike,
// spec 2026-07-17). This header is the ONLY place DANIOS_LANDSCAPE is read;
// everything else branches on layout::kLandscape or uses these constants so
// both orientation paths compile in every build.
#pragma once

#include <lvgl.h>

namespace layout {

#ifdef DANIOS_LANDSCAPE
constexpr bool kLandscape = true;
#else
constexpr bool kLandscape = false;
#endif

constexpr lv_coord_t kScreenW = kLandscape ? 320 : 240;
constexpr lv_coord_t kScreenH = kLandscape ? 240 : 320;

// App-screen back-bar height (owned here so apps and Launcher agree).
constexpr lv_coord_t kTopBarH = 32;
// The container apps build into: full width, below the top bar.
constexpr lv_coord_t kAppW = kScreenW;
constexpr lv_coord_t kAppH = kScreenH - kTopBarH;  // 288 portrait, 208 landscape

// Home grid: 80 px cells — 3x80 = 240, 4x80 = 320, both exact.
constexpr int kGridCols = kLandscape ? 4 : 3;

}  // namespace layout
```

- [ ] **Step 2: Add the landscape env to `platformio.ini`** (at the end)

```ini
; Landscape spike (spec 2026-07-17): same firmware, UI rotated 90°, USB-C
; right. Flash A/B against [env:cyd] to compare orientations.
[env:cyd-landscape]
extends = env:cyd
build_flags =
    ${env:cyd.build_flags}
    -DDANIOS_LANDSCAPE=1
```

- [ ] **Step 3: DisplayService constants** — in `src/services/DisplayService.h`, add `#include "core/Layout.h"` below `#include <lvgl.h>`, and replace lines 32-38 (`kHorRes`/`kVerRes`/`kBufRows`/`kBufPixels`) with:

```cpp
  static constexpr int16_t kHorRes = layout::kScreenW;
  static constexpr int16_t kVerRes = layout::kScreenH;
  // Single buffer (14,400 bytes = 7200 px), same budget both orientations:
  // flushCb is synchronous, so a second buffer would never overlap render
  // with flush — it only earns its RAM if the flush ever goes async.
  static constexpr size_t kBufPixels = 7200;
```

Also update the class comment on line 12-13 (`rotation 7 portrait 240x320`, `240x30 draw buffer`) to say the orientation comes from `layout::` and the buffer is 7200 px.

- [ ] **Step 4: DisplayService rotation** — in `src/services/DisplayService.cpp`, replace line 8 (`tft_.setRotation(7);`) with:

```cpp
  // Portrait = rotation 7, USB-C down. Landscape = rotation 6, USB-C right —
  // pinned empirically by the Task 1 hardware probe (docs/DISPLAY.md).
  tft_.setRotation(layout::kLandscape ? 6 : 7);
```

(substitute the probed value if it wasn't 6). If Task 1 captured a fresh landscape array, replace line 14-15 with:

```cpp
  static uint16_t kTouchCalPortrait[8] = {3830, 319, 3876, 3631, 724, 181, 521, 3482};
  static uint16_t kTouchCalLandscape[8] = {/* Task 1 captured values */};
  tft_.setTouchCalibrate(layout::kLandscape ? kTouchCalLandscape : kTouchCalPortrait);
```

If the rotation-7 array survived, leave lines 9-15 untouched.

- [ ] **Step 5: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`. (Everything still lays out for portrait; the landscape binary is rotated with overflowing UI — that's expected until Tasks 3-5.)

- [ ] **Step 6: Commit**

```bash
git add src/core/Layout.h src/services/DisplayService.h src/services/DisplayService.cpp platformio.ini
git commit -m "feat: DANIOS_LANDSCAPE flag — layout constants, rotation, cyd-landscape env"
```

---

### Task 3: Launcher + StatusBar on layout constants

**Files:**
- Modify: `src/core/StatusBar.cpp:6`
- Modify: `src/core/Launcher.h:36,47`
- Modify: `src/core/Launcher.cpp:108,189,215-216`

**Interfaces:**
- Consumes: `layout::kScreenW/kScreenH/kTopBarH/kAppW/kAppH/kGridCols` (Task 2).
- Produces: the app container handed to `App::buildUI` is `layout::kAppW × layout::kAppH` (320×208 landscape, 240×288 portrait) — Tasks 4-5 rely on this.

- [ ] **Step 1: StatusBar** — add `#include "core/Layout.h"` to `src/core/StatusBar.cpp` and change line 6 to:

```cpp
  lv_obj_set_size(bar_, layout::kScreenW, kHeight);
```

- [ ] **Step 2: Launcher.h** — add `#include "core/Layout.h"` (with the other includes), delete line 36 (`static constexpr lv_coord_t kTopBarH = 32;`), and change line 47 to:

```cpp
  LauncherModel model_{layout::kGridCols};
```

- [ ] **Step 3: Launcher.cpp** — replace the three hardcoded-size call sites (`kTopBarH` references now resolve to `layout::kTopBarH`; add `using layout::kTopBarH;` at the top of the file below the includes to keep the call sites unchanged):

Line 108: `lv_obj_set_size(gridContainer_, layout::kScreenW, layout::kScreenH - StatusBar::kHeight);`
Line 189: `lv_obj_set_size(topBar, layout::kScreenW, kTopBarH);`
Line 216: `lv_obj_set_size(appContainer_, layout::kAppW, layout::kAppH);`

Fit check (no code change, just verify the arithmetic in review): 7 registered apps → portrait 3 cols × 3 rows × 98 px = 294 ≤ 296 px grid area; landscape 4 cols × 2 rows × 98 = 196 ≤ 216 px. Cells stay 80×98.

- [ ] **Step 4: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/core/StatusBar.cpp src/core/Launcher.h src/core/Launcher.cpp
git commit -m "feat: launcher + status bar size from layout constants (4-col landscape grid)"
```

---

### Task 4: Weather — landscape split screen

**Files:**
- Modify: `src/apps/weather/WeatherApp.h` (add one private method declaration)
- Modify: `src/apps/weather/WeatherApp.cpp:107-199`

**Interfaces:**
- Consumes: `layout::kLandscape/kAppW/kAppH`; existing helpers in WeatherApp.cpp's anonymous namespace: `makeArtSlot(parent, storage, path, w, h, fallback, hideIfMissing=false)`, `makeReadout(parent)`, `shownTemp(celsius, useF)`; model fns `conditionFromWmo`, `conditionLabelPt`, `artSlots`, `tempBand`; `kCharacterPath`, `kDayNames`.
- Produces: nothing consumed later; portrait `render()` body byte-identical.

- [ ] **Step 1: Declare the landscape renderer** — in `src/apps/weather/WeatherApp.h`, next to the existing `void render(const ForecastWx& f, bool stale);` declaration add:

```cpp
  void renderLandscape(const ForecastWx& f, bool stale);
```

- [ ] **Step 2: Branch at the top of `render()`** — in `src/apps/weather/WeatherApp.cpp` add `#include "core/Layout.h"` (with the project includes), then insert after the two reset lines (lines 108-109, `lv_obj_clean(root_); statusLbl_ = nullptr;`):

```cpp
  if (layout::kLandscape) {
    renderLandscape(f, stale);
    return;
  }
```

The rest of the portrait body stays untouched.

- [ ] **Step 3: Implement `renderLandscape`** (place it right after `render`):

```cpp
// Landscape (spec 2026-07-17): split screen. Left half is a pure art panel —
// the portrait-drawn art center-cropped/zoomed, no text over it. Right half
// is a clean data panel on the app background.
void WeatherApp::renderLandscape(const ForecastWx& f, bool stale) {
  const bool useF = store_->getBool("units.f", false);
  const Condition cond = conditionFromWmo(f.current.wmoCode);
  const ArtSlots art =
      artSlots(tempBand(f.current.tempC), cond, f.current.isDay);

  constexpr lv_coord_t kArtW = layout::kAppW / 2;  // 160
  constexpr lv_coord_t kArtH = layout::kAppH;      // 208

  // Art panel clips its children, so the oversized portrait art is
  // center-cropped by parking it at negative offsets.
  lv_obj_t* artPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(artPanel);
  lv_obj_set_pos(artPanel, 0, 0);
  lv_obj_set_size(artPanel, kArtW, kArtH);
  lv_obj_clear_flag(artPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* bg = makeArtSlot(artPanel, *storage_, art.background, 240, 288,
                             lv_color_white());
  lv_obj_set_pos(bg, (kArtW - 240) / 2, (kArtH - 288) / 2);  // center-crop
  lv_obj_set_style_radius(bg, 0, 0);

  // Character stack (188x222 sources): zoomed to the panel width
  // (256 * 160 / 188 ≈ 218). lv_img zoom scales around the centre pivot and
  // keeps the 188x222 layout box, so BOTTOM_MID plus the half-height delta
  // (222 - 189) / 2 ≈ 16 rests her feet on the panel's bottom edge.
  constexpr uint16_t kZoom = 218;
  const struct {
    const char* path;
    lv_color_t fallback;
    bool hideIfMissing;
  } kLayers[] = {
      {kCharacterPath, lv_palette_main(LV_PALETTE_GREY), false},
      {art.outfit, lv_palette_main(LV_PALETTE_GREY), true},
      {art.overlay, lv_palette_main(LV_PALETTE_ORANGE), true},
  };
  for (const auto& layer : kLayers) {
    lv_obj_t* img = makeArtSlot(artPanel, *storage_, layer.path, 188, 222,
                                layer.fallback, layer.hideIfMissing);
    lv_img_set_zoom(img, kZoom);
    lv_obj_align(img, LV_ALIGN_BOTTOM_MID, 0, 16);
  }

  // Data panel: flex column of plain labels on the dark app background —
  // no white readout pills needed, nothing sits over art here.
  lv_obj_t* dataPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(dataPanel);
  lv_obj_set_pos(dataPanel, kArtW, 0);
  lv_obj_set_size(dataPanel, layout::kAppW - kArtW, kArtH);
  lv_obj_set_style_pad_all(dataPanel, 8, 0);
  lv_obj_set_style_pad_row(dataPanel, 6, 0);
  lv_obj_set_flex_flow(dataPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(dataPanel, LV_OBJ_FLAG_SCROLLABLE);

  auto addLine = [dataPanel](const char* txt) {
    lv_obj_t* l = lv_label_create(dataPanel);
    lv_label_set_text(l, txt);
    return l;
  };

  char buf[80];
  snprintf(buf, sizeof buf, "%d°%c  %s", shownTemp(f.current.tempC, useF),
           useF ? 'F' : 'C', conditionLabelPt(cond));
  addLine(buf);
  addLine(store_->getString("loc.city", "").c_str());
  if (f.dayCount > 0) {
    snprintf(buf, sizeof buf, LV_SYMBOL_UP "%d°  " LV_SYMBOL_DOWN "%d°",
             shownTemp(f.days[0].tmaxC, useF), shownTemp(f.days[0].tminC, useF));
    addLine(buf);
  }
  addLine("");  // spacer row between readings and the forecast list
  for (int i = 0; i < f.dayCount; ++i) {
    snprintf(buf, sizeof buf, "%s: %s %d°/%d°", kDayNames[i],
             conditionLabelPt(conditionFromWmo(f.days[i].wmoCode)),
             shownTemp(f.days[i].tmaxC, useF), shownTemp(f.days[i].tminC, useF));
    addLine(buf);
  }

  if (stale) setStatus(LV_SYMBOL_WARNING " desatualizado");
}
```

- [ ] **Step 4: Landscape-safe `setStatus` and `renderEmpty`** — still in WeatherApp.cpp:

In `setStatus` (line ~198), replace the align call with:

```cpp
    // Portrait: below the hi/lo readout (top-right corner). Landscape: the
    // data panel's bottom-right, which the forecast list never reaches.
    if (layout::kLandscape)
      lv_obj_align(statusLbl_, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    else
      lv_obj_align(statusLbl_, LV_ALIGN_TOP_RIGHT, -8, 30);
```

In `renderEmpty` (line 187), replace `lv_obj_set_width(lbl, 224);` with:

```cpp
  lv_obj_set_width(lbl, layout::kAppW - 16);
```

- [ ] **Step 5: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add src/apps/weather/WeatherApp.h src/apps/weather/WeatherApp.cpp
git commit -m "feat: weather landscape split screen — art panel left, data panel right"
```

---

### Task 5: Minesweeper — landscape presets, slim HUD, two-column setup

**Files:**
- Modify: `src/apps/minesweeper/MinesweeperApp.h` (two private declarations)
- Modify: `src/apps/minesweeper/MinesweeperApp.cpp:18-23` (presets/clamps), `:142-213` (showStart), `:290-297` (HUD)

**Interfaces:**
- Consumes: `layout::kLandscape`; existing members `resumable()`, `accumMs_`, `stepCtx_`, `setupLbl_`, `makeStepBtn(txt, x, y, ctx)`, `refreshSetupLabels()`, callbacks `onResume/onPresetEasy/onPresetHard/onPlay`; `fmtTime`, `store_`.
- Produces: nothing consumed later. Landscape best times live under NEW NVS keys (`mines_best_easy_l` / `mines_best_hard_l`) because the landscape boards are different games — portrait bests must not be compared against them.

- [ ] **Step 1: Orientation-dependent presets and clamps** — in the anonymous namespace of `MinesweeperApp.cpp`, add `#include "core/Layout.h"` (with the project includes) and replace lines 18-23 with:

```cpp
// Landscape boards are wide-short (208 px tall app area, spec 2026-07-17):
// 6 rows x 12 cols max keeps 26 px cells. Mine ratios track the portrait
// presets (Fácil ~22%, Difícil ~28%). Separate best-time keys — a 6x9 Fácil
// is not the same game as the portrait 9x9, so times don't compare.
constexpr Preset kEasy = layout::kLandscape
                             ? Preset{6, 9, 12, "mines_best_easy_l"}
                             : Preset{9, 9, 18, "mines_best_easy"};
constexpr Preset kHard = layout::kLandscape
                             ? Preset{6, 12, 20, "mines_best_hard_l"}
                             : Preset{13, 9, 33, "mines_best_hard"};

// Board HUD: one row above the grid; slimmed in landscape to buy grid height.
constexpr lv_coord_t kHudH = layout::kLandscape ? 28 : 32;

constexpr uint8_t kMinRC = 5;
constexpr uint8_t kMaxRows = layout::kLandscape ? 6 : 14;
constexpr uint8_t kMaxCols = 12;
```

(Note: `loadSetup()` already clamps NVS-persisted rows/cols through these maxima, so a portrait-saved 13-row config loads as 6 rows in the landscape build — no extra code.)

- [ ] **Step 2: Declare the landscape start screen + shared button helper** — in `MinesweeperApp.h` private section, next to `makeStepBtn`:

```cpp
  void showStartLandscape();  // two-column layout for the 320x208 container
  lv_obj_t* makeTextBtn(const char* text, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, lv_event_cb_t cb);
```

- [ ] **Step 3: Extract the shared button helper** — in `MinesweeperApp.cpp` (next to `makeStepBtn`):

```cpp
lv_obj_t* MinesweeperApp::makeTextBtn(const char* text, lv_coord_t x,
                                      lv_coord_t y, lv_coord_t w, lv_coord_t h,
                                      lv_event_cb_t cb) {
  lv_obj_t* btn = lv_btn_create(root_);
  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);
  return btn;
}
```

- [ ] **Step 4: Branch `showStart` and add the landscape layout** — insert after the `loadSetup();` line in `showStart()` (line 146):

```cpp
  if (layout::kLandscape) {
    showStartLandscape();
    return;
  }
```

Then add, right after `showStart`'s closing brace:

```cpp
// Landscape start screen: steppers in the left column (labels above the
// -/value/+ row — the portrait single-row arrangement is 192 px wide and
// doesn't fit a 160 px column), actions stacked in the right column.
void MinesweeperApp::showStartLandscape() {
  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, "Minas");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

  // Left column: x 8..144.
  static const char* kNames[] = {"Linhas", "Colunas", "Minas"};
  lv_coord_t y = 26;
  for (uint8_t f = 0; f < 3; ++f) {
    lv_obj_t* name = lv_label_create(root_);
    lv_label_set_text(name, kNames[f]);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 8, y);

    stepCtx_[f * 2] = {this, f, -1};
    stepCtx_[f * 2 + 1] = {this, f, +1};
    makeStepBtn("-", 8, y + 16, &stepCtx_[f * 2]);
    setupLbl_[f] = lv_label_create(root_);
    lv_obj_set_width(setupLbl_[f], 44);
    lv_obj_set_style_text_align(setupLbl_[f], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(setupLbl_[f], LV_ALIGN_TOP_LEFT, 50, y + 24);
    makeStepBtn("+", 100, y + 16, &stepCtx_[f * 2 + 1]);
    y += 52;
  }
  refreshSetupLabels();

  // Right column: x 168..312, actions stacked.
  y = 26;
  if (resumable()) {
    char t[16];
    fmtTime(t, sizeof t, accumMs_ / 1000);
    char label[32];
    snprintf(label, sizeof label, "Continuar (%s)", t);
    makeTextBtn(label, 168, y, 144, 34, onResume);
    y += 38;
  }
  struct PresetBtn {
    const Preset& p;
    const char* name;
    lv_event_cb_t cb;
  };
  const PresetBtn pb[] = {{kEasy, "Fácil", onPresetEasy},
                          {kHard, "Difícil", onPresetHard}};
  for (const PresetBtn& b : pb) {
    char best[16] = "-";
    const uint32_t bt = store_->getU32(b.p.bestKey, 0);
    if (bt != 0) fmtTime(best, sizeof best, bt);
    char label[48];
    snprintf(label, sizeof label, "%s\nmelhor: %s", b.name, best);
    makeTextBtn(label, 168, y, 144, 40, b.cb);
    y += 44;
  }
  makeTextBtn("Jogar", 168, y, 144, 36, onPlay);
}
```

(Fit check: right column worst case 26+38+44+44+36 = 188 ≤ 208; left column 26+3×52 = 182 ≤ 208.)

- [ ] **Step 5: Slim-HUD flag button** — in `showBoard()` (line 297), the 26 px-tall flag button needs a smaller y offset inside the 28 px HUD:

```cpp
  lv_obj_align(flagBtn_, LV_ALIGN_TOP_RIGHT, -4, layout::kLandscape ? 1 : 3);
```

(The board grid itself needs NO change: `newGame()` already computes `cellPx_` from the container's content size minus `kHudH`, and `showBoard()` centers it.)

- [ ] **Step 6: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 7: Commit**

```bash
git add src/apps/minesweeper/MinesweeperApp.h src/apps/minesweeper/MinesweeperApp.cpp
git commit -m "feat: minesweeper landscape — wide presets, slim HUD, two-column setup"
```

---

### Task 6: Verification — builds, native tests, on-device A/B

**⚠️ USER-IN-THE-LOOP:** the on-device checklist needs the user's eyes and fingers.

**Files:** none (verification only).

- [ ] **Step 1: Both device envs compile**

Run: `pio run -e cyd -e cyd-landscape`
Expected: two `SUCCESS` lines; note RAM/Flash deltas between envs (should be negligible).

- [ ] **Step 2: Native tests pass**

Run: `pio test -e native`
Expected: all existing suites `PASSED` (nothing in `lib/` changed; this guards against accidental breakage).

- [ ] **Step 3: Flash landscape and walk the checklist with the user**

Run: `pio run -e cyd-landscape -t upload` (free the serial port first).

Ask the user to hold the device USB-C-right and confirm each:

1. Boot splash centered, bottom-cropped, no artifacts.
2. Home: status bar full width; 7 icons in a 4+3 grid; taps open the app under the finger (touch accuracy).
3. Weather: art fills the left half with no text over it; temp/city/hi-lo/3-day list readable on the right; stale badge (if shown) sits bottom-right.
4. Minesweeper setup: steppers left, Fácil/Difícil/Jogar right, nothing clipped; steppers clamp at 6 rows / 12 cols.
5. Minesweeper game: HUD row slim, full 12-col Difícil board fits with 26 px cells, flags/reveals land on the right cells, win/lose overlay centered.
6. Back button + status bar behave everywhere; known-broken apps (Calculator, Oracle, Pet, Pomodoro, Settings) open and clip at the bottom but don't crash.

- [ ] **Step 4: Portrait regression**

Run: `pio run -e cyd -t upload`
Ask the user to spot-check: home grid 3-wide as before, weather portrait unchanged, minesweeper start screen unchanged, portrait best-time entries still shown.

- [ ] **Step 5: Record the verdict**

Append the on-device findings (what looked good/bad, user's leaning on making landscape permanent) to the spec's "Decision criteria" section and commit:

```bash
git add docs/superpowers/specs/2026-07-17-landscape-ui-design.md
git commit -m "docs: landscape spike on-device verification notes"
```
