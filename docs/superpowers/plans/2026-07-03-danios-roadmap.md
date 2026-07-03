# danios Implementation Roadmap

**Date:** 2026-07-03
**Spec:** [`docs/superpowers/specs/2026-06-03-esp32-gift-device-design.md`](../specs/2026-06-03-esp32-gift-device-design.md)

The spec is broken into **10 implementation plans**: 5 foundation slices (each a
vertical, end-to-end-testable piece of the base the apps sit on) and 5 app plans.
This document is the **contract between plans** — the plan index, execution order,
global constraints, and the shared interfaces every plan must use verbatim.

> **For plan authors and executors:** interface names, signatures, file paths, and
> NVS keys defined here are **authoritative**. Plans must not rename them. Where
> this doc leaves detail open, the individual plan decides.

---

## 1. Plan index & execution order

| # | Plan file | Delivers | E2E-testable outcome |
| --- | --- | --- | --- |
| F1 | `2026-07-03-foundation-1-lvgl-touch.md` | LVGL bound to LovyanGFX, touch driver, native test env, git init | Tap an on-screen button; a counter increments |
| F2 | `2026-07-03-foundation-2-launcher.md` | App interface, Launcher grid, status bar, Settings shell, navigation | Navigate launcher → stub app → back; gear opens empty Settings |
| F3 | `2026-07-03-foundation-3-storage-settings.md` | StorageService (SD), SettingsService (NVS), LVGL SD image loading, Settings→Display/Units/About, screen sleep | Change brightness, reboot, it persists; icons load from SD; SD-missing boot error |
| F4 | `2026-07-03-foundation-4-wifi-time.md` | RadioManager, WiFiService, TimeService (NTP + manual), Settings→WiFi/Clock, boot-time sync | Join a WiFi network from the device with the on-screen keyboard; status bar shows correct local time |
| F5 | `2026-07-03-foundation-5-bluetooth-audio.md` | BluetoothAudioService (A2DP source), Settings→Bluetooth, WiFi-XOR-BT enforcement complete | Pair a speaker from Settings; hear a test tone through it |
| A1 | `2026-07-03-app-calculator.md` | Calculator app | Working four-function calculator on device |
| A2 | `2026-07-03-app-oracle.md` | Oracle app | One stable wisdom entry per day from SD; random fallback when clock unknown |
| A3 | `2026-07-03-app-weather.md` | Weather app + Settings→Weather location + boot prefetch | Character dressed for live local weather; stale-cache fallback |
| A4 | `2026-07-03-app-music.md` | Music app | Pick an MP3 from the SD card, hear it on the Bluetooth speaker |
| A5 | `2026-07-03-app-pet.md` | Pet app + launcher badge | Hatch, name, feed/play/clean/scold the pet; state survives reboot; badge on Critical |

**Dependency graph** (a plan may start once everything left of it is merged):

```
F1 → F2 → F3 → F4 → F5
      │     │     │    └─→ A4 Music (needs F3 SD + F5 BT)
      │     │     ├─→ A3 Weather (needs F4 WiFi/time; F3 SD for art)
      │     │     ├─→ A2 Oracle  (needs F3 SD + F4 time)
      │     │     └─→ A5 Pet     (needs F3 NVS/SD + F4 time)
      └─→ A1 Calculator (needs only F2)
```

Foundation slices are strictly sequential. After F2, Calculator can proceed in
parallel with F3+. After F4, Oracle/Weather/Pet can proceed in parallel with F5.

**Hardware note:** per `docs/hardware.md`, the board on hand may be a bare ESP32
devkit (no display/touch/SD) while the CYD is on order. F4/F5 service logic and
all native tests run fine on the devkit; F1–F3 on-device verification steps and
all app UI verification need the CYD. Each plan's verification steps say which
board they need.

---

## 2. Global constraints (inherited by every plan)

Copy this section into each plan's **Global Constraints** header.

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
  spec deviation §5 below.
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
- **SD layout & NVS keys:** exactly as pinned in §4.1/§4.2 — never invent new
  paths/keys outside your plan's reserved set.

---

## 3. Repository layout

