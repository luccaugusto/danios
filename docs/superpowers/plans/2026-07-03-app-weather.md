# Weather App (A3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Date:** 2026-07-06
**Spec:** [`docs/superpowers/specs/apps/weather.md`](../specs/apps/weather.md)
**Roadmap slot:** A3 (see [roadmap](2026-07-03-danios-roadmap.md) §1/§3/§4 — names/paths/keys there are authoritative)

**Goal:** Character dressed for the live local weather, with stale-cache fallback — app id `"weather"` replaces the `weatherStub` registration in `src/main.cpp`; the plan also delivers **Settings → Local do clima** and fills F4's **boot-time weather prefetch** hook.

**Architecture:** All mapping logic (temperature→band, WMO code→condition group, data→art slots, °C/°F display conversion, UTC-offset→POSIX-TZ string) lives in pure std-C++17 `lib/weather_model/`; all JSON→struct parsing (ip-api geolocation, Open-Meteo forecast) lives in `lib/weather_parse/` via ArduinoJson — both TDD'd on `pio test -e native`. A device-only fetch pipeline `src/apps/weather/WeatherFetch` (geolocate-if-needed → fetch → cache to NVS → set timezone) is shared by the boot prefetch and the app. `WeatherApp` is a thin LVGL wrapper that renders the cached forecast instantly, then refreshes over WiFi from `tick()`.

**Tech Stack:** C++17 (both envs), LVGL 8.4 (v8 API), ArduinoJson `^7.4.0` (both envs), `HTTPClient` + `WiFiClientSecure` (arduino-esp32), PlatformIO (`cyd` device env, `native` host-test env), Unity.

**Prerequisites:** F4 (`2026-07-03-foundation-4-wifi-time.md`) is **merged**: this plan consumes `RadioManager`, `WiFiService`, `TimeService`, `lib/date_utils/`, and grafts onto the `// A3: weather boot prefetch hook` marker F4 left in `src/main.cpp`. Task 0 verifies every anchor.

## Global Constraints

(Copied from roadmap §2 — every task inherits these.)

- **Board:** ESP32-2432S024C (CYD 2.4" capacitive), ESP32-WROOM-32, **no PSRAM**.
  520 KB SRAM total; budget carefully (LVGL buffers ~29 KB, LVGL heap 48 KB,
  WiFi ~50 KB or BT Classic ~64 KB — **never both**, MP3 decode ~30 KB).
- **Platform:** PlatformIO, `platform = espressif32@7.0.1`, `board = esp32dev`,
  `framework = arduino` (arduino-esp32 3.x). Partition scheme:
  `board_build.partitions = huge_app.csv` (no OTA — spec non-goal).
- **Display:** landscape-native 320×240 clone driven via
  `include/LGFX_ESP32_2432S024C.hpp` — **do not change panel/memory dims (320×240)
  or `offset_rotation` (0)**. All UI renders portrait 240×320 via
  `tft.setRotation(7)`, USB-C down. See `docs/DISPLAY.md`.
- **LVGL:** `lvgl/lvgl@8.4.0` (v8 API — not v9). `lv_conf.h` lives in `include/`.
  UI code runs on the Arduino loop task only (LVGL is not thread-safe) — all
  fetching here is blocking-on-the-loop-task by design, with a status label +
  `lv_refr_now` painted first.
- **C++17** on both envs.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager`.
  `WeatherApp::requiredRadio()` returns `RadioMode::WiFi` — the **Launcher**
  requests the radio on app entry and releases it on exit; this plan's code
  only ever calls `WiFiService::isConnected()/connect()`, never `WiFi.*` power.
- **TDD, native-first:** all pure logic lives in `lib/<module>/` and is
  unit-tested with `pio test -e native` (Unity). `lib/weather_model/` is
  std C++ only; `lib/weather_parse/` may include ArduinoJson (roadmap §3
  footnote — it is platform-independent and runs on native).
- **Commits:** small, frequent, conventional (`feat:`, `test:`, `fix:`, `docs:`).
- **SD layout & NVS keys:** this plan owns NVS keys `loc.mode` (u8 0=auto
  1=manual), `loc.lat`/`loc.lon` (float), `loc.city` (str), `wx.json` (str, last
  snapshot), `wx.day` (u32 dateKey) — namespace `"danios"`, nothing else. Art
  lives under `S:/art/weather/`; icon (when drawn) `S:/art/icons/weather.bin`.

### Plan-specific facts (verified against the current tree + the F4 plan)

- The Launcher gives `App::buildUI(lv_obj_t* parent)` a style-stripped container
  below its own 32 px top bar: **240 wide × 288 tall**, origin (0,0) at the
  container's top-left (`src/core/Launcher.cpp`). The back arrow is the
  launcher's — the app must not add its own.
- Device UI language is **Portuguese**. The default font is the custom
  `montserrat_pt_14` covering full Latin-1 (so `°`, `ã`, `é`, `ó` render) plus
  the FontAwesome symbol range. **No larger font is enabled** in `lv_conf.h`
  (only `LV_FONT_MONTSERRAT_14` + the custom 14 px) — do not reference
  `LV_FONT_MONTSERRAT_2x`/`_3x`; all readouts use the default font.
- PlatformIO adds each `lib/<module>/` to the include path: include the modules
  as `#include <weather_model.h>` / `#include <weather_parse.h>` (same pattern
  as `<calc_engine.h>` / `<settings_store.h>`). For `date_utils`, **use the
  same include form F4 landed** (Task 0 records it; the code below assumes
  `#include <date_utils/date_utils.h>` per the F4 plan — adjust if F4 shipped
  `<date_utils.h>`).
- Launcher label/icon come from `catalog::kWeather` in `src/apps/app_catalog.h`
  (`"Clima"`, icon `nullptr`). Drawing `S:/art/icons/weather.bin` is **out of
  scope**; the launcher renders its colored-letter fallback. Do not edit the
  catalog.
- `StorageService::exists()` takes bare SD paths (`"/art/weather/x.bin"`);
  LVGL image sources take the drive-letter form (`"S:/art/weather/x.bin"`).
  Convert by skipping the first two chars of the `S:` path.
- `SettingsApp.cpp` holds `kSectionNames[] = {"Tela", "Unidades", "Sobre"}`
  pre-F4; F4 appends its WiFi and Clock entries. This plan **appends one entry
  at the end** — the code below assumes the post-F4 array has 5 entries (new
  entry = index 5); Task 0 confirms the real count and Task 8 says how to
  adjust.
- NVS string values cap out near **4000 bytes**; a 3-day Open-Meteo response is
  ~800 bytes. `WeatherFetch` refuses to cache bodies ≥ 3500 bytes.
- `main.cpp` already disables the weather app in the launcher when the SD card
  is missing (F3 block) — no change needed for the spec's "SD missing" row.
- Spec non-goals: no hourly forecast, no multiple saved locations, no weather
  alerts, no IANA timezone names. Do not add them.

## File Structure

| File | Task | Responsibility |
| --- | --- | --- |
| Create `lib/weather_model/weather_model.h` + `.cpp` | 1–3 | Pure mapping: bands, WMO groups, °C/°F, PT labels, art slots, POSIX TZ |
| Create `test/test_weather_model/test_main.cpp` | 1–3 | Unity tests (native) |
| Modify `platformio.ini` | 4 | ArduinoJson `^7.4.0` in both envs |
| Create `lib/weather_parse/weather_parse.h` + `.cpp` | 4–5 | ip-api + Open-Meteo JSON → plain structs; forecast URL builder |
| Create `test/test_weather_parse/test_main.cpp` | 4–5 | Unity tests with canned JSON (native) |
| Create `src/apps/weather/WeatherFetch.h` + `.cpp` | 6 | Device-only fetch/cache pipeline (HTTP + NVS + TZ) |
| Modify `src/main.cpp` (boot hook) | 6 | Fill F4's `// A3: weather boot prefetch hook` |
| Create `src/apps/weather/WeatherApp.h` + `.cpp` | 7 | LVGL wrapper: render cache, refresh from tick, placeholders |
| Modify `src/main.cpp` (registration) | 7 | Replace `weatherStub` with `WeatherApp` |
| Create `src/apps/settings/WeatherLocationSection.cpp` | 8 | Settings → Local do clima (auto/manual) |
| Modify `src/apps/settings/Sections.h` + `SettingsApp.cpp` | 8 | Declare + wire the new section |

### Behavior locked in here (implemented across Tasks 1–7)

- **Geolocation policy** (spec: "call rarely, cache the result"): auto mode
  calls ip-api only when there are no stored coordinates **or** on the first
  fetch of a new calendar day (`wx.day != dateKey(today)`) — which also
  refreshes the fixed TZ offset (roadmap deviation §5.4). Manual mode never
  geolocates and never touches the timezone.
- **Cache policy:** the raw Open-Meteo response body is stored verbatim in
  `wx.json` (re-parsed with `weather_parse` on load) and `wx.day` records the
  local dateKey of the last successful fetch. Cache is written only after a
  successful parse.
- **Refresh policy:** render cache instantly in `buildUI`; first network
  refresh runs from `tick()` ~400 ms later (so the cached frame paints first);
  then every 20 min while the app is open; plus the boot prefetch.
