# Foundation 3 — Storage + Settings Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give danios persistent settings (NVS via `Preferences`), a mounted SD card with an LVGL `S:` filesystem drive for art loading, the Settings → Display/Units/About sections, and battery-saving screen sleep — with graceful SD-missing behavior at boot.

**Architecture:** Pure logic lives in `lib/` (an `ISettingsStore` interface + in-memory fake, and filename filter/sort helpers), TDD'd natively with zero Arduino/LVGL includes. Thin device wrappers sit in `src/services/`: `SettingsService` implements `ISettingsStore` over `Preferences` (namespace `"danios"`), `StorageService` wraps `SD` on VSPI, and `LvglFs` registers an LVGL v8 filesystem driver on drive letter `S` so any widget can `lv_img_set_src(img, "S:/art/icons/<id>.bin")`. Boot flow in `main.cpp` follows spec §3.4 steps 1–2 (mount SD, load settings) and implements spec §6.5 SD-missing behavior; the main loop gains screen sleep per spec §6.4.

**Tech Stack:** PlatformIO (`espressif32@7.0.1`, arduino-esp32 3.x), LVGL 8.4.0, LovyanGFX, `Preferences` (NVS), `SD` (VSPI), Unity native tests, C++17.

**Prerequisites:** F1 (`2026-07-03-foundation-1-lvgl-touch.md` — DisplayService with `setBrightness(uint8_t)`, TouchService, LVGL bound to LovyanGFX, `[env:native]` test env, git init) and F2 (`2026-07-03-foundation-2-launcher.md` — `src/core/App.h`, Launcher with `setAppEnabled`, StatusBar, SettingsApp shell) are merged. This plan modifies files those plans created; where it grafts into F1/F2 code it names the anchor point.

## Global Constraints

(Copied verbatim from roadmap §2 — `docs/superpowers/plans/2026-07-03-danios-roadmap.md`. Every task's requirements implicitly include this section.)

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
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager` (roadmap §4.6).
  No service or app touches `WiFi.*` / `esp_bt_*` power state directly.
- **TDD, native-first:** all pure logic lives in `lib/<module>/` with **zero
  Arduino/LVGL includes** (std C++ only) and is unit-tested with
  `pio test -e native` (Unity). Services/UI wrap the pure logic thinly.
- **Commits:** small, frequent, conventional (`feat:`, `test:`, `fix:`, `docs:`).
  The repo is git-initialized in F1 Task 0; every later plan assumes git exists.
- **SD layout & NVS keys:** exactly as pinned in roadmap §4.1/§4.2 — never invent new
  paths/keys outside your plan's reserved set.

**F3's reserved NVS keys** (roadmap §4.2, namespace `"danios"`):

| Key | Type | Default |
| --- | --- | --- |
| `disp.bright` | u8 range 0–255 (stored via `setU32`/`getU32` — `ISettingsStore` has no u8 accessor) | 160 |
| `disp.sleep_s` | u16 seconds, 0 = never (stored via `setU32`/`getU32`) | 60 |
| `units.f` | bool (false = °C) | false |

---

## File map

| File | Task | Responsibility |
| --- | --- | --- |
| `lib/settings_store/settings_store.h` (create) | 1 | `ISettingsStore` interface (roadmap §4.4 verbatim) + header-only `FakeSettingsStore` |
| `test/test_settings_store/test_main.cpp` (create) | 1 | Native tests for the fake |
| `src/services/SettingsService.h` / `.cpp` (create) | 2 | `ISettingsStore` over `Preferences("danios")` |
| `lib/fs_names/fs_names.h` / `.cpp` (create) | 3 | Pure filename filter/sort (case-insensitive ext, skip hidden/dirs, sort) |
| `test/test_fs_names/test_main.cpp` (create) | 3 | Native tests for fs_names |
| `src/services/StorageService.h` / `.cpp` (create) | 4 | SD mount (`SD.begin(5)`), `exists`, `listFiles`, `readLines` (roadmap §4.9 verbatim) |
| `src/services/LvglFs.h` / `.cpp` (create) | 5 | LVGL v8 FS driver on drive `S` over SD `File` handles + `lvglFsExists` |
| `src/core/Launcher.cpp` (modify) | 5 | Treat missing icon file as `iconPath() == nullptr` → F2 fallback tile |
| `src/core/Version.h` (create) | 6 | `DANIOS_VERSION "0.3.0"` |
| `src/main.cpp` (modify) | 6, 7, 10 | Boot flow (spec §3.4 steps 1–2), SD-missing error (spec §6.5), screen sleep (spec §6.4) |
| `src/apps/settings/Sections.h` (create) | 7 | Declarations of the three `buildSection` free functions |
| `src/apps/settings/SettingsApp.h` / `.cpp` (modify) | 7–9 | Menu list ↔ section-page navigation, dependency injection |
| `src/apps/settings/DisplaySection.cpp` (create) | 7 | Brightness slider (10–255, live) + sleep-timeout dropdown |
| `src/apps/settings/UnitsSection.cpp` (create) | 8 | °C/°F switch |
| `src/apps/settings/AboutSection.cpp` (create) | 9 | Firmware version, free heap, SD status |

---

### Task 1: `lib/settings_store/` — ISettingsStore + FakeSettingsStore

**Files:**
- Create: `lib/settings_store/settings_store.h`
- Test: `test/test_settings_store/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++17 only — zero Arduino/LVGL includes).
- Produces: `ISettingsStore` exactly as roadmap §4.4 — `getU32/setU32`, `getI32/setI32`, `getFloat/setFloat`, `getBool/setBool`, `getString/setString`, `remove` — and `FakeSettingsStore` (header-only, in-memory `std::map` per type). Task 2's `SettingsService`, every Settings section, and all later plans (F4/F5/A2–A5) consume `ISettingsStore` by `#include <settings_store.h>` — never by re-declaring it.

- [ ] **Step 1: Write the failing test**

Create `test/test_settings_store/test_main.cpp`:

```cpp
#include <unity.h>
#include <settings_store.h>

void setUp() {}
void tearDown() {}

void test_defaults_returned_when_key_missing() {
  FakeSettingsStore s;
  TEST_ASSERT_EQUAL_UINT32(160u, s.getU32("disp.bright", 160u));
  TEST_ASSERT_EQUAL_INT32(-5, s.getI32("nope", -5));
  TEST_ASSERT_EQUAL_FLOAT(1.5f, s.getFloat("nope", 1.5f));
  TEST_ASSERT_FALSE(s.getBool("units.f", false));
  TEST_ASSERT_TRUE(s.getBool("nope2", true));
  TEST_ASSERT_EQUAL_STRING("UTC0", s.getString("tz", "UTC0").c_str());
}

void test_u32_roundtrip() {
  FakeSettingsStore s;
  s.setU32("disp.bright", 200u);
  TEST_ASSERT_EQUAL_UINT32(200u, s.getU32("disp.bright", 160u));
}

void test_i32_roundtrip_negative() {
  FakeSettingsStore s;
  s.setI32("pet.disc", -3);
  TEST_ASSERT_EQUAL_INT32(-3, s.getI32("pet.disc", 0));
}

void test_float_roundtrip() {
  FakeSettingsStore s;
  s.setFloat("loc.lat", -23.55f);
  TEST_ASSERT_EQUAL_FLOAT(-23.55f, s.getFloat("loc.lat", 0.0f));
}

void test_bool_roundtrip() {
  FakeSettingsStore s;
  s.setBool("units.f", true);
  TEST_ASSERT_TRUE(s.getBool("units.f", false));
  s.setBool("units.f", false);
  TEST_ASSERT_FALSE(s.getBool("units.f", true));
}

void test_string_roundtrip() {
  FakeSettingsStore s;
  s.setString("wifi.ssid", "HomeNet");
  TEST_ASSERT_EQUAL_STRING("HomeNet", s.getString("wifi.ssid", "").c_str());
}

void test_overwrite_replaces_value() {
  FakeSettingsStore s;
  s.setU32("disp.sleep_s", 30u);
  s.setU32("disp.sleep_s", 300u);
  TEST_ASSERT_EQUAL_UINT32(300u, s.getU32("disp.sleep_s", 60u));
}

void test_remove_erases_key_for_all_types() {
  FakeSettingsStore s;
  s.setU32("k", 9u);
  s.setString("k", "nine");
  s.setBool("k", true);
  s.remove("k");
  TEST_ASSERT_EQUAL_UINT32(1u, s.getU32("k", 1u));
  TEST_ASSERT_EQUAL_STRING("d", s.getString("k", "d").c_str());
  TEST_ASSERT_FALSE(s.getBool("k", false));
}

void test_distinct_typed_keys_coexist() {
  FakeSettingsStore s;
  s.setU32("disp.bright", 42u);
  s.setBool("units.f", true);
  s.setString("wifi.ssid", "net");
  s.setFloat("loc.lon", 1.25f);
  s.setI32("pet.disc", -1);
  TEST_ASSERT_EQUAL_UINT32(42u, s.getU32("disp.bright", 0u));
  TEST_ASSERT_TRUE(s.getBool("units.f", false));
  TEST_ASSERT_EQUAL_STRING("net", s.getString("wifi.ssid", "").c_str());
  TEST_ASSERT_EQUAL_FLOAT(1.25f, s.getFloat("loc.lon", 0.0f));
  TEST_ASSERT_EQUAL_INT32(-1, s.getI32("pet.disc", 0));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_defaults_returned_when_key_missing);
  RUN_TEST(test_u32_roundtrip);
  RUN_TEST(test_i32_roundtrip_negative);
  RUN_TEST(test_float_roundtrip);
  RUN_TEST(test_bool_roundtrip);
  RUN_TEST(test_string_roundtrip);
  RUN_TEST(test_overwrite_replaces_value);
  RUN_TEST(test_remove_erases_key_for_all_types);
  RUN_TEST(test_distinct_typed_keys_coexist);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_settings_store`
Expected: FAIL — compile error `settings_store.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

Create `lib/settings_store/settings_store.h`. The interface block is roadmap §4.4 **verbatim**; the fake is header-only with one `std::map` per type (mirrors NVS semantics: a get with the wrong type for a key returns the default, because each type lives in its own map).

```cpp
// lib/settings_store/settings_store.h — std C++ only
#pragma once
#include <cstdint>
#include <map>
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

// In-memory implementation for native tests. One map per type, matching NVS
// behavior where a key read as the wrong type yields the caller's default.
class FakeSettingsStore : public ISettingsStore {
public:
  uint32_t getU32(const char* key, uint32_t def) override {
    auto it = u32_.find(key);
    return it == u32_.end() ? def : it->second;
  }
  void setU32(const char* key, uint32_t v) override { u32_[key] = v; }

  int32_t getI32(const char* key, int32_t def) override {
    auto it = i32_.find(key);
    return it == i32_.end() ? def : it->second;
  }
  void setI32(const char* key, int32_t v) override { i32_[key] = v; }

  float getFloat(const char* key, float def) override {
    auto it = flt_.find(key);
    return it == flt_.end() ? def : it->second;
  }
  void setFloat(const char* key, float v) override { flt_[key] = v; }

  bool getBool(const char* key, bool def) override {
    auto it = bool_.find(key);
    return it == bool_.end() ? def : it->second;
  }
  void setBool(const char* key, bool v) override { bool_[key] = v; }

  std::string getString(const char* key, const std::string& def) override {
    auto it = str_.find(key);
    return it == str_.end() ? def : it->second;
  }
  void setString(const char* key, const std::string& v) override { str_[key] = v; }

  void remove(const char* key) override {
    u32_.erase(key);
    i32_.erase(key);
    flt_.erase(key);
    bool_.erase(key);
    str_.erase(key);
  }

private:
  std::map<std::string, uint32_t>    u32_;
  std::map<std::string, int32_t>     i32_;
  std::map<std::string, float>       flt_;
  std::map<std::string, bool>        bool_;
  std::map<std::string, std::string> str_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_settings_store`
Expected: PASS — `9 test cases: 9 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add lib/settings_store/settings_store.h test/test_settings_store/test_main.cpp
git commit -m "feat: add ISettingsStore interface and FakeSettingsStore (roadmap 4.4)"
```

---

### Task 2: `SettingsService` — ISettingsStore over NVS Preferences

**Files:**
- Create: `src/services/SettingsService.h`
- Create: `src/services/SettingsService.cpp`

**Interfaces:**
- Consumes: `ISettingsStore` from `lib/settings_store/settings_store.h` (Task 1); `Preferences` from arduino-esp32.
- Produces: `class SettingsService : public ISettingsStore` with `void begin()` (opens NVS namespace `"danios"` read-write). One global instance owned by `main.cpp` (Task 6); consumers receive it **by reference as `ISettingsStore&`** — no consumer ever includes `SettingsService.h` except `main.cpp`.

This is a thin device-only wrapper — no native test is possible (NVS needs the chip); its logic is exercised by the fake's contract (Task 1) plus the on-device verification (Task 11). The check here is that the device build stays green.

- [ ] **Step 1: Write the header**

Create `src/services/SettingsService.h`:

```cpp
// NVS-backed ISettingsStore (namespace "danios"). Owned by main.cpp; everyone
// else takes ISettingsStore& — do not include this header outside main.cpp.
#pragma once
#include <Preferences.h>
#include <settings_store.h>

class SettingsService : public ISettingsStore {
public:
  void begin();  // opens NVS namespace "danios" read-write; call once in setup()