```
platformio.ini                    envs: cyd (device), native (tests)
include/
  lv_conf.h                       LVGL config (F1)
  LGFX_ESP32_2432S024C.hpp        display config (exists — do not modify)
lib/                              PURE LOGIC — std C++17 only, no Arduino/LVGL
  date_utils/                     civil-date math (F4)
  settings_store/                 ISettingsStore interface + FakeSettingsStore (F3)
  radio_policy/                   radio state machine (F4)
  calc_engine/                    calculator (A1)
  oracle_picker/                  date-seeded shuffle (A2)
  weather_model/                  band/condition mapping, units (A3)
  weather_parse/                  JSON→structs via ArduinoJson* (A3)
  playlist/                       music dir scan/ordering logic (A4)
  pet_model/                      pet state machine (A5)
src/
  main.cpp                        boot flow + main loop
  core/                           App.h, Launcher, StatusBar (F2)
  services/                       DisplayService, TouchService, StorageService,
                                  SettingsService, RadioManager, WiFiService,
                                  TimeService, BluetoothAudioService
  apps/
    settings/                     Settings screen + its sections (F2–F5, A3)
    calculator/  oracle/  weather/  music/  pet/
test/
  test_<module>/test_main.cpp     one dir per lib module, runs on native
```

*`weather_parse` may include ArduinoJson (it's platform-independent and works on
native — add `bblanchon/ArduinoJson@^7.4.0` to both envs).

`[env:native]`: `platform = native`, `test_build_src = false` (tests build only
`lib/` + `test/`), Unity as test framework (PlatformIO default).

---

## 4. Shared interface contract

### 4.1 SD card layout (spec §6.3)

```
/music/*.mp3              /oracle/wisdom.txt
/art/weather/             /art/icons/          /art/oracle/         /art/pet/
```

Art files are **LVGL v8 true-color RGB565 `.bin`** images (converted offline with
the LVGL image converter), loaded through the LVGL FS driver registered on drive
letter **`S`** (F3), e.g. `lv_img_set_src(img, "S:/art/icons/weather.bin")`.
Every app must render a **placeholder** (colored `lv_obj` box or `LV_SYMBOL_*`)
when its art file is missing — art is hand-drawn later and arrives incrementally.

### 4.2 NVS keys

Single namespace `"danios"` via `Preferences`. Keys (≤15 chars, pinned):

| Owner | Keys |
| --- | --- |
| F3 | `disp.bright` (u8 0–255, default 160), `disp.sleep_s` (u16 seconds, 0 = never, default 60), `units.f` (bool, default false = °C) |
| F4 | `wifi.ssid` (str), `wifi.pass` (str), `tz` (str, **POSIX TZ string** e.g. `"<-03>3"`, default `"UTC0"`) |
| F5 | `bt.addr` (str, `AA:BB:CC:DD:EE:FF`), `bt.name` (str) |
| A3 | `loc.mode` (u8 0=auto 1=manual), `loc.lat`/`loc.lon` (float), `loc.city` (str), `wx.json` (str, last snapshot), `wx.day` (u32 dateKey) |
| A5 | `pet.name` (str), `pet.birth` (u32 dateKey), `pet.alive` (bool), `pet.fed`/`pet.played`/`pet.cleaned`/`pet.rested` (u32 dateKey each), `pet.sickday` (u32), `pet.illday` (u32), `pet.disc` (i8), `pet.care` (i16), `pet.mess` (u8), `pet.scold` (u32 dateKey), `pet.nightint` (u32 dateKey) |

### 4.3 Pure-logic primitives — `lib/date_utils/` (F4)

```cpp
// lib/date_utils/date_utils.h   — std C++ only
#pragma once
#include <cstdint>

struct LocalDate { int16_t year; int8_t month; int8_t day; };

bool     operator==(const LocalDate&, const LocalDate&);
uint32_t dateKey(const LocalDate& d);          // YYYYMMDD as u32; {0,0,0} → 0
LocalDate fromDateKey(uint32_t key);           // inverse; 0 → {0,0,0}
int32_t  daysBetween(const LocalDate& a, const LocalDate& b); // b - a, civil-days
```

`dateKey == 0` is the universal sentinel for "never / unknown".

### 4.4 Settings store — `lib/settings_store/` (F3)

Pure interface so app logic is native-testable; NVS implementation lives in
`src/services/SettingsService.{h,cpp}`.

```cpp
// lib/settings_store/settings_store.h — std C++ only
#pragma once
#include <cstdint>
#include <string>

class ISettingsStore {
public:
  virtual ~ISettingsStore() = default;
  virtual uint32_t    getU32(const char* key, uint32_t def) = 0;
  virtual void        setU32(const char* key, uint32_t v) = 0;
  virtual int32_t     getI32(const char* key, int32_t def) = 0;
  virtual void        setI32(const char* key, int32_t v) = 0;
  virtual float       getFloat(const char* key, float def) = 0;
  virtual void        setFloat(const char* key, float v) = 0;
  virtual bool        getBool(const char* key, bool def) = 0;
  virtual void        setBool(const char* key, bool v) = 0;
  virtual std::string getString(const char* key, const std::string& def) = 0;
  virtual void        setString(const char* key, const std::string& v) = 0;
  virtual void        remove(const char* key) = 0;
};

class FakeSettingsStore : public ISettingsStore { /* in-memory map, for tests */ };
```