- **Placeholder rule** (roadmap §4.1): every art slot renders a flat colored
  box when its `.bin` is missing; a condition with no overlay/background slot
  renders nothing there. Art file names (maker-tunable constants in
  `weather_model.cpp`): outfits `outfit_{freezing,cold,mild,warm,hot}.bin`;
  overlays `ov_sunglasses.bin` (Clear, day only), `ov_umbrella.bin`
  (Rain/Storm), `ov_scarf.bin` (Snow); backgrounds
  `bg_{clear,clear_night,cloudy,fog,rain,snow,storm}.bin` — all under
  `S:/art/weather/`.
- **PT copy:** condition labels `Ensolarado / Nublado / Neblina / Chuva /
  Neve / Tempestade / --`; mini-forecast columns `Hoje / Amanhã / Depois`;
  stale marker `desatualizado`; failed-refresh marker `sem WiFi`; no-data
  screen `"Não consegui ver o céu agora.\n\nVerifique o WiFi em
  Configurações."`; settings section `Local do clima`.

## Task Right-Sizing Overview

0. Preflight — verify the F4 baseline and record anchors
1. `weather_model`: temperature bands + WMO condition groups (TDD)
2. `weather_model`: °C/°F display conversion, PT labels, art-slot table (TDD)
3. `weather_model`: UTC offset → POSIX TZ string (TDD)
4. ArduinoJson dep + `weather_parse`: ip-api geolocation parsing (TDD)
5. `weather_parse`: Open-Meteo forecast parsing + URL builder (TDD)
6. `WeatherFetch` device pipeline + boot prefetch in `main.cpp`
7. `WeatherApp` UI wrapper + registration
8. Settings → Local do clima section
9. On-device verification (manual — needs the CYD + a WiFi network)

---

### Task 0: Preflight — verify the F4 baseline

**Files:** none (verification only).

**Interfaces:**
- Consumes: the merged F1–F4 tree.
- Produces: confidence that this plan's graft points exist, plus two recorded
  facts later tasks need (the `date_utils` include form; the
  `kSectionNames` count).

- [ ] **Step 1: Confirm builds and tests are green**

Run: `cd /home/lucca/repos/danios && pio test -e native && pio run -e cyd`
Expected: all native tests PASS; device build `SUCCESS`.

- [ ] **Step 2: Confirm the F4 anchors**

Run:
```bash
cd /home/lucca/repos/danios && \
  grep -n "A3: weather boot prefetch hook" src/main.cpp && \
  grep -n "setTimezone" src/services/TimeService.h && \
  grep -n "bool connect" src/services/WiFiService.h && \
  grep -n "isConnected" src/services/WiFiService.h
```
Expected: one hit each. If any is missing, **stop — F4 is not merged.**

- [ ] **Step 3: Record the two variable anchors**

