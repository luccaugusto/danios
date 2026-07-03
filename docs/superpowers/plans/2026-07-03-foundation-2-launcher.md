# Foundation 2 — App Framework + Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Roadmap row:** F2 (`docs/superpowers/plans/2026-07-03-danios-roadmap.md`)
**Spec sections:** §3.1 Layer 3, §3.3 Launcher, §3.5 main loop, §5 Settings (shell only)
**E2E outcome (roadmap §1):** Navigate launcher → stub app → back; gear opens empty Settings.

**Goal:** Build the app framework (`App` interface), the home-screen Launcher grid with status bar, a Settings shell, and five stub apps, wired into `main.cpp` so the device boots to a navigable home screen.

**Architecture:** Pure grid/badge/enabled bookkeeping lives in `lib/launcher_model/` (std C++17 only, TDD'd natively). `src/core/Launcher.{h,cpp}` is a thin LVGL wrapper around that model, owning two LVGL screens (home = status bar + icon grid; app = back-arrow top bar + content container) and the app lifecycle. `src/core/StatusBar.{h,cpp}` is the 24 px home strip. Apps implement the roadmap §4.5 `App` interface; F2 ships a `SettingsApp` shell and a reusable `StubApp` registered five times.

**Tech Stack:** PlatformIO (`espressif32@7.0.1`, arduino-esp32 3.x), LVGL 8.4.0 (v8 API), LovyanGFX via F1's DisplayService, Unity tests on `[env:native]`, C++17.

## Global Constraints

(Copied verbatim from roadmap §2 — every task's requirements implicitly include this section.)

- **Board:** ESP32-2432S024C (CYD 2.4" capacitive), ESP32-WROOM-32, **no PSRAM**.
  520 KB SRAM total; budget carefully (LVGL buffers ~29 KB, LVGL heap 48 KB,
  WiFi ~50 KB or BT Classic ~64 KB — **never both**, MP3 decode ~30 KB).
- **Platform:** PlatformIO, `platform = espressif32@7.0.1`, `board = esp32dev`,
  `framework = arduino` (arduino-esp32 3.x). Partition scheme:
  `board_build.partitions = huge_app.csv` (no OTA — spec non-goal).
- **Display:** landscape-native 320×240 clone driven via
  `include/LGFX_ESP32_2432S024C.hpp` — **do not change panel/memory dims (320×240)
  or `offset_rotation` (0)**. All UI renders portrait 240×320 via
  `tft.setRotation(7)`, USB-C down. See `docs/DISPLAY.md` — read it before any
  display work.
- **Pins:** display HSPI (SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST −1, BL 27
  PWM); touch CST820 I²C (SDA 33, SCL 32, RST 25, INT 21 — **poll, don't trust
  INT**), addr 0x15; SD on VSPI (SCK 18, MISO 19, MOSI 23, CS 5). Three separate
  buses. RGB LED 4/16/17 (active-low), photoresistor 34, speaker amp 26.
- **No battery-voltage ADC on this board** (IP5603 is charge/boost only) — see
  spec deviation §5 of the roadmap.
- **LVGL:** `lvgl/lvgl@8.4.0` (v8 API — not v9). `lv_conf.h` lives in `include/`,
  enabled with `build_flags = -DLV_CONF_INCLUDE_SIMPLE -Iinclude`. Two draw
  buffers of 240×30 px. UI code runs on the Arduino loop task only (LVGL is not
  thread-safe).
- **C++17** on both envs: `build_unflags = -std=gnu++11`,
  `build_flags = -std=gnu++17`.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager` (§4.6).
  No service or app touches `WiFi.*` / `esp_bt_*` power state directly.
- **TDD, native-first:** all pure logic lives in `lib/<module>/` with **zero
  Arduino/LVGL includes** (std C++ only) and is unit-tested with
  `pio test -e native` (Unity). Services/UI wrap the pure logic thinly.
- **Commits:** small, frequent, conventional (`feat:`, `test:`, `fix:`, `docs:`).
  The repo is git-initialized in F1 Task 0; every later plan assumes git exists.
- **SD layout & NVS keys:** exactly as pinned in roadmap §4.1/§4.2 — never invent
  new paths/keys outside your plan's reserved set.

---

## What F2 consumes from F1 (do not recreate)

F1 delivered: LVGL 8.4 bound to LovyanGFX at portrait 240×320, the CST820 touch
driver feeding LVGL, `[env:native]` with Unity, and a git-initialized repo.
This plan consumes:

- `src/services/DisplayService.{h,cpp}` — assumed surface: `void begin()`
  (init panel + LVGL + flush binding) and `void tick()` (LVGL tick +
  `lv_timer_handler()` pump), called from `setup()`/`loop()`.
- `src/services/TouchService.{h,cpp}` — assumed surface: `void begin()`
  (registers the LVGL pointer indev; LVGL polls it afterward).
- `include/lv_conf.h` with the default Montserrat-14 font (LVGL v8 built-in
  symbols `LV_SYMBOL_WIFI`, `LV_SYMBOL_BLUETOOTH`, `LV_SYMBOL_SETTINGS`,
  `LV_SYMBOL_LEFT` available).
- `[env:native]` (`platform = native`, `test_build_src = false`, Unity, C++17).

> **F1 API adaptation point:** the roadmap pins F1's file paths but not its
> method names. If F1's actual methods differ (e.g. `init()` instead of
> `begin()`), adapt **only** the four lines marked `// F1 API` in `src/main.cpp`
> (Task 6). Nothing else in this plan calls the F1 services.

## What F2 produces (later plans consume verbatim)

- `src/core/App.h` — `App` interface + `enum class RadioMode` (roadmap §4.5, verbatim).
- `src/core/Launcher.{h,cpp}` — the roadmap §4.5 `Launcher` public API.
- `src/core/StatusBar.{h,cpp}` — `setClockText` (F4), `setRadio` (F4/F5),
  `setBatteryText` (future board revision; renders nothing while empty —
  roadmap deviation §5.1: this board has **no battery-voltage ADC**).
- `src/apps/settings/SettingsApp.{h,cpp}` — Settings shell; F3/F4/F5/A3 append
  section files at the marked registration point (roadmap §4.11).
- `src/apps/StubApp.h` — placeholder app; each app plan (A1–A5) replaces its
  stub registration in `main.cpp` with the real app.
- `lib/launcher_model/` — pure bookkeeping (grid slots, badge/enabled state).

## File structure

```
lib/launcher_model/launcher_model.h      pure model: registration order → grid slots,
lib/launcher_model/launcher_model.cpp    badge/enabled bookkeeping (std C++17 only)
test/test_launcher_model/test_main.cpp   native Unity tests for the model
src/core/App.h                           App interface + RadioMode (roadmap §4.5 verbatim)
src/core/StatusBar.h                     24 px home status strip: clock, radio glyph,
src/core/StatusBar.cpp                   battery hook (empty), gear button
src/core/Launcher.h                      LVGL wrapper: home grid, app screen with back
src/core/Launcher.cpp                    bar, open/goHome lifecycle, badge/enabled UI
src/apps/StubApp.h                       header-only "coming soon" placeholder app
src/apps/settings/SettingsApp.h          Settings shell (lv_list + section
src/apps/settings/SettingsApp.cpp        registration point)
src/main.cpp                             REWRITTEN: services → StatusBar + Launcher →
                                         register apps → show → loop ticks
README.md                                MODIFIED: five-app list (roadmap deviation §5.3)
```

**Decided details (open in the roadmap, fixed here):**

- The **grid shows five icons** (weather, music, calc, oracle, pet). Settings is
  registered with the Launcher (so `openApp("settings")` works) but is **not a
  grid icon** — spec §3.3 pins "grid of five app icons" with the gear in the
  status bar as the Settings entry point.
- Grid geometry: 3 columns × 80 px cells, 110 px tall, below the 24 px status
  bar. Registration order fills rows left-to-right, top-to-bottom.
- `Launcher` takes `StatusBar&` in its constructor (constructor not pinned by
  the roadmap) so it can build the bar into the home screen and wire the gear.
- On a **granted** radio request the Launcher calls `StatusBar::setRadio(mode)`,
  and `setRadio(RadioMode::None)` on `goHome()` — glyph stays correct for free
  once F4 wires `RadioManager::request` into `setRadioRequest`.
- If `radioRequest_` is set and returns `false`, `openApp` shows a modal
  `lv_msgbox` ("Radio unavailable right now.") and stays home. Unreachable in
  F2 (unset → always true); the path exists for F4/F5.

---

### Task 0: Preflight — verify the F1 baseline

No code. Confirms the artifacts this plan consumes actually exist. **Board:**
none needed (build-only).

**Files:**
- None (verification only).

**Interfaces:**
- Consumes: F1 outputs listed above.
- Produces: nothing.

- [ ] **Step 1: Verify git repo and F1 files exist**

Run:
```bash
git -C /home/lucca/repos/danios log --oneline -3
ls /home/lucca/repos/danios/src/services/DisplayService.h \
   /home/lucca/repos/danios/src/services/TouchService.h \
   /home/lucca/repos/danios/include/lv_conf.h
```
Expected: three commit lines; all three paths listed with no "No such file" error.
**If any file is missing, STOP — F1 is not complete. Do not start F2.**

- [ ] **Step 2: Verify device build is green**

Run: `pio run -e cyd`
Expected: ends with `[SUCCESS]`.

- [ ] **Step 3: Verify native test env runs**

Run: `pio test -e native`
Expected: exits 0 — F1's native tests `PASSED` (or "No tests found" style
success if F1 shipped none; a build error means the env is broken — STOP).

---

### Task 1: `lib/launcher_model/` — registration order and grid mapping (native TDD)

Pure std-C++ model: apps register in order; grid position and row/col slots are
derived from registration order, skipping non-grid entries (Settings).

**Files:**
- Create: `lib/launcher_model/launcher_model.h`
- Create: `lib/launcher_model/launcher_model.cpp`
- Test: `test/test_launcher_model/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++17 only — zero Arduino/LVGL includes).
- Produces (Task 2 extends, Task 4's Launcher consumes):
  - `struct GridSlot { int row; int col; };`
  - `LauncherModel(int columns)` — explicit; Launcher uses `LauncherModel{3}`.
  - `int registerApp(const std::string& id, bool inGrid = true)` → registration
    index, `-1` on duplicate/empty id.
  - `int count() const`, `int gridCount() const`
  - `int indexOf(const std::string& id) const` → registration index, `-1` unknown.
  - `int gridIndexOf(const std::string& id) const` → 0-based grid position,
    `-1` if unknown or not in grid.
  - `const std::string& idAtGrid(int gridIndex) const` (precondition:
    `0 <= gridIndex < gridCount()`)
  - `GridSlot slotOf(int gridIndex) const` → `{gridIndex / columns, gridIndex % columns}`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_launcher_model/test_main.cpp`:

```cpp
#include <unity.h>

#include "launcher_model.h"

void setUp() {}
void tearDown() {}

// Five grid apps in the pinned registration order + settings off-grid.
static LauncherModel makeFive() {
  LauncherModel m(3);
  m.registerApp("weather");
  m.registerApp("music");
  m.registerApp("calc");
  m.registerApp("oracle");
  m.registerApp("pet");
  m.registerApp("settings", /*inGrid=*/false);
  return m;
}

void test_registration_order_and_count() {
  LauncherModel m(3);
  TEST_ASSERT_EQUAL_INT(0, m.registerApp("weather"));
  TEST_ASSERT_EQUAL_INT(1, m.registerApp("music"));
  TEST_ASSERT_EQUAL_INT(2, m.count());
  TEST_ASSERT_EQUAL_INT(0, m.indexOf("weather"));
  TEST_ASSERT_EQUAL_INT(1, m.indexOf("music"));
  TEST_ASSERT_EQUAL_INT(-1, m.indexOf("nope"));
}

void test_duplicate_and_empty_ids_rejected() {
  LauncherModel m(3);
  m.registerApp("weather");
  TEST_ASSERT_EQUAL_INT(-1, m.registerApp("weather"));
  TEST_ASSERT_EQUAL_INT(-1, m.registerApp(""));
  TEST_ASSERT_EQUAL_INT(1, m.count());
}

void test_grid_excludes_non_grid_entries() {
  LauncherModel m = makeFive();
  TEST_ASSERT_EQUAL_INT(6, m.count());
  TEST_ASSERT_EQUAL_INT(5, m.gridCount());
  TEST_ASSERT_EQUAL_INT(-1, m.gridIndexOf("settings"));
  TEST_ASSERT_EQUAL_INT(0, m.gridIndexOf("weather"));
  TEST_ASSERT_EQUAL_INT(4, m.gridIndexOf("pet"));
  TEST_ASSERT_EQUAL_STRING("weather", m.idAtGrid(0).c_str());
  TEST_ASSERT_EQUAL_STRING("pet", m.idAtGrid(4).c_str());
}

void test_grid_slots_three_columns() {
  LauncherModel m = makeFive();
  GridSlot s0 = m.slotOf(0);
  TEST_ASSERT_EQUAL_INT(0, s0.row);
  TEST_ASSERT_EQUAL_INT(0, s0.col);
  GridSlot s2 = m.slotOf(2);
  TEST_ASSERT_EQUAL_INT(0, s2.row);
  TEST_ASSERT_EQUAL_INT(2, s2.col);
  GridSlot s3 = m.slotOf(3);
  TEST_ASSERT_EQUAL_INT(1, s3.row);
  TEST_ASSERT_EQUAL_INT(0, s3.col);
  GridSlot s4 = m.slotOf(4);
  TEST_ASSERT_EQUAL_INT(1, s4.row);
  TEST_ASSERT_EQUAL_INT(1, s4.col);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_registration_order_and_count);
  RUN_TEST(test_duplicate_and_empty_ids_rejected);
  RUN_TEST(test_grid_excludes_non_grid_entries);
  RUN_TEST(test_grid_slots_three_columns);
  return UNITY_END();
}
```

- [ ] **Step 2: Run the tests — expect FAIL**

Run: `pio test -e native -f test_launcher_model`
Expected: **FAIL** (build error: `launcher_model.h: No such file or directory`).

- [ ] **Step 3: Implement the model**

Create `lib/launcher_model/launcher_model.h`:

```cpp
// lib/launcher_model/launcher_model.h — std C++17 only, no Arduino/LVGL.
// Pure bookkeeping behind src/core/Launcher: registration order → grid slots.
#pragma once

#include <string>
#include <vector>

struct GridSlot {
  int row;
  int col;
};

class LauncherModel {
 public:
  explicit LauncherModel(int columns);

  // Returns the registration index (0-based), or -1 for a duplicate/empty id.
  // Registration order = grid order (roadmap §4.5). inGrid=false entries
  // (Settings) are openable but get no grid slot.
  int registerApp(const std::string& id, bool inGrid = true);

  int count() const;      // all registered entries
  int gridCount() const;  // only inGrid entries

  int indexOf(const std::string& id) const;      // registration index, -1 unknown
  int gridIndexOf(const std::string& id) const;  // grid position, -1 if not in grid

  // Precondition: 0 <= gridIndex < gridCount().
  const std::string& idAtGrid(int gridIndex) const;
  GridSlot slotOf(int gridIndex) const;

 private:
  struct Entry {
    std::string id;
    bool inGrid;
    bool badge;
    bool enabled;
  };

  int columns_;
  std::vector<Entry> entries_;     // registration order
  std::vector<int> gridToEntry_;   // grid index → entries_ index
};
```

Create `lib/launcher_model/launcher_model.cpp`:

```cpp
#include "launcher_model.h"

LauncherModel::LauncherModel(int columns) : columns_(columns < 1 ? 1 : columns) {}

int LauncherModel::registerApp(const std::string& id, bool inGrid) {
  if (id.empty() || indexOf(id) >= 0) return -1;
  entries_.push_back(Entry{id, inGrid, /*badge=*/false, /*enabled=*/true});
  const int idx = static_cast<int>(entries_.size()) - 1;
  if (inGrid) gridToEntry_.push_back(idx);
  return idx;
}

int LauncherModel::count() const { return static_cast<int>(entries_.size()); }

int LauncherModel::gridCount() const { return static_cast<int>(gridToEntry_.size()); }

int LauncherModel::indexOf(const std::string& id) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].id == id) return static_cast<int>(i);
  }
  return -1;
}

int LauncherModel::gridIndexOf(const std::string& id) const {
  for (size_t g = 0; g < gridToEntry_.size(); ++g) {
    if (entries_[static_cast<size_t>(gridToEntry_[g])].id == id) {
      return static_cast<int>(g);
    }
  }
  return -1;
}

const std::string& LauncherModel::idAtGrid(int gridIndex) const {
  return entries_[static_cast<size_t>(gridToEntry_[static_cast<size_t>(gridIndex)])].id;
}

GridSlot LauncherModel::slotOf(int gridIndex) const {
  return GridSlot{gridIndex / columns_, gridIndex % columns_};
}
```

- [ ] **Step 4: Run the tests — expect PASS**

Run: `pio test -e native -f test_launcher_model`
Expected: **PASS** — `4 test cases: 4 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add lib/launcher_model test/test_launcher_model
git commit -m "feat: launcher model registration and grid mapping"
```

---

### Task 2: `lib/launcher_model/` — badge and enabled bookkeeping (native TDD)

Extends the model with the state behind `Launcher::setBadge` /
`Launcher::setAppEnabled` and the "can this id open?" gate.

**Files:**
- Modify: `lib/launcher_model/launcher_model.h`
- Modify: `lib/launcher_model/launcher_model.cpp`
- Test: `test/test_launcher_model/test_main.cpp`

**Interfaces:**
- Consumes: Task 1's `LauncherModel`.
- Produces (Task 4's Launcher consumes):
  - `bool setBadge(const std::string& id, bool on)` → `false` if id unknown.
  - `bool badgeAtGrid(int gridIndex) const` (default `false`)
  - `bool setEnabled(const std::string& id, bool en)` → `false` if id unknown.
  - `bool enabledAtGrid(int gridIndex) const` (default `true`)
  - `bool enabled(const std::string& id) const` → unknown id ⇒ `false`.
  - `bool canOpen(const std::string& id) const` → known **and** enabled
    (grid membership irrelevant: `canOpen("settings")` is `true`).

- [ ] **Step 1: Write the failing tests**

Append to `test/test_launcher_model/test_main.cpp` (above `main`):

```cpp
void test_badge_bookkeeping() {
  LauncherModel m = makeFive();
  TEST_ASSERT_FALSE(m.badgeAtGrid(4));               // pet, default off
  TEST_ASSERT_TRUE(m.setBadge("pet", true));
  TEST_ASSERT_TRUE(m.badgeAtGrid(4));
  TEST_ASSERT_TRUE(m.setBadge("pet", false));
  TEST_ASSERT_FALSE(m.badgeAtGrid(4));
  TEST_ASSERT_FALSE(m.setBadge("nope", true));       // unknown id rejected
}

void test_enabled_bookkeeping_and_can_open() {
  LauncherModel m = makeFive();
  TEST_ASSERT_TRUE(m.enabledAtGrid(1));              // music, default enabled
  TEST_ASSERT_TRUE(m.canOpen("music"));
  TEST_ASSERT_TRUE(m.setEnabled("music", false));
  TEST_ASSERT_FALSE(m.enabledAtGrid(1));
  TEST_ASSERT_FALSE(m.canOpen("music"));
  TEST_ASSERT_TRUE(m.setEnabled("music", true));
  TEST_ASSERT_TRUE(m.canOpen("music"));
  TEST_ASSERT_FALSE(m.canOpen("nope"));              // unknown id
  TEST_ASSERT_FALSE(m.setEnabled("nope", false));
  TEST_ASSERT_TRUE(m.canOpen("settings"));           // off-grid, still openable
}
```

And add the two `RUN_TEST` lines in `main` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_badge_bookkeeping);
  RUN_TEST(test_enabled_bookkeeping_and_can_open);
```

- [ ] **Step 2: Run the tests — expect FAIL**

Run: `pio test -e native -f test_launcher_model`
Expected: **FAIL** (build error: no member named `badgeAtGrid` / `setBadge` in
`LauncherModel`).

- [ ] **Step 3: Implement badge/enabled state**

In `lib/launcher_model/launcher_model.h`, add to the `public:` section (after
`GridSlot slotOf(int gridIndex) const;`):

```cpp
  // Badge (red dot) and enabled (greyed icon) bookkeeping. Setters return
  // false when the id is unknown. Defaults: badge=false, enabled=true.
  bool setBadge(const std::string& id, bool on);
  bool badgeAtGrid(int gridIndex) const;
  bool setEnabled(const std::string& id, bool en);
  bool enabledAtGrid(int gridIndex) const;
  bool enabled(const std::string& id) const;  // unknown id → false
  bool canOpen(const std::string& id) const;  // known && enabled
```

Append to `lib/launcher_model/launcher_model.cpp`:

```cpp
bool LauncherModel::setBadge(const std::string& id, bool on) {
  const int i = indexOf(id);
  if (i < 0) return false;
  entries_[static_cast<size_t>(i)].badge = on;
  return true;
}

bool LauncherModel::badgeAtGrid(int gridIndex) const {
  return entries_[static_cast<size_t>(gridToEntry_[static_cast<size_t>(gridIndex)])].badge;
}

bool LauncherModel::setEnabled(const std::string& id, bool en) {
  const int i = indexOf(id);
  if (i < 0) return false;
  entries_[static_cast<size_t>(i)].enabled = en;
  return true;
}

bool LauncherModel::enabledAtGrid(int gridIndex) const {
  return entries_[static_cast<size_t>(gridToEntry_[static_cast<size_t>(gridIndex)])].enabled;
}

bool LauncherModel::enabled(const std::string& id) const {
  const int i = indexOf(id);
  return i >= 0 && entries_[static_cast<size_t>(i)].enabled;
}

bool LauncherModel::canOpen(const std::string& id) const { return enabled(id); }
```

- [ ] **Step 4: Run the tests — expect PASS**

Run: `pio test -e native -f test_launcher_model`
Expected: **PASS** — `6 test cases: 6 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add lib/launcher_model test/test_launcher_model
git commit -m "feat: launcher model badge and enabled bookkeeping"
```

---

### Task 3: `src/core/App.h` (roadmap §4.5 verbatim) + `src/core/StatusBar.{h,cpp}`

The `App` interface is copied **verbatim** from roadmap §4.5 — do not rename or
reorder anything. StatusBar is a thin LVGL wrapper (no native test possible —
LVGL UI; verified by device compile here and on-device in Task 8).

**Roadmap deviation §5.1 (stated here on purpose):** this board has **no
battery-voltage ADC** (the IP5603 is charge/boost only), so the spec §3.3
battery % and §6.4 low-battery warn/dim are waived. `setBatteryText` exists as
a hook for a future board revision and **renders nothing while the text is
empty** — F2 never sets it.

**Files:**
- Create: `src/core/App.h`
- Create: `src/core/StatusBar.h`
- Create: `src/core/StatusBar.cpp`

**Interfaces:**
- Consumes: `lvgl.h` (F1's LVGL 8.4 setup).
- Produces:
  - `enum class RadioMode : uint8_t { None, WiFi, Bluetooth };` and the `App`
    interface, exactly as roadmap §4.5 (Tasks 4/5/6 and every later plan consume).
  - `StatusBar::kHeight` (= 24), `void build(lv_obj_t* parent, std::function<void()> onGear)`,
    `void setClockText(const char* text)`, `void setRadio(RadioMode mode)`,
    `void setBatteryText(const char* text)` (Task 4's Launcher and F4/F5 consume).

- [ ] **Step 1: Create `src/core/App.h` (verbatim from roadmap §4.5)**

```cpp
// src/core/App.h
#pragma once
#include <lvgl.h>

enum class RadioMode : uint8_t { None, WiFi, Bluetooth };

class App {
public:
  virtual ~App() = default;
  virtual const char* id() const = 0;            // stable key: "weather", "music",
                                                 // "calc", "oracle", "pet", "settings"
  virtual const char* title() const = 0;         // label under launcher icon
  virtual const char* iconPath() const = 0;      // "S:/art/icons/<id>.bin", or
                                                 // nullptr → launcher draws fallback
  virtual RadioMode requiredRadio() const = 0;
  virtual void onEnter() = 0;                    // called before buildUI
  virtual void buildUI(lv_obj_t* parent) = 0;    // build widgets into parent
  virtual void onExit() = 0;                     // launcher deletes widgets AFTER this
  virtual void tick(uint32_t now_ms) {}          // called every loop while active
};
```

- [ ] **Step 2: Create `src/core/StatusBar.h`**

```cpp
// src/core/StatusBar.h — 24 px top strip on the home screen (roadmap §4.5).
// Clock text ("--:--" until F4 wires TimeService), radio glyph, gear → Settings.
// Battery: roadmap deviation §5.1 — no battery ADC on this board; the
// setBatteryText hook renders nothing while its text is empty.
#pragma once

#include <lvgl.h>

#include <functional>

#include "App.h"  // RadioMode

class StatusBar {
 public:
  static constexpr lv_coord_t kHeight = 24;

  // Builds the bar as a child of `parent` (the launcher's home screen).
  // `onGear` is invoked when the gear button is tapped.
  void build(lv_obj_t* parent, std::function<void()> onGear);

  void setClockText(const char* text);    // F4 wires TimeService::hhmm here
  void setRadio(RadioMode mode);          // None → blank, WiFi/BT → LVGL symbol
  void setBatteryText(const char* text);  // "" (default) renders nothing — §5.1

 private:
  static void onGearClicked(lv_event_t* e);

  lv_obj_t* bar_ = nullptr;
  lv_obj_t* clockLabel_ = nullptr;
  lv_obj_t* radioLabel_ = nullptr;
  lv_obj_t* batteryLabel_ = nullptr;
  std::function<void()> onGear_;
};
```

- [ ] **Step 3: Create `src/core/StatusBar.cpp`**

```cpp
#include "core/StatusBar.h"

void StatusBar::build(lv_obj_t* parent, std::function<void()> onGear) {
  onGear_ = std::move(onGear);

  bar_ = lv_obj_create(parent);
  lv_obj_remove_style_all(bar_);
  lv_obj_set_size(bar_, 240, kHeight);
  lv_obj_set_pos(bar_, 0, 0);
  lv_obj_set_style_bg_color(bar_, lv_color_hex(0x1B2026), 0);
  lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, 0);
  lv_obj_clear_flag(bar_, LV_OBJ_FLAG_SCROLLABLE);

  clockLabel_ = lv_label_create(bar_);
  lv_label_set_text(clockLabel_, "--:--");  // F4 replaces via setClockText
  lv_obj_align(clockLabel_, LV_ALIGN_LEFT_MID, 6, 0);

  radioLabel_ = lv_label_create(bar_);
  lv_label_set_text(radioLabel_, "");  // RadioMode::None → blank
  lv_obj_align(radioLabel_, LV_ALIGN_RIGHT_MID, -64, 0);

  batteryLabel_ = lv_label_create(bar_);
  lv_label_set_text(batteryLabel_, "");  // §5.1: stays empty on this board
  lv_obj_align(batteryLabel_, LV_ALIGN_RIGHT_MID, -38, 0);

  lv_obj_t* gearBtn = lv_btn_create(bar_);
  lv_obj_set_size(gearBtn, 28, 20);
  lv_obj_align(gearBtn, LV_ALIGN_RIGHT_MID, -2, 0);
  lv_obj_set_ext_click_area(gearBtn, 8);  // 24 px bar → widen the touch target
  lv_obj_t* gearLabel = lv_label_create(gearBtn);
  lv_label_set_text(gearLabel, LV_SYMBOL_SETTINGS);
  lv_obj_center(gearLabel);
  lv_obj_add_event_cb(gearBtn, onGearClicked, LV_EVENT_CLICKED, this);
}

void StatusBar::setClockText(const char* text) {
  if (clockLabel_) lv_label_set_text(clockLabel_, text);
}

void StatusBar::setRadio(RadioMode mode) {
  if (!radioLabel_) return;
  switch (mode) {
    case RadioMode::WiFi:
      lv_label_set_text(radioLabel_, LV_SYMBOL_WIFI);
      break;
    case RadioMode::Bluetooth:
      lv_label_set_text(radioLabel_, LV_SYMBOL_BLUETOOTH);
      break;
    case RadioMode::None:
      lv_label_set_text(radioLabel_, "");
      break;
  }
}

void StatusBar::setBatteryText(const char* text) {
  if (batteryLabel_) lv_label_set_text(batteryLabel_, text);
}

void StatusBar::onGearClicked(lv_event_t* e) {
  auto* self = static_cast<StatusBar*>(lv_event_get_user_data(e));
  if (self->onGear_) self->onGear_();
}
```

- [ ] **Step 4: Compile check (PlatformIO builds every `src/**/*.cpp`)**

Run: `pio run -e cyd`
Expected: ends with `[SUCCESS]` (StatusBar.cpp compiles, which also
type-checks App.h).

- [ ] **Step 5: Commit**

```bash
git add src/core/App.h src/core/StatusBar.h src/core/StatusBar.cpp
git commit -m "feat: app interface (roadmap 4.5) and home status bar"
```

---

### Task 4: `src/core/Launcher.{h,cpp}` — grid + lifecycle wrapper

The Launcher's **public API is roadmap §4.5 verbatim** (plus the constructor,
which the roadmap leaves open). It wraps `LauncherModel` for all bookkeeping.
Lifecycle order is pinned: `openApp` = radioRequest → `onEnter` →
`buildUI(container)`; `goHome` = `onExit` → delete container children → home
screen → radioRequest(None). No native test possible (LVGL UI); verified by
device compile here and on-device in Task 8 — all decision logic it depends on
is already natively tested in Tasks 1–2.

**Files:**
- Create: `src/core/Launcher.h`
- Create: `src/core/Launcher.cpp`

**Interfaces:**
- Consumes:
  - `App`, `RadioMode` from `src/core/App.h` (Task 3).
  - `StatusBar` from `src/core/StatusBar.h` (Task 3): `kHeight`, `build`, `setRadio`.
  - `LauncherModel`, `GridSlot` from `lib/launcher_model/launcher_model.h` (Tasks 1–2).
- Produces (roadmap §4.5 — Task 6's `main.cpp`, F3 (`setAppEnabled`), F4
  (`setRadioRequest`), A5 (`setBadge`) consume):
  - `Launcher(StatusBar& statusBar)`
  - `void registerApp(App* app);` — call order = grid order; id "settings" gets
    no grid icon (gear opens it).
  - `void show();`
  - `void openApp(const char* id);`
  - `void goHome();`
  - `void setBadge(const char* appId, bool on);`
  - `void setAppEnabled(const char* appId, bool en);`
  - `void setRadioRequest(std::function<bool(RadioMode)> fn);` — unset ⇒ always true.
  - `void tick(uint32_t now_ms);` — forwards to the active app.

- [ ] **Step 1: Create `src/core/Launcher.h`**

```cpp
// src/core/Launcher.h — home grid + app lifecycle (roadmap §4.5).
// Public API is the roadmap contract verbatim; do not rename methods.
#pragma once

#include <lvgl.h>

#include <functional>
#include <memory>
#include <vector>

#include "core/App.h"
#include "core/StatusBar.h"
#include "launcher_model.h"

class Launcher {
 public:
  explicit Launcher(StatusBar& statusBar);

  void registerApp(App* app);                     // call order = grid order
  void show();                                    // build/refresh home screen
  void openApp(const char* id);                   // radio switch + lifecycle
  void goHome();                                  // apps call this for back/home
  void setBadge(const char* appId, bool on);      // red dot on an app icon
  void setAppEnabled(const char* appId, bool en); // greyed icon; tap → hint msgbox
  void setRadioRequest(std::function<bool(RadioMode)> fn);
                                                  // unset → treated as always-true;
                                                  // F4 wires RadioManager::request
  void tick(uint32_t now_ms);                     // forwards to active app

 private:
  struct IconCtx {
    Launcher* self;
    const char* appId;
  };

  static constexpr lv_coord_t kTopBarH = 32;  // app-screen back bar height

  void buildHomeScreen();
  void rebuildGrid();
  void buildAppScreen();
  void showDisabledHint(const char* title);
  static void onIconClicked(lv_event_t* e);
  static void onBackClicked(lv_event_t* e);

  StatusBar& statusBar_;
  LauncherModel model_{3};                 // 3-column grid
  std::vector<App*> apps_;                 // index == model_ registration index
  App* active_ = nullptr;
  std::function<bool(RadioMode)> radioRequest_;

  lv_obj_t* homeScreen_ = nullptr;
  lv_obj_t* gridContainer_ = nullptr;
  lv_obj_t* appScreen_ = nullptr;
  lv_obj_t* appTitleLabel_ = nullptr;
  lv_obj_t* appContainer_ = nullptr;       // parent passed to App::buildUI
  std::vector<lv_obj_t*> cells_;           // index = grid index
  std::vector<lv_obj_t*> badges_;          // index = grid index
  std::vector<std::unique_ptr<IconCtx>> iconCtxs_;  // stable addrs for LVGL cbs
};
```

- [ ] **Step 2: Create `src/core/Launcher.cpp`**

```cpp
#include "core/Launcher.h"

#include <cctype>
#include <cstring>

namespace {
constexpr lv_coord_t kCellW = 80;
constexpr lv_coord_t kCellH = 110;
constexpr lv_coord_t kIconSize = 64;
// Fallback icon colors, indexed by grid position (art arrives with F3).
constexpr uint32_t kIconColors[] = {0x4A90D9, 0x50B86C, 0xE0A030,
                                    0x9B59B6, 0xE05050, 0x6C7A89};
constexpr int kIconColorCount = 6;
}  // namespace

Launcher::Launcher(StatusBar& statusBar) : statusBar_(statusBar) {}

void Launcher::registerApp(App* app) {
  const bool inGrid = std::strcmp(app->id(), "settings") != 0;
  if (model_.registerApp(app->id(), inGrid) < 0) return;  // duplicate id: ignore
  apps_.push_back(app);
}

void Launcher::show() {
  if (!homeScreen_) buildHomeScreen();
  rebuildGrid();
  lv_scr_load(homeScreen_);
}

void Launcher::openApp(const char* id) {
  const int idx = model_.indexOf(id);
  if (idx < 0) return;
  if (active_) goHome();  // defensive: close whatever is open first
  App* app = apps_[static_cast<size_t>(idx)];
  if (!model_.canOpen(id)) {
    showDisabledHint(app->title());
    return;
  }
  // Pinned lifecycle: radioRequest → onEnter → buildUI(container).
  const RadioMode mode = app->requiredRadio();
  const bool granted = radioRequest_ ? radioRequest_(mode) : true;
  if (!granted) {
    lv_obj_t* mbox =
        lv_msgbox_create(NULL, app->title(), "Radio unavailable right now.", NULL, true);
    lv_obj_set_width(mbox, 220);
    lv_obj_center(mbox);
    return;
  }
  statusBar_.setRadio(mode);
  app->onEnter();
  buildAppScreen();
  lv_label_set_text(appTitleLabel_, app->title());
  app->buildUI(appContainer_);
  lv_scr_load(appScreen_);
  active_ = app;
}

void Launcher::goHome() {
  // Pinned lifecycle: onExit → delete container children → home → radio None.
  if (active_) {
    active_->onExit();
    lv_obj_clean(appContainer_);
    active_ = nullptr;
  }
  if (!homeScreen_) buildHomeScreen();
  lv_scr_load(homeScreen_);
  if (radioRequest_) radioRequest_(RadioMode::None);
  statusBar_.setRadio(RadioMode::None);
}

void Launcher::setBadge(const char* appId, bool on) {
  if (!model_.setBadge(appId, on)) return;
  const int g = model_.gridIndexOf(appId);
  if (g < 0 || static_cast<size_t>(g) >= badges_.size()) return;
  if (on) {
    lv_obj_clear_flag(badges_[static_cast<size_t>(g)], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(badges_[static_cast<size_t>(g)], LV_OBJ_FLAG_HIDDEN);
  }
}

void Launcher::setAppEnabled(const char* appId, bool en) {
  if (!model_.setEnabled(appId, en)) return;
  const int g = model_.gridIndexOf(appId);
  if (g < 0 || static_cast<size_t>(g) >= cells_.size()) return;
  lv_obj_set_style_opa(cells_[static_cast<size_t>(g)], en ? LV_OPA_COVER : LV_OPA_40, 0);
}

void Launcher::setRadioRequest(std::function<bool(RadioMode)> fn) {
  radioRequest_ = std::move(fn);
}

void Launcher::tick(uint32_t now_ms) {
  if (active_) active_->tick(now_ms);
}

void Launcher::buildHomeScreen() {
  homeScreen_ = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(homeScreen_, lv_color_hex(0x101418), 0);
  lv_obj_set_style_text_color(homeScreen_, lv_color_white(), 0);

  statusBar_.build(homeScreen_, [this] { openApp("settings"); });

  gridContainer_ = lv_obj_create(homeScreen_);
  lv_obj_remove_style_all(gridContainer_);
  lv_obj_set_pos(gridContainer_, 0, StatusBar::kHeight);
  lv_obj_set_size(gridContainer_, 240, 320 - StatusBar::kHeight);
  lv_obj_clear_flag(gridContainer_, LV_OBJ_FLAG_SCROLLABLE);
}

void Launcher::rebuildGrid() {
  lv_obj_clean(gridContainer_);
  cells_.clear();
  badges_.clear();
  iconCtxs_.clear();

  for (int g = 0; g < model_.gridCount(); ++g) {
    App* app = apps_[static_cast<size_t>(model_.indexOf(model_.idAtGrid(g)))];
    const GridSlot slot = model_.slotOf(g);

    lv_obj_t* cell = lv_obj_create(gridContainer_);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, kCellW, kCellH);
    lv_obj_set_pos(cell, slot.col * kCellW, slot.row * kCellH);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn = lv_btn_create(cell);
    lv_obj_set_size(btn, kIconSize, kIconSize);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 8);
    if (app->iconPath() != nullptr) {
      // F3+: hand-drawn icon from SD via the LVGL FS driver (drive 'S').
      lv_obj_t* img = lv_img_create(btn);
      lv_img_set_src(img, app->iconPath());
      lv_obj_center(img);
    } else {
      // Fallback: colored rounded box + first letter (art/SD arrive in F3).
      lv_obj_set_style_bg_color(btn, lv_color_hex(kIconColors[g % kIconColorCount]), 0);
      lv_obj_set_style_radius(btn, 12, 0);
      lv_obj_t* letter = lv_label_create(btn);
      lv_label_set_text_fmt(letter, "%c",
                            std::toupper(static_cast<unsigned char>(app->title()[0])));
      lv_obj_center(letter);
    }
    auto ctx = std::make_unique<IconCtx>(IconCtx{this, app->id()});
    lv_obj_add_event_cb(btn, onIconClicked, LV_EVENT_CLICKED, ctx.get());
    iconCtxs_.push_back(std::move(ctx));

    lv_obj_t* title = lv_label_create(cell);
    lv_label_set_text(title, app->title());
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t* badge = lv_obj_create(btn);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 12, 12);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 4, -4);
    if (!model_.badgeAtGrid(g)) lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    badges_.push_back(badge);

    if (!model_.enabledAtGrid(g)) lv_obj_set_style_opa(cell, LV_OPA_40, 0);
    cells_.push_back(cell);
  }
}

