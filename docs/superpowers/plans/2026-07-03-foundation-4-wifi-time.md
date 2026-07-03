# Foundation 4 — WiFi + Time Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give danios a single-owner radio switch (RadioManager, WiFi arm), on-device WiFi setup with an on-screen keyboard, and a real clock (NTP + manual set) shown live in the status bar.

**Architecture:** Two pure modules are TDD'd natively first — `lib/date_utils/` (civil-date math every date-driven app reuses) and `lib/radio_policy/` (the WiFi-XOR-Bluetooth state machine as a pure transition table). Thin device services wrap them: `RadioManager` executes transition plans (WiFi arm real, Bluetooth arm stubbed until F5), `WiFiService` handles scan/credentials/connect, `TimeService` handles NTP with a POSIX-TZ fixed offset, manual set, and date/clock queries. The Settings screen gains WiFi and Clock sections, `main.cpp` gains spec §3.4 boot step 4 (brief WiFi-up → NTP sync → marked weather-prefetch hook → radio off), and the main loop drives the status-bar clock and radio glyph.

**Tech Stack:** Arduino-ESP32 (`WiFi.h`, `esp_wifi.h`, SNTP via `configTzTime`), LVGL 8.4 (`lv_keyboard`, `lv_roller`), Unity native tests, ISettingsStore (F3) for persistence.

**Prerequisites:** F1 (`2026-07-03-foundation-1-lvgl-touch.md` — DisplayService, TouchService, `[env:native]`), F2 (`2026-07-03-foundation-2-launcher.md` — App.h/RadioMode, Launcher with `setRadioRequest`, StatusBar with `setClockText`/`setRadio`, SettingsApp shell), F3 (`2026-07-03-foundation-3-storage-settings.md` — ISettingsStore/SettingsService, `Sections.h` pattern, `SettingsApp::setDeps`, boot flow steps 1–2) are merged. Where this plan grafts into their files it names the exact anchor.

## Global Constraints

(Copied from the roadmap — `2026-07-03-danios-roadmap.md` §2. That document's §4 interfaces are authoritative for every name below.)

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
- **Pins:** display HSPI (SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST −1, BL 27
  PWM); touch CST820 I²C (SDA 33, SCL 32, RST 25, INT 21 — poll), addr 0x15; SD on
  VSPI (SCK 18, MISO 19, MOSI 23, CS 5).
- **No battery-voltage ADC on this board** (roadmap deviation §5.1).
- **LVGL:** `lvgl/lvgl@8.4.0` (v8 API). `lv_conf.h` in `include/`. Two draw
  buffers of 240×30 px. UI code runs on the Arduino loop task only.
- **C++17** on both envs.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager`.
  No service or app touches `WiFi.*` / `esp_bt_*` power state directly —
  **this plan builds that single owner.**
- **TDD, native-first:** pure logic in `lib/<module>/` (std C++ only), tested
  with `pio test -e native` (Unity).
- **Commits:** small, frequent, conventional. Repo git-initialized in F1.
- **SD layout & NVS keys:** exactly as roadmap §4.1/§4.2. This plan owns
  `wifi.ssid`, `wifi.pass`, `tz` (POSIX TZ string, default `"UTC0"`).

## File map

| File | Task | Responsibility |
| --- | --- | --- |
| `lib/date_utils/date_utils.h` + `.cpp` (create) | 1 | `LocalDate`, `dateKey`, `fromDateKey`, `daysBetween` (roadmap §4.3 verbatim) |
| `test/test_date_utils/test_main.cpp` (create) | 1 | Native tests |
| `lib/radio_policy/radio_policy.h` + `.cpp` (create) | 2 | `RadioState`, `RadioAction`, `RadioPlan`, `planTransition` (roadmap §4.6 verbatim) |
| `test/test_radio_policy/test_main.cpp` (create) | 2 | Native tests — all 9 transitions |
| `src/services/RadioManager.h` + `.cpp` (create) | 3 | Executes RadioPlans; WiFi arm real, BT arm stubbed for F5 |
| `src/services/WiFiService.h` + `.cpp` (create) | 4 | Scan / credentials / connect (roadmap §4.7 verbatim) |
| `src/services/TimeService.h` + `.cpp` (create) | 5 | NTP + manual time, POSIX TZ (roadmap §4.8 verbatim) |
| `src/apps/settings/WifiSection.cpp` (create) | 6 | Settings → WiFi (scan, keyboard, connect, forget) |
| `src/apps/settings/ClockSection.cpp` (create) | 7 | Settings → Clock (Sync now, manual set) |
| `src/apps/settings/Sections.h` (modify) | 6, 7 | Add the two new builder declarations |
| `src/apps/settings/SettingsApp.{h,cpp}` (modify) | 6, 7 | Extend `setDeps`, `kSectionNames`, `showSection` |
| `src/main.cpp` (modify) | 3, 8 | Radio wiring, boot step 4, loop clock/glyph updates |

---

### Task 0: Preflight — verify the F1–F3 baseline

**Files:** none (verification only).

**Interfaces:**
- Consumes: the merged F1–F3 tree.
- Produces: confidence that this plan's anchors exist.

- [ ] **Step 1: Confirm builds and tests are green**

Run: `cd /home/lucca/repos/danios && pio test -e native && pio run -e cyd`
Expected: all F1–F3 native tests PASS; device build `SUCCESS`.

- [ ] **Step 2: Confirm the anchors this plan grafts onto**

Run: `grep -n "setRadioRequest" src/core/Launcher.h && grep -n "setDeps" src/apps/settings/SettingsApp.h && grep -n "kSectionNames" src/apps/settings/SettingsApp.cpp && grep -n "setClockText\|setRadio" src/core/StatusBar.h`
Expected: one hit each. If any is missing, stop — F2/F3 are not merged.

---

### Task 1: `lib/date_utils/` — civil-date math (native TDD)

**Files:**
- Create: `lib/date_utils/date_utils.h`
- Create: `lib/date_utils/date_utils.cpp`
- Test: `test/test_date_utils/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++ only).
- Produces (roadmap §4.3 — Oracle, Weather, Pet all consume these):
  `struct LocalDate { int16_t year; int8_t month; int8_t day; };`,
  `bool operator==(const LocalDate&, const LocalDate&)`,
  `uint32_t dateKey(const LocalDate&)`, `LocalDate fromDateKey(uint32_t)`,
  `int32_t daysBetween(const LocalDate& a, const LocalDate& b)` (= b − a).
  `dateKey == 0` is the universal "never / unknown" sentinel.