`SettingsService : public ISettingsStore` wraps `Preferences` (namespace
`"danios"`). One global instance owned by `main.cpp`.

### 4.5 App framework — `src/core/App.h` (F2)

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

Launcher (F2) — `src/core/Launcher.{h,cpp}`:

```cpp
class Launcher {
public:
  void registerApp(App* app);                    // call order = grid order
  void show();                                   // build/refresh home screen
  void openApp(const char* id);                  // radio switch + lifecycle
  void goHome();                                 // apps call this for back/home
  void setBadge(const char* appId, bool on);     // red dot on an app icon (Pet uses it)
  void setAppEnabled(const char* appId, bool en);// F3: SD-dependent apps greyed out
                                                 // when the card is missing; tapping
                                                 // a disabled icon shows a hint msgbox
  void setRadioRequest(std::function<bool(RadioMode)> fn);
                                                 // F2: unset → treated as always-true.
                                                 // F4 wires RadioManager::request here.
  void tick(uint32_t now_ms);                    // forwards to active app
};
```

Lifecycle on `openApp`: `radio.request(app->requiredRadio())` → `app->onEnter()`
→ `app->buildUI(container)`. On `goHome`: `app->onExit()` → delete container
children → home screen → `radio.request(RadioMode::None)`.

Every app screen gets a **top bar with a back arrow** (provided by the Launcher
as part of the app container) that calls `goHome()` — apps build UI below it.

StatusBar (F2) — `src/core/StatusBar.{h,cpp}`: time (`--:--` until known),
radio glyph (none/WiFi/BT), gear button → `openApp("settings")`. Battery: see §5.

### 4.6 RadioManager — `src/services/RadioManager.{h,cpp}` (F4, BT arm completed F5)

Pure state machine in `lib/radio_policy/`:

```cpp
// lib/radio_policy/radio_policy.h — std C++ only
#pragma once
#include <cstdint>
enum class RadioState : uint8_t { Off, WiFiOn, BtOn };
enum class RadioAction : uint8_t { None, StopBt, StopWiFi, StartWiFi, StartBt };

// Returns the ordered actions (max 2) to reach `want` from `have`.
struct RadioPlan { RadioAction steps[2]; };
RadioPlan planTransition(RadioState have, RadioState want);
```

Service wrapper:

```cpp
class RadioManager {
public:
  RadioState current() const;
  bool request(RadioMode mode);   // Mode::None→Off, WiFi→WiFiOn, Bluetooth→BtOn.
                                  // Executes teardown-then-bringup per planTransition.
                                  // Returns false if bringup fails (state = Off).
};
```

Until F5, `request(Bluetooth)` returns false with state Off (BT arm stubbed).

### 4.7 WiFiService — `src/services/WiFiService.{h,cpp}` (F4)

```cpp
struct WifiNetwork { std::string ssid; int8_t rssi; bool secured; };

class WiFiService {
public:
  bool hasCredentials() const;                       // NVS wifi.ssid non-empty
  void setCredentials(const std::string& ssid, const std::string& pass); // + saves
  void forget();
  std::vector<WifiNetwork> scan();                   // blocking, ~2 s
  bool connect(uint32_t timeout_ms = 15000);         // uses stored creds; blocking
  bool isConnected() const;
};
```

Only `RadioManager` powers the radio; `WiFiService.connect()` assumes
`RadioState::WiFiOn`.

### 4.8 TimeService — `src/services/TimeService.{h,cpp}` (F4)

```cpp
class TimeService {
public:
  bool isTimeKnown() const;              // set by NTP success or manual set
  LocalDate today() const;               // local date; {0,0,0} if unknown
  int  minutesSinceMidnight() const;     // -1 if unknown
  void hhmm(char out[6]) const;          // "14:07" or "--:--"
  bool syncNow();                        // NTP; asks RadioManager for WiFi,
                                         // restores previous radio state after
  void setManual(const LocalDate& d, int hour, int minute);
  void setTimezone(const std::string& posixTz); // persists to NVS "tz", applies TZ env
};
```

**Timezone convention:** we do NOT map IANA zone names. The Weather app's
geolocation call requests ip-api's `offset` field (seconds from UTC) and converts
it to a fixed-offset POSIX string (e.g. `-10800` → `"<-03>3"`) passed to
`setTimezone()`. No DST transitions on-device — the offset refreshes on the next
geolocation. This is an accepted simplification (see §5).

Apps that need dates (Oracle, Pet) take `TimeService&`; their **pure logic**
takes plain `LocalDate` / `uint32_t dateKey` values so it tests natively.

