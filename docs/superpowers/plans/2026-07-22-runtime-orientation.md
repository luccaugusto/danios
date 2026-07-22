# Runtime Orientation Setting Implementation Plan

> **For agentic workers:** Execute task-by-task with a fresh implementer per task and a review after each. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Orientation becomes a reboot-to-apply NVS setting (`disp.landscape`, default portrait) with a Settings → Tela toggle; the `DANIOS_LANDSCAPE` flag and `cyd-landscape` env are removed — one binary serves both orientations.

**Architecture:** `layout::` constants turn into boot-time globals set by `layout::init(bool)` (called in `setup()` after `settings.begin()`, before any UI). Names are unchanged so existing call sites stay untouched; only code that used `layout::` in constant expressions restructures (DisplayService members, minesweeper presets, weather locals, main.cpp boot-logo, Launcher's model columns).

**Tech Stack:** PlatformIO espressif32, LVGL 8.4, ESP32-2432S024 on `/dev/ttyUSB0`.

**Spec:** `docs/superpowers/specs/2026-07-22-runtime-orientation-design.md`

## Global Constraints

- Branch `landscape-ui`.
- After this work, `DANIOS_LANDSCAPE` must not appear anywhere; `pio run -e cyd` is the only device env.
- Behavior identity: with `disp.landscape` unset/false the device must behave exactly like today's portrait build; with it true, exactly like today's `cyd-landscape` build.
- `layout::` names (`kLandscape`, `kScreenW`, `kScreenH`, `kAppW`, `kAppH`, `kGridCols`, `kTopBarH`) keep their spellings — call sites outside the files named in tasks must not change.
- PT UI strings: codepoints 0x20-0x7F / 0xA0-0xFF only, no em/en dashes (docs: montserrat_pt_14).
- Build gate per task: `pio run -e cyd` SUCCESS (task 1 also removes the other env). Native tests in Tasks 1 and 4.
- Serial/flash workflow: free the port before flashing.

---

### Task 1: layout::init runtime core + env collapse + launcher columns

**Files:**
- Modify: `src/core/Layout.h` (rewrite)
- Create: `src/core/Layout.cpp`
- Modify: `src/services/DisplayService.h` (delete kHorRes/kVerRes), `src/services/DisplayService.cpp:begin()`
- Modify: `src/main.cpp` (init call + kBootLogo local)
- Modify: `lib/launcher_model/launcher_model.h/.cpp` (setColumns), `src/core/Launcher.h:47`, `src/core/Launcher.cpp:buildHomeScreen()`
- Modify: `platformio.ini` (delete `[env:cyd-landscape]`)
- Test: `test/test_launcher_model/` (add setColumns case to the existing suite)

**Interfaces:**
- Produces: `void layout::init(bool landscape)`; the `layout::` globals (same names/types as today, now non-constexpr except `kTopBarH`); `LauncherModel::setColumns(int)`. Task 2 relies on the globals being set before any app UI builds.

- [ ] **Step 1: Rewrite `src/core/Layout.h`**

```cpp
// src/core/Layout.h — orientation-derived UI dimensions.
// Orientation is a runtime setting (NVS "disp.landscape", spec 2026-07-22):
// layout::init() runs once in setup() after settings.begin() and BEFORE any
// display/UI code, so every consumer reads post-init values. The `k` names
// are kept from the compile-time era so ~40 call sites stay unchanged —
// treat them as set-once-at-boot, never reassigned afterwards.
#pragma once

#include <lvgl.h>

namespace layout {

// Set once by init(); portrait defaults protect static-init readers.
extern bool kLandscape;
extern lv_coord_t kScreenW;   // 240 portrait, 320 landscape
extern lv_coord_t kScreenH;   // 320 portrait, 240 landscape

// App-screen back-bar height (orientation-independent).
constexpr lv_coord_t kTopBarH = 32;
extern lv_coord_t kAppW;      // container apps build into
extern lv_coord_t kAppH;      // 288 portrait, 208 landscape
extern int kGridCols;         // 3 portrait, 4 landscape (80 px cells)

// Call exactly once from setup(), after settings.begin() and before
// DisplayService::begin() or any lv_* call that sizes widgets.
void init(bool landscape);

}  // namespace layout
```

- [ ] **Step 2: Create `src/core/Layout.cpp`**

```cpp
#include "core/Layout.h"

namespace layout {

bool kLandscape = false;
lv_coord_t kScreenW = 240;
lv_coord_t kScreenH = 320;
lv_coord_t kAppW = 240;
lv_coord_t kAppH = 320 - kTopBarH;
int kGridCols = 3;

void init(bool landscape) {
  kLandscape = landscape;
  kScreenW = landscape ? 320 : 240;
  kScreenH = landscape ? 240 : 320;
  kAppW = kScreenW;
  kAppH = kScreenH - kTopBarH;
  kGridCols = landscape ? 4 : 3;
}

}  // namespace layout
```

- [ ] **Step 3: DisplayService** — in the `.h`, delete the `kHorRes`/`kVerRes` members (keep `kBufPixels = 7200`) and fix the class comment; in `begin()`, replace their uses:

```cpp
  dispDrv_.hor_res = layout::kScreenW;
  dispDrv_.ver_res = layout::kScreenH;
```
(The rotation/calibration ternaries already read `layout::kLandscape` at runtime — no change.)

- [ ] **Step 4: main.cpp** — in `setup()`, immediately after `settings.begin();` add:

```cpp
  // Orientation is a reboot-to-apply setting (spec 2026-07-22): resolve it
  // before ANY display or widget code runs.
  layout::init(settings.getBool("disp.landscape", false));
```

and change the boot-logo constant (a `static constexpr` ternary can no longer read the runtime flag) to a plain local:

```cpp
  const char* const kBootLogo = layout::kLandscape
      ? "S:/art/ls/boot-logo.bin"   // 173x208, pre-scaled (LVGL can't zoom SD art)
      : "S:/art/boot-logo.bin";     // 240x288
```

- [ ] **Step 5: Launcher columns** — the global `Launcher` constructs before `setup()`, so `model_{layout::kGridCols}` would bake in the pre-init default. Add to `lib/launcher_model/launcher_model.h` (public section):

```cpp
  // Grid geometry can be set after registration (orientation is only known
  // once NVS is read); columns feed slotOf() math only.
  void setColumns(int columns);
```

`.cpp`: `void LauncherModel::setColumns(int columns) { columns_ = columns; }`

In `src/core/Launcher.h` line 47: `LauncherModel model_{3};  // columns set for real in buildHomeScreen()`
In `Launcher.cpp::buildHomeScreen()`, first line: `model_.setColumns(layout::kGridCols);`

- [ ] **Step 6: Native test** — in the existing launcher-model suite add:

```cpp
void test_set_columns_after_registration() {
  LauncherModel m(3);
  for (int i = 0; i < 5; ++i) m.registerApp("app" + std::to_string(i));
  m.setColumns(4);
  TEST_ASSERT_EQUAL_INT(0, m.slotOf(3).row);
  TEST_ASSERT_EQUAL_INT(3, m.slotOf(3).col);
  TEST_ASSERT_EQUAL_INT(1, m.slotOf(4).row);
  TEST_ASSERT_EQUAL_INT(0, m.slotOf(4).col);
}
```
(register it in the suite's RUN_TEST list; follow the file's existing naming.)

- [ ] **Step 7: platformio.ini** — delete the whole `[env:cyd-landscape]` block. Grep check: `grep -rn "DANIOS_LANDSCAPE" src include platformio.ini test lib` must return ONLY the Layout.h history comment if any — after this task the token must not appear in code (comments referencing the old era are allowed only in specs/docs).

- [ ] **Step 8: Gates**

Run: `pio run -e cyd` → SUCCESS; `pio test -e native` → all pass (incl. new test); `pio run -e cyd-landscape` → must now FAIL with unknown environment.

- [ ] **Step 9: Commit**

```bash
git add -A src/core lib/launcher_model src/services src/main.cpp platformio.ini test
git commit -m "feat: orientation from NVS at boot — layout::init, env collapse, launcher setColumns"
```

---

### Task 2: constexpr consumers → runtime (minesweeper, weather)

**Files:**
- Modify: `src/apps/minesweeper/MinesweeperApp.cpp` (anonymous namespace + uses)
- Modify: `src/apps/weather/WeatherApp.cpp:renderLandscape` (two locals)

**Interfaces:**
- Consumes: `layout::` globals (Task 1). No produced interfaces.

- [ ] **Step 1: Minesweeper presets/clamps become functions** — the namespace-scope `constexpr Preset kEasy/kHard`, `kHudH`, `kMaxRows` initializers would now run at static-init (before `layout::init`). Replace that block with call-time functions (all uses are post-boot):

```cpp
// Orientation is resolved at boot (layout::init), so preset/clamp values are
// computed at call time, not static-init. Landscape boards are wide-short
// (208 px tall app area): 6 rows x 12 cols max keeps 26 px cells. Mine
// ratios track the portrait presets (Fácil ~22%, Difícil ~28%). Separate
// best-time keys — a 6x9 Fácil is not the same game as the portrait 9x9.
const Preset& easyPreset() {
  static const Preset kPortrait{9, 9, 18, "mines_best_easy"};
  static const Preset kLs{6, 9, 12, "mines_best_easy_l"};
  return layout::kLandscape ? kLs : kPortrait;
}
const Preset& hardPreset() {
  static const Preset kPortrait{13, 9, 33, "mines_best_hard"};
  static const Preset kLs{6, 12, 20, "mines_best_hard_l"};
  return layout::kLandscape ? kLs : kPortrait;
}
// Board HUD row height; slimmer in landscape to buy grid height.
inline lv_coord_t hudH() { return layout::kLandscape ? 28 : 32; }

constexpr uint8_t kMinRC = 5;
inline uint8_t maxRows() { return layout::kLandscape ? 6 : 14; }
constexpr uint8_t kMaxCols = 12;
```

Then update every use in the file: `kEasy` → `easyPreset()`, `kHard` → `hardPreset()`, `kHudH` → `hudH()`, `kMaxRows` → `maxRows()` (they appear in `bestKeyFor`, `loadSetup`, `stepSetup`, `showStart`/`showStartLandscape` preset buttons, `newGame`, `showBoard`). The `PresetBtn` tables that held `const Preset&` members keep working — bind them to `easyPreset()`/`hardPreset()` results (the statics have process lifetime).

- [ ] **Step 2: Weather locals** — in `renderLandscape`, change the two `constexpr lv_coord_t kArtW/kArtH` declarations to `const lv_coord_t` (values/uses unchanged).

- [ ] **Step 3: Gates**

Run: `pio run -e cyd` → SUCCESS. Grep check: `grep -n "constexpr.*layout::" src -r` → no hits.

- [ ] **Step 4: Commit**

```bash
git add src/apps/minesweeper/MinesweeperApp.cpp src/apps/weather/WeatherApp.cpp
git commit -m "refactor: orientation-dependent constexprs become call-time functions"
```

---

### Task 3: Settings → Tela landscape toggle with restart offer

**Files:**
- Modify: `src/apps/settings/DisplaySection.cpp`

**Interfaces:**
- Consumes: `ISettingsStore::getBool/setBool`, `esp_restart()` (`<esp_system.h>`), `layout::kLandscape` (only to preset the switch — the SAVED value governs, so read the store, not layout, in case the user toggled without rebooting).

- [ ] **Step 1: Add the toggle row** at the end of `buildDisplaySection` (after the Suspender dropdown), plus the handlers in the anonymous namespace:

```cpp
// (anonymous namespace, next to sleepChanged)
void orientationMsgboxCb(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  const uint16_t btn = lv_msgbox_get_active_btn(mbox);
  lv_msgbox_close(mbox);
  if (btn == 0) esp_restart();  // "Reiniciar"
}

void orientationChanged(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  const bool landscape = lv_obj_has_state(sw, LV_STATE_CHECKED);
  ctx.store->setBool("disp.landscape", landscape);
  static const char* kBtns[] = {"Reiniciar", "Depois", ""};
  lv_obj_t* m = lv_msgbox_create(NULL, "Orientação",
                                 "Reinicie para aplicar.", kBtns, true);
  lv_obj_add_event_cb(m, orientationMsgboxCb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_center(m);
}
```

```cpp
// (end of buildDisplaySection)
  lv_obj_t* orientLbl = lv_label_create(parent);
  lv_label_set_text(orientLbl, "Deitado (USB à esquerda)");

  lv_obj_t* sw = lv_switch_create(parent);
  if (store.getBool("disp.landscape", false))
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, orientationChanged, LV_EVENT_VALUE_CHANGED, nullptr);
```

Add `#include <esp_system.h>` with the includes. String check: "Orientação"/"à" are in 0xA0-0xFF ✓, no em dashes.

- [ ] **Step 2: Gate**

Run: `pio run -e cyd` → SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/apps/settings/DisplaySection.cpp
git commit -m "feat: settings toggle for landscape orientation (reboot to apply)"
```

---

### Task 4: Verification — single binary, both orientations via the toggle

**⚠️ USER-IN-THE-LOOP.**

- [ ] **Step 1:** `pio run -e cyd` SUCCESS; `pio test -e native` all pass; `pio run -e cyd-landscape` fails (env gone); `grep -rn DANIOS_LANDSCAPE src include lib test platformio.ini` → no hits.
- [ ] **Step 2:** Flash (`pio run -e cyd -t upload`). Device must boot **portrait** (key unset → default) and look identical to the old portrait build.
- [ ] **Step 3:** User: Settings → Tela → toggle "Deitado" → msgbox → **Reiniciar**. Device reboots straight into landscape (USB left, touch accurate, boot splash ls/ logo, apps in landscape layouts — spot-check weather, minesweeper, pomodoro, pet, calculator).
- [ ] **Step 4:** User: toggle back off → **Depois** → nothing happens; manual power cycle → boots portrait.
- [ ] **Step 5:** Record outcome in the spec, commit docs.