void Launcher::buildAppScreen() {
  if (appScreen_) return;
  appScreen_ = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(appScreen_, lv_color_hex(0x101418), 0);
  lv_obj_set_style_text_color(appScreen_, lv_color_white(), 0);

  // Top bar with back arrow — provided by the Launcher; apps build below it.
  lv_obj_t* topBar = lv_obj_create(appScreen_);
  lv_obj_remove_style_all(topBar);
  lv_obj_set_size(topBar, 240, kTopBarH);
  lv_obj_set_pos(topBar, 0, 0);
  lv_obj_set_style_bg_color(topBar, lv_color_hex(0x1B2026), 0);
  lv_obj_set_style_bg_opa(topBar, LV_OPA_COVER, 0);
  lv_obj_clear_flag(topBar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* backBtn = lv_btn_create(topBar);
  lv_obj_set_size(backBtn, 40, 28);
  lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 2, 0);
  lv_obj_set_ext_click_area(backBtn, 6);
  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT);
  lv_obj_center(backLabel);
  lv_obj_add_event_cb(backBtn, onBackClicked, LV_EVENT_CLICKED, this);

  appTitleLabel_ = lv_label_create(topBar);
  lv_obj_align(appTitleLabel_, LV_ALIGN_CENTER, 0, 0);

  appContainer_ = lv_obj_create(appScreen_);
  lv_obj_remove_style_all(appContainer_);
  lv_obj_set_pos(appContainer_, 0, kTopBarH);
  lv_obj_set_size(appContainer_, 240, 320 - kTopBarH);
}