### 4.9 StorageService — `src/services/StorageService.{h,cpp}` (F3)

```cpp
class StorageService {
public:
  bool begin();                                   // SD.begin(5); false if missing
  bool mounted() const;
  bool exists(const char* path);
  std::vector<std::string> listFiles(const char* dir, const char* ext); // sorted, non-recursive
  bool readLines(const char* path, std::vector<std::string>& out);      // trims \r, skips empty
};
```

F3 also registers the LVGL FS driver (`S:` → SD) used for all art loading.

### 4.10 BluetoothAudioService — `src/services/BluetoothAudioService.{h,cpp}` (F5)

```cpp
struct BtDevice { std::string name; std::string addr; };

// Audio callback contract: fill up to `frames` stereo int16 frames at 44100 Hz,
// return frames actually written. Return 0 = silence.
using AudioSourceFn = int32_t (*)(int16_t* stereo_buf, int32_t frames, void* ctx);

class BluetoothAudioService {
public:
  std::vector<BtDevice> scan(uint32_t ms = 8000);     // blocking discovery
  bool connect(const std::string& addr);              // A2DP source connect
  void disconnect();
  bool isConnected() const;
  void setSource(AudioSourceFn fn, void* ctx);        // Music app plugs decoder here
  std::string pairedAddr() const;                     // NVS bt.addr ("" if none)
  void savePaired(const BtDevice& d);
  void forgetPaired();
};
```

### 4.11 Settings screen (built incrementally)

`src/apps/settings/SettingsApp.{h,cpp}` is an `App` (id `"settings"`,
`requiredRadio() = None` — WiFi/BT sections request the radio themselves while
open). F2 creates the shell: an `lv_list` menu of sections. Each later plan adds
its section file:

| Section file (`src/apps/settings/`) | Plan |
| --- | --- |
| `DisplaySection.cpp` (brightness slider, sleep dropdown), `UnitsSection.cpp` (°C/°F switch), `AboutSection.cpp` (fw version, free heap) | F3 |
| `WifiSection.cpp` (scan list → tap → `lv_keyboard` password → connect/save; forget), `ClockSection.cpp` (Sync now, manual date/time rollers) | F4 |
| `BluetoothSection.cpp` (scan, pick, connect, forget) | F5 |
| `WeatherLocationSection.cpp` (auto/manual toggle + manual lat/lon/city entry) | A3 |

Shell API each section implements:
`void buildSection(lv_obj_t* parent, /* deps by reference */)`.

---

## 5. Deviations from the spec (agreed, hardware-forced or discovered)

1. **Battery % is not measurable** — the board has no battery-sense ADC (IP5603
   is charge/boost only; see `docs/hardware.md`). Status bar shows **no numeric
   battery %**; the low-battery warn/dim behavior of spec §6.4 is dropped.
   `StatusBar` keeps a `setBatteryText(const char*)` hook so a future board
   revision can plug in. Spec §3.3/§6.4 partially waived.
2. **A2DP on arduino-esp32 3.x is a risk** — `docs/hardware.md` recommends core
   2.0.x for the verified A2DP combo, but the proven display config runs on
   `espressif32@7.0.1` (core 3.x). **F5 Task 0 is a de-risk spike**: build
   ESP32-A2DP v1.8.x as source + sine tone on core 3.x
   (`-DA2DP_I2S_AUDIOTOOLS=1` if needed). If blocked, the documented fallback is
   pinning the platform back to core 2.0.x and re-verifying the display config
   (LovyanGFX supports both cores).
3. **README says four apps** — stale; the spec's five apps (incl. Pet) are
   authoritative. F2 updates the README app list.
4. **Fixed-offset timezone, no DST table** — spec §6.2 says "timezone derived
   from geolocation"; we derive a fixed UTC offset from ip-api's `offset` field
   rather than shipping an IANA→POSIX database. The clock can drift ±1 h across
   a DST change until the next geolocation refresh. Accepted for a gift device;
   manual date/time set remains available in Settings → Clock.

---

## 6. Plan-authoring conventions (applies to all 10 plans)

- Follow `superpowers:writing-plans`: required header, bite-sized TDD steps with
  real code (no placeholders), exact paths, exact commands with expected output,
  a commit step per task.
- Native-testable logic first (in `lib/`), then the thin service/UI wrapper, then
  an **on-device verification task** at the end of the plan (manual steps +
  expected observations, stating which board it needs).
- Reuse, don't redefine: consume interfaces from §4 by `#include`, never copy
  their declarations into new headers.
- Each plan ends with a **"Definition of done"** checklist: native tests green
  (`pio test -e native`), device build green (`pio run -e cyd`), the plan's E2E
  outcome from §1 observed on hardware.
