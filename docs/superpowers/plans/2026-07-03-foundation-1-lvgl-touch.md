# Foundation 1 — LVGL + Touch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Roadmap row:** F1 (`docs/superpowers/plans/2026-07-03-danios-roadmap.md` §1)
**Spec sections:** §3.1 Layer 1+2 (Display/Touch services), §8 (testing strategy)

**Goal:** Bind LVGL 8.4 to the proven LovyanGFX display config, add a polled CST820 touch driver feeding an LVGL pointer indev, stand up the native unit-test environment, and git-init the repo — proven end-to-end by an on-screen button whose tap counter increments.

**Architecture:** The existing `include/LGFX_ESP32_2432S024C.hpp` (display bring-up, milestone 1 — **done, do not modify**) is wrapped by a new `DisplayService` that owns the LGFX instance, initializes LVGL with two 240×30 draw buffers, and flushes LVGL's render output into LovyanGFX. A new `TouchService` polls the CST820 over I²C and feeds LVGL as a pointer input device; the raw→screen coordinate mapping is a pure-logic module in `lib/touch_transform/` that is TDD'd on the native env first (this is also the repo's first native test, proving `[env:native]` works). `src/main.cpp`'s throwaway diagnostic is replaced by an LVGL smoke screen (centered "tap me" button + tap counter).

**Tech Stack:** PlatformIO (`espressif32@7.0.1`, Arduino framework), LovyanGFX `^1.2.0`, LVGL `8.4.0` (v8 API), Unity (native tests), C++17.

**Produces for later plans (F2+):** `DisplayService`, `TouchService`, working LVGL at 240×320 rotation 7, `lib/touch_transform/`, the `[env:native]` test environment, and the git repository itself.

**Out of scope:** App framework/Launcher (F2), SD/StorageService (F3), WiFi/BT (F4/F5).

**Hardware prerequisite:** Tasks 0–5 need no hardware beyond a host with PlatformIO Core and a host C++ compiler (native tests). **Task 6 (on-device verification) requires the CYD board (ESP32-2432S024C)** — per `docs/hardware.md`, the board currently on hand is a bare ESP32 devkit with no display/touch, which cannot verify this plan's E2E outcome. If the CYD hasn't arrived, complete Tasks 0–5 (all build- and native-test-verifiable) and leave Task 6 pending.

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
  roadmap spec deviation §5.
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

## Design decision: RGB565 byte order (pinned here, referenced by later plans)

The ILI9341-class panel wants **big-endian** RGB565 on the wire; LVGL renders
**native-endian** (little-endian on ESP32). `docs/hardware.md` lists two valid
glue options. This plan pins the **swap-free** one:

- `lv_conf.h` sets **`LV_COLOR_16_SWAP 0`** — LVGL's internal color format stays
  standard, so converter-generated image assets (F3's `S:` drive `.bin` art) and
  fonts need **no** byte-swapping at conversion time.
- The `DisplayService` flush callback casts LVGL's buffer to `lgfx::rgb565_t*`
  and calls `writePixels()` — LovyanGFX performs the endian conversion during
  the SPI write (its optimized path; no extra buffer, DMA-friendly).

Rejected alternative: `LV_COLOR_16_SWAP 1` + raw `pushPixelsDMA` — it would
require every future image asset to be generated pre-swapped, an easy foot-gun.

Task 3 also adds this decision to `docs/DISPLAY.md`, which explicitly reserves a
section for the LVGL glue lines ("## NOTE" at the end of that file).

---

### Task 0: Initialize the git repository

The repo is **not yet under version control**. Everything after this task
assumes git exists (roadmap §2). No code changes — snapshot the proven
milestone-1 state as the initial commit.

**Files:**
- Create: `.gitignore`
- No source changes.

**Interfaces:**
- Consumes: nothing.
- Produces: a git repository at `/home/lucca/repos/danios` with branch `main`;
  every later task (and every later plan) commits on top of it.

- [ ] **Step 1: Initialize the repository**

Run:
```bash
cd /home/lucca/repos/danios && git init -b main
```
Expected: `Initialized empty Git repository in /home/lucca/repos/danios/.git/`

- [ ] **Step 2: Write `.gitignore`**

Create `.gitignore` with exactly:

```gitignore
.pio/
.claude/settings.local.json
```

- [ ] **Step 3: Stage everything and inspect**

Run:
```bash
cd /home/lucca/repos/danios && git add -A && git status --short
```
Expected: staged (`A`) entries including `.gitignore`, `platformio.ini`,
`src/main.cpp`, `include/LGFX_ESP32_2432S024C.hpp`, `README.md`, and the
`docs/` tree. **Must NOT list anything under `.pio/` or
`.claude/settings.local.json`** — if it does, fix `.gitignore` before
committing.

- [ ] **Step 4: Initial commit**

```bash
cd /home/lucca/repos/danios && git commit -m "chore: initial commit — display bring-up (milestone 1) + docs"
```
Expected: `[main (root-commit) <hash>] chore: initial commit — display bring-up (milestone 1) + docs`

---

### Task 1: Toolchain upgrade — platformio.ini envs + `lv_conf.h`

Adds the LVGL 8.4 dependency, C++17 on both envs, the `huge_app.csv` partition
scheme, the `[env:native]` test environment, and the LVGL config header (LVGL
won't compile without it, so it lands in the same task). Also pins the platform
to `espressif32@7.0.1` — per `docs/HANDOVER.md` that is the version the
unpinned `espressif32` already resolved to during display bring-up, so this is
an idempotent pin, not an upgrade.

**Files:**
- Modify: `platformio.ini` (full replacement below)
- Create: `include/lv_conf.h`

**Interfaces:**
- Consumes: nothing.
- Produces: `[env:cyd]` (device build with LVGL 8.4, C++17, huge_app partitions)
  and `[env:native]` (host tests, `test_build_src = false`, C++17) — consumed by
  every subsequent task and plan. `include/lv_conf.h` with `LV_COLOR_16_SWAP 0`
  (consumed by Task 3's flush callback) and `LV_MEM_SIZE (48U * 1024U)`.

- [ ] **Step 1: Replace `platformio.ini`**

Full new content:

```ini
; danios — ESP32-2432S024C ("Cheap Yellow Display", 2.4" capacitive)
; See docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md for the design
; and docs/superpowers/plans/2026-07-03-danios-roadmap.md for the plan contract.

[platformio]
default_envs = cyd

[env:cyd]
platform = espressif32@7.0.1
board = esp32dev            ; ESP32-WROOM-32 (what the S024C is built on)
framework = arduino
monitor_speed = 115200
upload_speed = 460800       ; safe for the CH340; bump to 921600 later if reliable
board_build.partitions = huge_app.csv
build_unflags = -std=gnu++11
build_flags =
    -std=gnu++17
    -DLV_CONF_INCLUDE_SIMPLE
    -Iinclude
lib_deps =
    lovyan03/LovyanGFX@^1.2.0
    lvgl/lvgl@8.4.0

; Host-side unit tests (Unity). Builds only lib/ + test/ — pure std-C++ logic,
; zero Arduino/LVGL includes allowed in anything this env compiles.
[env:native]
platform = native
test_build_src = false
build_unflags = -std=gnu++11
build_flags = -std=gnu++17
```

- [ ] **Step 2: Create `include/lv_conf.h`**

LVGL 8.4 reads this via `-DLV_CONF_INCLUDE_SIMPLE -Iinclude`; anything not
defined here falls back to the `lv_conf_internal.h` default, so the file lists
only deliberate choices. Full content:

```c
/**
 * danios lv_conf.h — LVGL 8.4 configuration for the ESP32-2432S024C.
 *
 * Only deliberate overrides live here; every other option falls back to the
 * LVGL 8.4 default via lv_conf_internal.h. Constraints: 520 KB SRAM, NO PSRAM
 * (roadmap §2) — keep the LVGL heap at 48 KB and features lean.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/*==================== COLOR ====================*/
/* RGB565. LV_COLOR_16_SWAP stays 0: the DisplayService flush callback hands
 * LVGL's buffer to LovyanGFX as lgfx::rgb565_t*, and LovyanGFX converts to the
 * panel's big-endian order during the SPI write (see docs/DISPLAY.md,
 * "LVGL glue"). Image assets stay standard non-swapped RGB565. */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*==================== MEMORY ====================*/
/* LVGL heap from internal SRAM — 48 KB per the roadmap §2 RAM budget. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)

/*==================== HAL ====================*/
/* Tick straight from Arduino millis(): no lv_tick_inc() calls anywhere. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 30   /* ms — screen refresh cadence */
#define LV_INDEV_DEF_READ_PERIOD 30  /* ms — touch poll cadence     */

/*==================== LOGGING / DEBUG ====================*/
#define LV_USE_LOG 0          /* flip to 1 + serial print cb when debugging LVGL */
#define LV_USE_PERF_MONITOR 0 /* flip to 1 to see FPS/CPU overlay */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/*==================== FONTS ====================*/
/* Montserrat 14 (default UI text) + 20 (headings/buttons). More sizes cost
 * flash; enable per-plan as UIs need them. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*==================== WIDGETS ====================*/
/* v8 enables the core set by default. Extras this device will not use stay
 * off (flash savings). Later plans flip individual ones back on if needed
 * (e.g. A5 Pet may want LV_USE_ANIMIMG for sprites).
 *
 * Deliberately KEPT ON (defaults) because later foundation plans rely on
 * them: btn, label, img, list (F2 settings menu), slider (F3 brightness),
 * switch (F3 units), dropdown (F3 sleep), roller (F4 clock), textarea +
 * keyboard (F4 wifi password), btnmatrix (A1 calculator), msgbox (F3
 * disabled-app hint), bar, arc, checkbox, table. */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

#endif /* LV_CONF_H */
```

- [ ] **Step 3: Verify the device env builds with LVGL**

Run:
```bash
cd /home/lucca/repos/danios && pio run -e cyd
```
Expected: first run downloads `lvgl @ 8.4.0` into `.pio/libdeps/cyd/`, then
compiles LVGL and the existing diagnostic `src/main.cpp`, ending with
`========================= [SUCCESS] =========================` and a
`RAM:`/`Flash:` usage summary. (The diagnostic sketch doesn't reference LVGL
yet — this step proves `lv_conf.h` + flags are correct, since LVGL's own
sources compile against them.)

If it fails with `lv_conf.h: No such file or directory`, the
`-DLV_CONF_INCLUDE_SIMPLE -Iinclude` build flags didn't take — re-check Step 1.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add platformio.ini include/lv_conf.h && git commit -m "feat: add LVGL 8.4 + lv_conf, native test env, C++17, huge_app partitions"
```
Expected: `[main <hash>] feat: add LVGL 8.4 + lv_conf, native test env, C++17, huge_app partitions`

---

### Task 2: `lib/touch_transform/` — pure raw→screen coordinate mapping (native TDD)

The CST820 reports **raw landscape-native coordinates (320×240), like the
panel silicon**; the UI runs portrait 240×320 at rotation 7 (swap + both
mirrors — `docs/DISPLAY.md`). This module maps raw→screen, parameterized by raw
dimensions and swap/mirror flags, because per `docs/DISPLAY.md` the actual
flags **must be verified against on-screen targets** (Task 6) and may need
flipping — a parameter change, not a code change. Std C++ only, zero
Arduino/LVGL includes.

This is also the repo's **first native test run**, proving `[env:native]`
works end-to-end.

**Files:**
- Create: `lib/touch_transform/touch_transform.h`
- Create: `lib/touch_transform/touch_transform.cpp`
- Test: `test/test_touch_transform/test_main.cpp`

**Interfaces:**
- Consumes: `[env:native]` from Task 1.
- Produces (consumed by Task 4 `TouchService` via `#include <touch_transform.h>`):
  - `struct TouchPoint { int16_t x; int16_t y; };`
  - `struct TouchTransform { int16_t raw_w; int16_t raw_h; bool swap_xy; bool mirror_x; bool mirror_y; };`
  - `TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y);`
    — clamps raw into `[0, raw_w)×[0, raw_h)`, then swap (if `swap_xy`), then
    mirrors in **output** space. Output space is `raw_h×raw_w` when swapped,
    else `raw_w×raw_h`.

- [ ] **Step 1: Write the failing tests (first TDD cycle: identity + clamping)**

Create `test/test_touch_transform/test_main.cpp`:

```cpp
// Native unit tests for lib/touch_transform — the raw CST820 → screen mapper.
// Raw space is landscape-native 320x240 (like the panel); screen is portrait
// 240x320 at rotation 7 (swap + mirror both). Flags are parameters because
// docs/DISPLAY.md requires verifying them against on-screen targets.
#include <unity.h>
#include <touch_transform.h>

void setUp() {}
void tearDown() {}

static TouchTransform base() {
  TouchTransform t;
  t.raw_w = 320;
  t.raw_h = 240;
  t.swap_xy = false;
  t.mirror_x = false;
  t.mirror_y = false;
  return t;
}

void test_identity_passes_through() {
  const TouchPoint p = transformTouch(base(), 10, 20);
  TEST_ASSERT_EQUAL_INT16(10, p.x);
  TEST_ASSERT_EQUAL_INT16(20, p.y);
}

void test_clamps_negative_raw_to_zero() {
  const TouchPoint p = transformTouch(base(), -5, -1);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_clamps_overrange_raw_to_edge() {
  const TouchPoint p = transformTouch(base(), 999, 400);
  TEST_ASSERT_EQUAL_INT16(319, p.x);  // raw_w - 1
  TEST_ASSERT_EQUAL_INT16(239, p.y);  // raw_h - 1
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_identity_passes_through);
  RUN_TEST(test_clamps_negative_raw_to_zero);
  RUN_TEST(test_clamps_overrange_raw_to_edge);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```bash
cd /home/lucca/repos/danios && pio test -e native
```
Expected: **build error** — `touch_transform.h: No such file or directory`
(the module doesn't exist yet; a compile failure is this cycle's red state).

- [ ] **Step 3: Create the header and a deliberately-wrong stub**

Create `lib/touch_transform/touch_transform.h`:

```cpp
// Raw capacitive-touch → screen coordinate mapping. PURE LOGIC: std C++ only,
// zero Arduino/LVGL includes (roadmap §2 — native-tested with pio test -e native).
//
// The danios CST820 reports raw coordinates in the panel's landscape-native
// 320x240 space; the UI is portrait 240x320 at LovyanGFX rotation 7
// (swap + mirror both). The swap/mirror flags are runtime parameters because
// docs/DISPLAY.md requires verifying them against on-screen targets — a
// mismatch is fixed by flipping a flag in TouchService, not by editing logic.
#pragma once
#include <cstdint>

struct TouchPoint {
  int16_t x;
  int16_t y;
};

struct TouchTransform {
  int16_t raw_w = 0;      // raw coordinate-space width  (danios CST820: 320)
  int16_t raw_h = 0;      // raw coordinate-space height (danios CST820: 240)
  bool swap_xy = false;   // swap axes first (landscape-native -> portrait)
  bool mirror_x = false;  // then mirror across the OUTPUT-space width
  bool mirror_y = false;  // then mirror across the OUTPUT-space height
};

// Clamps (raw_x, raw_y) into [0, raw_w) x [0, raw_h), applies swap then
// mirrors. Output space is raw_h x raw_w when swap_xy is set, else
// raw_w x raw_h — so the result is always a valid on-screen point.
TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y);
```

Create `lib/touch_transform/touch_transform.cpp` (stub — returns a constant so
the tests fail on assertions, proving they actually test something):

```cpp
#include "touch_transform.h"

TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y) {
  (void)t;
  (void)raw_x;
  (void)raw_y;
  return {0, 0};
}
```

- [ ] **Step 4: Run tests to verify they fail on assertions**

Run:
```bash
cd /home/lucca/repos/danios && pio test -e native
```
Expected: compiles, then FAILS — e.g.
`test_main.cpp:...:test_identity_passes_through [FAILED]` with
`Expected 10 Was 0`, and the summary `3 Tests 2 Failures 0 Ignored` (the
clamp-to-zero test passes by coincidence with the `{0, 0}` stub; the other
two fail). Overall result: `[FAILED]`.

- [ ] **Step 5: Implement the real transform**

Replace `lib/touch_transform/touch_transform.cpp` with:

```cpp
#include "touch_transform.h"

namespace {
int16_t clampTo(int16_t v, int16_t max_exclusive) {
  if (v < 0) return 0;
  if (v >= max_exclusive) return static_cast<int16_t>(max_exclusive - 1);
  return v;
}
}  // namespace

TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y) {
  int16_t x = clampTo(raw_x, t.raw_w);
  int16_t y = clampTo(raw_y, t.raw_h);
  int16_t out_w = t.raw_w;
  int16_t out_h = t.raw_h;
  if (t.swap_xy) {
    const int16_t tmp = x;
    x = y;
    y = tmp;
    out_w = t.raw_h;
    out_h = t.raw_w;
  }
  if (t.mirror_x) x = static_cast<int16_t>(out_w - 1 - x);
  if (t.mirror_y) y = static_cast<int16_t>(out_h - 1 - y);
  return {x, y};
}
```

- [ ] **Step 6: Run tests to verify they pass**

Run:
```bash
cd /home/lucca/repos/danios && pio test -e native
```
Expected: `3 Tests 0 Failures 0 Ignored` … `========= [PASSED] =========`
— this also confirms the native env itself works (first green native run in
the repo).

- [ ] **Step 7: Write the failing tests (second TDD cycle: swap, mirrors, rotation 7)**

Add these test functions to `test/test_touch_transform/test_main.cpp` (above
`main`), and the corresponding `RUN_TEST` lines inside `main` before
`return UNITY_END();`:

```cpp
void test_swap_only_transposes_axes() {
  TouchTransform t = base();
  t.swap_xy = true;
  const TouchPoint p = transformTouch(t, 300, 10);
  TEST_ASSERT_EQUAL_INT16(10, p.x);   // output space is 240 wide
  TEST_ASSERT_EQUAL_INT16(300, p.y);  // ...and 320 tall
}

void test_mirror_x_only_flips_across_width() {
  TouchTransform t = base();
  t.mirror_x = true;
  const TouchPoint p = transformTouch(t, 0, 0);
  TEST_ASSERT_EQUAL_INT16(319, p.x);  // out_w (=raw_w, no swap) - 1
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_mirror_y_only_flips_across_height() {
  TouchTransform t = base();
  t.mirror_y = true;
  const TouchPoint p = transformTouch(t, 0, 0);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(239, p.y);  // out_h (=raw_h, no swap) - 1
}

// Rotation 7 = swap + mirror both (MADCTL MV|MX|MY), landscape 320x240 raw
// to portrait 240x320 screen — the danios default configuration.
static TouchTransform rotation7() {
  TouchTransform t = base();
  t.swap_xy = true;
  t.mirror_x = true;
  t.mirror_y = true;
  return t;
}

void test_rotation7_maps_raw_origin_to_screen_bottom_right() {
  const TouchPoint p = transformTouch(rotation7(), 0, 0);
  TEST_ASSERT_EQUAL_INT16(239, p.x);
  TEST_ASSERT_EQUAL_INT16(319, p.y);
}

void test_rotation7_maps_raw_far_corner_to_screen_origin() {
  const TouchPoint p = transformTouch(rotation7(), 319, 239);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_rotation7_maps_raw_center_to_screen_center() {
  const TouchPoint p = transformTouch(rotation7(), 160, 120);
  TEST_ASSERT_EQUAL_INT16(119, p.x);  // 239 - 120
  TEST_ASSERT_EQUAL_INT16(159, p.y);  // 319 - 160
}

void test_rotation7_clamps_before_transforming() {
  const TouchPoint p = transformTouch(rotation7(), 999, -3);
  // clamp -> (319, 0); swap -> (0, 319); mirror -> (239, 0)
  TEST_ASSERT_EQUAL_INT16(239, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}
```

`main` becomes:

```cpp
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_identity_passes_through);
  RUN_TEST(test_clamps_negative_raw_to_zero);
  RUN_TEST(test_clamps_overrange_raw_to_edge);
  RUN_TEST(test_swap_only_transposes_axes);
  RUN_TEST(test_mirror_x_only_flips_across_width);
  RUN_TEST(test_mirror_y_only_flips_across_height);
  RUN_TEST(test_rotation7_maps_raw_origin_to_screen_bottom_right);
  RUN_TEST(test_rotation7_maps_raw_far_corner_to_screen_origin);
  RUN_TEST(test_rotation7_maps_raw_center_to_screen_center);
  RUN_TEST(test_rotation7_clamps_before_transforming);
  return UNITY_END();
}
```

- [ ] **Step 8: Run tests — new ones must pass against the Step-5 implementation**

Run:
```bash
cd /home/lucca/repos/danios && pio test -e native
```
Expected: `10 Tests 0 Failures 0 Ignored` … `[PASSED]`. (The Step-5
implementation already covers swap/mirror; these tests lock the rotation-7
corner semantics that Task 6's on-device corner test depends on. If any fail,
fix `touch_transform.cpp` — the expected values above are the contract.)

- [ ] **Step 9: Commit**

```bash
cd /home/lucca/repos/danios && git add lib/touch_transform test/test_touch_transform && git commit -m "feat: add touch_transform raw->screen mapping (native TDD)"
```
Expected: `[main <hash>] feat: add touch_transform raw->screen mapping (native TDD)`

---

### Task 3: `DisplayService` — LVGL bound to LovyanGFX (+ DISPLAY.md glue note)

Thin service owning the LGFX instance and the LVGL display driver: init,
two 240×30 draw buffers, flush callback, brightness, tick. Hardware-facing —
verification here is a clean device build; behavior is proven on hardware in
Task 6. Also pays the documentation debt `docs/DISPLAY.md` reserved for this
moment (its trailing "## NOTE" section).

**Files:**
- Create: `src/services/DisplayService.h`
- Create: `src/services/DisplayService.cpp`
- Modify: `docs/DISPLAY.md:49-52` (replace the trailing `## NOTE:` section)
- Do **not** touch: `include/LGFX_ESP32_2432S024C.hpp` (proven config — frozen)

**Interfaces:**
- Consumes: `LGFX` from `include/LGFX_ESP32_2432S024C.hpp` (Task 0 initial
  commit); `lv_conf.h` (`LV_COLOR_16_SWAP 0`) from Task 1.
- Produces (consumed by Task 5 `main.cpp`, and by F2+ plans):
  - `class DisplayService` with
    `void begin()` — panel init (rotation 7, brightness 160), `lv_init()`,
    buffers, flush registration; call once before any LVGL call;
    `void setBrightness(uint8_t level)` — backlight PWM 0–255 (F3 drives this
    from NVS `disp.bright`);
    `void tick()` — `lv_timer_handler()`; call every `loop()` iteration.
  - A registered LVGL display: after `begin()`, `lv_scr_act()` etc. are usable.

- [ ] **Step 1: Write `src/services/DisplayService.h`**

```cpp
// LVGL <-> LovyanGFX glue for the danios display. Owns the LGFX instance and
// the LVGL display driver. Read docs/DISPLAY.md before touching this file —
// the panel is a landscape-native 320x240 clone; portrait UI = rotation 7.
#pragma once

#include <lvgl.h>

#include "LGFX_ESP32_2432S024C.hpp"

class DisplayService {
public:
  // Panel init (rotation 7 portrait 240x320, brightness 160), lv_init(),
  // two 240x30 draw buffers, flush-callback registration.
  // Call exactly once from setup(), before any other LVGL call.
  void begin();

  // Backlight PWM, 0-255. F3's Settings->Display drives this from NVS.
  void setBrightness(uint8_t level);

  // Pumps LVGL (lv_timer_handler). Call every loop() iteration, from the
  // Arduino loop task only — LVGL is not thread-safe (roadmap §2).
  void tick();

private:
  static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                      lv_color_t* pixels);

  static constexpr int16_t kHorRes = 240;
  static constexpr int16_t kVerRes = 320;
  static constexpr size_t kBufPixels = 240 * 30;  // per buffer; x2 ~= 28.8 KB

  LGFX tft_;
  lv_disp_draw_buf_t drawBuf_{};
  lv_disp_drv_t dispDrv_{};
  lv_color_t buf1_[kBufPixels];
  lv_color_t buf2_[kBufPixels];
};
```

- [ ] **Step 2: Write `src/services/DisplayService.cpp`**

```cpp
#include "DisplayService.h"

void DisplayService::begin() {
  tft_.init();
  tft_.setRotation(7);    // portrait, USB-C down (240x320) — docs/DISPLAY.md
  tft_.setBrightness(160);

  lv_init();
  lv_disp_draw_buf_init(&drawBuf_, buf1_, buf2_, kBufPixels);
  lv_disp_drv_init(&dispDrv_);
  dispDrv_.hor_res = kHorRes;
  dispDrv_.ver_res = kVerRes;
  dispDrv_.flush_cb = flushCb;
  dispDrv_.draw_buf = &drawBuf_;
  dispDrv_.user_data = this;
  lv_disp_drv_register(&dispDrv_);
}

void DisplayService::setBrightness(uint8_t level) {
  tft_.setBrightness(level);
}

void DisplayService::tick() {
  lv_timer_handler();
}

// Byte order: LV_COLOR_16_SWAP is 0 (lv_conf.h), so LVGL renders
// native-endian RGB565. Casting the buffer to lgfx::rgb565_t* makes
// LovyanGFX convert to the panel's big-endian order during the SPI write —
// the swap-free glue option from docs/hardware.md, pinned in this plan and
// documented in docs/DISPLAY.md "LVGL glue".
void DisplayService::flushCb(lv_disp_drv_t* drv, const lv_area_t* area,
                             lv_color_t* pixels) {
  auto* self = static_cast<DisplayService*>(drv->user_data);
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  self->tft_.startWrite();
  self->tft_.setAddrWindow(area->x1, area->y1, w, h);
  self->tft_.writePixels(reinterpret_cast<lgfx::rgb565_t*>(&pixels->full),
                         static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
  self->tft_.endWrite();
  lv_disp_flush_ready(drv);
}
```

- [ ] **Step 3: Verify the device env still builds**

Run:
```bash
cd /home/lucca/repos/danios && pio run -e cyd
```
Expected: `[SUCCESS]`. (The service compiles and links; `main.cpp` doesn't use
it yet — that's Task 5.) If `writePixels` overload resolution fails, check the
cast is to `lgfx::rgb565_t*` (that exact type is what triggers LovyanGFX's
byte-swapping write path).

- [ ] **Step 4: Update `docs/DISPLAY.md` — replace the reserved NOTE with the real glue lines**

In `docs/DISPLAY.md`, replace the entire trailing section:

```markdown
## NOTE:

when the foundation grows: once LVGL enters the picture, this doc is where its display driver glue (buffer size, flush callback binding to LovyanGFX) should get its few lines
```

with:

```markdown
## LVGL glue (added by F1)

- LVGL **8.4** (v8 API), `include/lv_conf.h`, enabled via
  `-DLV_CONF_INCLUDE_SIMPLE -Iinclude`.
- `src/services/DisplayService.{h,cpp}` owns the `LGFX` instance and the LVGL
  display driver: **two 240×30 draw buffers** (~28.8 KB total, static — no
  PSRAM on this board).
- **Byte order:** `LV_COLOR_16_SWAP 0`. The flush callback casts LVGL's buffer
  to `lgfx::rgb565_t*` and calls `writePixels()` inside
  `startWrite()/setAddrWindow()/endWrite()` — LovyanGFX converts to the
  panel's big-endian RGB565 during the SPI write. Consequence: **image assets
  stay standard (non-swapped) RGB565** in the LVGL image converter.
- Touch: CST820 polled over I²C by `src/services/TouchService.{h,cpp}` and fed
  to LVGL as a pointer indev; raw landscape-native coordinates are mapped by
  `lib/touch_transform/` (swap + mirror flags **verified against on-screen
  targets**, per the gotcha above).
```

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/DisplayService.h src/services/DisplayService.cpp docs/DISPLAY.md && git commit -m "feat: add DisplayService — LVGL 8.4 bound to LovyanGFX, glue documented"
```
Expected: `[main <hash>] feat: add DisplayService — LVGL 8.4 bound to LovyanGFX, glue documented`

---

### Task 4: `TouchService` — CST820 polling driver → LVGL pointer indev

Polls the CST820 over I²C (SDA 33, SCL 32, RST 25, addr 0x15, 400 kHz).
**Polling, never the INT pin** — per roadmap §2 / `docs/hardware.md`, INT is on
GPIO 21 on some units and 22 on others; polling sidesteps it entirely. The chip
sleeps: RST must be pulsed and ~300 ms waited before the first I²C access.
Raw coordinates go through `touch_transform` (Task 2) before reaching LVGL.

**Files:**
- Create: `src/services/TouchService.h`
- Create: `src/services/TouchService.cpp`

**Interfaces:**
- Consumes: `TouchTransform`, `TouchPoint`,
  `transformTouch(const TouchTransform&, int16_t, int16_t)` from Task 2
  (via `#include <touch_transform.h>`); a registered LVGL display from Task 3
  (`lv_indev_drv_register` requires a display to exist).
- Produces (consumed by Task 5 `main.cpp`; F2+ never touch it directly — input
  flows through LVGL):
  - `class TouchService` with `void begin()` — reset pulse + wake wait, I²C
    init, LVGL pointer-indev registration. Call after `DisplayService::begin()`.

- [ ] **Step 1: Write `src/services/TouchService.h`**

```cpp
// CST820 capacitive touch -> LVGL pointer indev.
// POLLED over I2C: the INT line is GPIO 21 on some units and 22 on others
// (docs/hardware.md) — do not use it. Raw coordinates are landscape-native
// like the panel and are mapped to portrait rotation-7 screen space by
// lib/touch_transform. The swap/mirror flags below are VERIFIED ON-SCREEN in
// this plan's final task (docs/DISPLAY.md: never trust board-def touch flags).
#pragma once

#include <lvgl.h>
#include <touch_transform.h>

class TouchService {
public:
  // CST820 reset pulse + ~300 ms wake wait, I2C init (SDA 33, SCL 32,
  // 400 kHz), auto-sleep disable, LVGL pointer-indev registration.
  // Call exactly once, after DisplayService::begin().
  void begin();

private:
  static void readCb(lv_indev_drv_t* drv, lv_indev_data_t* data);
  bool readRaw(int16_t& raw_x, int16_t& raw_y);  // true while a finger is down
  void writeRegister(uint8_t reg, uint8_t value);

  TouchTransform transform_{};
  lv_indev_drv_t indevDrv_{};
  lv_indev_t* indev_ = nullptr;
  int16_t lastX_ = 0;  // LVGL wants the last point retained on release
  int16_t lastY_ = 0;
  bool wasPressed_ = false;
};
```

- [ ] **Step 2: Write `src/services/TouchService.cpp`**

```cpp
#include "TouchService.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t kAddr = 0x15;
constexpr int kPinSda = 33;
constexpr int kPinScl = 32;
constexpr int kPinRst = 25;
constexpr uint32_t kI2cHz = 400000;

// CST820 register map (register-compatible with CST816S basic reads —
// docs/hardware.md). We burst-read 6 bytes starting at 0x01:
//   0x01 GestureID   (unused — we poll raw points, no gesture handling)
//   0x02 FingerNum   (0 = no touch)
//   0x03 XposH       (bits 3:0 = X[11:8])
//   0x04 XposL       (X[7:0])
//   0x05 YposH       (bits 3:0 = Y[11:8])
//   0x06 YposL       (Y[7:0])
constexpr uint8_t kRegGesture = 0x01;
constexpr uint8_t kRegDisAutoSleep = 0xFE;  // non-zero disables auto-sleep
}  // namespace

void TouchService::begin() {
  // Raw space is landscape-native like the panel (320x240); the display runs
  // rotation 7 = swap + mirror both, so touch starts with the same flags.
  // Task 6 verifies these against on-screen targets and flips them if the
  // corner test disagrees (docs/DISPLAY.md gotcha).
  transform_.raw_w = 320;
  transform_.raw_h = 240;
  transform_.swap_xy = true;
  transform_.mirror_x = true;
  transform_.mirror_y = true;

  // The chip sleeps: pulse RST low->high, then wait ~300 ms before the first
  // I2C access (docs/hardware.md) or it simply won't ACK.
  pinMode(kPinRst, OUTPUT);
  digitalWrite(kPinRst, LOW);
  delay(10);
  digitalWrite(kPinRst, HIGH);
  delay(300);

  Wire.begin(kPinSda, kPinScl, kI2cHz);
  writeRegister(kRegDisAutoSleep, 0x01);  // keep it awake for polling

  lv_indev_drv_init(&indevDrv_);
  indevDrv_.type = LV_INDEV_TYPE_POINTER;
  indevDrv_.read_cb = readCb;
  indevDrv_.user_data = this;
  indev_ = lv_indev_drv_register(&indevDrv_);

  Serial.println("[danios] touch: CST820 polling indev registered");
}

void TouchService::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool TouchService::readRaw(int16_t& raw_x, int16_t& raw_y) {
  Wire.beginTransmission(kAddr);
  Wire.write(kRegGesture);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom(kAddr, static_cast<uint8_t>(6)) != 6) return false;

  (void)Wire.read();                                   // 0x01 gesture (unused)
  const uint8_t fingers = Wire.read();                 // 0x02
  const uint8_t xh = Wire.read();                      // 0x03
  const uint8_t xl = Wire.read();                      // 0x04
  const uint8_t yh = Wire.read();                      // 0x05
  const uint8_t yl = Wire.read();                      // 0x06

  if (fingers == 0) return false;
  raw_x = static_cast<int16_t>(((xh & 0x0F) << 8) | xl);
  raw_y = static_cast<int16_t>(((yh & 0x0F) << 8) | yl);
  return true;
}

void TouchService::readCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  auto* self = static_cast<TouchService*>(drv->user_data);
  int16_t rawX = 0;
  int16_t rawY = 0;
  const bool pressed = self->readRaw(rawX, rawY);
  if (pressed) {
    const TouchPoint p = transformTouch(self->transform_, rawX, rawY);
    self->lastX_ = p.x;
    self->lastY_ = p.y;
    if (!self->wasPressed_) {
      // One line per touch-down. Task 6's four-corner verification reads
      // exactly this output over the serial monitor.
      Serial.printf("[touch] raw=(%d,%d) screen=(%d,%d)\n",
                    rawX, rawY, p.x, p.y);
    }
  }
  self->wasPressed_ = pressed;
  data->point.x = self->lastX_;  // keep last point on release (LVGL v8 rule)
  data->point.y = self->lastY_;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
```

- [ ] **Step 3: Verify the device env builds**

Run:
```bash
cd /home/lucca/repos/danios && pio run -e cyd
```
Expected: `[SUCCESS]`. The LDF pulls `lib/touch_transform` in automatically
(that's why the include is `<touch_transform.h>`, not a relative path). If the
header isn't found, the `lib/touch_transform/` directory layout from Task 2 is
wrong.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/TouchService.h src/services/TouchService.cpp && git commit -m "feat: add TouchService — CST820 polling driver feeding LVGL pointer indev"
```
Expected: `[main <hash>] feat: add TouchService — CST820 polling driver feeding LVGL pointer indev`

---

### Task 5: Replace the diagnostic `main.cpp` with the LVGL smoke screen

The milestone-1 diagnostic sketch has done its job (it's preserved in the
Task 0 initial commit). Replace it with the F1 E2E scene: a centered
**"tap me"** button and a counter label that increments on every tap.

**Files:**
- Modify: `src/main.cpp` (full replacement below)

**Interfaces:**
- Consumes: `DisplayService::begin()/tick()` (Task 3),
  `TouchService::begin()` (Task 4).
- Produces: the F1 boot flow in `main.cpp` that F2 will extend (F2 replaces the
  smoke screen with the Launcher but keeps the `displayService`/`touchService`
  boot order and the `displayService.tick()` loop).

- [ ] **Step 1: Replace `src/main.cpp`**

Full new content:

```cpp
// danios — F1 smoke screen: LVGL + touch, end to end.
// A centered "tap me" button and a counter label that increments per tap.
// Proves: DisplayService flush path, TouchService -> LVGL pointer indev,
// LVGL event dispatch, and the loop-task tick. Replaced by the Launcher in F2.
//
// (The milestone-1 display diagnostic this file replaces lives in the initial
// git commit if it's ever needed again.)
#include <Arduino.h>
#include <lvgl.h>

#include "services/DisplayService.h"
#include "services/TouchService.h"

DisplayService displayService;
TouchService touchService;

namespace {

lv_obj_t* counterLabel = nullptr;
uint32_t tapCount = 0;

void onTap(lv_event_t*) {
  ++tapCount;
  lv_label_set_text_fmt(counterLabel, "taps: %u",
                        static_cast<unsigned>(tapCount));
  Serial.printf("[danios] tap %u\n", static_cast<unsigned>(tapCount));
}

void buildSmokeScreen() {
  lv_obj_t* scr = lv_scr_act();

  lv_obj_t* btn = lv_btn_create(scr);
  lv_obj_set_size(btn, 120, 60);
  lv_obj_center(btn);
  lv_obj_add_event_cb(btn, onTap, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* btnLabel = lv_label_create(btn);
  lv_label_set_text(btnLabel, "tap me");
  lv_obj_center(btnLabel);

  counterLabel = lv_label_create(scr);
  lv_label_set_text(counterLabel, "taps: 0");
  lv_obj_align(counterLabel, LV_ALIGN_CENTER, 0, 70);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[danios] F1 smoke screen starting");

  displayService.begin();  // panel + LVGL + flush (must be first)
  touchService.begin();    // CST820 -> LVGL indev (needs a display registered)
  buildSmokeScreen();

  Serial.println("[danios] UI up — tap the button");
}

void loop() {
  displayService.tick();  // lv_timer_handler()
  delay(5);
}
```

- [ ] **Step 2: Verify the device env builds and note the RAM figure**

Run:
```bash
cd /home/lucca/repos/danios && pio run -e cyd
```
Expected: `[SUCCESS]`, with a `RAM: [...] used ... bytes` line. Static RAM
should now include the two 240×30 LVGL buffers (~28.8 KB) + the 48 KB LVGL
heap; expect roughly 90–110 KB used (well under the 320 KB DRAM limit). Record
the actual figure — append it as a body line to the Step 4 commit — it's the
baseline for the roadmap §2 RAM budget as F4/F5 add radios.

- [ ] **Step 3: Verify native tests still pass**

Run:
```bash
cd /home/lucca/repos/danios && pio test -e native
```
Expected: `10 Tests 0 Failures 0 Ignored` … `[PASSED]` (`test_build_src =
false` means the new `main.cpp` is irrelevant to this env — this step guards
exactly that property).

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/main.cpp && git commit -m "feat: replace display diagnostic with LVGL tap-counter smoke screen"
```
Expected: `[main <hash>] feat: replace display diagnostic with LVGL tap-counter smoke screen`

---

### Task 6: On-device verification (requires the CYD board)

**Board required: the ESP32-2432S024C (CYD) itself.** The bare ESP32 devkit
currently on hand (`docs/hardware.md`) has no display or CST820 — it cannot
run this task. If the CYD hasn't arrived yet, stop here with Tasks 0–5
committed; this task is the plan's hardware gate.

Per `docs/DISPLAY.md`: **touch swap/mirror flags must be verified against
on-screen targets, never trusted from board definitions** — this clone needed
non-standard display flags (rotation 7), and its touch may too. The
`TouchService` flags are parameters precisely so this task can fix them
without logic changes.

**Files:**
- Possibly modify: `src/services/TouchService.cpp` (transform flags only, per
  the corner-test table below)

**Interfaces:**
- Consumes: everything Tasks 1–5 produced.
- Produces: the verified F1 E2E outcome (roadmap §1, row F1: "Tap an on-screen
  button; a counter increments") and hardware-confirmed touch-transform flags
  that all later UI plans silently rely on.

- [ ] **Step 1: Flash the device**

Connect the CYD over USB-C, then run:
```bash
cd /home/lucca/repos/danios && pio run -e cyd -t upload
```
Expected: upload completes on `/dev/ttyUSB0` (CH340), ending with
`[SUCCESS]`.

- [ ] **Step 2: Open the serial monitor**

Run:
```bash
cd /home/lucca/repos/danios && pio device monitor
```
Expected boot lines:
```
[danios] F1 smoke screen starting
[danios] touch: CST820 polling indev registered
[danios] UI up — tap the button
```
If the touch line never appears or boot hangs before it, the CST820 didn't
wake — re-check the RST pulse timing in `TouchService::begin()` (the ~300 ms
wait is mandatory) and that nothing else drives GPIO 25.

- [ ] **Step 3: Visual check — orientation and rendering**

Hold the device portrait, **USB-C down**. Expected:
- A centered button labeled "tap me", `taps: 0` below it, upright text.
- Colors sane: the default LVGL theme's blue button reads **blue**, not
  orange/red (if colors are inverted-looking, the flush callback byte-order
  cast regressed — see Task 3 Step 2).
- No tearing, no 80-px offset bands, no wrapped strips (those were the
  milestone-1 geometry symptoms; they must not reappear with LVGL driving).

- [ ] **Step 4: Four-corner touch verification (the DISPLAY.md-mandated check)**

Watch the serial monitor and tap each **screen corner** (USB-C down), then
the center. Each touch-down prints one `[touch] raw=(..) screen=(..)` line.
Expected `screen=` values, within ~20 px:

| Where you tap | Expected `screen=(x,y)` |
| --- | --- |
| top-left | `(0..20, 0..20)` |
| top-right | `(219..239, 0..20)` |
| bottom-left | `(0..20, 299..319)` |
| bottom-right | `(219..239, 299..319)` |
| center | `(~120, ~160)` |

If the values disagree, fix **only the transform parameters** in
`TouchService::begin()` per this table, re-flash, and re-run this step:

| Symptom (tapping top-left) | Fix |
| --- | --- |
| `screen≈(239, 319)` (diagonally opposite) | toggle **both** `mirror_x` and `mirror_y` |
| `screen≈(239, 0)` (flipped horizontally) | toggle `mirror_x` |
| `screen≈(0, 319)` (flipped vertically) | toggle `mirror_y` |
| dragging a finger **left–right** changes `screen` **y** instead of x | toggle `swap_xy` |
| `raw=` x never exceeds ~239 and `raw=` y reaches ~319 (chip is portrait-native after all) | set `raw_w = 240`, `raw_h = 320`, `swap_xy = false`, redo the corner test for the mirrors |

- [ ] **Step 5: E2E — the roadmap F1 outcome**

Tap the "tap me" button five times. Expected:
- The button shows LVGL's pressed visual on each tap (touch and render agree).
- The label advances `taps: 1` … `taps: 5`, matching serial lines
  `[danios] tap 1` … `[danios] tap 5` — one per physical tap, no double
  counts, no missed taps.
- Rapid tapping and a press-drag-release off the button behave sanely (drag
  off = no count, per LVGL `LV_EVENT_CLICKED` semantics).

- [ ] **Step 6: Commit the verified flags (only if Step 4 changed them)**

If `TouchService::begin()` flags were adjusted:
```bash
cd /home/lucca/repos/danios && git add src/services/TouchService.cpp && git commit -m "fix: correct touch transform flags from on-device corner test"
```
Expected: `[main <hash>] fix: correct touch transform flags from on-device corner test`
If no flags changed, there is nothing to commit — record in the task notes
that the rotation-7 defaults were confirmed on hardware.

---

## Definition of done

- [ ] `pio test -e native` — all 10 touch_transform tests green (the repo's
      native test env is proven working).
- [ ] `pio run -e cyd` — device build green with LVGL 8.4, C++17,
      `huge_app.csv` partitions.
- [ ] **E2E outcome on hardware (roadmap §1, F1):** tapping the on-screen
      button increments the counter, verified on the CYD.
- [ ] Four-corner touch verification passed; final transform flags committed
      (or defaults confirmed) — per `docs/DISPLAY.md`, verified against
      on-screen targets, not board-def flags.
- [ ] `docs/DISPLAY.md` "LVGL glue" section replaced the reserved NOTE
      (buffers, `LV_COLOR_16_SWAP 0` + `writePixels(lgfx::rgb565_t*)`
      decision, touch mapping).
- [ ] Git history exists from Task 0 with small conventional commits;
      `.pio/` and `.claude/settings.local.json` untracked.
- [ ] `include/LGFX_ESP32_2432S024C.hpp` untouched (frozen, proven config).
- [ ] Handed to F2: `DisplayService`, `TouchService`, working LVGL at 240×320
      rotation 7, `lib/touch_transform/`, `[env:native]`, the git repo.