void Launcher::showDisabledHint(const char* title) {
  lv_obj_t* mbox = lv_msgbox_create(
      NULL, title, "Unavailable — insert the SD card and reboot.", NULL, true);
  lv_obj_set_width(mbox, 220);
  lv_obj_center(mbox);
}

void Launcher::onIconClicked(lv_event_t* e) {
  auto* ctx = static_cast<IconCtx*>(lv_event_get_user_data(e));
  ctx->self->openApp(ctx->appId);
}

void Launcher::onBackClicked(lv_event_t* e) {
  static_cast<Launcher*>(lv_event_get_user_data(e))->goHome();
}
```

- [ ] **Step 3: Compile check**

Run: `pio run -e cyd`
Expected: ends with `[SUCCESS]`. (PlatformIO's LDF pulls `lib/launcher_model`
in via the `#include "launcher_model.h"` chain. If it reports
`launcher_model.h: No such file`, add `lib_ldf_mode = deep` to `[env:cyd]` —
should not be needed with default chain mode.)

- [ ] **Step 4: Commit**

```bash
git add src/core/Launcher.h src/core/Launcher.cpp
git commit -m "feat: launcher grid and app lifecycle"
```

---

### Task 5: Settings shell + StubApp

`SettingsApp` (id `"settings"`, `RadioMode::None` — roadmap §4.11) shows an
`lv_list` with a placeholder row; later plans append section files at the
marked registration point. `StubApp` is a header-only placeholder registered
five times in Task 6; each app plan (A1–A5) replaces its stub registration in
`main.cpp`. No native test (LVGL UI); compile check + Task 8.