  uint32_t    getU32(const char* key, uint32_t def) override;
  void        setU32(const char* key, uint32_t v) override;
  int32_t     getI32(const char* key, int32_t def) override;
  void        setI32(const char* key, int32_t v) override;
  float       getFloat(const char* key, float def) override;
  void        setFloat(const char* key, float v) override;
  bool        getBool(const char* key, bool def) override;
  void        setBool(const char* key, bool v) override;
  std::string getString(const char* key, const std::string& def) override;
  void        setString(const char* key, const std::string& v) override;
  void        remove(const char* key) override;

private:
  Preferences prefs_;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/SettingsService.cpp`:

```cpp
#include "SettingsService.h"

void SettingsService::begin() {
  prefs_.begin("danios", /*readOnly=*/false);
}

uint32_t SettingsService::getU32(const char* key, uint32_t def) {
  return prefs_.getUInt(key, def);
}
void SettingsService::setU32(const char* key, uint32_t v) {
  prefs_.putUInt(key, v);
}

int32_t SettingsService::getI32(const char* key, int32_t def) {
  return prefs_.getInt(key, def);
}
void SettingsService::setI32(const char* key, int32_t v) {
  prefs_.putInt(key, v);
}

float SettingsService::getFloat(const char* key, float def) {
  return prefs_.getFloat(key, def);
}
void SettingsService::setFloat(const char* key, float v) {
  prefs_.putFloat(key, v);
}

bool SettingsService::getBool(const char* key, bool def) {
  return prefs_.getBool(key, def);
}
void SettingsService::setBool(const char* key, bool v) {
  prefs_.putBool(key, v);
}

std::string SettingsService::getString(const char* key, const std::string& def) {
  String s = prefs_.getString(key, String(def.c_str()));
  return std::string(s.c_str());
}
void SettingsService::setString(const char* key, const std::string& v) {
  prefs_.putString(key, v.c_str());
}

void SettingsService::remove(const char* key) {
  prefs_.remove(key);
}
```

- [ ] **Step 3: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS` (the new files compile; nothing references them yet).

- [ ] **Step 4: Verify native tests still pass**

Run: `pio test -e native`
Expected: all suites PASS (`SettingsService.cpp` lives in `src/`, which native tests do not build — `test_build_src = false`).

- [ ] **Step 5: Commit**

```bash
git add src/services/SettingsService.h src/services/SettingsService.cpp
git commit -m "feat: add SettingsService, NVS-backed ISettingsStore (namespace danios)"
```

---

### Task 3: `lib/fs_names/` — pure filename filtering and sorting

**Files:**
- Create: `lib/fs_names/fs_names.h`
- Create: `lib/fs_names/fs_names.cpp`
- Test: `test/test_fs_names/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++17 only).
- Produces (F3-defined, consumed by Task 4's `StorageService` and later by A4's playlist code):
  - `struct FsEntry { std::string name; bool isDir; };`
  - `bool hasExtension(const std::string& name, const std::string& ext);` — case-insensitive suffix match; empty `ext` matches everything.
  - `bool isHiddenName(const std::string& name);` — true for names starting with `'.'` (covers `.DS_Store`, `._foo` is not dot-prefixed but `.Trashes` etc. are; FAT32 junk that starts with `.` is the common case).
  - `std::vector<std::string> filterAndSortNames(const std::vector<FsEntry>& entries, const std::string& ext);` — keeps non-directory, non-hidden entries whose name matches `ext`, returns names sorted ascending byte-wise (so `"C.mp3"` sorts before `"a.mp3"` — deterministic, locale-free).

- [ ] **Step 1: Write the failing test**

Create `test/test_fs_names/test_main.cpp`:

```cpp
#include <unity.h>
#include <fs_names.h>

void setUp() {}
void tearDown() {}

void test_has_extension_case_insensitive() {
  TEST_ASSERT_TRUE(hasExtension("song.mp3", ".mp3"));
  TEST_ASSERT_TRUE(hasExtension("SONG.MP3", ".mp3"));
  TEST_ASSERT_TRUE(hasExtension("icon.bin", ".BIN"));
  TEST_ASSERT_TRUE(hasExtension("mixed.Mp3", ".mP3"));
}

void test_has_extension_rejects_wrong_and_short() {
  TEST_ASSERT_FALSE(hasExtension("song.wav", ".mp3"));
  TEST_ASSERT_FALSE(hasExtension("mp3", ".mp3"));      // shorter than ext
  TEST_ASSERT_FALSE(hasExtension("", ".mp3"));
  TEST_ASSERT_FALSE(hasExtension("song.mp3x", ".mp3")); // suffix only
}

void test_empty_ext_matches_everything() {
  TEST_ASSERT_TRUE(hasExtension("anything.xyz", ""));
  TEST_ASSERT_TRUE(hasExtension("noext", ""));
}

void test_hidden_name_detection() {
  TEST_ASSERT_TRUE(isHiddenName(".DS_Store"));
  TEST_ASSERT_TRUE(isHiddenName(".hidden"));
  TEST_ASSERT_FALSE(isHiddenName("song.mp3"));
  TEST_ASSERT_FALSE(isHiddenName(""));
}

void test_filter_skips_dirs_and_hidden() {
  std::vector<FsEntry> in = {
      {"song.mp3", false},
      {".DS_Store", false},   // hidden -> skipped
      {"albums", true},       // directory -> skipped
      {"other.mp3", false},
  };
  auto out = filterAndSortNames(in, ".mp3");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("other.mp3", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("song.mp3", out[1].c_str());
}

void test_filter_by_extension_case_insensitive() {
  std::vector<FsEntry> in = {
      {"b.MP3", false},
      {"readme.txt", false},
      {"a.mp3", false},
  };
  auto out = filterAndSortNames(in, ".mp3");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("a.mp3", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("b.MP3", out[1].c_str());
}

void test_sort_is_bytewise_ascending() {
  std::vector<FsEntry> in = {
      {"b.bin", false},
      {"C.bin", false},   // 'C' (0x43) < 'a' (0x61) byte-wise
      {"a.bin", false},
  };
  auto out = filterAndSortNames(in, ".bin");
  TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("C.bin", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("a.bin", out[1].c_str());
  TEST_ASSERT_EQUAL_STRING("b.bin", out[2].c_str());
}

void test_empty_ext_keeps_all_files_but_not_dirs() {
  std::vector<FsEntry> in = {
      {"wisdom.txt", false},
      {"art", true},
      {"notes.md", false},
  };
  auto out = filterAndSortNames(in, "");
  TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)out.size());
  TEST_ASSERT_EQUAL_STRING("notes.md", out[0].c_str());
  TEST_ASSERT_EQUAL_STRING("wisdom.txt", out[1].c_str());
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_has_extension_case_insensitive);
  RUN_TEST(test_has_extension_rejects_wrong_and_short);
  RUN_TEST(test_empty_ext_matches_everything);
  RUN_TEST(test_hidden_name_detection);
  RUN_TEST(test_filter_skips_dirs_and_hidden);
  RUN_TEST(test_filter_by_extension_case_insensitive);
  RUN_TEST(test_sort_is_bytewise_ascending);
  RUN_TEST(test_empty_ext_keeps_all_files_but_not_dirs);
  return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pio test -e native -f test_fs_names`
Expected: FAIL — compile error `fs_names.h: No such file or directory`.

- [ ] **Step 3: Write the implementation**

Create `lib/fs_names/fs_names.h`:

```cpp
// lib/fs_names/fs_names.h — std C++ only. Pure filename filtering/sorting for
// SD directory listings (used by StorageService::listFiles and A4 playlist).
#pragma once
#include <string>
#include <vector>

struct FsEntry {
  std::string name;  // basename only, no path
  bool isDir;
};

// Case-insensitive suffix match, e.g. hasExtension("A.MP3", ".mp3") == true.
// Empty ext matches everything.
bool hasExtension(const std::string& name, const std::string& ext);

// True for dotfiles (".DS_Store" and friends) that should never be listed.
bool isHiddenName(const std::string& name);

// Keep non-directory, non-hidden entries matching ext; return names sorted
// ascending byte-wise (deterministic, locale-free).
std::vector<std::string> filterAndSortNames(const std::vector<FsEntry>& entries,
                                            const std::string& ext);
```

Create `lib/fs_names/fs_names.cpp`:

```cpp
#include "fs_names.h"

#include <algorithm>
#include <cctype>

bool hasExtension(const std::string& name, const std::string& ext) {
  if (ext.empty()) return true;
  if (name.size() < ext.size()) return false;
  const size_t off = name.size() - ext.size();
  for (size_t i = 0; i < ext.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(name[off + i])) !=
        std::tolower(static_cast<unsigned char>(ext[i]))) {
      return false;
    }
  }
  return true;
}

bool isHiddenName(const std::string& name) {
  return !name.empty() && name[0] == '.';
}

std::vector<std::string> filterAndSortNames(const std::vector<FsEntry>& entries,
                                            const std::string& ext) {
  std::vector<std::string> out;
  for (const auto& e : entries) {
    if (e.isDir) continue;
    if (isHiddenName(e.name)) continue;
    if (!hasExtension(e.name, ext)) continue;
    out.push_back(e.name);
  }
  std::sort(out.begin(), out.end());
  return out;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `pio test -e native -f test_fs_names`
Expected: PASS — `8 test cases: 8 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add lib/fs_names/ test/test_fs_names/test_main.cpp
git commit -m "feat: add fs_names pure filename filter/sort helpers"
```

---

### Task 4: `StorageService` — SD card on VSPI

**Files:**
- Create: `src/services/StorageService.h`
- Create: `src/services/StorageService.cpp`

**Interfaces:**
- Consumes: `FsEntry` / `filterAndSortNames` from `lib/fs_names/fs_names.h` (Task 3); Arduino `SD` library (VSPI defaults: SCK 18, MISO 19, MOSI 23; CS 5 — see `docs/hardware.md`).
- Produces: `StorageService` **exactly** as roadmap §4.9:
  - `bool begin();` — `SD.begin(5)`; false if card missing.
  - `bool mounted() const;`
  - `bool exists(const char* path);`
  - `std::vector<std::string> listFiles(const char* dir, const char* ext);` — sorted, non-recursive.
  - `bool readLines(const char* path, std::vector<std::string>& out);` — trims `\r`, skips empty lines.
  One global instance owned by `main.cpp` (Task 6). A2 (Oracle) consumes `readLines`; A4 (Music) consumes `listFiles`.

- [ ] **Step 1: Write the header**

Create `src/services/StorageService.h`:

```cpp
// SD card access (VSPI: SCK 18, MISO 19, MOSI 23, CS 5 — separate bus from the
// display's HSPI, no sharing issues). Roadmap 4.9 — signatures are pinned.
#pragma once
#include <string>
#include <vector>

class StorageService {
public:
  bool begin();                                   // SD.begin(5); false if missing
  bool mounted() const;
  bool exists(const char* path);
  std::vector<std::string> listFiles(const char* dir, const char* ext); // sorted, non-recursive
  bool readLines(const char* path, std::vector<std::string>& out);      // trims \r, skips empty

private:
  bool mounted_ = false;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/StorageService.cpp`:

```cpp
#include "StorageService.h"

#include <SD.h>
#include <fs_names.h>

bool StorageService::begin() {
  // VSPI default pins (SCK 18, MISO 19, MOSI 23) + CS 5. Three separate buses
  // on this board; the display's HSPI is untouched.
  mounted_ = SD.begin(5);
  return mounted_;
}

bool StorageService::mounted() const {
  return mounted_;
}

bool StorageService::exists(const char* path) {
  return mounted_ && SD.exists(path);
}

std::vector<std::string> StorageService::listFiles(const char* dir, const char* ext) {
  std::vector<FsEntry> entries;
  if (mounted_) {
    File d = SD.open(dir);
    if (d && d.isDirectory()) {
      for (File f = d.openNextFile(); f; f = d.openNextFile()) {
        // arduino-esp32 3.x File::name() returns the basename (no path).
        entries.push_back(FsEntry{std::string(f.name()), f.isDirectory()});
        f.close();
      }
    }
    if (d) d.close();
  }
  return filterAndSortNames(entries, ext ? std::string(ext) : std::string());
}

bool StorageService::readLines(const char* path, std::vector<std::string>& out) {
  out.clear();
  if (!mounted_) return false;
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  std::string line;
  auto flush = [&out, &line]() {
    while (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) out.push_back(line);
    line.clear();
  };
  while (f.available()) {
    const char c = static_cast<char>(f.read());
    if (c == '\n') {
      flush();
    } else {
      line += c;
    }
  }
  flush();  // last line may lack a trailing newline
  f.close();
  return true;
}
```

- [ ] **Step 3: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Verify native tests still pass**

Run: `pio test -e native`
Expected: all suites PASS.

- [ ] **Step 5: Commit**

```bash
git add src/services/StorageService.h src/services/StorageService.cpp
git commit -m "feat: add StorageService — SD mount, exists, listFiles, readLines (roadmap 4.9)"
```

---

### Task 5: LVGL FS driver on drive `S` + launcher icon fallback

**Files:**
- Create: `src/services/LvglFs.h`
- Create: `src/services/LvglFs.cpp`
- Modify: `src/core/Launcher.cpp` (F2 file — the icon-creation code inside `Launcher::show()`)

**Interfaces:**
- Consumes: LVGL v8 `lv_fs_drv_t` API; Arduino `SD` (`File` handles); F2's `Launcher::show()` fallback-tile branch (the path taken when `App::iconPath()` returns `nullptr`).
- Produces (F3-defined):
  - `void lvglFsRegisterSd();` — registers drive letter `'S'` mapping to the SD card. Call **once**, after a successful `StorageService::begin()` (Task 6 does this). After registration, `lv_img_set_src(img, "S:/art/icons/calc.bin")` works everywhere — this is the roadmap §4.1 art-loading contract every app plan relies on.
  - `bool lvglFsExists(const char* path);` — true if an LVGL-path (`"S:/..."`) opens for read. Safe to call even if the drive is not registered (returns false).

**Note on LVGL v8 path handling:** `lv_fs` strips the `"S:"` prefix before invoking the driver callbacks — `"S:/art/icons/calc.bin"` arrives at `open_cb` as `"/art/icons/calc.bin"`. The open callback defends against a missing leading slash anyway.

- [ ] **Step 1: Write the header**

Create `src/services/LvglFs.h`:

```cpp
// LVGL v8 filesystem driver: drive letter 'S' -> SD card (read-only).
// Register once after a successful SD mount; then any LVGL widget can load
// "S:/art/..." paths (roadmap 4.1).
#pragma once

void lvglFsRegisterSd();

// True if `path` (e.g. "S:/art/icons/calc.bin") opens for read. Returns false
// when the file is missing OR the S drive was never registered (no SD card).
bool lvglFsExists(const char* path);
```

- [ ] **Step 2: Write the implementation**

Create `src/services/LvglFs.cpp`:

```cpp
#include "LvglFs.h"

#include <SD.h>
#include <lvgl.h>

#include <cstdio>

namespace {

void* fsOpen(lv_fs_drv_t* /*drv*/, const char* path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD) return nullptr;  // read-only driver
  char full[128];
  snprintf(full, sizeof(full), "%s%s", (path[0] == '/') ? "" : "/", path);
  File f = SD.open(full, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return nullptr;
  }
  return new File(f);  // heap-owned handle; freed in fsClose
}

lv_fs_res_t fsClose(lv_fs_drv_t* /*drv*/, void* file_p) {
  File* f = static_cast<File*>(file_p);
  f->close();
  delete f;
  return LV_FS_RES_OK;
}

lv_fs_res_t fsRead(lv_fs_drv_t* /*drv*/, void* file_p, void* buf, uint32_t btr,
                   uint32_t* br) {
  File* f = static_cast<File*>(file_p);
  *br = static_cast<uint32_t>(f->read(static_cast<uint8_t*>(buf), btr));
  return LV_FS_RES_OK;
}

lv_fs_res_t fsSeek(lv_fs_drv_t* /*drv*/, void* file_p, uint32_t pos,
                   lv_fs_whence_t whence) {
  File* f = static_cast<File*>(file_p);
  SeekMode m = SeekSet;
  if (whence == LV_FS_SEEK_CUR) m = SeekCur;
  else if (whence == LV_FS_SEEK_END) m = SeekEnd;
  return f->seek(pos, m) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

lv_fs_res_t fsTell(lv_fs_drv_t* /*drv*/, void* file_p, uint32_t* pos_p) {
  File* f = static_cast<File*>(file_p);
  *pos_p = static_cast<uint32_t>(f->position());
  return LV_FS_RES_OK;
}

lv_fs_drv_t g_drv;  // must outlive registration — static storage

}  // namespace

void lvglFsRegisterSd() {
  lv_fs_drv_init(&g_drv);
  g_drv.letter = 'S';
  g_drv.open_cb = fsOpen;
  g_drv.close_cb = fsClose;
  g_drv.read_cb = fsRead;
  g_drv.seek_cb = fsSeek;
  g_drv.tell_cb = fsTell;
  lv_fs_drv_register(&g_drv);
}

bool lvglFsExists(const char* path) {
  lv_fs_file_t f;
  if (lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return false;
  lv_fs_close(&f);
  return true;
}
```

- [ ] **Step 3: Graft the missing-icon guard into the Launcher**

Modify `src/core/Launcher.cpp`. F2's `Launcher::show()` already branches on `app->iconPath() == nullptr` to draw the fallback tile (colored `lv_obj` box / `LV_SYMBOL_*`). Find where the tile decides between real icon and fallback — it reads `app->iconPath()` — and insert the guard so a **present path but missing/unopenable file** takes the same fallback branch:

```cpp
// At the top of src/core/Launcher.cpp, with the other includes:
#include "../services/LvglFs.h"
```

```cpp
// Inside Launcher::show(), where each app tile's icon source is chosen.
// BEFORE (F2):
//   const char* icon = app->iconPath();
//   if (icon) { ...lv_img_set_src(img, icon)... } else { ...fallback tile... }
// AFTER (F3) — one added line:
const char* icon = app->iconPath();
if (icon && !lvglFsExists(icon)) icon = nullptr;  // missing art -> fallback tile
```

(Keep every other line of the F2 branch untouched — this guard only converts "file absent on SD / no SD" into the already-tested `nullptr` fallback path.)

- [ ] **Step 4: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/services/LvglFs.h src/services/LvglFs.cpp src/core/Launcher.cpp
git commit -m "feat: register LVGL S: drive over SD and fall back on missing icon art"
```

---

### Task 6: Boot flow — mount SD, load settings, SD-missing behavior

**Files:**
- Create: `src/core/Version.h`
- Modify: `src/main.cpp` (F1/F2 file)

**Interfaces:**
- Consumes: `SettingsService` (Task 2), `StorageService` (Task 4), `lvglFsRegisterSd()` (Task 5), F1's `DisplayService::setBrightness(uint8_t)`, F2's `Launcher::setAppEnabled(const char* appId, bool en)` and app ids `"weather"`, `"music"`, `"oracle"` (roadmap §4.5).
- Produces:
  - `src/core/Version.h` defining `DANIOS_VERSION "0.3.0"` (consumed by Task 9's AboutSection; later plans bump the version string here).
  - Global instances `SettingsService settings;` and `StorageService storage;` in `main.cpp` — the single owners per roadmap §4.4/§4.9. Later tasks/plans receive them by reference (`ISettingsStore&` / `StorageService&`).

**Boot order (spec §3.4 steps 1–2, then F1's step 3):** mount SD → load settings → init display/touch/LVGL → apply persisted brightness → register `S:` drive (only if mounted) → register apps → disable SD-dependent apps if unmounted → show launcher → show dismissable SD error on top if unmounted. Per spec §6.5: Weather, Music, and Oracle are SD-dependent and get disabled; **Pet stays enabled** (its state lives in NVS and it renders placeholder art); Calculator and Settings are unaffected.

- [ ] **Step 1: Create the version header**

Create `src/core/Version.h`:

```cpp
// Firmware version, shown in Settings -> About. Bump per foundation/app plan.
#pragma once
#define DANIOS_VERSION "0.3.0"
```

- [ ] **Step 2: Add services and boot steps to main.cpp**

Modify `src/main.cpp`. *(F1/F2 own this file's existing content; instance names below — `displayService`, `launcher`, `settingsApp` — must match the actual F1/F2 globals; adjust only the names, never the logic.)*

Add includes and globals near the top, with the other service globals:

```cpp
#include "core/Version.h"
#include "services/LvglFs.h"
#include "services/SettingsService.h"
#include "services/StorageService.h"

SettingsService settings;   // sole owner (roadmap 4.4); pass as ISettingsStore&
StorageService storage;     // sole owner (roadmap 4.9)
```

Add the SD-missing error helper above `setup()` (LVGL v8 msgbox with a `nullptr` parent is created on the top layer and is modal, so it covers the launcher until dismissed):

```cpp
static void sdErrorMsgboxCb(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  lv_msgbox_close(mbox);  // "OK" tapped -> launcher is already behind it
}

static void showSdMissingError() {
  static const char* kBtns[] = {"OK", ""};
  lv_obj_t* m = lv_msgbox_create(
      nullptr, "No memory card",
      "I couldn't find the microSD card, so Weather, Music and Oracle\n"
      "are taking a nap.\n\n"
      "Your pet is fine - it lives inside me, not on the card!\n\n"
      "Insert the card and restart to bring everything back.",
      kBtns, false);
  lv_obj_set_width(m, 230);  // near-full-width on the 240 px portrait screen
  lv_obj_center(m);
  lv_obj_add_event_cb(m, sdErrorMsgboxCb, LV_EVENT_VALUE_CHANGED, nullptr);
}
```

- [ ] **Step 3: Wire the boot sequence in setup()**

In `setup()`, insert spec §3.4 steps 1–2 **before** the F1 display/touch/LVGL init block:

```cpp
// Spec 3.4 step 1: mount SD (VSPI, CS 5).
const bool sdOk = storage.begin();
// Spec 3.4 step 2: open settings (NVS namespace "danios").
settings.begin();
```

Immediately **after** the F1 display/touch/LVGL init block (display is now up), replace F1's fixed `displayService.setBrightness(160);` call with the persisted value, and register the `S:` drive:

```cpp
// Apply persisted brightness (roadmap 4.2: disp.bright, default 160).
displayService.setBrightness(static_cast<uint8_t>(settings.getU32("disp.bright", 160)));

// Register the LVGL 'S:' drive so app icons/art load from SD (roadmap 4.1).
if (sdOk) lvglFsRegisterSd();
```

After the F2 app-registration block and **before** `launcher.show()`, add the spec §6.5 behavior:

```cpp
if (!sdOk) {
  // Spec 6.5: SD-dependent apps disabled. Pet is the exception (state in NVS,
  // placeholder art); Calculator and Settings never depend on the card.
  launcher.setAppEnabled("weather", false);
  launcher.setAppEnabled("music", false);
  launcher.setAppEnabled("oracle", false);
}
```

And immediately **after** `launcher.show()`:

```cpp
if (!sdOk) showSdMissingError();  // modal on top of the launcher, dismissable
```

- [ ] **Step 4: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/core/Version.h src/main.cpp
git commit -m "feat: boot flow — mount SD, load settings, SD-missing error + app disable"
```

---

### Task 7: Settings shell wiring + DisplaySection (brightness, sleep timeout)

**Files:**
- Create: `src/apps/settings/Sections.h`
- Create: `src/apps/settings/DisplaySection.cpp`
- Modify: `src/apps/settings/SettingsApp.h` (F2 file)
- Modify: `src/apps/settings/SettingsApp.cpp` (F2 file)
- Modify: `src/main.cpp` (inject dependencies)

**Interfaces:**
- Consumes: `ISettingsStore` (Task 1), F1's `DisplayService::setBrightness(uint8_t)`, Task 4's `StorageService`, F2's `SettingsApp` shell (an `App` with id `"settings"`, `requiredRadio() = None`, `buildUI(lv_obj_t*)` building an empty `lv_list` — F3 replaces that body).
- Produces:
  - `src/apps/settings/Sections.h` declaring the roadmap §4.11 section builders (`void buildSection(lv_obj_t* parent, /* deps by reference */)` shape). Tasks 8/9 add their functions here; F4/F5/A3 append theirs later:
    - `void buildDisplaySection(lv_obj_t* parent, ISettingsStore& store, DisplayService& display);`
    - `void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store);` *(implemented in Task 8)*
    - `void buildAboutSection(lv_obj_t* parent, StorageService& storage);` *(implemented in Task 9)*
  - `SettingsApp::setDeps(ISettingsStore& store, DisplayService& display, StorageService& storage)` — called once from `main.cpp` before app registration.
  - The menu↔section navigation pattern (`showMenu()` / `showSection(int)`) that Tasks 8/9 extend by adding one array entry and one switch case each.

**Behavior:** the brightness slider (range **10–255** so the user can't slide the panel to black) applies **live** via `DisplayService::setBrightness` on every `LV_EVENT_VALUE_CHANGED`, but persists `disp.bright` only on `LV_EVENT_RELEASED` — one NVS write per gesture, not per pixel of drag (flash wear). The sleep dropdown (Never/30 s/1 min/2 min/5 min → 0/30/60/120/300) persists `disp.sleep_s` immediately; the main loop (Task 10) reads it live, so no reboot is needed.

- [ ] **Step 1: Create the sections header**

Create `src/apps/settings/Sections.h`:

```cpp
// Settings section builders (roadmap 4.11). Each builds its widgets into
// `parent` (a flex-column container provided by SettingsApp::showSection) and
// takes its dependencies by reference. F4/F5/A3 append their declarations here.
#pragma once
#include <lvgl.h>
#include <settings_store.h>

class DisplayService;
class StorageService;

void buildDisplaySection(lv_obj_t* parent, ISettingsStore& store, DisplayService& display);
void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store);      // Task 8
void buildAboutSection(lv_obj_t* parent, StorageService& storage);    // Task 9
```

- [ ] **Step 2: Implement DisplaySection**

Create `src/apps/settings/DisplaySection.cpp`:

```cpp
#include "Sections.h"

#include "../../services/DisplayService.h"

namespace {

// Valid while the section is on screen (sections are rebuilt on every visit;
// the deps are process-lifetime singletons owned by main.cpp).
struct Ctx {
  ISettingsStore* store = nullptr;
  DisplayService* display = nullptr;
};
Ctx ctx;

const uint32_t kSleepSeconds[] = {0, 30, 60, 120, 300};
const int kSleepCount = 5;

void brightnessChanged(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  const int32_t v = lv_slider_get_value(slider);
  ctx.display->setBrightness(static_cast<uint8_t>(v));  // live preview
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
    ctx.store->setU32("disp.bright", static_cast<uint32_t>(v));  // persist once
  }
}

void sleepChanged(lv_event_t* e) {
  lv_obj_t* dd = lv_event_get_target(e);
  const uint16_t idx = lv_dropdown_get_selected(dd);
  if (idx < kSleepCount) {
    ctx.store->setU32("disp.sleep_s", kSleepSeconds[idx]);
  }
}

}  // namespace

void buildDisplaySection(lv_obj_t* parent, ISettingsStore& store, DisplayService& display) {
  ctx.store = &store;
  ctx.display = &display;

  lv_obj_t* brightLbl = lv_label_create(parent);
  lv_label_set_text(brightLbl, "Brightness");

  lv_obj_t* slider = lv_slider_create(parent);
  lv_obj_set_width(slider, LV_PCT(90));
  lv_slider_set_range(slider, 10, 255);  // 10 = floor: never fully dark
  lv_slider_set_value(slider, static_cast<int32_t>(store.getU32("disp.bright", 160)),
                      LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightnessChanged, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(slider, brightnessChanged, LV_EVENT_RELEASED, nullptr);

  lv_obj_t* sleepLbl = lv_label_create(parent);
  lv_label_set_text(sleepLbl, "Screen sleep");

  lv_obj_t* dd = lv_dropdown_create(parent);
  lv_obj_set_width(dd, LV_PCT(90));
  lv_dropdown_set_options(dd, "Never\n30 seconds\n1 minute\n2 minutes\n5 minutes");
  const uint32_t cur = store.getU32("disp.sleep_s", 60);
  uint16_t sel = 2;  // "1 minute" — matches the 60 s default
  for (int i = 0; i < kSleepCount; ++i) {
    if (kSleepSeconds[i] == cur) { sel = static_cast<uint16_t>(i); break; }
  }
  lv_dropdown_set_selected(dd, sel);
  lv_obj_add_event_cb(dd, sleepChanged, LV_EVENT_VALUE_CHANGED, nullptr);
}
```

- [ ] **Step 3: Extend the SettingsApp shell**

Modify `src/apps/settings/SettingsApp.h` — keep F2's `App` overrides (`id()`, `title()`, `iconPath()`, `requiredRadio()`, `onEnter()`, `onExit()`) exactly as they are; add the F3 members so the class reads:

```cpp
#pragma once
#include <settings_store.h>

#include "../../core/App.h"

class DisplayService;
class StorageService;

class SettingsApp : public App {
public:
  // --- F2 overrides: id/title/iconPath/requiredRadio/onEnter/onExit unchanged ---

  void buildUI(lv_obj_t* parent) override;

  // F3: dependency injection. Call once from main.cpp, before registerApp.
  void setDeps(ISettingsStore& store, DisplayService& display, StorageService& storage);

private:
  void showMenu();          // the lv_list of sections
  void showSection(int idx);
  static void menuClicked(lv_event_t* e);
  static void backClicked(lv_event_t* e);

  ISettingsStore* store_ = nullptr;
  DisplayService* display_ = nullptr;
  StorageService* storage_ = nullptr;
  lv_obj_t* root_ = nullptr;
};
```

Modify `src/apps/settings/SettingsApp.cpp` — replace F2's empty-list `buildUI` body with the menu/section navigation (leave the F2 override implementations for `id()`/`title()`/etc. untouched):

```cpp
#include "SettingsApp.h"

#include <cstdint>

#include "Sections.h"

namespace {
SettingsApp* g_self = nullptr;  // single instance, owned by main.cpp

// Tasks 8 and 9 extend this array and the switch in showSection.
const char* kSectionNames[] = {"Display"};
constexpr int kSectionCount =
    static_cast<int>(sizeof(kSectionNames) / sizeof(kSectionNames[0]));
}  // namespace

void SettingsApp::setDeps(ISettingsStore& store, DisplayService& display,
                          StorageService& storage) {
  store_ = &store;
  display_ = &display;
  storage_ = &storage;
}

void SettingsApp::buildUI(lv_obj_t* parent) {
  g_self = this;
  root_ = parent;
  showMenu();
}

void SettingsApp::showMenu() {
  lv_obj_clean(root_);
  lv_obj_t* list = lv_list_create(root_);
  lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
  for (int i = 0; i < kSectionCount; ++i) {
    lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_RIGHT, kSectionNames[i]);
    lv_obj_add_event_cb(btn, menuClicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<intptr_t>(i)));
  }
}

void SettingsApp::menuClicked(lv_event_t* e) {
  g_self->showSection(
      static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
}

void SettingsApp::backClicked(lv_event_t* /*e*/) {
  g_self->showMenu();
}

void SettingsApp::showSection(int idx) {
  lv_obj_clean(root_);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* back = lv_btn_create(root_);
  lv_obj_t* backLbl = lv_label_create(back);
  lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Settings");
  lv_obj_add_event_cb(back, backClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* body = lv_obj_create(root_);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(body, 12, 0);

  switch (idx) {
    case 0:
      buildDisplaySection(body, *store_, *display_);
      break;
    default:
      break;
  }
}
```

- [ ] **Step 4: Inject dependencies from main.cpp**

Modify `src/main.cpp`: in `setup()`, immediately **before** the F2 line that registers the settings app (`launcher.registerApp(&settingsApp);`), add:

```cpp
settingsApp.setDeps(settings, displayService, storage);
```

- [ ] **Step 5: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 6: Commit**

```bash
git add src/apps/settings/Sections.h src/apps/settings/DisplaySection.cpp \
        src/apps/settings/SettingsApp.h src/apps/settings/SettingsApp.cpp src/main.cpp
git commit -m "feat: settings shell navigation + Display section (brightness, sleep timeout)"
```

---

### Task 8: UnitsSection (°C/°F)

**Files:**
- Create: `src/apps/settings/UnitsSection.cpp`
- Modify: `src/apps/settings/SettingsApp.cpp` (add one menu entry + one switch case)

**Interfaces:**
- Consumes: `ISettingsStore` (Task 1); `buildUnitsSection` declaration already in `Sections.h` (Task 7); the `kSectionNames`/`showSection` extension points (Task 7).
- Produces: `void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store);` — persists `units.f` (bool, default false = °C). A3's Weather app reads `units.f` through `ISettingsStore`.

- [ ] **Step 1: Implement UnitsSection**

Create `src/apps/settings/UnitsSection.cpp`:

```cpp
#include "Sections.h"

namespace {
ISettingsStore* g_store = nullptr;  // valid while the section is on screen

void unitsChanged(lv_event_t* e) {
  lv_obj_t* sw = lv_event_get_target(e);
  g_store->setBool("units.f", lv_obj_has_state(sw, LV_STATE_CHECKED));
}
}  // namespace

void buildUnitsSection(lv_obj_t* parent, ISettingsStore& store) {
  g_store = &store;

  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, "Temperature unit\noff = Celsius, on = Fahrenheit");

  lv_obj_t* sw = lv_switch_create(parent);
  if (store.getBool("units.f", false)) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw, unitsChanged, LV_EVENT_VALUE_CHANGED, nullptr);
}
```

- [ ] **Step 2: Add the menu entry**

Modify `src/apps/settings/SettingsApp.cpp`:

The section-name array becomes:

```cpp
const char* kSectionNames[] = {"Display", "Units"};
```

And the `switch (idx)` in `showSection` gains:

```cpp
    case 1:
      buildUnitsSection(body, *store_);
      break;
```

- [ ] **Step 3: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/apps/settings/UnitsSection.cpp src/apps/settings/SettingsApp.cpp
git commit -m "feat: settings Units section — Celsius/Fahrenheit switch (units.f)"
```

---

### Task 9: AboutSection (version, free heap, SD status)

**Files:**
- Create: `src/apps/settings/AboutSection.cpp`
- Modify: `src/apps/settings/SettingsApp.cpp` (add one menu entry + one switch case)

**Interfaces:**
- Consumes: `DANIOS_VERSION` from `src/core/Version.h` (Task 6); `StorageService::mounted()` (Task 4); `esp_get_free_heap_size()` from `esp_system.h`; the Task 7 extension points.
- Produces: `void buildAboutSection(lv_obj_t* parent, StorageService& storage);`. Per roadmap deviation §5.1, **no battery %** is shown (spec §5 "About — battery %" is waived on this board — there is no battery-sense ADC).

- [ ] **Step 1: Implement AboutSection**

Create `src/apps/settings/AboutSection.cpp`:

```cpp
#include <esp_system.h>

#include <cstdio>

#include "../../core/Version.h"
#include "../../services/StorageService.h"
#include "Sections.h"

void buildAboutSection(lv_obj_t* parent, StorageService& storage) {
  char buf[128];
  snprintf(buf, sizeof(buf),
           "danios %s\n\n"
           "Free heap: %u bytes\n"
           "SD card: %s",
           DANIOS_VERSION, static_cast<unsigned>(esp_get_free_heap_size()),
           storage.mounted() ? "mounted" : "not found");
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, buf);
}
```

- [ ] **Step 2: Add the menu entry**

Modify `src/apps/settings/SettingsApp.cpp`:

The section-name array becomes:

```cpp
const char* kSectionNames[] = {"Display", "Units", "About"};
```

And the `switch (idx)` in `showSection` gains:

```cpp
    case 2:
      buildAboutSection(body, *storage_);
      break;
```

- [ ] **Step 3: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Verify native tests still pass**

Run: `pio test -e native`
Expected: all suites PASS (`17 test cases` across `test_settings_store` + `test_fs_names`, plus any F1/F2 suites).

- [ ] **Step 5: Commit**

```bash
git add src/apps/settings/AboutSection.cpp src/apps/settings/SettingsApp.cpp
git commit -m "feat: settings About section — version, free heap, SD status"
```

---

### Task 10: Screen sleep (spec §6.4)

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `lv_disp_get_inactive_time(NULL)` (LVGL v8), F1's `DisplayService::setBrightness(uint8_t)`, the `settings` global (Task 6), keys `disp.sleep_s` / `disp.bright`.
- Produces: screen-sleep behavior in the main loop. No new public interface — later plans need nothing from this task.

**How the waking tap is swallowed:** when the timeout elapses, before turning the backlight off we create a full-screen, transparent, clickable `lv_obj` ("sleep shield") on `lv_layer_top()`. LVGL routes the next press to the shield instead of whatever widget sits underneath. On `LV_EVENT_PRESSED` the shield restores brightness (screen lights up while the finger is still down); on `LV_EVENT_RELEASED` it deletes itself via `lv_obj_del_async` (safe inside its own event handler). The entire waking gesture — press and release — lands on the shield and never reaches the UI, so waking cannot accidentally launch an app or press a button. A touch also resets LVGL's inactivity clock automatically, so no extra bookkeeping is needed.

The sleep check is throttled to every 500 ms and reads `disp.sleep_s` from NVS on each check (NVS reads are RAM-cached by the IDF — cheap), so a timeout changed in Settings applies immediately, no reboot.

- [ ] **Step 1: Add the sleep machinery to main.cpp**

Modify `src/main.cpp`. Add above `setup()` (after the `settings`/`storage` globals from Task 6):

```cpp
// --- Screen sleep (spec 6.4): backlight off after disp.sleep_s of inactivity;
// --- any touch wakes; the waking tap is swallowed by a full-screen shield.
static lv_obj_t* g_sleepShield = nullptr;
static bool g_screenAsleep = false;

static void sleepShieldEvent(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    // Light up while the finger is still down.
    displayService.setBrightness(
        static_cast<uint8_t>(settings.getU32("disp.bright", 160)));
  } else if (code == LV_EVENT_RELEASED) {
    lv_obj_del_async(g_sleepShield);  // safe self-delete from own handler
    g_sleepShield = nullptr;
    g_screenAsleep = false;
  }
}

static void sleepTick() {
  static uint32_t lastCheck = 0;
  const uint32_t now = millis();
  if (now - lastCheck < 500) return;  // throttle: check twice a second
  lastCheck = now;

  if (g_screenAsleep) return;
  const uint32_t timeoutS = settings.getU32("disp.sleep_s", 60);
  if (timeoutS == 0) return;  // 0 = never sleep

  if (lv_disp_get_inactive_time(NULL) >= timeoutS * 1000u) {
    g_sleepShield = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_sleepShield);  // fully transparent
    lv_obj_set_size(g_sleepShield, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(g_sleepShield, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_sleepShield, sleepShieldEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_sleepShield, sleepShieldEvent, LV_EVENT_RELEASED, nullptr);
    displayService.setBrightness(0);  // backlight off — display asleep
    g_screenAsleep = true;
  }
}
```

- [ ] **Step 2: Call it from the main loop**

In `loop()`, after the existing F1/F2 calls (`lv_timer_handler()` and `launcher.tick(millis())`), add:

```cpp
  sleepTick();
```

- [ ] **Step 3: Verify device build stays green**

Run: `pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: screen sleep — backlight off on inactivity, tap-to-wake with swallowed tap"
```

---

### Task 11: On-device verification (needs the CYD + a FAT32 microSD)

**Files:** none (manual verification).

**Interfaces:**
- Consumes: everything F3 built. This is the roadmap §1 F3 E2E gate: *"Change brightness, reboot, it persists; icons load from SD; SD-missing boot error."*

**Hardware needed:** the ESP32-2432S024C (CYD) — **not** the bare devkit (`docs/hardware.md`: no display/touch/SD on the devkit) — plus a microSD card, 8–32 GB, formatted **FAT32**.

- [ ] **Step 1: Prepare the SD card**

1. Format the card FAT32.
2. Create the roadmap §4.1 directories: `/music`, `/oracle`, `/art/weather`, `/art/icons`, `/art/oracle`, `/art/pet`.
3. Produce at least two app icons with the LVGL online image converter (https://lvgl.io/tools/imageconverter): select **LVGL v8**, upload a small PNG (~64×64), Color format **CF_TRUE_COLOR**, output **Binary RGB565** — *if `LV_COLOR_16_SWAP` is `1` in `include/lv_conf.h`, pick **Binary RGB565 Swap** instead*. Name them after app ids: `calc.bin` and `settings.bin` (any two ids work), and copy them to `/art/icons/`. Deliberately leave the other ids missing to prove fallback.

- [ ] **Step 2: Flash and boot WITH the SD card**

Run: `pio run -e cyd -t upload && pio device monitor`

Verify:
- No SD-error message appears.
- Launcher tiles for `calc` and `settings` show the converted icons loaded from `S:/art/icons/*.bin`; the other tiles show the F2 fallback (colored box / symbol) — no crash, no blank tile.
- All apps are tappable (none greyed out).

- [ ] **Step 3: Verify brightness persistence**

1. Launcher → gear → Settings → Display.
2. Drag the brightness slider — backlight visibly changes **while dragging**.
3. Set it near minimum (slider stops at 10 — screen stays faintly visible, never black), release.
4. Power-cycle the device (unplug/replug USB).
5. Expected: the screen comes up at the dim level; Settings → Display shows the slider at the saved position.
6. Restore a comfortable brightness.

- [ ] **Step 4: Verify screen sleep + tap-to-wake**

1. Settings → Display → Screen sleep → **30 seconds**.
2. Go back to the launcher (so a real app screen is behind the shield) and don't touch the screen for 30 s.
3. Expected: backlight turns fully off within ~30.5 s.
4. Tap the middle of the screen once — directly over an app icon.
5. Expected: the screen lights up while your finger is down, and **the app under your finger does not open** (the tap was swallowed by the shield).
6. Set Screen sleep → **Never**; wait > 1 min; expected: screen never sleeps.

- [ ] **Step 5: Verify Units + About**

1. Settings → Units → toggle the switch on (°F). Power-cycle. Expected: switch comes back **on**.
2. Toggle it back off (°C default).
3. Settings → About. Expected: `danios 0.3.0`, a plausible free-heap figure (roughly 100–250 KB at idle), and `SD card: mounted`. No battery % anywhere (roadmap deviation §5.1).

- [ ] **Step 6: Boot WITHOUT the SD card**

1. Power off, eject the microSD, power on.
2. Expected, in order:
   - The friendly "No memory card" message appears over the launcher, near-full-width, with an OK button.
   - Tapping OK dismisses it, revealing the launcher.
   - Weather, Music, and Oracle tiles are greyed out; tapping one shows F2's disabled-app hint msgbox instead of opening.
   - **Pet is NOT greyed out** (state in NVS, placeholder art — spec §6.5 exception); Calculator and Settings open normally.
   - All launcher icons show fallback tiles (no `S:` drive), without errors.
   - Settings → About shows `SD card: not found`.
3. Power off, reinsert the card, power on — everything from Step 2 works again.

- [ ] **Step 7: Commit any fixes found**

If verification exposed bugs, fix them and commit each fix separately:

```bash
git add -A
git commit -m "fix: <specific issue found during F3 on-device verification>"
```

---

## Definition of done

- [ ] `pio test -e native` — green: `test_settings_store` (9 cases) and `test_fs_names` (8 cases) pass alongside all pre-existing F1/F2 suites.
- [ ] `pio run -e cyd` — device build succeeds.
- [ ] Roadmap §1 F3 E2E outcome observed on the CYD: change brightness → reboot → it persists; app icons load from `S:/art/icons/*.bin`; booting without the SD card shows the friendly error and greys out Weather/Music/Oracle (Pet, Calculator, Settings unaffected).
- [ ] Screen sleeps after the configured timeout, a tap wakes it, and the waking tap does not activate the UI underneath.
- [ ] All F3 NVS access goes through `ISettingsStore` with the pinned keys/defaults (`disp.bright` 160, `disp.sleep_s` 60, `units.f` false) in namespace `"danios"` — no stray `Preferences` usage outside `SettingsService`.
- [ ] No interface from roadmap §4 was renamed or re-declared; new F3 surface is limited to: `lib/fs_names/` (`FsEntry`, `hasExtension`, `isHiddenName`, `filterAndSortNames`), `src/services/LvglFs.h` (`lvglFsRegisterSd`, `lvglFsExists`), `src/apps/settings/Sections.h` (three `build*Section` functions), `SettingsApp::setDeps`, and `src/core/Version.h` (`DANIOS_VERSION`).