- [ ] **Step 1: Write the failing tests**

Create `test/test_date_utils/test_main.cpp`:

```cpp
#include <unity.h>

#include <date_utils/date_utils.h>

void setUp() {}
void tearDown() {}

void test_datekey_roundtrip() {
  LocalDate d{2026, 7, 3};
  TEST_ASSERT_EQUAL_UINT32(20260703u, dateKey(d));
  TEST_ASSERT_TRUE(fromDateKey(20260703u) == d);
}

void test_datekey_zero_sentinel() {
  LocalDate none{0, 0, 0};
  TEST_ASSERT_EQUAL_UINT32(0u, dateKey(none));
  TEST_ASSERT_TRUE(fromDateKey(0u) == none);
}

void test_days_between_same_day_is_zero() {
  LocalDate d{2026, 7, 3};
  TEST_ASSERT_EQUAL_INT32(0, daysBetween(d, d));
}

void test_days_between_simple() {
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2026, 7, 3}, {2026, 7, 4}));
  TEST_ASSERT_EQUAL_INT32(31, daysBetween({2026, 7, 1}, {2026, 8, 1}));
}

void test_days_between_across_year() {
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2025, 12, 31}, {2026, 1, 1}));
  TEST_ASSERT_EQUAL_INT32(365, daysBetween({2025, 1, 1}, {2026, 1, 1}));
}

void test_days_between_leap_year() {
  // 2024 is a leap year: Feb has 29 days.
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2024, 2, 28}, {2024, 2, 29}));
  TEST_ASSERT_EQUAL_INT32(2, daysBetween({2024, 2, 28}, {2024, 3, 1}));
  TEST_ASSERT_EQUAL_INT32(366, daysBetween({2024, 1, 1}, {2025, 1, 1}));
  // 2100 is NOT a leap year (divisible by 100, not by 400).
  TEST_ASSERT_EQUAL_INT32(1, daysBetween({2100, 2, 28}, {2100, 3, 1}));
}

void test_days_between_negative_span() {
  TEST_ASSERT_EQUAL_INT32(-1, daysBetween({2026, 7, 4}, {2026, 7, 3}));
  TEST_ASSERT_EQUAL_INT32(-365, daysBetween({2026, 1, 1}, {2025, 1, 1}));
}

void test_days_between_large_jump() {
  // Spec §4.5 clock-jump behavior relies on correct multi-year spans.
  TEST_ASSERT_EQUAL_INT32(9, daysBetween({2026, 6, 24}, {2026, 7, 3}));
  // 2000..2025 = 26*365 + 7 leap days = 9497; Jan 1 -> Jul 21 2026 = 201.
  TEST_ASSERT_EQUAL_INT32(9698, daysBetween({2000, 1, 1}, {2026, 7, 21}));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_datekey_roundtrip);
  RUN_TEST(test_datekey_zero_sentinel);
  RUN_TEST(test_days_between_same_day_is_zero);
  RUN_TEST(test_days_between_simple);
  RUN_TEST(test_days_between_across_year);
  RUN_TEST(test_days_between_leap_year);
  RUN_TEST(test_days_between_negative_span);
  RUN_TEST(test_days_between_large_jump);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_date_utils`
Expected: build FAILURE — `date_utils/date_utils.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `lib/date_utils/date_utils.h`:

```cpp
// danios date_utils — civil-date math shared by Oracle, Weather, and Pet.
// Pure std C++17; zero Arduino/LVGL includes (roadmap §2, tested on native).
#pragma once
#include <cstdint>

struct LocalDate {
  int16_t year;   // e.g. 2026; 0 = unknown
  int8_t month;   // 1..12;    0 = unknown
  int8_t day;     // 1..31;    0 = unknown
};

bool operator==(const LocalDate& a, const LocalDate& b);

// YYYYMMDD as u32 ({0,0,0} -> 0). 0 is the universal "never/unknown" sentinel.
uint32_t dateKey(const LocalDate& d);
LocalDate fromDateKey(uint32_t key);  // inverse; 0 -> {0,0,0}

// b - a in civil days (negative if b is before a).
int32_t daysBetween(const LocalDate& a, const LocalDate& b);
```

Create `lib/date_utils/date_utils.cpp`:

```cpp
#include "date_utils.h"

