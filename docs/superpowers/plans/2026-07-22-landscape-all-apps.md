# Landscape All-Apps Port Implementation Plan

> **For agentic workers:** Execute task-by-task with a fresh implementer per task and a review after each (subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Calculator, Pet, and Pomodoro to the landscape 320×208 app container, fix the two `main.cpp` portrait stragglers (msgbox width, boot logo), and verify Settings/Oracle on device — so the landscape build has no known-broken surfaces.

**Architecture:** Same regime as the spike: `src/core/Layout.h` constants everywhere, `#ifdef DANIOS_LANDSCAPE` only in Layout.h, landscape-specific layouts behind `if (layout::kLandscape)`, portrait code paths untouched. One new pre-scaled SD asset (boot logo) via the existing converter.

**Tech Stack:** PlatformIO (espressif32), LVGL 8.4, LovyanGFX, ESP32-2432S024 on `/dev/ttyUSB0`.

**Spec:** `docs/superpowers/specs/2026-07-22-landscape-all-apps-design.md`

## Global Constraints

- Branch `landscape-ui`, continuing from the spike commits.
- `#ifdef DANIOS_LANDSCAPE` stays confined to `src/core/Layout.h`.
- Portrait behavior must not change (constants replace literals 1:1; landscape-only branches).
- Oracle is explicitly out of scope (user-verified good as-is).
- LVGL 8 cannot zoom file-backed images — any art that must shrink ships as a pre-scaled `.bin`.
- Build gate per task: `pio run -e cyd -e cyd-landscape` → both SUCCESS. Host tests don't cover this code (`pio test -e native` guards `lib/` only, run in the final task).
- Serial/flash workflow: free the serial port before flashing.

---

### Task 1: Calculator constants + main.cpp msgbox & boot logo

**Files:**
- Modify: `src/apps/calculator/CalculatorApp.cpp:23-38`
- Modify: `src/main.cpp:101-113` (msgbox), `:175-191` (boot splash)
- Create: `sd/art/ls/boot-logo.bin` (generated asset, committed)

**Interfaces:**
- Consumes: `layout::kAppW/kAppH/kScreenW/kLandscape` from `src/core/Layout.h`.
- Produces: nothing later tasks rely on.

- [ ] **Step 1: Calculator** — add `#include "core/Layout.h"` to `CalculatorApp.cpp` (with the project includes), then:

Line 27: `lv_obj_set_width(displayLabel_, 240 - 16);` → `lv_obj_set_width(displayLabel_, layout::kAppW - 16);`
Line 34: `lv_obj_set_size(pad, 240, 288 - kDisplayH);` → `lv_obj_set_size(pad, layout::kAppW, layout::kAppH - kDisplayH);`
Update the stale "240×288 container" comment near line 23 to say the container is `layout::kAppW × kAppH`.

- [ ] **Step 2: Generate the landscape boot logo**

Run:
```bash
python3 assets/icons/svg_to_lvgl_bin.py assets/art/boot-logo.png sd/art/ls/boot-logo.bin --size 173x208
```
Expected: verify block prints `cf=5 w=173 h=208` (LV_IMG_CF_TRUE_COLOR_ALPHA) and the file exists. (173×208 = the 240×288 source scaled to fit the height above the 32px "Conectando" strip on the 240-tall landscape screen.)

- [ ] **Step 3: main.cpp** — add `#include "core/Layout.h"` (with the project includes), then:

Line 110 (SD-missing msgbox):
```cpp
  lv_obj_set_width(m, layout::kScreenW - 10);  // near-full-width either orientation
```
Line 181 (boot logo path — keep the surrounding comment, adjust it to mention the pre-scaled `ls/` variant):
```cpp
  static constexpr const char* kBootLogo = layout::kLandscape
      ? "S:/art/ls/boot-logo.bin"   // 173x208, pre-scaled (LVGL can't zoom SD art)
      : "S:/art/boot-logo.bin";     // 240x288
```
(`lvglFsExists(kBootLogo)` and the `TOP_MID` alignment already handle both; missing-file fallback unchanged.)

- [ ] **Step 4: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/apps/calculator/CalculatorApp.cpp src/main.cpp sd/art/ls/boot-logo.bin
git commit -m "feat: calculator + splash/msgbox on layout constants, ls/ boot logo"
```

---

### Task 2: Pomodoro landscape two-column

**Files:**
- Modify: `src/apps/pomodoro/PomodoroApp.cpp:37-107`

**Interfaces:**
- Consumes: `layout::kLandscape` from `src/core/Layout.h`.
- Produces: nothing later tasks rely on.

- [ ] **Step 1: Branch the widget positions in `buildUI`** — add `#include "core/Layout.h"`, then replace the four alignment/position call sites (art slot, countdown, start/stop button, stepper y-coordinates) so landscape forms two columns. Left column is centred at x=80 (`TOP_MID` x-offset −80); right column spans x 168..312:

```cpp
  // Landscape (spec 2026-07-22): two columns — art + countdown on the left
  // (centred at x=80), start/stop + steppers stacked on the right. Portrait
  // keeps its single-column ladder.
  lv_obj_align(slot, LV_ALIGN_TOP_MID, layout::kLandscape ? -80 : 0,
               layout::kLandscape ? 20 : 6);
  ...
  lv_obj_align(countdown_, LV_ALIGN_TOP_MID, layout::kLandscape ? -80 : 0,
               layout::kLandscape ? 150 : 132);
  ...
  if (layout::kLandscape) {
    lv_obj_set_size(btn, 144, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 168, 16);
  } else {
    lv_obj_set_size(btn, 150, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 186);
  }
  ...
  if (layout::kLandscape) {
    buildStepperRow(parent, 64, "Trabalho", &workVal_, onWorkMinus, onWorkPlus);
    buildStepperRow(parent, 118, "Pausa", &breakVal_, onBreakMinus, onBreakPlus);
  } else {
    buildStepperRow(parent, 232, "Trabalho", &workVal_, onWorkMinus, onWorkPlus);
    buildStepperRow(parent, 261, "Pausa", &breakVal_, onBreakMinus, onBreakPlus);
  }
```

- [ ] **Step 2: Make `buildStepperRow` two-line in landscape** — in portrait the name sits left of the −/value/+ cluster on one line (name at x 8, cluster at `TOP_RIGHT` −108..−8 ⇒ x 100..232 of 240). In landscape the cluster's same `TOP_RIGHT` offsets land at x 180..312, but the name must start at the right column's x 168 and would collide — so the name goes on its own line above the cluster:

```cpp
void PomodoroApp::buildStepperRow(lv_obj_t* parent, lv_coord_t y,
                                  const char* name, lv_obj_t** valLbl,
                                  lv_event_cb_t minusCb, lv_event_cb_t plusCb) {
  // Landscape: the row lives in the right column (x >= 168); the name sits on
  // its own line above the -/value/+ cluster, which keeps its TOP_RIGHT
  // offsets (they land at x 180..312 on the 320 screen). Portrait: one line.
  const lv_coord_t clusterY = layout::kLandscape ? y + 18 : y;
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, name);
  if (layout::kLandscape)
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 168, y);
  else
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y + 6);

  lv_obj_t* minus = lv_btn_create(parent);
  lv_obj_set_size(minus, 32, 24);
  lv_obj_align(minus, LV_ALIGN_TOP_RIGHT, -108, clusterY);
  ...
  lv_obj_align(*valLbl, LV_ALIGN_TOP_RIGHT, -42, clusterY + 6);
  ...
  lv_obj_align(plus, LV_ALIGN_TOP_RIGHT, -8, clusterY);
  ...
}
```
(Only the alignment lines change; labels, callbacks, and `stepperBtns_` bookkeeping stay identical.)

Fit check (verify in review, no code): left column — art 20..140, countdown 150..~205; right column — button 16..52, "Trabalho" 64 + cluster 82..106, "Pausa" 118 + cluster 136..160. All ≤ 208.

- [ ] **Step 3: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/apps/pomodoro/PomodoroApp.cpp
git commit -m "feat: pomodoro landscape two-column — art+countdown left, controls right"
```

---

### Task 3: Pet landscape two-column Alive screen + food modal clamp

**Files:**
- Modify: `src/apps/pet/PetApp.cpp:156-225` (buildAlive), `openFoodTray` (~line 280)
- Modify: `src/apps/pet/PetApp.h` (one helper declaration)

**Interfaces:**
- Consumes: `layout::kLandscape/kAppW/kAppH` from `src/core/Layout.h`.
- Produces: nothing later tasks rely on.

- [ ] **Step 1: Column helper** — declare in PetApp.h's private section:

```cpp
  static lv_obj_t* makeColumn(lv_obj_t* parent, lv_coord_t pctWidth);
```

and define in PetApp.cpp (near buildAlive):

```cpp
// One column of the landscape Alive screen: a style-stripped flex column
// that inherits the root's row layout.
lv_obj_t* PetApp::makeColumn(lv_obj_t* parent, lv_coord_t pctWidth) {
  lv_obj_t* col = lv_obj_create(parent);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_PCT(pctWidth), LV_PCT(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(col, 4, 0);
  return col;
}
```

- [ ] **Step 2: Two-column `buildAlive`** — add `#include "core/Layout.h"`, then restructure the top of `buildAlive` and re-parent its pieces. The existing widget-creation code is kept verbatim, only its parent handles change (`root_` → `left`/`right`):

```cpp
void PetApp::buildAlive() {
  const uint32_t today = todayKey();
  lv_obj_t* left = root_;
  lv_obj_t* right = root_;
  if (layout::kLandscape) {
    // Two columns (spec 2026-07-22): sprite/name/health left, needs +
    // actions right. The columns are flex children of the root row.
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(root_, 6, 0);
    left = makeColumn(root_, 44);
    right = makeColumn(root_, 52);
  } else {
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(root_, 4, 0);
    lv_obj_set_style_pad_top(root_, 6, 0);
  }
  ...
}
```

Then re-parent: `nameLbl`, `petImg_` (`makePetArt`), the mess row, and the `health` label onto `left`; the needs `grid`, the no-clock `hint`, and `btnRow` onto `right`. (In portrait `left == right == root_`, so the portrait tree is bit-identical.)

- [ ] **Step 3: Clamp the food tray modal** — in `openFoodTray`, replace the hardcoded size:

```cpp
  lv_obj_set_size(tray, LV_MIN(210, layout::kAppW - 8),
                  LV_MIN(250, layout::kAppH - 8));  // 210x200 landscape
```
(Portrait: `LV_MIN(210,232)=210`, `LV_MIN(250,280)=250` — unchanged.)

- [ ] **Step 4: Compile gate**

Run: `pio run -e cyd -e cyd-landscape`
Expected: both `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/apps/pet/PetApp.h src/apps/pet/PetApp.cpp
git commit -m "feat: pet landscape two-column alive screen, clamped food tray"
```

---

### Task 4: Verification — builds, native tests, full on-device sweep

**⚠️ USER-IN-THE-LOOP** for the device checks. New SD asset this round: the user must copy `sd/art/ls/boot-logo.bin` into the card's `art/ls/` folder first.

- [ ] **Step 1:** `pio run -e cyd -e cyd-landscape` → both SUCCESS; `pio test -e native` → all pass.
- [ ] **Step 2:** Flash landscape (`pio run -e cyd-landscape -t upload`), remind the user to copy the boot-logo asset, then walk the checklist:
  1. Boot splash: scaled logo, clean strip with "Conectando".
  2. Calculator: keypad fills 320 wide, all 5 rows visible and tappable.
  3. Pet: two-column Alive screen, mess taps work, food tray fits, egg/memorial/naming still fine.
  4. Pomodoro: art + countdown left, start/stop + both steppers right, all operable.
  5. Settings: menu, each section, and a keyboard modal usable at 208px.
  6. Weather / Minesweeper / Oracle: unchanged-good.
- [ ] **Step 3:** Portrait regression (owed since the spike): `pio run -e cyd -t upload`, user spot-checks every app for zero change.
- [ ] **Step 4:** Record outcomes in the spec's Verification section and commit.