Run: `grep -n "date_utils" src/services/TimeService.h && grep -n -A2 "kSectionNames\[\]" src/apps/settings/SettingsApp.cpp`
Expected: the include line shows whether F4 landed `<date_utils/date_utils.h>`
or `<date_utils.h>` (use that exact form in Task 6), and the section array
shows its current entries (Task 8 appends after the last one; the switch case
number is the array's current length).

---

### Task 1: `weather_model` — temperature bands + WMO condition groups

**Files:**
- Create: `lib/weather_model/weather_model.h`
- Create: `lib/weather_model/weather_model.cpp`
- Test: `test/test_weather_model/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++ only).
- Produces: `enum class TempBand : uint8_t { Freezing, Cold, Mild, Warm, Hot }`,
  `enum class Condition : uint8_t { Clear, Cloudy, Fog, Rain, Snow, Storm, Unknown }`,
  `TempBand tempBand(float celsius)`, `Condition conditionFromWmo(int code)`.
  Tasks 2–3 add functions to this same header; Tasks 6–7 consume all of them.

- [ ] **Step 1: Write the failing tests**

Create `test/test_weather_model/test_main.cpp`:

```cpp
// Host-side tests for weather_model (pio test -e native): the spec's
// data->art bridge — temperature bands, WMO condition groups, display
// conversion, art-slot selection, and the offset->POSIX-TZ rule — all
// verified off-device with the boundary values the spec calls out.
#include <unity.h>

#include <weather_model.h>

void setUp() {}
void tearDown() {}

static void assertBand(TempBand want, float celsius) {
  TEST_ASSERT_EQUAL_INT((int)want, (int)tempBand(celsius));
}

static void test_band_freezing_below_zero() {
  assertBand(TempBand::Freezing, -1.0f);
  assertBand(TempBand::Freezing, -0.1f);
  assertBand(TempBand::Freezing, -25.0f);
}

static void test_band_cold_0_to_9() {
  assertBand(TempBand::Cold, 0.0f);   // spec boundary: -1/0
  assertBand(TempBand::Cold, 9.0f);   // spec boundary: 9/10
  assertBand(TempBand::Cold, 9.9f);
}

static void test_band_mild_10_to_19() {
  assertBand(TempBand::Mild, 10.0f);
  assertBand(TempBand::Mild, 19.9f);
}

static void test_band_warm_20_to_27() {
  assertBand(TempBand::Warm, 20.0f);
  assertBand(TempBand::Warm, 27.0f);  // spec boundary: 27/28
  assertBand(TempBand::Warm, 27.9f);
}

static void test_band_hot_28_up() {
  assertBand(TempBand::Hot, 28.0f);
  assertBand(TempBand::Hot, 41.0f);
}

static void assertCond(Condition want, int code) {
  TEST_ASSERT_EQUAL_INT((int)want, (int)conditionFromWmo(code));
}

static void test_condition_clear() {
  assertCond(Condition::Clear, 0);
  assertCond(Condition::Clear, 1);
}

static void test_condition_cloudy() {
  assertCond(Condition::Cloudy, 2);
  assertCond(Condition::Cloudy, 3);
}

static void test_condition_fog() {
  assertCond(Condition::Fog, 45);
  assertCond(Condition::Fog, 48);
}

static void test_condition_rain() {
  assertCond(Condition::Rain, 51);  // drizzle start
  assertCond(Condition::Rain, 57);  // drizzle end
  assertCond(Condition::Rain, 61);  // rain start
  assertCond(Condition::Rain, 67);  // rain end
  assertCond(Condition::Rain, 80);  // showers start
  assertCond(Condition::Rain, 82);  // showers end
}

static void test_condition_snow() {
  assertCond(Condition::Snow, 71);
  assertCond(Condition::Snow, 77);
  assertCond(Condition::Snow, 85);
  assertCond(Condition::Snow, 86);
}

static void test_condition_storm() {
  assertCond(Condition::Storm, 95);
  assertCond(Condition::Storm, 96);
  assertCond(Condition::Storm, 99);
}

static void test_condition_unknown_codes() {
  // Every gap around the spec's ranges maps to Unknown, never crashes.
  assertCond(Condition::Unknown, -1);
  assertCond(Condition::Unknown, 4);
  assertCond(Condition::Unknown, 50);   // just below drizzle
  assertCond(Condition::Unknown, 58);   // between drizzle and rain
  assertCond(Condition::Unknown, 68);   // between rain and snow
  assertCond(Condition::Unknown, 78);   // between snow and showers
  assertCond(Condition::Unknown, 83);   // between showers and snow showers
  assertCond(Condition::Unknown, 87);
  assertCond(Condition::Unknown, 97);   // between storm codes
  assertCond(Condition::Unknown, 100);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_band_freezing_below_zero);
  RUN_TEST(test_band_cold_0_to_9);
  RUN_TEST(test_band_mild_10_to_19);
  RUN_TEST(test_band_warm_20_to_27);
  RUN_TEST(test_band_hot_28_up);
  RUN_TEST(test_condition_clear);
  RUN_TEST(test_condition_cloudy);
  RUN_TEST(test_condition_fog);
  RUN_TEST(test_condition_rain);
  RUN_TEST(test_condition_snow);
  RUN_TEST(test_condition_storm);
  RUN_TEST(test_condition_unknown_codes);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_weather_model`
Expected: **build error** — `weather_model.h: No such file or directory`

- [ ] **Step 3: Write the minimal implementation**

Create `lib/weather_model/weather_model.h`:

```cpp
// lib/weather_model/weather_model.h — the spec's data->art bridge (A3,
// roadmap §3). Pure std C++17, zero Arduino/LVGL includes; all mapping is
// native-tested. Maker-tunable constants (band edges, WMO groups, art file
// names) live here and in the .cpp — tweak them, rerun the tests.
#pragma once

#include <cstdint>
#include <string>

// Outfit is chosen by temperature band (spec table: <0, 0-9, 10-19, 20-27, >=28 °C).
enum class TempBand : uint8_t { Freezing, Cold, Mild, Warm, Hot };

// Overlay + background are chosen by WMO weather_code group (spec table).
enum class Condition : uint8_t { Clear, Cloudy, Fog, Rain, Snow, Storm, Unknown };

TempBand tempBand(float celsius);
Condition conditionFromWmo(int code);
```

Create `lib/weather_model/weather_model.cpp`:

```cpp
#include "weather_model.h"

TempBand tempBand(float celsius) {
  if (celsius < 0.0f) return TempBand::Freezing;
  if (celsius < 10.0f) return TempBand::Cold;
  if (celsius < 20.0f) return TempBand::Mild;
  if (celsius < 28.0f) return TempBand::Warm;
  return TempBand::Hot;
}

Condition conditionFromWmo(int code) {
  switch (code) {
    case 0: case 1: return Condition::Clear;
    case 2: case 3: return Condition::Cloudy;
    case 45: case 48: return Condition::Fog;
    case 95: case 96: case 99: return Condition::Storm;
    default: break;
  }
  if ((code >= 51 && code <= 57) ||   // drizzle
      (code >= 61 && code <= 67) ||   // rain
      (code >= 80 && code <= 82)) {   // rain showers
    return Condition::Rain;
  }
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) {  // snow
    return Condition::Snow;
  }
  return Condition::Unknown;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_weather_model`
Expected: `12 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/weather_model test/test_weather_model
git commit -m "feat: weather_model temperature bands + WMO condition groups"
```

---

### Task 2: `weather_model` — display conversion, PT labels, art slots

**Files:**
- Modify: `lib/weather_model/weather_model.h`
- Modify: `lib/weather_model/weather_model.cpp`
- Test: `test/test_weather_model/test_main.cpp`

**Interfaces:**
- Consumes: Task 1's `TempBand` / `Condition`.
- Produces: `float toDisplayTemp(float celsius, bool fahrenheit)` (storage/API
  stay metric — conversion is display-only, per spec),
  `const char* conditionLabelPt(Condition c)`,
  `struct ArtSlots { const char* outfit; const char* overlay; const char* background; }`
  (`overlay`/`background` may be `nullptr` = no slot for this condition),
  `ArtSlots artSlots(TempBand band, Condition cond, bool isDay)`.
  Task 7's UI consumes all four.

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_weather_model/test_main.cpp`:

```cpp
static void test_celsius_passthrough() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, toDisplayTemp(20.0f, false));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.5f, toDisplayTemp(-3.5f, false));
}

static void test_fahrenheit_conversion() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, toDisplayTemp(0.0f, true));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 212.0f, toDisplayTemp(100.0f, true));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, toDisplayTemp(-40.0f, true));
}

static void test_condition_labels_pt() {
  TEST_ASSERT_EQUAL_STRING("Ensolarado", conditionLabelPt(Condition::Clear));
  TEST_ASSERT_EQUAL_STRING("Nublado", conditionLabelPt(Condition::Cloudy));
  TEST_ASSERT_EQUAL_STRING("Neblina", conditionLabelPt(Condition::Fog));
  TEST_ASSERT_EQUAL_STRING("Chuva", conditionLabelPt(Condition::Rain));
  TEST_ASSERT_EQUAL_STRING("Neve", conditionLabelPt(Condition::Snow));
  TEST_ASSERT_EQUAL_STRING("Tempestade", conditionLabelPt(Condition::Storm));
  TEST_ASSERT_EQUAL_STRING("--", conditionLabelPt(Condition::Unknown));
}

static void test_art_outfit_per_band() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_freezing.bin",
      artSlots(TempBand::Freezing, Condition::Snow, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_cold.bin",
      artSlots(TempBand::Cold, Condition::Cloudy, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_mild.bin",
      artSlots(TempBand::Mild, Condition::Clear, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_warm.bin",
      artSlots(TempBand::Warm, Condition::Rain, true).outfit);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/outfit_hot.bin",
      artSlots(TempBand::Hot, Condition::Clear, true).outfit);
}

static void test_art_overlay_per_condition() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_sunglasses.bin",
      artSlots(TempBand::Hot, Condition::Clear, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_umbrella.bin",
      artSlots(TempBand::Mild, Condition::Rain, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_umbrella.bin",
      artSlots(TempBand::Warm, Condition::Storm, true).overlay);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/ov_scarf.bin",
      artSlots(TempBand::Freezing, Condition::Snow, true).overlay);
  TEST_ASSERT_NULL(artSlots(TempBand::Mild, Condition::Cloudy, true).overlay);
  TEST_ASSERT_NULL(artSlots(TempBand::Mild, Condition::Fog, true).overlay);
}

static void test_art_background_per_condition() {
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_clear.bin",
      artSlots(TempBand::Warm, Condition::Clear, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_cloudy.bin",
      artSlots(TempBand::Warm, Condition::Cloudy, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_fog.bin",
      artSlots(TempBand::Warm, Condition::Fog, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_rain.bin",
      artSlots(TempBand::Warm, Condition::Rain, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_snow.bin",
      artSlots(TempBand::Cold, Condition::Snow, true).background);
  TEST_ASSERT_EQUAL_STRING(
      "S:/art/weather/bg_storm.bin",
      artSlots(TempBand::Warm, Condition::Storm, true).background);
}

static void test_art_clear_night_variant() {
  // is_day polish (spec): night gets its own clear sky, and no sunglasses.
  const ArtSlots night = artSlots(TempBand::Mild, Condition::Clear, false);
  TEST_ASSERT_EQUAL_STRING("S:/art/weather/bg_clear_night.bin", night.background);
  TEST_ASSERT_NULL(night.overlay);
}

static void test_art_unknown_condition_has_no_slots() {
  const ArtSlots s = artSlots(TempBand::Mild, Condition::Unknown, true);
  TEST_ASSERT_EQUAL_STRING("S:/art/weather/outfit_mild.bin", s.outfit);
  TEST_ASSERT_NULL(s.overlay);
  TEST_ASSERT_NULL(s.background);
}
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_celsius_passthrough);
  RUN_TEST(test_fahrenheit_conversion);
  RUN_TEST(test_condition_labels_pt);
  RUN_TEST(test_art_outfit_per_band);
  RUN_TEST(test_art_overlay_per_condition);
  RUN_TEST(test_art_background_per_condition);
  RUN_TEST(test_art_clear_night_variant);
  RUN_TEST(test_art_unknown_condition_has_no_slots);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_weather_model`
Expected: **build error** — `'toDisplayTemp' was not declared in this scope`

- [ ] **Step 3: Write the implementation**

In `lib/weather_model/weather_model.h`, add after `conditionFromWmo`:

```cpp
// Display-only conversion (spec: storage and API stay metric). Pass the F3
// NVS flag units.f as `fahrenheit`.
float toDisplayTemp(float celsius, bool fahrenheit);

// Portuguese condition label for on-screen text (device UI language).
const char* conditionLabelPt(Condition c);

// Which art files to show. overlay/background are nullptr when the condition
// has no such slot (Cloudy/Fog/Unknown overlay; Unknown background). Files
// live on the SD card; the UI renders a placeholder box when one is missing.
struct ArtSlots {
  const char* outfit;      // never nullptr
  const char* overlay;     // accessory (sunglasses/umbrella/scarf) or nullptr
  const char* background;  // scene behind the character, or nullptr = plain
};
ArtSlots artSlots(TempBand band, Condition cond, bool isDay);
```

In `lib/weather_model/weather_model.cpp`, add after `conditionFromWmo`:

```cpp
float toDisplayTemp(float celsius, bool fahrenheit) {
  return fahrenheit ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

const char* conditionLabelPt(Condition c) {
  switch (c) {
    case Condition::Clear:  return "Ensolarado";
    case Condition::Cloudy: return "Nublado";
    case Condition::Fog:    return "Neblina";
    case Condition::Rain:   return "Chuva";
    case Condition::Snow:   return "Neve";
    case Condition::Storm:  return "Tempestade";
    default:                return "--";
  }
}

namespace {
// Maker-tunable art table (spec "Data -> art bridge"). Indexed by TempBand.
const char* kOutfit[] = {
    "S:/art/weather/outfit_freezing.bin", "S:/art/weather/outfit_cold.bin",
    "S:/art/weather/outfit_mild.bin", "S:/art/weather/outfit_warm.bin",
    "S:/art/weather/outfit_hot.bin"};
}  // namespace

ArtSlots artSlots(TempBand band, Condition cond, bool isDay) {
  ArtSlots s{kOutfit[static_cast<int>(band)], nullptr, nullptr};
  switch (cond) {
    case Condition::Clear:
      if (isDay) s.overlay = "S:/art/weather/ov_sunglasses.bin";
      s.background = isDay ? "S:/art/weather/bg_clear.bin"
                           : "S:/art/weather/bg_clear_night.bin";
      break;
    case Condition::Cloudy:
      s.background = "S:/art/weather/bg_cloudy.bin";
      break;
    case Condition::Fog:
      s.background = "S:/art/weather/bg_fog.bin";
      break;
    case Condition::Rain:
      s.overlay = "S:/art/weather/ov_umbrella.bin";
      s.background = "S:/art/weather/bg_rain.bin";
      break;
    case Condition::Snow:
      s.overlay = "S:/art/weather/ov_scarf.bin";
      s.background = "S:/art/weather/bg_snow.bin";
      break;
    case Condition::Storm:
      s.overlay = "S:/art/weather/ov_umbrella.bin";
      s.background = "S:/art/weather/bg_storm.bin";
      break;
    default:
      break;  // Unknown: plain background, no accessory
  }
  return s;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_weather_model`
Expected: `20 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/weather_model test/test_weather_model
git commit -m "feat: weather_model display conversion, PT labels, art slots"
```

---

### Task 3: `weather_model` — UTC offset → POSIX TZ string

**Files:**
- Modify: `lib/weather_model/weather_model.h`
- Modify: `lib/weather_model/weather_model.cpp`
- Test: `test/test_weather_model/test_main.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `std::string posixTzFromOffset(int32_t offsetSeconds)` —
  `offsetSeconds` is ip-api's `offset` field (seconds **east** of UTC).
  Task 6 passes the result to `TimeService::setTimezone()` (roadmap §4.8 +
  deviation §5.4: fixed offset, no IANA names, no DST table).

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_weather_model/test_main.cpp`:

```cpp
static void test_tz_utc_minus_3() {
  // The spec's own example: -10800 -> "<-03>3".
  TEST_ASSERT_EQUAL_STRING("<-03>3", posixTzFromOffset(-10800).c_str());
}

static void test_tz_utc_plus_2() {
  // POSIX offsets are inverted (seconds WEST of UTC): east zones are negative.
  TEST_ASSERT_EQUAL_STRING("<+02>-2", posixTzFromOffset(7200).c_str());
}

static void test_tz_utc_zero() {
  TEST_ASSERT_EQUAL_STRING("<+00>0", posixTzFromOffset(0).c_str());
}

static void test_tz_half_hour_east() {
  // India, UTC+5:30.
  TEST_ASSERT_EQUAL_STRING("<+0530>-5:30", posixTzFromOffset(19800).c_str());
}

static void test_tz_half_hour_west() {
  // Newfoundland, UTC-3:30.
  TEST_ASSERT_EQUAL_STRING("<-0330>3:30", posixTzFromOffset(-12600).c_str());
}

static void test_tz_quarter_hour() {
  // Nepal, UTC+5:45.
  TEST_ASSERT_EQUAL_STRING("<+0545>-5:45", posixTzFromOffset(20700).c_str());
}
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_tz_utc_minus_3);
  RUN_TEST(test_tz_utc_plus_2);
  RUN_TEST(test_tz_utc_zero);
  RUN_TEST(test_tz_half_hour_east);
  RUN_TEST(test_tz_half_hour_west);
  RUN_TEST(test_tz_quarter_hour);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_weather_model`
Expected: **build error** — `'posixTzFromOffset' was not declared in this scope`

- [ ] **Step 3: Write the implementation**

In `lib/weather_model/weather_model.h`, add after `artSlots`:

```cpp
// ip-api `offset` (seconds EAST of UTC) -> fixed-offset POSIX TZ string for
// TimeService::setTimezone(), e.g. -10800 -> "<-03>3" (roadmap §4.8 +
// deviation 4: no IANA names, no DST — the offset refreshes on the next
// geolocation).
std::string posixTzFromOffset(int32_t offsetSeconds);
```

In `lib/weather_model/weather_model.cpp`, add `#include <cstdio>` under
`#include "weather_model.h"`, then add at the end:

```cpp
std::string posixTzFromOffset(int32_t offsetSeconds) {
  const int32_t abs = offsetSeconds < 0 ? -offsetSeconds : offsetSeconds;
  const int h = abs / 3600;
  const int m = (abs % 3600) / 60;

  // The <name> mirrors the UTC offset ("<-03>", "<+0530>"); the POSIX offset
  // after it is inverted — hours WEST of UTC — per IEEE 1003.1 TZ format.
  const char nameSign = offsetSeconds < 0 ? '-' : '+';
  const char* posixSign = offsetSeconds > 0 ? "-" : "";
  char buf[20];
  if (m == 0) {
    std::snprintf(buf, sizeof(buf), "<%c%02d>%s%d", nameSign, h, posixSign, h);
  } else {
    std::snprintf(buf, sizeof(buf), "<%c%02d%02d>%s%d:%02d", nameSign, h, m,
                  posixSign, h, m);
  }
  return buf;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_weather_model`
Expected: `26 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/weather_model test/test_weather_model
git commit -m "feat: weather_model UTC offset to POSIX TZ string"
```

---

### Task 4: ArduinoJson dependency + `weather_parse` — geolocation parsing

**Files:**
- Modify: `platformio.ini` (both envs)
- Create: `lib/weather_parse/weather_parse.h`
- Create: `lib/weather_parse/weather_parse.cpp`
- Test: `test/test_weather_parse/test_main.cpp`

**Interfaces:**
- Consumes: ArduinoJson `^7.4.0` (platform-independent — roadmap §3 footnote
  explicitly allows it in this lib and on native).
- Produces: `struct GeoResult { bool ok; float lat; float lon; std::string city; int32_t offsetSec; }`
  and `GeoResult parseGeo(const char* json)`. Task 6's fetch pipeline consumes
  them.

- [ ] **Step 1: Add ArduinoJson to both envs**

In `platformio.ini`, append to `[env:cyd]`'s `lib_deps`:

```ini
    bblanchon/ArduinoJson@^7.4.0
```

and add to `[env:native]` (which currently has no `lib_deps`):

```ini
lib_deps = bblanchon/ArduinoJson@^7.4.0
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_weather_parse/test_main.cpp`:

```cpp
// Host-side tests for weather_parse (pio test -e native): canned ip-api and
// Open-Meteo responses -> plain structs. The canned JSON mirrors the real
// API shapes for the exact spec-pinned request URLs.
#include <unity.h>

#include <weather_parse.h>

void setUp() {}
void tearDown() {}

// ip-api.com/json/?fields=status,country,city,lat,lon,timezone,offset
static const char* kGeoOk = R"({
  "status": "success",
  "country": "Brazil",
  "city": "Curitiba",
  "lat": -25.4284,
  "lon": -49.2733,
  "timezone": "America/Sao_Paulo",
  "offset": -10800
})";

static void test_geo_success() {
  const GeoResult g = parseGeo(kGeoOk);
  TEST_ASSERT_TRUE(g.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -25.4284f, g.lat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -49.2733f, g.lon);
  TEST_ASSERT_EQUAL_STRING("Curitiba", g.city.c_str());
  TEST_ASSERT_EQUAL_INT32(-10800, g.offsetSec);
}

static void test_geo_fail_status() {
  // ip-api answers 200 with status:"fail" for private/reserved IPs.
  const GeoResult g = parseGeo(
      R"({"status":"fail","message":"private range","query":"192.168.0.1"})");
  TEST_ASSERT_FALSE(g.ok);
}

static void test_geo_malformed_json() {
  TEST_ASSERT_FALSE(parseGeo("not json at all").ok);
  TEST_ASSERT_FALSE(parseGeo("").ok);
  TEST_ASSERT_FALSE(parseGeo(R"({"status":"success","lat":)").ok);
}

static void test_geo_missing_coords() {
  TEST_ASSERT_FALSE(parseGeo(R"({"status":"success","city":"X"})").ok);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geo_success);
  RUN_TEST(test_geo_fail_status);
  RUN_TEST(test_geo_malformed_json);
  RUN_TEST(test_geo_missing_coords);
  return UNITY_END();
}
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `pio test -e native -f test_weather_parse`
Expected: **build error** — `weather_parse.h: No such file or directory`

- [ ] **Step 4: Write the implementation**

Create `lib/weather_parse/weather_parse.h`:

```cpp
// lib/weather_parse/weather_parse.h — geolocation + forecast JSON -> plain
// structs (A3, roadmap §3). Uses ArduinoJson (platform-independent; runs on
// native). Callers never touch JSON: they get these structs or ok=false.
#pragma once

#include <cstdint>
#include <string>

// ip-api.com geolocation (fields=status,country,city,lat,lon,timezone,offset).
struct GeoResult {
  bool ok = false;
  float lat = 0.0f;
  float lon = 0.0f;
  std::string city;
  int32_t offsetSec = 0;  // seconds EAST of UTC (ip-api "offset")
};
GeoResult parseGeo(const char* json);
```

Create `lib/weather_parse/weather_parse.cpp`:

```cpp
#include "weather_parse.h"

#include <ArduinoJson.h>

GeoResult parseGeo(const char* json) {
  GeoResult r;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return r;
  if (doc["status"] != "success") return r;
  if (!doc["lat"].is<float>() || !doc["lon"].is<float>()) return r;
  r.lat = doc["lat"].as<float>();
  r.lon = doc["lon"].as<float>();
  r.city = doc["city"] | "";
  r.offsetSec = doc["offset"] | 0;
  r.ok = true;
  return r;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `pio test -e native -f test_weather_parse`
Expected: `4 Tests 0 Failures 0 Ignored` — PASSED (PlatformIO downloads
ArduinoJson on the first run)

- [ ] **Step 6: Commit**

```bash
git add platformio.ini lib/weather_parse test/test_weather_parse
git commit -m "feat: weather_parse ip-api geolocation parsing (+ArduinoJson dep)"
```

---

### Task 5: `weather_parse` — forecast parsing + URL builder

**Files:**
- Modify: `lib/weather_parse/weather_parse.h`
- Modify: `lib/weather_parse/weather_parse.cpp`
- Test: `test/test_weather_parse/test_main.cpp`

**Interfaces:**
- Consumes: Task 4's ArduinoJson setup.
- Produces:
  `struct CurrentWx { float tempC; int humidity; bool isDay; int wmoCode; float windKmh; }`,
  `struct DayWx { int wmoCode; float tmaxC; float tminC; }`,
  `struct ForecastWx { bool ok; CurrentWx current; DayWx days[3]; int dayCount; }`,
  `ForecastWx parseForecast(const char* json)`,
  `std::string forecastUrl(float lat, float lon)` (the spec-pinned Open-Meteo
  request). Tasks 6–7 consume all of them.

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_weather_parse/test_main.cpp`:

```cpp
// Open-Meteo /v1/forecast response for the spec-pinned query (trimmed to the
// fields we request; extra fields like *_units are present in real replies
// and must be ignored gracefully).
static const char* kForecastOk = R"({
  "latitude": -25.5,
  "longitude": -49.25,
  "utc_offset_seconds": -10800,
  "current_units": {"temperature_2m": "°C", "wind_speed_10m": "km/h"},
  "current": {
    "time": "2026-07-06T14:15",
    "interval": 900,
    "temperature_2m": 18.3,
    "relative_humidity_2m": 62,
    "is_day": 1,
    "weather_code": 3,
    "wind_speed_10m": 9.8
  },
  "daily_units": {"temperature_2m_max": "°C"},
  "daily": {
    "time": ["2026-07-06", "2026-07-07", "2026-07-08"],
    "weather_code": [3, 61, 0],
    "temperature_2m_max": [19.1, 15.2, 21.0],
    "temperature_2m_min": [11.4, 9.8, 8.9]
  }
})";