bool operator==(const LocalDate& a, const LocalDate& b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

uint32_t dateKey(const LocalDate& d) {
  if (d.year == 0 && d.month == 0 && d.day == 0) return 0;
  return static_cast<uint32_t>(d.year) * 10000u +
         static_cast<uint32_t>(d.month) * 100u + static_cast<uint32_t>(d.day);
}

LocalDate fromDateKey(uint32_t key) {
  if (key == 0) return {0, 0, 0};
  return {static_cast<int16_t>(key / 10000u),
          static_cast<int8_t>((key / 100u) % 100u),
          static_cast<int8_t>(key % 100u)};
}

namespace {
// Howard Hinnant's days_from_civil: serial day number from 1970-01-01.
// http://howardhinnant.github.io/date_algorithms.html — proleptic Gregorian.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);           // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}
}  // namespace

int32_t daysBetween(const LocalDate& a, const LocalDate& b) {
  return static_cast<int32_t>(
      daysFromCivil(b.year, static_cast<unsigned>(b.month),
                    static_cast<unsigned>(b.day)) -
      daysFromCivil(a.year, static_cast<unsigned>(a.month),
                    static_cast<unsigned>(a.day)));
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_date_utils`
Expected: `8 Tests 0 Failures 0 Ignored` — PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add lib/date_utils test/test_date_utils && \
  git commit -m "feat: add date_utils civil-date math (native TDD)"
```

---

### Task 2: `lib/radio_policy/` — WiFi-XOR-BT transition table (native TDD)

**Files:**
- Create: `lib/radio_policy/radio_policy.h`
- Create: `lib/radio_policy/radio_policy.cpp`
- Test: `test/test_radio_policy/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++ only).
- Produces (roadmap §4.6 verbatim — RadioManager executes these plans):
  `enum class RadioState : uint8_t { Off, WiFiOn, BtOn };`,
  `enum class RadioAction : uint8_t { None, StopBt, StopWiFi, StartWiFi, StartBt };`,
  `struct RadioPlan { RadioAction steps[2]; };`,
  `RadioPlan planTransition(RadioState have, RadioState want);`

- [ ] **Step 1: Write the failing tests — all 9 have→want pairs**

Create `test/test_radio_policy/test_main.cpp`:

```cpp
#include <unity.h>

#include <radio_policy/radio_policy.h>

void setUp() {}
void tearDown() {}

static void assertPlan(RadioState have, RadioState want, RadioAction s0,
                       RadioAction s1) {
  RadioPlan p = planTransition(have, want);
  TEST_ASSERT_EQUAL_INT((int)s0, (int)p.steps[0]);
  TEST_ASSERT_EQUAL_INT((int)s1, (int)p.steps[1]);
}

void test_noop_transitions() {
  assertPlan(RadioState::Off, RadioState::Off, RadioAction::None, RadioAction::None);
  assertPlan(RadioState::WiFiOn, RadioState::WiFiOn, RadioAction::None, RadioAction::None);
  assertPlan(RadioState::BtOn, RadioState::BtOn, RadioAction::None, RadioAction::None);
}

void test_bringup_from_off() {
  assertPlan(RadioState::Off, RadioState::WiFiOn, RadioAction::StartWiFi, RadioAction::None);
  assertPlan(RadioState::Off, RadioState::BtOn, RadioAction::StartBt, RadioAction::None);
}

void test_teardown_to_off() {
  assertPlan(RadioState::WiFiOn, RadioState::Off, RadioAction::StopWiFi, RadioAction::None);
  assertPlan(RadioState::BtOn, RadioState::Off, RadioAction::StopBt, RadioAction::None);
}

void test_swap_tears_down_first() {
  // The XOR rule (spec §2.2): teardown ALWAYS precedes bringup.
  assertPlan(RadioState::WiFiOn, RadioState::BtOn, RadioAction::StopWiFi, RadioAction::StartBt);
  assertPlan(RadioState::BtOn, RadioState::WiFiOn, RadioAction::StopBt, RadioAction::StartWiFi);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_noop_transitions);
  RUN_TEST(test_bringup_from_off);
  RUN_TEST(test_teardown_to_off);
  RUN_TEST(test_swap_tears_down_first);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_radio_policy`
Expected: build FAILURE — `radio_policy/radio_policy.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `lib/radio_policy/radio_policy.h`:

```cpp
// danios radio_policy — the WiFi-XOR-Bluetooth rule (spec §2.2) as a pure
// transition table. RadioManager (src/services/) executes the returned steps.
// Pure std C++17; zero Arduino includes.
#pragma once
#include <cstdint>

enum class RadioState : uint8_t { Off, WiFiOn, BtOn };
enum class RadioAction : uint8_t { None, StopBt, StopWiFi, StartWiFi, StartBt };

// Ordered actions (max 2) to reach `want` from `have`. Unused slots = None.
struct RadioPlan {
  RadioAction steps[2];
};

RadioPlan planTransition(RadioState have, RadioState want);
```

Create `lib/radio_policy/radio_policy.cpp`:

```cpp
#include "radio_policy.h"

RadioPlan planTransition(RadioState have, RadioState want) {
  if (have == want) return {{RadioAction::None, RadioAction::None}};

  RadioAction stop = RadioAction::None;
  if (have == RadioState::WiFiOn) stop = RadioAction::StopWiFi;
  if (have == RadioState::BtOn) stop = RadioAction::StopBt;

  RadioAction start = RadioAction::None;
  if (want == RadioState::WiFiOn) start = RadioAction::StartWiFi;
  if (want == RadioState::BtOn) start = RadioAction::StartBt;

  // Teardown always precedes bringup — both radios never coexist (no PSRAM).
  if (stop != RadioAction::None) return {{stop, start}};
  return {{start, RadioAction::None}};
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_radio_policy`
Expected: `4 Tests 0 Failures 0 Ignored` — PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add lib/radio_policy test/test_radio_policy && \
  git commit -m "feat: add radio_policy WiFi-XOR-BT transition table (native TDD)"
```

---

### Task 3: `RadioManager` — the single radio owner (WiFi arm)

**Files:**
- Create: `src/services/RadioManager.h`
- Create: `src/services/RadioManager.cpp`
- Modify: `src/main.cpp` (global + `launcher.setRadioRequest` wiring)

**Interfaces:**
- Consumes: `radio_policy` (Task 2), `RadioMode` from `src/core/App.h` (F2),
  `Launcher::setRadioRequest(std::function<bool(RadioMode)>)` (F2).
- Produces (roadmap §4.6): `RadioState RadioManager::current() const;`,
  `bool RadioManager::request(RadioMode mode);` — Mode::None→Off, WiFi→WiFiOn,
  Bluetooth→BtOn; executes teardown-then-bringup; returns false (state Off) if
  bringup fails. **Until F5, `request(Bluetooth)` returns false with state Off**
  — the two BT private hooks below carry `// F5` markers where F5 grafts in.

- [ ] **Step 1: Write the header**

Create `src/services/RadioManager.h`:

```cpp
// danios RadioManager — sole owner of radio power state (spec §2.2, §3.1).
// Nothing else may touch WiFi.mode()/esp_wifi_*/btStart()/esp_bt_* power.
#pragma once
#include <radio_policy/radio_policy.h>

#include "../core/App.h"  // RadioMode

class RadioManager {
 public:
  RadioState current() const { return state_; }

  // Executes teardown-then-bringup per planTransition. Returns false if the
  // bringup step fails; the manager then lands in Off (never half-switched).
  bool request(RadioMode mode);

 private:
  bool execute(RadioAction action);
  bool startWiFi();
  void stopWiFi();
  bool startBt();  // F5 replaces the stub body (returns false until then)
  void stopBt();   // F5 replaces the stub body (no-op until then)

  RadioState state_ = RadioState::Off;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/RadioManager.cpp`:

```cpp
#include "RadioManager.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
RadioState wanted(RadioMode mode) {
  switch (mode) {
    case RadioMode::WiFi:      return RadioState::WiFiOn;
    case RadioMode::Bluetooth: return RadioState::BtOn;
    default:                   return RadioState::Off;
  }
}
}  // namespace

bool RadioManager::request(RadioMode mode) {
  const RadioState want = wanted(mode);
  const RadioPlan plan = planTransition(state_, want);

  for (RadioAction step : plan.steps) {
    if (step == RadioAction::None) continue;
    if (!execute(step)) {
      // Bringup failed after teardown already ran: we are radio-less.
      state_ = RadioState::Off;
      Serial.printf("[radio] request failed at step %d, heap=%u\n",
                    static_cast<int>(step), esp_get_free_heap_size());
      return false;
    }
  }
  state_ = want;
  Serial.printf("[radio] state=%d heap=%u\n", static_cast<int>(state_),
                esp_get_free_heap_size());
  return true;
}

bool RadioManager::execute(RadioAction action) {
  switch (action) {
    case RadioAction::StartWiFi: return startWiFi();
    case RadioAction::StopWiFi:  stopWiFi(); return true;
    case RadioAction::StartBt:   return startBt();
    case RadioAction::StopBt:    stopBt(); return true;
    default:                     return true;
  }
}

bool RadioManager::startWiFi() { return WiFi.mode(WIFI_STA); }

void RadioManager::stopWiFi() {
  WiFi.disconnect(true /*wifioff*/, true /*eraseap — RAM only, NVS creds are ours*/);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
}

// F5: BluetoothAudioService power hooks graft in here. Until then the BT arm
// is a stub so request(Bluetooth) returns false with state Off (roadmap §4.6).
bool RadioManager::startBt() { return false; }  // F5
void RadioManager::stopBt() {}                  // F5
```

- [ ] **Step 3: Wire it into main.cpp**

Modify `src/main.cpp` — add with the other service globals (after
`static StorageService storage;` from F3):

```cpp
#include "services/RadioManager.h"

static RadioManager radioManager;
```

and in `setup()`, immediately after the F2 line `launcher.registerApp(&settingsApp);`:

```cpp
  launcher.setRadioRequest(
      [](RadioMode m) { return radioManager.request(m); });
```

- [ ] **Step 4: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/RadioManager.h \
  src/services/RadioManager.cpp src/main.cpp && \
  git commit -m "feat: add RadioManager (WiFi arm; BT stubbed for F5)"
```

---

### Task 4: `WiFiService` — scan, credentials, connect

**Files:**
- Create: `src/services/WiFiService.h`
- Create: `src/services/WiFiService.cpp`
- Modify: `src/main.cpp` (global)

**Interfaces:**
- Consumes: `ISettingsStore` (F3) for `wifi.ssid`/`wifi.pass`; assumes
  `RadioState::WiFiOn` (only RadioManager powers the radio).
- Produces (roadmap §4.7 verbatim):
  `struct WifiNetwork { std::string ssid; int8_t rssi; bool secured; };` and
  `WiFiService` with `hasCredentials()`, `setCredentials(ssid, pass)`,
  `forget()`, `scan()`, `connect(timeout_ms = 15000)`, `isConnected()`.

- [ ] **Step 1: Write the header**

Create `src/services/WiFiService.h`:

```cpp
// danios WiFiService — scan/credentials/connect (spec §3.1). Assumes the
// radio is already in RadioState::WiFiOn (RadioManager owns power).
#pragma once
#include <settings_store/settings_store.h>

#include <string>
#include <vector>

struct WifiNetwork {
  std::string ssid;
  int8_t rssi;
  bool secured;
};

class WiFiService {
 public:
  explicit WiFiService(ISettingsStore& store) : store_(store) {}

  bool hasCredentials() const;
  void setCredentials(const std::string& ssid, const std::string& pass);  // + saves
  void forget();
  std::vector<WifiNetwork> scan();               // blocking, ~2 s
  bool connect(uint32_t timeout_ms = 15000);     // stored creds; blocking
  bool isConnected() const;

 private:
  ISettingsStore& store_;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/WiFiService.cpp`:

```cpp
#include "WiFiService.h"

#include <Arduino.h>
#include <WiFi.h>

bool WiFiService::hasCredentials() const {
  return !store_.getString("wifi.ssid", "").empty();
}

void WiFiService::setCredentials(const std::string& ssid,
                                 const std::string& pass) {
  store_.setString("wifi.ssid", ssid);
  store_.setString("wifi.pass", pass);
}

void WiFiService::forget() {
  store_.remove("wifi.ssid");
  store_.remove("wifi.pass");
  WiFi.disconnect();
}

std::vector<WifiNetwork> WiFiService::scan() {
  std::vector<WifiNetwork> out;
  const int16_t n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/false);
  for (int16_t i = 0; i < n; ++i) {
    out.push_back({std::string(WiFi.SSID(i).c_str()),
                   static_cast<int8_t>(WiFi.RSSI(i)),
                   WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
  }
  WiFi.scanDelete();
  return out;
}

bool WiFiService::connect(uint32_t timeout_ms) {
  const std::string ssid = store_.getString("wifi.ssid", "");
  if (ssid.empty()) return false;
  const std::string pass = store_.getString("wifi.pass", "");

  WiFi.begin(ssid.c_str(), pass.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      Serial.printf("[wifi] connect to \"%s\" timed out\n", ssid.c_str());
      return false;
    }
    delay(100);
  }
  Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool WiFiService::isConnected() const { return WiFi.status() == WL_CONNECTED; }
```

- [ ] **Step 3: Add the global to main.cpp**

Modify `src/main.cpp` — after `static RadioManager radioManager;`:

```cpp
#include "services/WiFiService.h"

static WiFiService wifiService(settings);
```

- [ ] **Step 4: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/WiFiService.h \
  src/services/WiFiService.cpp src/main.cpp && \
  git commit -m "feat: add WiFiService (scan/credentials/connect)"
```

---

### Task 5: `TimeService` — NTP, manual set, POSIX TZ

**Files:**
- Create: `src/services/TimeService.h`
- Create: `src/services/TimeService.cpp`
- Modify: `src/main.cpp` (global + `begin()` call)

**Interfaces:**
- Consumes: `LocalDate` (Task 1), `RadioManager` (Task 3), `WiFiService`
  (Task 4), `ISettingsStore` (F3) for the `tz` key.
- Produces (roadmap §4.8 verbatim): `isTimeKnown()`, `today()`,
  `minutesSinceMidnight()`, `hhmm(char out[6])`, `syncNow()`,
  `setManual(const LocalDate&, int hour, int minute)`,
  `setTimezone(const std::string& posixTz)`. Plus `begin()` (this plan's
  addition: applies the persisted TZ at boot — called from `setup()`).
- **Timezone convention (roadmap §4.8):** POSIX TZ strings only. A3's Weather
  app later derives one from ip-api's `offset` field. No IANA names on-device.

- [ ] **Step 1: Write the header**

Create `src/services/TimeService.h`:

```cpp
// danios TimeService — NTP sync + manual set + local date/clock queries
// (spec §3.1, §6.2). Time is "known" once NTP succeeds or the user sets it
// manually; until then Oracle falls back to random picks (spec §4.4).
#pragma once
#include <date_utils/date_utils.h>
#include <settings_store/settings_store.h>

#include <string>

class RadioManager;
class WiFiService;

class TimeService {
 public:
  TimeService(RadioManager& radio, WiFiService& wifi, ISettingsStore& store)
      : radio_(radio), wifi_(wifi), store_(store) {}

  void begin();                          // apply persisted TZ (call in setup())

  bool isTimeKnown() const { return known_; }
  LocalDate today() const;               // {0,0,0} if unknown
  int minutesSinceMidnight() const;      // -1 if unknown
  void hhmm(char out[6]) const;          // "14:07" or "--:--"

  // NTP; asks RadioManager for WiFi, restores the previous radio state after.
  bool syncNow();

  void setManual(const LocalDate& d, int hour, int minute);
  void setTimezone(const std::string& posixTz);  // persists "tz" + applies

 private:
  RadioManager& radio_;
  WiFiService& wifi_;
  ISettingsStore& store_;
  bool known_ = false;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/TimeService.cpp`:

```cpp
#include "TimeService.h"

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "../core/App.h"  // RadioMode
#include "RadioManager.h"
#include "WiFiService.h"

namespace {
// Any epoch after 2023-11 proves SNTP actually set the clock (the ESP32
// cold-boots believing it's 1970).
constexpr time_t kSaneEpoch = 1700000000;
}  // namespace

void TimeService::begin() {
  const std::string tz = store_.getString("tz", "UTC0");
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

LocalDate TimeService::today() const {
  if (!known_) return {0, 0, 0};
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return {static_cast<int16_t>(t.tm_year + 1900),
          static_cast<int8_t>(t.tm_mon + 1), static_cast<int8_t>(t.tm_mday)};
}

int TimeService::minutesSinceMidnight() const {
  if (!known_) return -1;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour * 60 + t.tm_min;
}

void TimeService::hhmm(char out[6]) const {
  if (!known_) {
    snprintf(out, 6, "--:--");
    return;
  }
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  snprintf(out, 6, "%02d:%02d", t.tm_hour, t.tm_min);
}

bool TimeService::syncNow() {
  const RadioState prev = radio_.current();
  if (prev != RadioState::WiFiOn && !radio_.request(RadioMode::WiFi)) {
    return false;
  }

  bool ok = wifi_.isConnected() || wifi_.connect();
  if (ok) {
    const std::string tz = store_.getString("tz", "UTC0");
    configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
    const uint32_t start = millis();
    ok = false;
    while (millis() - start < 15000) {
      if (time(nullptr) > kSaneEpoch) {
        ok = true;
        break;
      }
      delay(100);
    }
  }
  if (ok) known_ = true;

  // Restore whatever the radio was doing before we borrowed it.
  if (prev == RadioState::Off) radio_.request(RadioMode::None);
  else if (prev == RadioState::BtOn) radio_.request(RadioMode::Bluetooth);
  Serial.printf("[time] syncNow %s\n", ok ? "ok" : "FAILED");
  return ok;
}

void TimeService::setManual(const LocalDate& d, int hour, int minute) {
  struct tm t = {};
  t.tm_year = d.year - 1900;
  t.tm_mon = d.month - 1;
  t.tm_mday = d.day;   // mktime normalizes out-of-range days (e.g. Feb 31)
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_isdst = -1;
  const time_t epoch = mktime(&t);  // interprets via the active TZ
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  known_ = true;
}

void TimeService::setTimezone(const std::string& posixTz) {
  store_.setString("tz", posixTz);
  setenv("TZ", posixTz.c_str(), 1);
  tzset();
}
```

- [ ] **Step 3: Add the global + begin() to main.cpp**

Modify `src/main.cpp` — after `static WiFiService wifiService(settings);`:

```cpp
#include "services/TimeService.h"

static TimeService timeService(radioManager, wifiService, settings);
```

and in `setup()`, right after the F3 settings-load step:

```cpp
  timeService.begin();  // apply persisted TZ before anything reads the clock
```

- [ ] **Step 4: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/TimeService.h \
  src/services/TimeService.cpp src/main.cpp && \
  git commit -m "feat: add TimeService (NTP, manual set, POSIX TZ)"
```

---

### Task 6: Settings → WiFi section

**Files:**
- Create: `src/apps/settings/WifiSection.cpp`
- Modify: `src/apps/settings/Sections.h`
- Modify: `src/apps/settings/SettingsApp.h` (extend `setDeps` + members)
- Modify: `src/apps/settings/SettingsApp.cpp` (`kSectionNames`, `showSection`)
- Modify: `src/main.cpp` (the `setDeps` call site)

**Interfaces:**
- Consumes: `RadioManager::request` (Task 3), `WiFiService` (Task 4), F3's
  `Sections.h`/`kSectionNames`/`showSection` extension points, LVGL v8
  `lv_keyboard`.
- Produces: `void buildWifiSection(lv_obj_t* parent, RadioManager& radio, WiFiService& wifi);`
  Also extends F3's `SettingsApp::setDeps` to
  `setDeps(ISettingsStore&, DisplayService&, StorageService&, RadioManager&, WiFiService&, TimeService&)`
  — Task 7 consumes the `TimeService&` (both sections land in one signature
  change so `main.cpp` is edited once). F5 appends its own parameter later.
- **Radio-while-open rule (roadmap §4.11):** the section requests WiFi when
  built and releases to `None` when its widgets are deleted — implemented via
  an `LV_EVENT_DELETE` callback on the section body, so it fires for both the
  in-section back button (`lv_obj_clean` in `showMenu`) and leaving the app.

- [ ] **Step 1: Extend Sections.h and SettingsApp**

Modify `src/apps/settings/Sections.h` — extend the existing forward-declaration
block and builder list:

```cpp
class RadioManager;   // Task 3
class WiFiService;    // Task 4
class TimeService;    // Task 5

void buildWifiSection(lv_obj_t* parent, RadioManager& radio, WiFiService& wifi);
void buildClockSection(lv_obj_t* parent, TimeService& time);  // Task 7
```

Modify `src/apps/settings/SettingsApp.h` — replace the F3 `setDeps` declaration
and add members:

```cpp
  void setDeps(ISettingsStore& store, DisplayService& display,
               StorageService& storage, RadioManager& radio, WiFiService& wifi,
               TimeService& time);
```

```cpp
  RadioManager* radio_ = nullptr;
  WiFiService* wifi_ = nullptr;
  TimeService* time_ = nullptr;
```

Modify `src/apps/settings/SettingsApp.cpp` — replace the F3 `setDeps` body,
extend `kSectionNames`, and add the switch cases:

```cpp
void SettingsApp::setDeps(ISettingsStore& store, DisplayService& display,
                          StorageService& storage, RadioManager& radio,
                          WiFiService& wifi, TimeService& time) {
  store_ = &store;
  display_ = &display;
  storage_ = &storage;
  radio_ = &radio;
  wifi_ = &wifi;
  time_ = &time;
}
```

```cpp
const char* kSectionNames[] = {"Display", "Units", "About", "WiFi", "Clock"};
```

```cpp
    case 3:
      buildWifiSection(body, *radio_, *wifi_);
      break;
    case 4:
      buildClockSection(body, *time_);  // Task 7
      break;
```

Modify `src/main.cpp` — replace the F3 `setDeps` call:

```cpp
settingsApp.setDeps(settings, displayService, storage, radioManager,
                    wifiService, timeService);
```

- [ ] **Step 2: Implement the section**

Create `src/apps/settings/WifiSection.cpp`:

```cpp
// Settings -> WiFi (spec §5): scan nearby networks, tap one, type the password
// on the LVGL keyboard, connect and save. Fully on-device, no phone involved.
#include <lvgl.h>

#include "../../core/App.h"  // RadioMode
#include "../../services/RadioManager.h"
#include "../../services/WiFiService.h"
#include "Sections.h"

namespace {
struct WifiUi {
  RadioManager* radio;
  WiFiService* wifi;
  lv_obj_t* body;
  lv_obj_t* list;        // scan results
  lv_obj_t* status;      // one-line status label
  char pendingSsid[33];  // network tapped, awaiting password
};
WifiUi ui;  // one Settings screen at a time (single LVGL task) — safe

void setStatus(const char* msg) {
  lv_label_set_text(ui.status, msg);
  lv_refr_now(nullptr);  // repaint before a blocking connect/scan
}

void rebuildForgetRow();

void connectWithPassword(const char* pass) {
  ui.wifi->setCredentials(ui.pendingSsid, pass);
  setStatus("Connecting...");
  if (ui.wifi->connect()) {
    setStatus("Connected " LV_SYMBOL_OK);
  } else {
    setStatus("Failed - check the password");
  }
  rebuildForgetRow();
}

void keyboardEvent(lv_event_t* e) {
  lv_obj_t* kb = lv_event_get_current_target(e);
  lv_obj_t* ta = lv_keyboard_get_textarea(kb);
  if (lv_event_get_code(e) == LV_EVENT_READY) {  // checkmark pressed
    connectWithPassword(lv_textarea_get_text(ta));
  }
  // READY or CANCEL: tear the modal down. Async — never delete an ancestor
  // of the object whose event is still being dispatched.
  lv_obj_del_async(lv_obj_get_parent(kb));
}

void askPassword() {
  // Full-screen modal on the top layer: textarea + keyboard.
  lv_obj_t* modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* prompt = lv_label_create(modal);
  lv_label_set_text_fmt(prompt, "Password for %s:", ui.pendingSsid);

  lv_obj_t* ta = lv_textarea_create(modal);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, true);
  lv_obj_set_width(ta, LV_PCT(100));

  lv_obj_t* kb = lv_keyboard_create(modal);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_CANCEL, nullptr);
}

void networkClicked(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_current_target(e);
  // Button text is "ssid  (-60)" — the SSID was stashed as user data instead.
  const char* ssid = static_cast<const char*>(lv_event_get_user_data(e));
  (void)btn;
  snprintf(ui.pendingSsid, sizeof(ui.pendingSsid), "%s", ssid);
  askPassword();
}

void scanClicked(lv_event_t*) {
  setStatus("Scanning...");
  lv_obj_clean(ui.list);
  auto nets = ui.wifi->scan();
  if (nets.empty()) {
    lv_list_add_text(ui.list, "No networks found");
  }
  for (auto& n : nets) {
    char row[64];
    snprintf(row, sizeof(row), "%s (%d)%s", n.ssid.c_str(), n.rssi,
             n.secured ? "" : " open");
    lv_obj_t* btn = lv_list_add_btn(ui.list, LV_SYMBOL_WIFI, row);
    // The button outlives `nets`; copy the SSID into the button's user data.
    char* owned = static_cast<char*>(lv_mem_alloc(n.ssid.size() + 1));
    memcpy(owned, n.ssid.c_str(), n.ssid.size() + 1);
    lv_obj_add_event_cb(btn, networkClicked, LV_EVENT_CLICKED, owned);
    lv_obj_add_event_cb(
        btn, [](lv_event_t* ev) { lv_mem_free(lv_event_get_user_data(ev)); },
        LV_EVENT_DELETE, owned);
  }
  setStatus("Tap a network to join");
}

void forgetClicked(lv_event_t*) {
  ui.wifi->forget();
  setStatus("Network forgotten");
  rebuildForgetRow();
}

lv_obj_t* forgetBtn = nullptr;

void rebuildForgetRow() {
  if (forgetBtn) {
    lv_obj_del(forgetBtn);
    forgetBtn = nullptr;
  }
  if (!ui.wifi->hasCredentials()) return;
  forgetBtn = lv_btn_create(ui.body);
  lv_obj_t* lbl = lv_label_create(forgetBtn);
  lv_label_set_text(lbl, LV_SYMBOL_TRASH " Forget network");
  lv_obj_add_event_cb(forgetBtn, forgetClicked, LV_EVENT_CLICKED, nullptr);
}

void bodyDeleted(lv_event_t*) {
  // Radio-while-open rule: release WiFi when the section goes away.
  forgetBtn = nullptr;
  ui.radio->request(RadioMode::None);
}
}  // namespace

void buildWifiSection(lv_obj_t* parent, RadioManager& radio,
                      WiFiService& wifi) {
  ui = {};
  ui.radio = &radio;
  ui.wifi = &wifi;
  ui.body = parent;

  ui.status = lv_label_create(parent);

  lv_obj_t* scanBtn = lv_btn_create(parent);
  lv_obj_t* scanLbl = lv_label_create(scanBtn);
  lv_label_set_text(scanLbl, LV_SYMBOL_REFRESH " Scan");
  lv_obj_add_event_cb(scanBtn, scanClicked, LV_EVENT_CLICKED, nullptr);

  ui.list = lv_list_create(parent);
  lv_obj_set_width(ui.list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.list, 1);

  rebuildForgetRow();
  lv_obj_add_event_cb(parent, bodyDeleted, LV_EVENT_DELETE, nullptr);

  if (radio.request(RadioMode::WiFi)) {
    setStatus(wifi.isConnected() ? "Connected " LV_SYMBOL_OK
                                 : "Tap Scan to find networks");
  } else {
    setStatus("Radio unavailable");
  }
}
```

- [ ] **Step 3: Add a temporary Clock stub so the link stays green**

`showSection`'s case 4 references `buildClockSection`, which Task 7 defines —
without a body the build fails at link time. Keep every step green by
appending a temporary definition at the bottom of `WifiSection.cpp`:

```cpp
// Temporary stub — Task 7 Step 1 deletes this when ClockSection.cpp lands.
void buildClockSection(lv_obj_t* parent, TimeService&) {
  lv_label_set_text(lv_label_create(parent), "Clock settings arrive in Task 7");
}
```

- [ ] **Step 4: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/apps/settings/WifiSection.cpp \
  src/apps/settings/Sections.h src/apps/settings/SettingsApp.h \
  src/apps/settings/SettingsApp.cpp src/main.cpp && \
  git commit -m "feat: Settings -> WiFi section (scan, keyboard, connect, forget)"
```

---

### Task 7: Settings → Clock section

**Files:**
- Create: `src/apps/settings/ClockSection.cpp`
- Modify: `src/apps/settings/WifiSection.cpp` (delete the temporary stub)

**Interfaces:**
- Consumes: `TimeService` (Task 5), the `Sections.h` declaration and switch
  case added in Task 6.
- Produces: `void buildClockSection(lv_obj_t* parent, TimeService& time);`

- [ ] **Step 1: Remove the temporary stub**

Modify `src/apps/settings/WifiSection.cpp`: delete the three-line
`buildClockSection` stub added at the end of Task 6 Step 3.

- [ ] **Step 2: Implement the section**

Create `src/apps/settings/ClockSection.cpp`:

```cpp
// Settings -> Clock (spec §5, §6.2): "Sync now" forces an NTP re-sync (borrowing
// WiFi via TimeService); manual date/time set covers the no-WiFi case and gives
// the Oracle a correct day to work from.
#include <lvgl.h>

#include <cstdio>

#include "../../services/TimeService.h"
#include "Sections.h"

namespace {
struct ClockUi {
  TimeService* time;
  lv_obj_t* status;
  lv_obj_t* year;
  lv_obj_t* month;
  lv_obj_t* day;
  lv_obj_t* hour;
  lv_obj_t* minute;
};
ClockUi ui;

void refreshStatus() {
  char clock[6];
  ui.time->hhmm(clock);
  const LocalDate d = ui.time->today();
  if (ui.time->isTimeKnown()) {
    lv_label_set_text_fmt(ui.status, "Now: %04d-%02d-%02d %s", d.year, d.month,
                          d.day, clock);
  } else {
    lv_label_set_text(ui.status, "Clock not set");
  }
}

void syncClicked(lv_event_t*) {
  lv_label_set_text(ui.status, "Syncing...");
  lv_refr_now(nullptr);  // repaint before the blocking sync
  ui.time->syncNow();
  refreshStatus();
}

// Builds "2026\n2027\n..." style roller option strings once, statically.
lv_obj_t* makeRoller(lv_obj_t* parent, int from, int to, int sel) {
  static char buf[1024];
  size_t off = 0;
  for (int v = from; v <= to; ++v) {
    off += snprintf(buf + off, sizeof(buf) - off, "%02d%s", v,
                    v == to ? "" : "\n");
  }
  lv_obj_t* r = lv_roller_create(parent);
  lv_roller_set_options(r, buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(r, 3);
  lv_roller_set_selected(r, sel - from, LV_ANIM_OFF);
  return r;
}

void applyClicked(lv_event_t*) {
  const LocalDate d{
      static_cast<int16_t>(2026 + lv_roller_get_selected(ui.year)),
      static_cast<int8_t>(1 + lv_roller_get_selected(ui.month)),
      static_cast<int8_t>(1 + lv_roller_get_selected(ui.day))};
  // Day roller always offers 1..31; mktime in setManual normalizes overshoot
  // (e.g. Feb 31 -> Mar 2/3), documented behavior for this simple UI.
  ui.time->setManual(d, static_cast<int>(lv_roller_get_selected(ui.hour)),
                     static_cast<int>(lv_roller_get_selected(ui.minute)));
  refreshStatus();
}
}  // namespace

void buildClockSection(lv_obj_t* parent, TimeService& time) {
  ui = {};
  ui.time = &time;

  ui.status = lv_label_create(parent);

  lv_obj_t* syncBtn = lv_btn_create(parent);
  lv_obj_t* syncLbl = lv_label_create(syncBtn);
  lv_label_set_text(syncLbl, LV_SYMBOL_REFRESH " Sync now");
  lv_obj_add_event_cb(syncBtn, syncClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* caption = lv_label_create(parent);
  lv_label_set_text(caption, "Manual set  (Y / M / D / h / m)");

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  const LocalDate d = time.isTimeKnown() ? time.today() : LocalDate{2026, 7, 3};
  const int mins = time.isTimeKnown() ? time.minutesSinceMidnight() : 720;
  ui.year = makeRoller(row, 2026, 2045, d.year);
  ui.month = makeRoller(row, 1, 12, d.month);
  ui.day = makeRoller(row, 1, 31, d.day);
  ui.hour = makeRoller(row, 0, 23, mins / 60);
  ui.minute = makeRoller(row, 0, 59, mins % 60);

  lv_obj_t* applyBtn = lv_btn_create(parent);
  lv_obj_t* applyLbl = lv_label_create(applyBtn);
  lv_label_set_text(applyLbl, LV_SYMBOL_OK " Apply");
  lv_obj_add_event_cb(applyBtn, applyClicked, LV_EVENT_CLICKED, nullptr);

  refreshStatus();
}
```

- [ ] **Step 3: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/apps/settings/ClockSection.cpp \
  src/apps/settings/WifiSection.cpp && \
  git commit -m "feat: Settings -> Clock section (sync now, manual set)"
```

---

### Task 8: Boot step 4 + live status bar

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: everything above; F2's `StatusBar::setClockText/setRadio`; F3's
  boot steps 1–2 already in `setup()`.
- Produces: spec §3.4 boot step 4 with the **marked A3 prefetch hook**
  (`// A3: weather boot prefetch hook`) and the 1 Hz status-bar update loop.
  The loop is the single writer of the radio glyph from here on (F2's
  launcher-driven glyph writes are redundant but harmless — same values).

- [ ] **Step 1: Add boot step 4 to setup()**

Modify `src/main.cpp` — in `setup()`, after all `launcher.registerApp(...)`
calls and the `setRadioRequest` wiring, **before** `launcher.show()`:

```cpp
  // Boot flow step 4 (spec §3.4): if a network is saved, bring WiFi up briefly
  // to sync time, prefetch weather, then drop the radio. Bounded ~23 s worst
  // case (8 s connect + 15 s NTP); a splash label keeps the screen honest.
  if (wifiService.hasCredentials()) {
    lv_obj_t* splash = lv_label_create(lv_scr_act());
    lv_label_set_text(splash, "Connecting" LV_SYMBOL_WIFI);
    lv_obj_center(splash);
    lv_refr_now(nullptr);

    if (radioManager.request(RadioMode::WiFi) && wifiService.connect(8000)) {
      timeService.syncNow();  // already WiFiOn -> no radio dance inside
      // A3: weather boot prefetch hook
    }
    radioManager.request(RadioMode::None);
    lv_obj_del(splash);
  }
```

- [ ] **Step 2: Add the 1 Hz status-bar update to loop()**

Modify `src/main.cpp` — in `loop()`, after the F3 screen-sleep check:

```cpp
  // Status bar: clock + radio glyph, once per second (spec §3.3).
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus >= 1000) {
    lastStatus = millis();
    char clock[6];
    timeService.hhmm(clock);
    statusBar.setClockText(clock);
    switch (radioManager.current()) {
      case RadioState::WiFiOn: statusBar.setRadio(RadioMode::WiFi); break;
      case RadioState::BtOn:   statusBar.setRadio(RadioMode::Bluetooth); break;
      default:                 statusBar.setRadio(RadioMode::None); break;
    }
  }
```

- [ ] **Step 3: Verify device build + native suite**

Run: `cd /home/lucca/repos/danios && pio test -e native && pio run -e cyd`
Expected: all native tests PASS (date_utils 8, radio_policy 4, plus F1/F3
suites); device build `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/main.cpp && \
  git commit -m "feat: boot-time WiFi sync (spec 3.4 step 4) + live status bar"
```

---

### Task 9: On-device verification

**Files:** none (manual verification).

**Interfaces:**
- Consumes: everything F4 built. This is the roadmap §1 F4 E2E gate: *"Join a
  WiFi network from the device with the on-screen keyboard; status bar shows
  correct local time."*

**Board note:** Steps 1–2 run on the **bare ESP32 devkit** (serial-only, no
display needed — see `docs/hardware.md`); steps 3–6 need the **CYD**.

- [ ] **Step 1 (devkit or CYD): radio transitions over serial**

Flash: `pio run -e cyd -t upload && pio device monitor`
Expected on boot with no saved network: `[radio]` lines never appear (radio
stays off), heap ~150–250 KB logged if a transition runs.

- [ ] **Step 2 (devkit or CYD): boot sync path**

Pre-seed credentials once via the UI (step 3) or temporarily hardcode
`settings.setString("wifi.ssid", ...)` in `setup()` (remove after). Reboot.
Expected serial: `[wifi] connected, ip=...` then `[time] syncNow ok`, then
`[radio] state=0 ...` (Off) — WiFi dropped after sync per spec §3.4.

- [ ] **Step 3 (CYD): join a network on-device**

Settings → WiFi → Scan → tap your network → type the password on the LVGL
keyboard → checkmark. Expected: "Connecting..." then "Connected ✓"; a wrong
password shows "Failed - check the password".

- [ ] **Step 4 (CYD): clock**

After step 3, Settings → Clock → "Sync now". Expected: "Now: <today's local
date> <local time>" matching your phone; status bar shows the same HH:MM
within a second of leaving Settings.

- [ ] **Step 5 (CYD): manual set with no WiFi**

Settings → WiFi → Forget network. Reboot. Status bar shows `--:--`. Settings →
Clock → set rollers to an arbitrary date/time → Apply. Expected: status label
and status bar show the set time; it keeps ticking.

- [ ] **Step 6 (CYD): XOR guard**

While in Settings → WiFi (radio glyph = WiFi), go home and open a stub app.
Expected: glyph returns to none within a second (launcher requested
RadioMode::None on the section teardown and app entry) — the WiFi radio never
lingers.

- [ ] **Step 7: Commit any fixes found**

```bash
cd /home/lucca/repos/danios && git add -A && \
  git commit -m "fix: F4 on-device verification findings"
```
(Skip if nothing changed.)

---

## Definition of done (roadmap §6)

- [ ] `pio test -e native` — green, including the 8 date_utils and 4
      radio_policy tests added here.
- [ ] `pio run -e cyd` — `SUCCESS`.
- [ ] Roadmap §1 F4 E2E outcome observed on the CYD: joined a WiFi network via
      the on-screen keyboard; status bar shows the correct local time.
- [ ] Spec §3.4 step 4 boot behavior observed (sync then radio off), with the
      `// A3: weather boot prefetch hook` marker present in `main.cpp`.
- [ ] `request(Bluetooth)` verified to return false leaving state Off (the F5
      stub contract) — one temporary serial test line, then removed.