**Files:**
- Create: `src/apps/settings/SettingsApp.h`
- Create: `src/apps/settings/SettingsApp.cpp`
- Create: `src/apps/StubApp.h`

**Interfaces:**
- Consumes: `App`, `RadioMode` from `src/core/App.h` (Task 3).
- Produces:
  - `class SettingsApp : public App` — id `"settings"`, title `"Settings"`,
    `iconPath() == nullptr`, `requiredRadio() == RadioMode::None`. F3/F4/F5/A3
    append `buildSection(lv_obj_t* parent, /* deps */)` calls inside
    `SettingsApp::buildUI` at the marked registration point (roadmap §4.11).
  - `class StubApp : public App` — `StubApp(const char* id, const char* title)`,
    `iconPath() == nullptr`, `requiredRadio() == RadioMode::None`.

- [ ] **Step 1: Create `src/apps/settings/SettingsApp.h`**

```cpp
// src/apps/settings/SettingsApp.h — Settings shell (roadmap §4.11).
// F2 ships only the shell; sections are appended by later plans.
#pragma once

#include "core/App.h"

class SettingsApp : public App {
 public:
  const char* id() const override { return "settings"; }
  const char* title() const override { return "Settings"; }
  const char* iconPath() const override { return nullptr; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override {}
};
```