static void test_forecast_current() {
  const ForecastWx f = parseForecast(kForecastOk);
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 18.3f, f.current.tempC);
  TEST_ASSERT_EQUAL_INT(62, f.current.humidity);
  TEST_ASSERT_TRUE(f.current.isDay);
  TEST_ASSERT_EQUAL_INT(3, f.current.wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.8f, f.current.windKmh);
}

static void test_forecast_days() {
  const ForecastWx f = parseForecast(kForecastOk);
  TEST_ASSERT_EQUAL_INT(3, f.dayCount);
  TEST_ASSERT_EQUAL_INT(3, f.days[0].wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 19.1f, f.days[0].tmaxC);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.4f, f.days[0].tminC);
  TEST_ASSERT_EQUAL_INT(61, f.days[1].wmoCode);
  TEST_ASSERT_EQUAL_INT(0, f.days[2].wmoCode);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.9f, f.days[2].tminC);
}

static void test_forecast_missing_current() {
  TEST_ASSERT_FALSE(parseForecast(R"({"latitude":-25.5})").ok);
}

static void test_forecast_malformed() {
  TEST_ASSERT_FALSE(parseForecast("<html>502 Bad Gateway</html>").ok);
  TEST_ASSERT_FALSE(parseForecast("").ok);
}

static void test_forecast_short_daily_arrays() {
  // Fewer daily entries than requested must not crash or over-read.
  const ForecastWx f = parseForecast(R"({
    "current": {"temperature_2m": 5.0, "relative_humidity_2m": 80,
                "is_day": 0, "weather_code": 71, "wind_speed_10m": 20.1},
    "daily": {"weather_code": [71], "temperature_2m_max": [2.0],
              "temperature_2m_min": [-4.0]}
  })");
  TEST_ASSERT_TRUE(f.ok);
  TEST_ASSERT_FALSE(f.current.isDay);
  TEST_ASSERT_EQUAL_INT(1, f.dayCount);
  TEST_ASSERT_EQUAL_INT(71, f.days[0].wmoCode);
}