- [ ] **Step 2: Create `src/apps/settings/SettingsApp.cpp`**

```cpp
#include "apps/settings/SettingsApp.h"

void SettingsApp::buildUI(lv_obj_t* parent) {
  lv_obj_t* list = lv_list_create(parent);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));

  // ================= SECTION REGISTRATION POINT =================
  // Later plans append their section here, one file per section
  // (roadmap §4.11), each exposing:
  //   void buildSection(lv_obj_t* parent, /* deps by reference */);
  //
  //   DisplaySection.cpp, UnitsSection.cpp, AboutSection.cpp   (F3)
  //   WifiSection.cpp, ClockSection.cpp                        (F4)
  //   BluetoothSection.cpp                                     (F5)
  //   WeatherLocationSection.cpp                               (A3)
  //
  // Delete the placeholder row below when the first real section lands.
  // ===============================================================
  lv_list_add_text(list, "No settings yet");
}
```

- [ ] **Step 3: Create `src/apps/StubApp.h`**

```cpp
// src/apps/StubApp.h — header-only placeholder app. Registered five times in
// main.cpp ("weather", "music", "calc", "oracle", "pet"); each real app plan
// (A1–A5) replaces its stub registration with the real App.
#pragma once

#include "core/App.h"

class StubApp : public App {
 public:
  StubApp(const char* id, const char* title) : id_(id), title_(title) {}

  const char* id() const override { return id_; }
  const char* title() const override { return title_; }
  const char* iconPath() const override { return nullptr; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text_fmt(label, "%s\ncoming soon", title_);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
  }
  void onExit() override {}

 private:
  const char* id_;
  const char* title_;
};
```

- [ ] **Step 4: Compile check**

Run: `pio run -e cyd`
Expected: ends with `[SUCCESS]` (SettingsApp.cpp compiles; StubApp.h is
type-checked when `main.cpp` includes it in Task 6).

- [ ] **Step 5: Commit**

```bash
git add src/apps/settings/SettingsApp.h src/apps/settings/SettingsApp.cpp src/apps/StubApp.h
git commit -m "feat: settings shell and stub apps"
```

---

### Task 6: Rewrite `src/main.cpp` — boot flow + main loop

Replaces F1's demo `main.cpp` entirely. Boot: init services → build StatusBar +
Launcher → register 5 stubs + settings → `launcher.show()`. Loop (spec §3.5):
display tick + launcher tick. `setRadioRequest` is deliberately **not** called
(unset ⇒ always true) — F4 wires `RadioManager::request` here.

**Files:**
- Modify: `src/main.cpp` (full replacement)

**Interfaces:**
- Consumes:
  - `DisplayService`/`TouchService` (F1 — see the adaptation note at the top of
    this plan; only the `// F1 API` lines may change).
  - `StatusBar` (Task 3), `Launcher` (Task 4), `SettingsApp`/`StubApp` (Task 5).
- Produces: the boot flow every later plan extends (F3 inserts storage/settings
  init before `launcher.show()`; F4 inserts radio/time init and
  `launcher.setRadioRequest(...)`; A1–A5 swap stub registrations).