static void test_forecast_url_pins_spec_query() {
  const std::string url = forecastUrl(-25.4284f, -49.2733f);
  TEST_ASSERT_EQUAL_STRING(
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=-25.4284&longitude=-49.2733"
      "&current=temperature_2m,relative_humidity_2m,is_day,weather_code,"
      "wind_speed_10m"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&forecast_days=3",
      url.c_str());
}
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_forecast_current);
  RUN_TEST(test_forecast_days);
  RUN_TEST(test_forecast_missing_current);
  RUN_TEST(test_forecast_malformed);
  RUN_TEST(test_forecast_short_daily_arrays);
  RUN_TEST(test_forecast_url_pins_spec_query);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_weather_parse`
Expected: **build error** — `'ForecastWx' does not name a type`

- [ ] **Step 3: Write the implementation**

In `lib/weather_parse/weather_parse.h`, add after `parseGeo`:

```cpp
// Open-Meteo /v1/forecast, current + 3-day daily (spec-pinned query).
struct CurrentWx {
  float tempC = 0.0f;
  int humidity = 0;      // %
  bool isDay = true;
  int wmoCode = -1;      // WMO weather_code; -1 maps to Condition::Unknown
  float windKmh = 0.0f;
};

struct DayWx {
  int wmoCode = -1;
  float tmaxC = 0.0f;
  float tminC = 0.0f;
};

struct ForecastWx {
  bool ok = false;
  CurrentWx current{};
  DayWx days[3]{};       // days[0] = today
  int dayCount = 0;      // 0..3 actually filled
};
ForecastWx parseForecast(const char* json);

// The spec-pinned Open-Meteo request for these coordinates.
std::string forecastUrl(float lat, float lon);
```

In `lib/weather_parse/weather_parse.cpp`, add `#include <cstdio>` under
`#include <ArduinoJson.h>`, then add after `parseGeo`:

```cpp
ForecastWx parseForecast(const char* json) {
  ForecastWx f;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return f;

  JsonObjectConst cur = doc["current"];
  if (cur.isNull() || !cur["temperature_2m"].is<float>()) return f;
  f.current.tempC = cur["temperature_2m"].as<float>();
  f.current.humidity = cur["relative_humidity_2m"] | 0;
  f.current.isDay = (cur["is_day"] | 1) != 0;
  f.current.wmoCode = cur["weather_code"] | -1;
  f.current.windKmh = cur["wind_speed_10m"] | 0.0f;

  JsonArrayConst codes = doc["daily"]["weather_code"];
  JsonArrayConst tmax = doc["daily"]["temperature_2m_max"];
  JsonArrayConst tmin = doc["daily"]["temperature_2m_min"];
  const size_t n = codes.size() < tmax.size() ? codes.size() : tmax.size();
  for (size_t i = 0; i < n && i < tmin.size() && f.dayCount < 3; ++i) {
    f.days[f.dayCount].wmoCode = codes[i] | -1;
    f.days[f.dayCount].tmaxC = tmax[i] | 0.0f;
    f.days[f.dayCount].tminC = tmin[i] | 0.0f;
    ++f.dayCount;
  }
  f.ok = true;
  return f;
}

std::string forecastUrl(float lat, float lon) {
  char buf[256];
  std::snprintf(
      buf, sizeof(buf),
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=%.4f&longitude=%.4f"
      "&current=temperature_2m,relative_humidity_2m,is_day,weather_code,"
      "wind_speed_10m"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&forecast_days=3",
      static_cast<double>(lat), static_cast<double>(lon));
  return buf;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_weather_parse`
Expected: `10 Tests 0 Failures 0 Ignored` — PASSED

Also run the full suite: `pio test -e native`
Expected: all test dirs PASSED (including `test_weather_model` 26 and
`test_weather_parse` 10).

- [ ] **Step 5: Commit**

```bash
git add lib/weather_parse test/test_weather_parse
git commit -m "feat: weather_parse Open-Meteo forecast parsing + URL builder"
```

---

### Task 6: `WeatherFetch` pipeline + boot prefetch