- [ ] **Step 1: Replace the entire contents of `src/main.cpp`**

```cpp
// src/main.cpp — danios boot flow + main loop (spec §3.4/§3.5, F2 slice).
// F3 adds storage/settings init; F4 adds radio/time init + setRadioRequest;
// each app plan (A1–A5) replaces its StubApp registration with the real app.
#include <Arduino.h>
#include <lvgl.h>

#include "apps/StubApp.h"
#include "apps/settings/SettingsApp.h"
#include "core/Launcher.h"
#include "core/StatusBar.h"
#include "services/DisplayService.h"  // F1 API
#include "services/TouchService.h"    // F1 API

static DisplayService displayService;
static TouchService touchService;
static StatusBar statusBar;
static Launcher launcher(statusBar);

// Grid order = registration order (roadmap §4.5); ids pinned by App::id() docs.
static StubApp weatherStub("weather", "Weather");
static StubApp musicStub("music", "Music");
static StubApp calcStub("calc", "Calc");
static StubApp oracleStub("oracle", "Oracle");
static StubApp petStub("pet", "Pet");
static SettingsApp settingsApp;

void setup() {
  Serial.begin(115200);

  displayService.begin();  // F1 API: panel + LVGL + flush binding
  touchService.begin();    // F1 API: LVGL pointer indev

  launcher.registerApp(&weatherStub);
  launcher.registerApp(&musicStub);
  launcher.registerApp(&calcStub);
  launcher.registerApp(&oracleStub);
  launcher.registerApp(&petStub);
  launcher.registerApp(&settingsApp);  // off-grid; opened via the gear button

  launcher.show();
  Serial.println("danios: launcher up");
}

void loop() {
  displayService.tick();     // F1 API: LVGL tick + lv_timer_handler
  launcher.tick(millis());   // forwards to the active app
  delay(5);
}
```