**Files:**
- Create: `src/apps/weather/WeatherFetch.h`
- Create: `src/apps/weather/WeatherFetch.cpp`
- Modify: `src/main.cpp` (fill F4's `// A3: weather boot prefetch hook`)

**Interfaces:**
- Consumes: `parseGeo`/`parseForecast`/`forecastUrl` (Tasks 4–5),
  `posixTzFromOffset` (Task 3), `ISettingsStore` (F3),
  `TimeService::setTimezone/today` (F4), `dateKey` (F4 `date_utils` — use the
  include form recorded in Task 0), `HTTPClient` + `WiFiClientSecure`
  (arduino-esp32).
- Produces: `bool weatherRefresh(ISettingsStore& store, TimeService& time)` —
  **assumes the radio is up and WiFi is connected** (callers handle that:
  the boot flow already connected; the app calls `WiFiService::connect()`
  first). Task 7's app and the boot hook both call it.

No native test — this is thin HTTP/NVS glue over the tested libs; the device
build is the check and Task 9 verifies behavior on hardware.

- [ ] **Step 1: Write the header**

Create `src/apps/weather/WeatherFetch.h`:

```cpp
// src/apps/weather/WeatherFetch.h — the fetch/cache pipeline shared by the
// boot-time prefetch (spec §3.4 step 4) and the Weather app (A3). Callers
// must already have WiFi connected (RadioManager/WiFiService are their job —
// this file never touches radio power).
#pragma once

#include <settings_store.h>

class TimeService;

// Geolocate if needed (auto mode + no stored coords, or first fetch of a new
// local day — which also refreshes the fixed TZ offset), fetch the Open-Meteo
// forecast, and cache it (NVS wx.json + wx.day). Returns true when a fresh
// forecast was parsed and cached.
bool weatherRefresh(ISettingsStore& store, TimeService& time);
```

- [ ] **Step 2: Write the implementation**

Create `src/apps/weather/WeatherFetch.cpp` (adjust the `date_utils` include to
the form Task 0 recorded):

```cpp
#include "apps/weather/WeatherFetch.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <date_utils/date_utils.h>
#include <weather_model.h>
#include <weather_parse.h>

#include "services/TimeService.h"

namespace {
// Out-of-range latitude = "no stored location yet" (valid range is ±90).
constexpr float kNoCoord = 999.0f;
// NVS string values cap out near 4000 bytes; a 3-day forecast is ~800.
constexpr size_t kMaxCacheLen = 3500;

// One geolocation call: ip-api free tier is HTTP-only, ~45 req/min — the
// caller's freshness rule keeps this rare. URL is spec-pinned.
bool geolocate(ISettingsStore& store, TimeService& time) {
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(
      "http://ip-api.com/json/"
      "?fields=status,country,city,lat,lon,timezone,offset");
  const int code = http.GET();
  if (code != 200) {
    http.end();
    Serial.printf("[wx] geo http %d\n", code);
    return false;
  }
  const String body = http.getString();
  http.end();

  const GeoResult geo = parseGeo(body.c_str());
  if (!geo.ok) {
    Serial.println("[wx] geo parse failed");
    return false;
  }

  store.setFloat("loc.lat", geo.lat);
  store.setFloat("loc.lon", geo.lon);
  store.setString("loc.city", geo.city);
  // Fixed-offset TZ from the geolocation (roadmap §4.8 + deviation 4).
  time.setTimezone(posixTzFromOffset(geo.offsetSec));
  Serial.printf("[wx] geo %s %.4f,%.4f offset=%d\n", geo.city.c_str(),
                static_cast<double>(geo.lat), static_cast<double>(geo.lon),
                static_cast<int>(geo.offsetSec));
  return true;
}
}  // namespace

bool weatherRefresh(ISettingsStore& store, TimeService& time) {
  const bool manual = store.getU32("loc.mode", 0) == 1;
  float lat = store.getFloat("loc.lat", kNoCoord);
  float lon = store.getFloat("loc.lon", kNoCoord);

  // Auto mode re-geolocates when there are no coords yet, or on the first
  // fetch of a new local day (wx.day is the last successful fetch's dateKey).
  const uint32_t todayKey = dateKey(time.today());
  const bool newDay = todayKey != 0 && store.getU32("wx.day", 0) != todayKey;
  if (!manual && (lat == kNoCoord || lon == kNoCoord || newDay)) {
    if (geolocate(store, time)) {
      lat = store.getFloat("loc.lat", kNoCoord);
      lon = store.getFloat("loc.lon", kNoCoord);
    }
  }
  if (lat == kNoCoord || lon == kNoCoord) return false;  // nowhere to look up

  WiFiClientSecure client;
  client.setInsecure();  // spec: HTTPS without cert pinning
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, forecastUrl(lat, lon).c_str());
  const int code = http.GET();
  if (code != 200) {
    http.end();
    Serial.printf("[wx] forecast http %d\n", code);
    return false;
  }
  const String body = http.getString();
  http.end();

  if (!parseForecast(body.c_str()).ok) {
    Serial.println("[wx] forecast parse failed");
    return false;
  }
  if (body.length() >= kMaxCacheLen) {
    Serial.println("[wx] body too large to cache");
    return false;
  }
  store.setString("wx.json", body.c_str());
  store.setU32("wx.day", todayKey);
  Serial.printf("[wx] forecast cached (%u bytes)\n", body.length());
  return true;
}
```

- [ ] **Step 3: Fill the boot prefetch hook**

In `src/main.cpp`, add the include after the other `apps/` includes:

```cpp
#include "apps/weather/WeatherFetch.h"
```

Replace F4's marker line `// A3: weather boot prefetch hook` (inside the
boot-step-4 block, right after `timeService.syncNow();`) with:

```cpp
      weatherRefresh(settings, timeService);  // A3 boot prefetch (spec §3.4 step 4)
```

- [ ] **Step 4: Verify device build + native suite**

Run: `pio run -e cyd && pio test -e native`
Expected: device build `SUCCESS`; all native tests still PASS.

- [ ] **Step 5: Commit**

```bash
git add src/apps/weather/WeatherFetch.h src/apps/weather/WeatherFetch.cpp src/main.cpp
git commit -m "feat: weather fetch/cache pipeline + boot-time prefetch"
```

---

### Task 7: `WeatherApp` UI wrapper + registration

**Files:**
- Create: `src/apps/weather/WeatherApp.h`
- Create: `src/apps/weather/WeatherApp.cpp`
- Modify: `src/main.cpp` (include, instance, `setDeps`, registration)

**Interfaces:**
- Consumes: everything above; `App` from `src/core/App.h`; `catalog::kWeather`
  from `src/apps/app_catalog.h`; `WiFiService::isConnected/connect`,
  `TimeService` (passed through to `weatherRefresh`),
  `StorageService::exists` (art placeholders), `ISettingsStore`
  (`units.f`, `loc.city`, `wx.json`).
- Produces: `class WeatherApp : public App` with
  `void setDeps(ISettingsStore&, WiFiService&, TimeService&, StorageService&)`
  — `main.cpp` creates one static instance, calls `setDeps`, and registers it
  in the **first** grid slot (replacing `weatherStub`).

No native test — thin LVGL glue over the tested libs; the device build is the
check and Task 9 verifies behavior on hardware.

- [ ] **Step 1: Write the header**

Create `src/apps/weather/WeatherApp.h`:

```cpp
// src/apps/weather/WeatherApp.h — Weather app (A3, spec §4.1). Thin LVGL
// wrapper: renders the cached forecast instantly, refreshes over WiFi from
// tick() (first ~400 ms after entry, then every 20 min while open). All
// mapping lives in lib/weather_model, parsing in lib/weather_parse, fetching
// in WeatherFetch — this file only draws.
#pragma once

#include <settings_store.h>
#include <weather_parse.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class WiFiService;
class TimeService;
class StorageService;

class WeatherApp : public App {
 public:
  // Call once from main.cpp before registerApp.
  void setDeps(ISettingsStore& store, WiFiService& wifi, TimeService& time,
               StorageService& storage);

  const char* id() const override { return "weather"; }
  const char* title() const override { return catalog::kWeather.title; }
  const char* iconPath() const override { return catalog::kWeather.icon; }
  RadioMode requiredRadio() const override { return RadioMode::WiFi; }
  void onEnter() override;
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;

 private:
  void render(const ForecastWx& f, bool stale);  // full rebuild of root_
  void renderEmpty();          // no data at all: friendly hint (spec §6.5)
  bool renderCached();         // true if a parseable cache was rendered
  void refreshNow(uint32_t now_ms);  // blocking: connect + fetch + re-render
  void setStatus(const char* msg);   // top-right marker (atualizando/stale)

  ISettingsStore* store_ = nullptr;
  WiFiService* wifi_ = nullptr;
  TimeService* time_ = nullptr;
  StorageService* storage_ = nullptr;

  lv_obj_t* root_ = nullptr;       // launcher-owned 240x288 container
  lv_obj_t* statusLbl_ = nullptr;  // child of root_, recreated per render

  bool pendingRefresh_ = false;
  uint32_t enteredMs_ = 0;
  uint32_t lastFetchMs_ = 0;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/apps/weather/WeatherApp.cpp`:

```cpp
#include "apps/weather/WeatherApp.h"

#include <Arduino.h>

#include <cmath>

#include <weather_model.h>

#include "apps/weather/WeatherFetch.h"
#include "services/StorageService.h"
#include "services/WiFiService.h"

namespace {
constexpr uint32_t kRefreshMs = 20u * 60u * 1000u;  // spec: ~15-30 min timer
constexpr uint32_t kFirstFetchDelayMs = 400;  // let the cached frame paint

const char* kDayNames[3] = {"Hoje", "Amanhã", "Depois"};

// One art slot: the SD image when present, else a flat colored box (roadmap
// §4.1 placeholder rule — hand-drawn art arrives incrementally). A nullptr
// path means the condition has no such slot: render nothing.
lv_obj_t* makeArtSlot(lv_obj_t* parent, StorageService& storage,
                      const char* lvglPath, lv_coord_t w, lv_coord_t h,
                      lv_color_t fallback) {
  lv_obj_t* img = lv_img_create(parent);
  lv_obj_set_size(img, w, h);
  if (lvglPath != nullptr && storage.exists(lvglPath + 2)) {  // "S:/x" -> "/x"
    lv_img_set_src(img, lvglPath);
  } else if (lvglPath != nullptr) {
    lv_obj_set_style_bg_color(img, fallback, 0);
    lv_obj_set_style_bg_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(img, 6, 0);
  } else {
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
  }
  return img;
}

// White-backed label so readings stay legible over any background art.
lv_obj_t* makeReadout(lv_obj_t* parent) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_bg_color(lbl, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(lbl, LV_OPA_70, 0);
  lv_obj_set_style_pad_hor(lbl, 4, 0);
  lv_obj_set_style_radius(lbl, 4, 0);
  return lbl;
}

int shownTemp(float celsius, bool useF) {
  return static_cast<int>(lroundf(toDisplayTemp(celsius, useF)));
}
}  // namespace

void WeatherApp::setDeps(ISettingsStore& store, WiFiService& wifi,
                         TimeService& time, StorageService& storage) {
  store_ = &store;
  wifi_ = &wifi;
  time_ = &time;
  storage_ = &storage;
}

void WeatherApp::onEnter() {
  enteredMs_ = millis();
  lastFetchMs_ = 0;
  pendingRefresh_ = true;
}

void WeatherApp::buildUI(lv_obj_t* parent) {
  root_ = parent;
  // Cache first (spec: instant display); the network refresh runs from
  // tick() so this frame reaches the screen before we block on WiFi.
  if (!renderCached()) renderEmpty();
}

void WeatherApp::onExit() {
  root_ = nullptr;  // launcher deletes the widgets after this
  statusLbl_ = nullptr;
}

void WeatherApp::tick(uint32_t now_ms) {
  if (root_ == nullptr) return;
  if (pendingRefresh_) {
    if (now_ms - enteredMs_ < kFirstFetchDelayMs) return;
    pendingRefresh_ = false;
    refreshNow(now_ms);
    return;
  }
  if (now_ms - lastFetchMs_ >= kRefreshMs) refreshNow(now_ms);
}

bool WeatherApp::renderCached() {
  const std::string cached = store_->getString("wx.json", "");
  if (cached.empty()) return false;
  const ForecastWx f = parseForecast(cached.c_str());
  if (!f.ok) return false;
  render(f, /*stale=*/true);
  return true;
}

void WeatherApp::render(const ForecastWx& f, bool stale) {
  lv_obj_clean(root_);
  statusLbl_ = nullptr;

  const bool useF = store_->getBool("units.f", false);
  const Condition cond = conditionFromWmo(f.current.wmoCode);
  const ArtSlots art =
      artSlots(tempBand(f.current.tempC), cond, f.current.isDay);

  // Background fills the whole 240x288 app container.
  lv_obj_t* bg = makeArtSlot(root_, *storage_, art.background, 240, 288,
                             lv_palette_lighten(LV_PALETTE_BLUE, 4));
  lv_obj_set_pos(bg, 0, 0);
  lv_obj_set_style_radius(bg, 0, 0);

  // Character: outfit box with the condition accessory at its shoulder.
  lv_obj_t* outfit = makeArtSlot(root_, *storage_, art.outfit, 110, 130,
                                 lv_palette_main(LV_PALETTE_GREY));
  lv_obj_align(outfit, LV_ALIGN_TOP_MID, 0, 64);
  lv_obj_t* overlay = makeArtSlot(root_, *storage_, art.overlay, 48, 48,
                                  lv_palette_main(LV_PALETTE_ORANGE));
  lv_obj_align(overlay, LV_ALIGN_TOP_MID, 52, 56);

  // Readings (spec): current temp + condition, city, today's high/low.
  lv_obj_t* temp = makeReadout(root_);
  lv_label_set_text_fmt(temp, "%d°%c  %s", shownTemp(f.current.tempC, useF),
                        useF ? 'F' : 'C', conditionLabelPt(cond));
  lv_obj_set_pos(temp, 8, 8);

  lv_obj_t* city = makeReadout(root_);
  lv_label_set_text(city, store_->getString("loc.city", "").c_str());
  lv_obj_set_pos(city, 8, 30);

  if (f.dayCount > 0) {
    lv_obj_t* hilo = makeReadout(root_);
    lv_label_set_text_fmt(hilo, LV_SYMBOL_UP "%d°  " LV_SYMBOL_DOWN "%d°",
                          shownTemp(f.days[0].tmaxC, useF),
                          shownTemp(f.days[0].tminC, useF));
    lv_obj_align(hilo, LV_ALIGN_TOP_MID, 0, 200);
  }

  // Mini-forecast strip along the bottom (spec: 2-3 day row).
  lv_obj_t* row = lv_obj_create(root_);
  lv_obj_remove_style_all(row);
  lv_obj_set_style_bg_color(row, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_70, 0);
  lv_obj_set_size(row, 240, 58);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  for (int i = 0; i < f.dayCount; ++i) {
    lv_obj_t* day = lv_label_create(row);
    lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(day, "%s\n%s\n%d°/%d°", kDayNames[i],
                          conditionLabelPt(conditionFromWmo(f.days[i].wmoCode)),
                          shownTemp(f.days[i].tmaxC, useF),
                          shownTemp(f.days[i].tminC, useF));
  }

  if (stale) setStatus(LV_SYMBOL_WARNING " desatualizado");
}

void WeatherApp::renderEmpty() {
  lv_obj_clean(root_);
  statusLbl_ = nullptr;
  lv_obj_t* lbl = lv_label_create(root_);
  lv_label_set_text(lbl,
                    "Não consegui ver o céu agora.\n\n"
                    "Verifique o WiFi em Configurações.");
  lv_obj_set_width(lbl, 224);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
}

void WeatherApp::setStatus(const char* msg) {
  if (root_ == nullptr) return;
  if (statusLbl_ == nullptr) {
    statusLbl_ = makeReadout(root_);
    lv_obj_align(statusLbl_, LV_ALIGN_TOP_RIGHT, -8, 8);
  }
  lv_label_set_text(statusLbl_, msg);
}

void WeatherApp::refreshNow(uint32_t now_ms) {
  lastFetchMs_ = now_ms;
  setStatus(LV_SYMBOL_REFRESH " atualizando");
  lv_refr_now(nullptr);  // paint before the blocking connect + fetch

  // The Launcher already put the radio in WiFi mode (requiredRadio); we only
  // need the connection itself.
  const bool online = wifi_->isConnected() || wifi_->connect();
  if (online && weatherRefresh(*store_, *time_)) {
    const ForecastWx f =
        parseForecast(store_->getString("wx.json", "").c_str());
    if (f.ok) {
      render(f, /*stale=*/false);
      return;
    }
  }
  // Failed: keep what's on screen, marked honestly (spec §6.5).
  if (store_->getString("wx.json", "").empty()) {
    renderEmpty();
  } else {
    setStatus(LV_SYMBOL_WARNING " sem WiFi");
  }
}
```

- [ ] **Step 3: Replace the stub registration in main.cpp**

In `src/main.cpp`, add the include next to the `WeatherFetch.h` one:

```cpp
#include "apps/weather/WeatherApp.h"
```

Replace `static StubApp weatherStub("weather", catalog::kWeather);` with:

```cpp
static WeatherApp weatherApp;
```

Replace `launcher.registerApp(&weatherStub);` with — **same position, first
registration, so the grid order is unchanged**:

```cpp
  weatherApp.setDeps(settings, wifiService, timeService, storage);
  launcher.registerApp(&weatherApp);
```

Do not touch the `setAppEnabled("weather", false)` block — the SD-missing
behavior (spec §6.5) is already wired by F3.

- [ ] **Step 4: Build for the device and re-run native tests**

Run: `pio run -e cyd && pio test -e native`
Expected: `SUCCESS`; all native tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/apps/weather src/main.cpp
git commit -m "feat: Weather app — character, readings, cache-first refresh"
```

---

### Task 8: Settings → Local do clima

**Files:**
- Create: `src/apps/settings/WeatherLocationSection.cpp`
- Modify: `src/apps/settings/Sections.h`
- Modify: `src/apps/settings/SettingsApp.cpp` (`kSectionNames` + `showSection`)

**Interfaces:**
- Consumes: F3's `Sections.h` builder pattern and `SettingsApp` shell
  (`kSectionNames` array + `showSection` switch); `ISettingsStore` — this
  section needs **no new `setDeps` parameter** (the store is already injected).
- Produces: `void buildWeatherLocationSection(lv_obj_t* parent, ISettingsStore& store);`
  Auto (default, geolocation) / manual override (lat/lon/city entry), per spec.
  Semantics: switching to **auto** clears `loc.*` so the next weather fetch
  re-geolocates; saving a **manual** location sets `loc.mode=1` + coords/city
  and clears `wx.day` so the next fetch is fresh for the new place.

- [ ] **Step 1: Declare the builder**

In `src/apps/settings/Sections.h`, add at the end of the builder list:

```cpp
void buildWeatherLocationSection(lv_obj_t* parent, ISettingsStore& store);  // A3
```

- [ ] **Step 2: Wire the shell**

In `src/apps/settings/SettingsApp.cpp`, append `"Local do clima"` as the
**last** entry of `kSectionNames` (after F4's WiFi/Clock entries), e.g.:

```cpp
const char* kSectionNames[] = {"Tela",    "Unidades", "Sobre",
                               "WiFi",    "Relógio",  "Local do clima"};
```

(Use whatever names F4 actually landed for entries 3–4 — only append; Task 0
recorded the array.) Then add the matching case to the `showSection` switch —
the case number is the new entry's index (5 if F4 landed two sections):

```cpp
    case 5:
      buildWeatherLocationSection(body, *store_);
      break;
```

- [ ] **Step 3: Implement the section**

Create `src/apps/settings/WeatherLocationSection.cpp`:

```cpp
// Settings -> Local do clima (A3 spec): auto (default — ip-api geolocation on
// the next weather fetch) vs. manual override (city/lat/lon typed in).
// Owns no radio and does no network itself — it only edits the loc.* keys the
// weather fetch pipeline reads.
#include <lvgl.h>

#include <cstdio>
#include <cstdlib>

#include "Sections.h"