- [ ] **Step 2: Build**

Run: `pio run -e cyd`
Expected: ends with `[SUCCESS]`. If the `// F1 API` lines fail to compile
because F1 named its methods differently, adapt **only those four lines** to
F1's actual names — nothing else.

- [ ] **Step 3: Run native tests (regression gate)**

Run: `pio test -e native`
Expected: all tests `PASSED` (6 launcher_model cases + F1's tests).

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire launcher, status bar, settings shell and stubs into main loop"
```

---

### Task 7: README — five apps including Pet (roadmap deviation §5.3)

The README predates the Pet app (spec §4.5) and says "four apps" / "4 app
icons". Roadmap §5.3: the spec's five apps are authoritative and F2 fixes this.

**Files:**
- Modify: `README.md` (lines 4–5 and the SD-layout block around line 84)

**Interfaces:**
- Consumes: nothing. Produces: nothing (docs only).

- [ ] **Step 1: Update the intro paragraph**

In `README.md`, replace:

```markdown
("Cheap Yellow Display"). Home-screen launcher with four apps — **Weather,
Music, Calculator, Oracle** — plus a Settings screen. Personal gift project.
```

with:

```markdown
("Cheap Yellow Display"). Home-screen launcher with five apps — **Weather,
Music, Calculator, Oracle, Pet** — plus a Settings screen. Personal gift project.
```

- [ ] **Step 2: Update the SD-layout block to match spec §6.3 / roadmap §4.1**

In `README.md`, replace:

```
/music/*.mp3           ← songs
/oracle/wisdom.txt     ← one wisdom entry per line
/art/weather/          ← outfits, condition overlays, backgrounds
/art/icons/            ← 4 app icons + gear
/art/oracle/           ← oracle frame
```

with:

```
/music/*.mp3           ← songs
/oracle/wisdom.txt     ← one wisdom entry per line
/art/weather/          ← outfits, condition overlays, backgrounds
/art/icons/            ← 5 app icons + gear
/art/oracle/           ← oracle frame
/art/pet/              ← egg/baby/child/teen/adult sprites, food icons, mess icon
```

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: five-app list incl. Pet (roadmap deviation 5.3)"
```

---

### Task 8: On-device verification (**needs the CYD board**)

Manual verification of the F2 E2E outcome. **Board required:
ESP32-2432S024C.** If only the bare devkit is on hand (see `docs/hardware.md`),
Tasks 0–7 are complete and this task stays open until the CYD arrives.

**Files:**
- Modify (temporarily, then revert): `src/main.cpp`

**Interfaces:**
- Consumes: everything produced by Tasks 1–6.
- Produces: the verified F2 E2E outcome.

- [ ] **Step 1: Flash**

Run: `pio run -e cyd -t upload` (board on `/dev/ttyUSB0`)
Expected: `[SUCCESS]`; serial monitor (`pio device monitor`) prints
`danios: launcher up`.

- [ ] **Step 2: Verify the home screen (portrait, USB-C down)**

Expected observations:
- 24 px dark status bar on top: `--:--` at the left, **no radio glyph**, **no
  battery text** (deviation §5.1), gear button at the right.
- Five colored rounded icons with a letter each and titles below, in order:
  row 1 = Weather, Music, Calc; row 2 = Oracle, Pet. No sixth (Settings) icon.

- [ ] **Step 3: Navigate home → each stub → back (×5)**

Tap each of the five icons in turn. Expected for every one:
- Screen switches to an app screen: 32 px top bar with a back arrow (left) and
  the app title (center), body shows "<Title>\ncoming soon" centered.
- Tapping the back arrow returns to the home screen with the grid intact.

- [ ] **Step 4: Gear → Settings shell → back**

Tap the gear in the status bar. Expected: app screen titled "Settings" with a
list containing the single row "No settings yet". Back arrow returns home.

- [ ] **Step 5: Badge + disabled-icon check (temporary test lines)**

In `src/main.cpp`, add directly after `launcher.show();`:

```cpp
  launcher.setBadge("pet", true);          // TEMP F2 verification — remove
  launcher.setAppEnabled("music", false);  // TEMP F2 verification — remove
```

Run: `pio run -e cyd -t upload`
Expected: red dot on the top-right of the Pet icon; Music cell greyed
(faded); tapping Music opens a message box "Unavailable — insert the SD card
and reboot." with a close button, and does **not** open the stub.

- [ ] **Step 6: Remove the temporary lines and re-flash**

Delete both `TEMP F2 verification` lines from `src/main.cpp`.

Run: `pio run -e cyd -t upload`
Expected: badge gone, Music icon normal and opens its stub again.

Run: `git status`
Expected: `working tree clean` (the temp lines were never committed).

---

## Definition of done (roadmap §6)

- [ ] Native tests green: `pio test -e native` → all cases `PASSED`
      (includes the 6 `test_launcher_model` cases).
- [ ] Device build green: `pio run -e cyd` → `[SUCCESS]`.
- [ ] F2 E2E outcome observed on hardware (roadmap §1): **navigate launcher →
      stub app → back; gear opens empty Settings** (Task 8, CYD board).
- [ ] Badge and disabled-icon paths demonstrated on hardware (Task 8 Step 5)
      and the temporary lines removed (Step 6).
- [ ] README lists five apps including Pet (deviation §5.3).
- [ ] All interface names match roadmap §4.5 verbatim (`App`, `RadioMode`,
      `Launcher` public methods, `StatusBar` setters).