namespace {
struct LocUi {
  ISettingsStore* store;
  lv_obj_t* modeSwitch;
  lv_obj_t* manualBox;  // hidden while auto
  lv_obj_t* city;
  lv_obj_t* lat;
  lv_obj_t* lon;
  lv_obj_t* status;
};
LocUi ui;  // one Settings screen at a time (single LVGL task) — safe

void refreshVisibility() {
  if (ui.store->getU32("loc.mode", 0) == 1) {
    lv_obj_clear_flag(ui.manualBox, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui.manualBox, LV_OBJ_FLAG_HIDDEN);
  }
}

void modeChanged(lv_event_t*) {
  const bool autoMode = lv_obj_has_state(ui.modeSwitch, LV_STATE_CHECKED);
  ui.store->setU32("loc.mode", autoMode ? 0 : 1);
  if (autoMode) {
    // Drop the stored place so the next weather fetch geolocates afresh.
    ui.store->remove("loc.lat");
    ui.store->remove("loc.lon");
    ui.store->remove("loc.city");
    lv_label_set_text(ui.status, "Local automático (pela internet)");
  } else {
    lv_label_set_text(ui.status, "Digite o local e salve");
  }
  refreshVisibility();
}

void kbEvent(lv_event_t* e) {
  // READY (checkmark) or CANCEL: done typing — the textarea keeps its text.
  lv_obj_del_async(lv_event_get_current_target(e));
}

void taClicked(lv_event_t* e) {
  lv_obj_t* ta = lv_event_get_current_target(e);
  lv_obj_t* kb = lv_keyboard_create(lv_layer_top());
  lv_keyboard_set_textarea(kb, ta);
  if (ta != ui.city) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_add_event_cb(kb, kbEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, kbEvent, LV_EVENT_CANCEL, nullptr);
}

lv_obj_t* makeEntry(lv_obj_t* parent, const char* placeholder,
                    const char* value) {
  lv_obj_t* ta = lv_textarea_create(parent);
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_textarea_set_text(ta, value);
  lv_obj_add_event_cb(ta, taClicked, LV_EVENT_CLICKED, nullptr);
  return ta;
}

void saveClicked(lv_event_t*) {
  const float lat = strtof(lv_textarea_get_text(ui.lat), nullptr);
  const float lon = strtof(lv_textarea_get_text(ui.lon), nullptr);
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    lv_label_set_text(ui.status, "Coordenadas inválidas");
    return;
  }
  ui.store->setU32("loc.mode", 1);
  ui.store->setFloat("loc.lat", lat);
  ui.store->setFloat("loc.lon", lon);
  ui.store->setString("loc.city", lv_textarea_get_text(ui.city));
  ui.store->remove("wx.day");  // next fetch is fresh for the new place
  lv_label_set_text(ui.status, "Local salvo " LV_SYMBOL_OK);
}
}  // namespace

void buildWeatherLocationSection(lv_obj_t* parent, ISettingsStore& store) {
  ui = {};
  ui.store = &store;

  const bool manual = store.getU32("loc.mode", 0) == 1;

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, "Local automático");
  ui.modeSwitch = lv_switch_create(row);
  if (!manual) lv_obj_add_state(ui.modeSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(ui.modeSwitch, modeChanged, LV_EVENT_VALUE_CHANGED,
                      nullptr);

  ui.manualBox = lv_obj_create(parent);
  lv_obj_set_width(ui.manualBox, LV_PCT(100));
  lv_obj_set_height(ui.manualBox, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(ui.manualBox, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(ui.manualBox, 8, 0);

  const float lat = store.getFloat("loc.lat", 999.0f);
  const float lon = store.getFloat("loc.lon", 999.0f);
  char num[16];
  ui.city = makeEntry(ui.manualBox, "Cidade",
                      store.getString("loc.city", "").c_str());
  snprintf(num, sizeof(num), "%.4f", static_cast<double>(lat));
  ui.lat = makeEntry(ui.manualBox, "Latitude", lat > 900.0f ? "" : num);
  snprintf(num, sizeof(num), "%.4f", static_cast<double>(lon));
  ui.lon = makeEntry(ui.manualBox, "Longitude", lon > 900.0f ? "" : num);

  lv_obj_t* saveBtn = lv_btn_create(ui.manualBox);
  lv_obj_t* saveLbl = lv_label_create(saveBtn);
  lv_label_set_text(saveLbl, LV_SYMBOL_OK " Salvar");
  lv_obj_add_event_cb(saveBtn, saveClicked, LV_EVENT_CLICKED, nullptr);

  ui.status = lv_label_create(parent);
  lv_label_set_text(ui.status, manual ? "Local manual"
                                      : "Local automático (pela internet)");

  refreshVisibility();
}
```

- [ ] **Step 4: Build for the device and re-run native tests**

Run: `pio run -e cyd && pio test -e native`
Expected: `SUCCESS`; all native tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/apps/settings/WeatherLocationSection.cpp src/apps/settings/Sections.h src/apps/settings/SettingsApp.cpp
git commit -m "feat: Settings -> Local do clima (auto/manual weather location)"
```

---

### Task 9: On-device verification (manual — needs the CYD + a WiFi network)

**Files:** none (verification only).

**Interfaces:** consumes the flashed firmware from Task 8. This is the roadmap
§1 A3 E2E gate: *"Character dressed for live local weather; stale-cache
fallback."* SD card inserted (weather is SD-gated in the launcher); art files
absent is fine — placeholders must show.

- [ ] **Step 1: Flash and prefetch**

Run: `pio run -e cyd -t upload` (CYD enumerates as `/dev/ttyUSB0`), then
`pio device monitor`.
With WiFi credentials already saved (F4): reboot and watch serial. Expected
order: `[wifi] connected`, `[time] syncNow ok`, `[wx] geo <your city> ...`
(first run of the day), `[wx] forecast cached (...)`, `[radio] state=0` (radio
dropped), `danios: launcher up` — boot prefetch works and the radio never
lingers.

- [ ] **Step 2: Live weather in the app**

Tap **Clima** (first grid slot, colored-letter fallback icon). Expected:
- The prefetched forecast renders instantly (no blank screen), briefly marked
  `desatualizado`, then `atualizando`, then the marker disappears (fresh).
- Character area: grey outfit box + orange accessory box (if the condition has
  one) over a light-blue background box — placeholders, since no art is on the
  card yet.
- Readings: current temp `NN°C` + PT condition; your city; today's
  high/low; bottom row `Hoje / Amanhã / Depois` with condition + max/min.
- Status bar shows the WiFi glyph while the app is open; back to the launcher
  → glyph returns to none within a second.
- Sanity-check the outfit/condition mapping against the real weather outside.

- [ ] **Step 3: °C/°F respect**

Settings → Unidades → switch to °F, reopen Clima. Expected: all temperatures
in °F (`64°F` etc.); serial still shows the API called in metric. Switch back.

- [ ] **Step 4: Stale-cache fallback (the E2E gate)**

Settings → WiFi → forget the network (or kill the router). Reopen Clima.
Expected: the cached weather renders with `desatualizado`, the refresh attempt
fails, marker becomes `sem WiFi` — app stays usable, no crash. Restore WiFi.

- [ ] **Step 5: No cache + no WiFi**

Run `pio run -e cyd -t erase` then re-upload (wipes NVS: credentials + cache).
Open Clima without configuring WiFi. Expected: the friendly
"Não consegui ver o céu agora / Verifique o WiFi em Configurações" screen.
Then re-join WiFi via Settings and confirm Clima recovers live weather.

- [ ] **Step 6: Manual location**

Settings → Local do clima → switch off "Local automático", enter Cidade
`Tóquio`, Latitude `35.68`, Longitude `139.69`, Salvar. Reopen Clima.
Expected: Tokyo's weather (plausibly different temps), city label `Tóquio`.
Serial shows **no** `[wx] geo` line (manual mode never geolocates).

- [ ] **Step 7: Back to auto + timezone**

Switch "Local automático" back on. Reopen Clima. Expected: serial shows a
fresh `[wx] geo` line, your city returns, and the status-bar clock still shows
the correct local time (TZ re-applied from the geolocation offset).

- [ ] **Step 8: Lifecycle + sleep**

Enter/leave Clima five times fast — no crash, no leftovers. Let the screen
sleep inside the app, tap to wake — the waking tap is swallowed (F3 shield)
and the app keeps refreshing on its 20-min timer.

- [ ] **Step 9: Commit any fixes found**

```bash
git add -A && git commit -m "fix: A3 on-device verification findings"
```
(Skip if nothing changed.)

---

## Definition of done

- [ ] `pio test -e native` — green, including `test_weather_model` (26) and
      `test_weather_parse` (10)
- [ ] `pio run -e cyd` — device build green
- [ ] Roadmap §1 A3 E2E outcome observed on hardware: character dressed for
      live local weather; stale-cache fallback (Task 9 checklist complete)
- [ ] Boot prefetch observed: forecast cached during boot step 4, radio
      dropped after (serial evidence)
- [ ] Settings → Local do clima works in both auto and manual modes
- [ ] Only the A3-reserved NVS keys touched (`loc.mode`, `loc.lat`, `loc.lon`,
      `loc.city`, `wx.json`, `wx.day`); no new SD paths outside
      `S:/art/weather/`
- [ ] `lib/weather_model/` has zero Arduino/LVGL includes;
      `lib/weather_parse/` includes only ArduinoJson; neither touches
      `WiFi.*`/radio power




