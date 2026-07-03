# Foundation 5 — Bluetooth Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pair a Bluetooth speaker from Settings and hear a test tone through it — completing the WiFi-XOR-Bluetooth radio switch so the Music app (A4) only has to plug a decoder into `BluetoothAudioService::setSource`.

**Architecture:** Task 0 is a **de-risk spike** (roadmap deviation §5.2): prove ESP32-A2DP works as an A2DP *source* on our arduino-esp32 3.x platform before building anything on it; a documented fallback pins the platform back to core 2.0.x. Then a pure `lib/bt_addr/` module (address string ↔ bytes) is TDD'd natively, `BluetoothAudioService` wraps `BluetoothA2DPSource` (GAP discovery scan, connect to a stored device, a frame-callback bridge to the roadmap's `AudioSourceFn` contract, NVS pairing persistence), the F4 stubs in `RadioManager` become real BT power hooks, and Settings gains a Bluetooth section with scan/pick/connect/forget plus a "Play test tone" row — the plan's E2E outcome.

**Tech Stack:** ESP32-A2DP v1.8.11 (pschatzmann, A2DP source), ESP-IDF GAP (`esp_bt_gap_*`) for discovery, LVGL 8.4, Unity native tests, ISettingsStore (F3).

**Prerequisites:** F1–F4 merged (`2026-07-03-foundation-1..4-*.md`). This plan replaces the `// F5` stub bodies F4 left in `src/services/RadioManager.{h,cpp}` and extends F3/F4's `SettingsApp::setDeps` / `kSectionNames` / `showSection` exactly as F4 did.

**Board note:** every task except the Settings-UI half of verification runs on the **bare ESP32 devkit** (it has BT Classic — see `docs/hardware.md`); the CYD is needed only for touch-driven verification.

## Global Constraints

(Copied from the roadmap — `2026-07-03-danios-roadmap.md` §2. That document's §4 interfaces are authoritative for every name below.)

- **Board:** ESP32-2432S024C (CYD 2.4" capacitive), ESP32-WROOM-32, **no PSRAM**.
  520 KB SRAM total; budget carefully (LVGL buffers ~29 KB, LVGL heap 48 KB,
  WiFi ~50 KB or BT Classic ~64 KB — **never both**, MP3 decode ~30 KB).
- **Platform:** PlatformIO, `platform = espressif32@7.0.1`, `board = esp32dev`,
  `framework = arduino` (arduino-esp32 3.x). Partition scheme:
  `board_build.partitions = huge_app.csv` (no OTA — spec non-goal).
  **This plan's Task 0 validates A2DP on this platform or triggers the
  documented core-2.0.x fallback (roadmap deviation §5.2).**
- **Display:** landscape-native 320×240 clone via `include/LGFX_ESP32_2432S024C.hpp`
  — do not change panel/memory dims or `offset_rotation`. UI is portrait 240×320,
  `setRotation(7)`. See `docs/DISPLAY.md`.
- **Pins:** display HSPI (SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, RST −1, BL 27);
  touch CST820 I²C (SDA 33, SCL 32, RST 25, addr 0x15, poll); SD on VSPI (CS 5).
- **No battery-voltage ADC on this board** (roadmap deviation §5.1).
- **LVGL:** `lvgl/lvgl@8.4.0` (v8 API), `lv_conf.h` in `include/`, two 240×30
  draw buffers, UI on the Arduino loop task only.
- **C++17** on both envs.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager` —
  this plan completes its Bluetooth arm. No app touches `esp_bt_*` power state.
- **TDD, native-first:** pure logic in `lib/<module>/` (std C++ only), tested
  with `pio test -e native` (Unity).
- **Commits:** small, frequent, conventional. Repo git-initialized in F1.
- **SD layout & NVS keys:** exactly as roadmap §4.1/§4.2. This plan owns
  `bt.addr` (str, `AA:BB:CC:DD:EE:FF`) and `bt.name` (str).

## File map

| File | Task | Responsibility |
| --- | --- | --- |
| `src/spike/a2dp_tone.cpp` (create) | 0 | Throwaway A2DP-source sine spike |
| `platformio.ini` (modify) | 0 | `[env:spike-a2dp]`; exclude `spike/` from `cyd`; A2DP lib dep |
| `docs/hardware.md` (modify) | 0 | Record the spike outcome (platform verified / fallback taken) |
| `lib/bt_addr/bt_addr.h` + `.cpp` (create) | 1 | Address string ↔ `uint8_t[6]` |
| `test/test_bt_addr/test_main.cpp` (create) | 1 | Native tests |
| `src/services/BluetoothAudioService.h` + `.cpp` (create) | 2 | Roadmap §4.10 verbatim + `powerOn`/`powerOff` for RadioManager |
| `src/services/RadioManager.{h,cpp}` (modify) | 3 | Replace the F4 `// F5` stubs with real BT hooks |
| `src/main.cpp` (modify) | 2, 3, 4 | Service global, BLE mem release, `setDeps` |
| `src/apps/settings/BluetoothSection.cpp` (create) | 4 | Settings → Bluetooth + test tone |
| `src/apps/settings/Sections.h`, `SettingsApp.{h,cpp}` (modify) | 4 | Section wiring |

---

### Task 0: De-risk spike — A2DP source tone on core 3.x

**Files:**
- Create: `src/spike/a2dp_tone.cpp`
- Modify: `platformio.ini`
- Modify: `docs/hardware.md` (record the outcome)

**Interfaces:**
- Consumes: nothing from the app tree (self-contained sketch).
- Produces: a **go/no-go decision** on `espressif32@7.0.1` for A2DP source, and
  the `ESP32-A2DP` dependency pinned in `platformio.ini`. Every later task
  assumes this task's outcome is recorded in `docs/hardware.md`.

- [ ] **Step 1: Add the spike env and the A2DP dependency**

Modify `platformio.ini`. In `[env:cyd]`, add the exclusion and the new dep:

```ini
build_src_filter = +<*> -<spike/>
lib_deps =
    ; ... existing F1–F4 deps unchanged ...
    https://github.com/pschatzmann/ESP32-A2DP.git#v1.8.11
```

Append the spike env:

```ini
[env:spike-a2dp]
extends = env:cyd
build_src_filter = +<spike/a2dp_tone.cpp>
```

- [ ] **Step 2: Write the spike sketch**

Create `src/spike/a2dp_tone.cpp` (edit `kSpeakerName` to your speaker's
advertised name before flashing):

```cpp
// F5 Task 0 spike (throwaway): prove ESP32-A2DP works as an A2DP SOURCE on
// espressif32@7.0.1 / arduino-esp32 3.x (roadmap deviation 5.2). Plays a
// 440 Hz sine to a named speaker. If this does not build/run, take the
// fallback in Step 4 — do NOT proceed to Task 1 with an unproven stack.
#include <Arduino.h>
#include <BluetoothA2DPSource.h>

#include <cmath>

static const char* kSpeakerName = "MY-SPEAKER";  // <-- your speaker's name

static BluetoothA2DPSource a2dp;
static float phase = 0.0f;

static int32_t getFrames(Frame* frames, int32_t count) {
  constexpr float kStep = 2.0f * PI * 440.0f / 44100.0f;
  for (int32_t i = 0; i < count; ++i) {
    const int16_t s = static_cast<int16_t>(8000.0f * sinf(phase));
    frames[i].channel1 = s;
    frames[i].channel2 = s;
    phase += kStep;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
  }
  return count;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("[spike] A2DP source starting, heap=%u\n",
                esp_get_free_heap_size());
  a2dp.set_data_callback_in_frames(getFrames);
  a2dp.start(kSpeakerName);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 2000) {
    last = millis();
    Serial.printf("[spike] connected=%d heap=%u\n", a2dp.is_connected(),
                  esp_get_free_heap_size());
  }
}
```

- [ ] **Step 3: Build, flash, listen**

Run: `cd /home/lucca/repos/danios && pio run -e spike-a2dp -t upload && pio device monitor`
Expected: build `SUCCESS`; serial shows `connected=1` within ~10–20 s of the
speaker being in pairing mode; a steady 440 Hz tone is audible. Note the
`heap=` figure while connected (expect roughly 90–150 KB free) — it feeds the
Music app's RAM budget.

If the **build fails** on API/IDF incompatibilities, first try adding
`-DA2DP_I2S_AUDIOTOOLS=1` to the spike env's `build_flags` (per
`docs/hardware.md`) and rebuild.

- [ ] **Step 4 (only if Step 3 still fails): the documented fallback**

1. In `platformio.ini`, change `platform = espressif32@7.0.1` →
   `platform = espressif32@6.9.0` (arduino-esp32 core 2.0.x — the combo
   `docs/hardware.md` calls verified for A2DP).
2. Run `pio run -e cyd -t upload` and re-verify the F1 smoke screen +
   F1 Task 6's four-corner touch test on the CYD (LovyanGFX supports both
   cores; the display config itself is core-independent, but **verify by
   eye**, don't assume).
3. Re-run Step 3.

- [ ] **Step 5: Record the outcome and commit**

Modify `docs/hardware.md`: under the "PlatformIO" heading, add one line —
either `A2DP source verified on espressif32@7.0.1 (F5 spike, <date>)` or
`Platform pinned to espressif32@6.9.0 for A2DP (F5 spike, <date>); display re-verified`.

```bash
cd /home/lucca/repos/danios && git add platformio.ini src/spike docs/hardware.md && \
  git commit -m "feat: A2DP source spike passes (F5 task 0); pin ESP32-A2DP v1.8.11"
```

---

### Task 1: `lib/bt_addr/` — address parsing (native TDD)

**Files:**
- Create: `lib/bt_addr/bt_addr.h`
- Create: `lib/bt_addr/bt_addr.cpp`
- Test: `test/test_bt_addr/test_main.cpp`

**Interfaces:**
- Consumes: nothing (std C++ only).
- Produces: `bool parseBtAddr(const std::string& s, uint8_t out[6]);` and
  `std::string formatBtAddr(const uint8_t addr[6]);` (uppercase,
  `AA:BB:CC:DD:EE:FF` — the NVS `bt.addr` wire format).

- [ ] **Step 1: Write the failing tests**

Create `test/test_bt_addr/test_main.cpp`:

```cpp
#include <unity.h>

#include <bt_addr/bt_addr.h>

void setUp() {}
void tearDown() {}

void test_parse_valid_uppercase() {
  uint8_t b[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr("A1:B2:C3:D4:E5:F6", b));
  const uint8_t want[6] = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(want, b, 6);
}

void test_parse_accepts_lowercase() {
  uint8_t b[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr("a1:b2:c3:d4:e5:f6", b));
  TEST_ASSERT_EQUAL_UINT8(0xA1, b[0]);
  TEST_ASSERT_EQUAL_UINT8(0xF6, b[5]);
}

void test_parse_rejects_bad_input() {
  uint8_t b[6] = {};
  TEST_ASSERT_FALSE(parseBtAddr("", b));
  TEST_ASSERT_FALSE(parseBtAddr("A1:B2:C3:D4:E5", b));        // too short
  TEST_ASSERT_FALSE(parseBtAddr("A1:B2:C3:D4:E5:F6:07", b));  // too long
  TEST_ASSERT_FALSE(parseBtAddr("A1-B2-C3-D4-E5-F6", b));     // wrong sep
  TEST_ASSERT_FALSE(parseBtAddr("G1:B2:C3:D4:E5:F6", b));     // bad hex
}

void test_format_roundtrip() {
  const uint8_t in[6] = {0x00, 0x1A, 0xFF, 0x0B, 0x9C, 0x5D};
  TEST_ASSERT_EQUAL_STRING("00:1A:FF:0B:9C:5D", formatBtAddr(in).c_str());
  uint8_t back[6] = {};
  TEST_ASSERT_TRUE(parseBtAddr(formatBtAddr(in), back));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(in, back, 6);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_valid_uppercase);
  RUN_TEST(test_parse_accepts_lowercase);
  RUN_TEST(test_parse_rejects_bad_input);
  RUN_TEST(test_format_roundtrip);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_bt_addr`
Expected: build FAILURE — `bt_addr/bt_addr.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `lib/bt_addr/bt_addr.h`:

```cpp
// danios bt_addr — Bluetooth address string <-> bytes. Wire format for the
// NVS "bt.addr" key is uppercase "AA:BB:CC:DD:EE:FF". Pure std C++17.
#pragma once
#include <cstdint>
#include <string>

bool parseBtAddr(const std::string& s, uint8_t out[6]);
std::string formatBtAddr(const uint8_t addr[6]);
```

Create `lib/bt_addr/bt_addr.cpp`:

```cpp
#include "bt_addr.h"

#include <cstdio>

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
}  // namespace

bool parseBtAddr(const std::string& s, uint8_t out[6]) {
  if (s.size() != 17) return false;
  for (int i = 0; i < 6; ++i) {
    const int base = i * 3;
    if (i > 0 && s[base - 1] != ':') return false;
    const int hi = hexVal(s[base]);
    const int lo = hexVal(s[base + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

std::string formatBtAddr(const uint8_t addr[6]) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
           addr[2], addr[3], addr[4], addr[5]);
  return buf;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd /home/lucca/repos/danios && pio test -e native -f test_bt_addr`
Expected: `4 Tests 0 Failures 0 Ignored` — PASS.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add lib/bt_addr test/test_bt_addr && \
  git commit -m "feat: add bt_addr string<->bytes helpers (native TDD)"
```

---

### Task 2: `BluetoothAudioService`

**Files:**
- Create: `src/services/BluetoothAudioService.h`
- Create: `src/services/BluetoothAudioService.cpp`
- Modify: `src/main.cpp` (global)

**Interfaces:**
- Consumes: `bt_addr` (Task 1), `ISettingsStore` (F3) for `bt.addr`/`bt.name`,
  `BluetoothA2DPSource` (Task 0's pinned lib), ESP-IDF GAP for discovery.
- Produces (roadmap §4.10 verbatim): `struct BtDevice { std::string name; std::string addr; };`,
  `using AudioSourceFn = int32_t (*)(int16_t* stereo_buf, int32_t frames, void* ctx);`
  (fill up to `frames` stereo int16 frames at 44100 Hz, return frames written,
  0 = silence), and `BluetoothAudioService` with `scan(ms = 8000)`,
  `connect(addr)`, `disconnect()`, `isConnected()`, `setSource(fn, ctx)`
  (`setSource(nullptr, nullptr)` reverts to silence), `pairedAddr()`,
  `savePaired(d)`, `forgetPaired()`.
  **This plan's additions** (RadioManager-only, Task 3 consumes them):
  `bool powerOn();` / `void powerOff();`.

- [ ] **Step 1: Write the header**

Create `src/services/BluetoothAudioService.h`:

```cpp
// danios BluetoothAudioService — A2DP source (spec §3.1): discover speakers,
// connect to the paired one, and stream whatever AudioSourceFn provides.
// Power state belongs to RadioManager (powerOn/powerOff are for it alone).
#pragma once
#include <settings_store/settings_store.h>

#include <cstdint>
#include <string>
#include <vector>

struct BtDevice {
  std::string name;
  std::string addr;  // "AA:BB:CC:DD:EE:FF" (bt_addr wire format)
};

// Audio callback contract (roadmap §4.10): fill up to `frames` stereo int16
// frames at 44100 Hz, return frames actually written. Return 0 = silence.
using AudioSourceFn = int32_t (*)(int16_t* stereo_buf, int32_t frames,
                                  void* ctx);

class BluetoothAudioService {
 public:
  explicit BluetoothAudioService(ISettingsStore& store) : store_(store) {}

  // --- RadioManager-only power hooks (Task 3). Apps: use RadioManager. ---
  bool powerOn();   // controller + bluedroid up (idempotent)
  void powerOff();  // A2DP end + controller stop. NEVER releases BT Classic
                    // memory (esp_bt_controller_mem_release is one-way and
                    // Music re-enters Bluetooth every session).

  std::vector<BtDevice> scan(uint32_t ms = 8000);  // blocking GAP discovery
  bool connect(const std::string& addr);           // A2DP source connect
  void disconnect();
  bool isConnected() const;

  void setSource(AudioSourceFn fn, void* ctx);  // Music plugs its decoder here

  std::string pairedAddr() const;  // NVS bt.addr ("" if none)
  void savePaired(const BtDevice& d);
  void forgetPaired();

 private:
  ISettingsStore& store_;
};
```

- [ ] **Step 2: Write the implementation**

Create `src/services/BluetoothAudioService.cpp`. The GAP/A2DP glue is the one
place in F5 where the spike (Task 0) may force small API-name adjustments —
the structure below is for ESP32-A2DP v1.8.11 and IDF 5.x GAP:

```cpp
#include "BluetoothAudioService.h"

#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include <bt_addr/bt_addr.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>

#include <cstring>

namespace {
BluetoothA2DPSource a2dp;

// Trampoline state (single service instance; BT callbacks are C-style).
AudioSourceFn g_sourceFn = nullptr;
void* g_sourceCtx = nullptr;
std::vector<BtDevice>* g_scanOut = nullptr;
uint8_t g_targetAddr[6] = {};
bool g_haveTarget = false;

int32_t frameBridge(Frame* frames, int32_t count) {
  // Frame is {int16_t channel1, channel2} — memory-compatible with the
  // interleaved stereo buffer AudioSourceFn expects.
  int16_t* buf = reinterpret_cast<int16_t*>(frames);
  int32_t written = 0;
  if (g_sourceFn != nullptr) written = g_sourceFn(buf, count, g_sourceCtx);
  if (written < count) {
    memset(buf + written * 2, 0, static_cast<size_t>(count - written) * 4);
  }
  return count;  // always feed a full buffer; the tail is silence
}

void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (event != ESP_BT_GAP_DISC_RES_EVT || g_scanOut == nullptr) return;

  std::string name;
  for (int i = 0; i < param->disc_res.num_prop; ++i) {
    esp_bt_gap_dev_prop_t& p = param->disc_res.prop[i];
    if (p.type == ESP_BT_GAP_DEV_PROP_BDNAME) {
      name.assign(static_cast<char*>(p.val), p.len);
    } else if (p.type == ESP_BT_GAP_DEV_PROP_EIR) {
      uint8_t len = 0;
      uint8_t* eirName = esp_bt_gap_resolve_eir_data(
          static_cast<uint8_t*>(p.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
      if (eirName != nullptr && len > 0) {
        name.assign(reinterpret_cast<char*>(eirName), len);
      }
    }
  }
  if (name.empty()) return;  // nameless results are unpickable in the UI

  const std::string addr = formatBtAddr(param->disc_res.bda);
  for (auto& d : *g_scanOut) {
    if (d.addr == addr) return;  // dedupe repeated inquiry responses
  }
  g_scanOut->push_back({name, addr});
}

// A2DP-source device filter: accept only the device we were asked to connect.
bool ssidFilter(const char* /*ssid*/, esp_bd_addr_t address, int /*rssi*/) {
  return g_haveTarget && memcmp(address, g_targetAddr, 6) == 0;
}
}  // namespace

bool BluetoothAudioService::powerOn() {
  if (!btStarted() && !btStart()) return false;
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    if (esp_bluedroid_init() != ESP_OK) return false;
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    if (esp_bluedroid_enable() != ESP_OK) return false;
  }
  return true;
}

void BluetoothAudioService::powerOff() {
  g_sourceFn = nullptr;
  a2dp.end(false);  // false: keep controller memory — BT must restart later
  if (btStarted()) btStop();
  Serial.printf("[bt] off, heap=%u\n", esp_get_free_heap_size());
}

std::vector<BtDevice> BluetoothAudioService::scan(uint32_t ms) {
  std::vector<BtDevice> out;
  g_scanOut = &out;
  esp_bt_gap_register_callback(gapCallback);
  // Inquiry length is in 1.28 s units, clamped to GAP's 0x01..0x30 range.
  uint8_t len = static_cast<uint8_t>(ms / 1280);
  if (len < 1) len = 1;
  if (len > 0x30) len = 0x30;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, len, 0);
  delay(ms);
  esp_bt_gap_cancel_discovery();
  g_scanOut = nullptr;
  Serial.printf("[bt] scan found %u device(s)\n",
                static_cast<unsigned>(out.size()));
  return out;
}

bool BluetoothAudioService::connect(const std::string& addr) {
  if (!parseBtAddr(addr, g_targetAddr)) return false;
  g_haveTarget = true;

  a2dp.set_data_callback_in_frames(frameBridge);
  a2dp.set_ssid_callback(ssidFilter);
  a2dp.set_auto_reconnect(false);  // RadioManager owns when BT lives; no
                                   // background reconnect attempts
  // The lib may retain the name pointer — keep the backing string alive.
  static std::string liveName;
  liveName = store_.getString("bt.name", "");
  a2dp.start(liveName.c_str());

  const uint32_t start = millis();
  while (!a2dp.is_connected() && millis() - start < 15000) delay(100);
  Serial.printf("[bt] connect %s: %s\n", addr.c_str(),
                a2dp.is_connected() ? "ok" : "FAILED");
  return a2dp.is_connected();
}

void BluetoothAudioService::disconnect() {
  g_haveTarget = false;
  a2dp.disconnect();
}

bool BluetoothAudioService::isConnected() const { return a2dp.is_connected(); }

void BluetoothAudioService::setSource(AudioSourceFn fn, void* ctx) {
  g_sourceCtx = ctx;
  g_sourceFn = fn;
}

std::string BluetoothAudioService::pairedAddr() const {
  return store_.getString("bt.addr", "");
}

void BluetoothAudioService::savePaired(const BtDevice& d) {
  store_.setString("bt.addr", d.addr);
  store_.setString("bt.name", d.name);
}

void BluetoothAudioService::forgetPaired() {
  store_.remove("bt.addr");
  store_.remove("bt.name");
}
```

- [ ] **Step 3: Add the global to main.cpp**

Modify `src/main.cpp` — after `static TimeService timeService(...);`:

```cpp
#include "services/BluetoothAudioService.h"

static BluetoothAudioService btAudio(settings);
```

- [ ] **Step 4: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`. If a v1.8.11 method name differs from the spike's working
code (e.g. `set_ssid_callback` signature), align this file with what the spike
proved — the spike is the ground truth for lib API names.

- [ ] **Step 5: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/BluetoothAudioService.h \
  src/services/BluetoothAudioService.cpp src/main.cpp && \
  git commit -m "feat: add BluetoothAudioService (A2DP source, GAP scan, NVS pairing)"
```

---

### Task 3: Complete `RadioManager`'s Bluetooth arm

**Files:**
- Modify: `src/services/RadioManager.h`
- Modify: `src/services/RadioManager.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `BluetoothAudioService::powerOn/powerOff` (Task 2).
- Produces: the roadmap §4.6 contract now holds in full —
  `request(RadioMode::Bluetooth)` really brings BT up (tearing WiFi down
  first), plus `void setBluetoothService(BluetoothAudioService* bt);` wired
  once from `main.cpp`.

**RAM note (spell this out in code review too):** we never call
`esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)` — it is one-way, and
Music re-enters Bluetooth every session. We *do* release the **BLE** half once
at boot (`ESP_BT_MODE_BLE`, ~30 KB back permanently) because danios never uses
BLE. That call must happen before the controller is ever initialized.

- [ ] **Step 1: Replace the F4 stubs**

Modify `src/services/RadioManager.h` — add above `private:`:

```cpp
  // F5: wire once from main.cpp. RadioManager drives BT power through this.
  void setBluetoothService(BluetoothAudioService* bt) { bt_ = bt; }
```

with the forward declaration at the top (`class BluetoothAudioService;`) and a
member `BluetoothAudioService* bt_ = nullptr;`.

Modify `src/services/RadioManager.cpp` — replace the two `// F5` stub bodies:

```cpp
#include "BluetoothAudioService.h"

bool RadioManager::startBt() { return bt_ != nullptr && bt_->powerOn(); }

void RadioManager::stopBt() {
  if (bt_ != nullptr) bt_->powerOff();
}
```

- [ ] **Step 2: Wire it + release BLE memory at boot**

Modify `src/main.cpp` — first lines of `setup()` (before any radio use):

```cpp
  // danios never uses BLE: release its controller memory once, permanently
  // (~30 KB back on a no-PSRAM board). Must precede any BT controller init.
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
```

(add `#include <esp_bt.h>` to main.cpp's includes) and after the
`launcher.setRadioRequest(...)` wiring from F4:

```cpp
  radioManager.setBluetoothService(&btAudio);
```

- [ ] **Step 3: Verify device build + the F4 stub contract flip**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

F4's Definition of done verified `request(Bluetooth)` == false; that contract
flips here. Temporarily add to the end of `setup()`:

```cpp
  Serial.printf("[f5-check] bt request -> %d (expect 1), heap=%u\n",
                radioManager.request(RadioMode::Bluetooth),
                esp_get_free_heap_size());
  radioManager.request(RadioMode::None);
```

Flash (devkit is fine), confirm `bt request -> 1`, then **delete the test
lines** and rebuild.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/services/RadioManager.h \
  src/services/RadioManager.cpp src/main.cpp && \
  git commit -m "feat: complete RadioManager Bluetooth arm; release BLE memory at boot"
```

---

### Task 4: Settings → Bluetooth section (+ test tone)

**Files:**
- Create: `src/apps/settings/BluetoothSection.cpp`
- Modify: `src/apps/settings/Sections.h`
- Modify: `src/apps/settings/SettingsApp.h` (extend `setDeps` + member)
- Modify: `src/apps/settings/SettingsApp.cpp` (`kSectionNames`, `showSection`)
- Modify: `src/main.cpp` (the `setDeps` call site)

**Interfaces:**
- Consumes: `RadioManager` (Task 3), `BluetoothAudioService` (Task 2), F3/F4's
  `Sections.h`/`setDeps`/`kSectionNames`/`showSection` extension points.
- Produces: `void buildBluetoothSection(lv_obj_t* parent, RadioManager& radio, BluetoothAudioService& bt);`
  and the final F-series `setDeps` signature:
  `setDeps(ISettingsStore&, DisplayService&, StorageService&, RadioManager&, WiFiService&, TimeService&, BluetoothAudioService&)`
  (A3 adds no deps — its section reuses `store_`).
- Same radio-while-open rule as F4's WiFi section: request Bluetooth on build,
  release to None via `LV_EVENT_DELETE` on the body.

- [ ] **Step 1: Extend Sections.h and SettingsApp**

Modify `src/apps/settings/Sections.h` — add:

```cpp
class BluetoothAudioService;  // F5

void buildBluetoothSection(lv_obj_t* parent, RadioManager& radio,
                           BluetoothAudioService& bt);
```

Modify `src/apps/settings/SettingsApp.h` — extend `setDeps` with a trailing
`BluetoothAudioService& bt` parameter and add `BluetoothAudioService* bt_ = nullptr;`
to the members. Modify `src/apps/settings/SettingsApp.cpp` accordingly
(`bt_ = &bt;` in `setDeps`), extend the array:

```cpp
const char* kSectionNames[] = {"Display", "Units", "About",
                               "WiFi",    "Clock", "Bluetooth"};
```

and the switch:

```cpp
    case 5:
      buildBluetoothSection(body, *radio_, *bt_);
      break;
```

Modify `src/main.cpp` — replace the F4 `setDeps` call:

```cpp
settingsApp.setDeps(settings, displayService, storage, radioManager,
                    wifiService, timeService, btAudio);
```

- [ ] **Step 2: Implement the section**

Create `src/apps/settings/BluetoothSection.cpp`:

```cpp
// Settings -> Bluetooth (spec §5): scan, pick, connect, forget a speaker.
// This is where the Music app redirects when nothing is paired (spec §4.2).
// Also hosts the F5 E2E gate: the "Play test tone" row.
#include <lvgl.h>

#include <cmath>

#include "../../core/App.h"  // RadioMode
#include "../../services/BluetoothAudioService.h"
#include "../../services/RadioManager.h"
#include "Sections.h"

namespace {
struct BtUi {
  RadioManager* radio;
  BluetoothAudioService* bt;
  lv_obj_t* body;
  lv_obj_t* status;
  lv_obj_t* list;
  lv_obj_t* pairedRow;   // "Paired: <name>" + Connect/Forget
  float tonePhase;
};
BtUi ui;

void setStatus(const char* msg) {
  lv_label_set_text(ui.status, msg);
  lv_refr_now(nullptr);  // repaint before blocking scan/connect
}

int32_t toneSource(int16_t* buf, int32_t frames, void* ctx) {
  float* phase = static_cast<float*>(ctx);
  constexpr float kStep = 2.0f * 3.14159265f * 440.0f / 44100.0f;
  for (int32_t i = 0; i < frames; ++i) {
    const int16_t s = static_cast<int16_t>(8000.0f * sinf(*phase));
    buf[i * 2] = s;
    buf[i * 2 + 1] = s;
    *phase += kStep;
    if (*phase > 6.2831853f) *phase -= 6.2831853f;
  }
  return frames;
}

void toneTimerDone(lv_timer_t* t) {
  ui.bt->setSource(nullptr, nullptr);  // back to silence
  lv_timer_del(t);
  setStatus("Tone done " LV_SYMBOL_OK);
}

void toneClicked(lv_event_t*) {
  if (!ui.bt->isConnected()) {
    setStatus("Connect a speaker first");
    return;
  }
  ui.tonePhase = 0.0f;
  ui.bt->setSource(toneSource, &ui.tonePhase);
  setStatus("Playing 440 Hz...");
  lv_timer_create(toneTimerDone, 2000, nullptr);
}

void rebuildPairedRow();

void connectTo(const BtDevice& d) {
  setStatus("Connecting...");
  if (ui.bt->connect(d.addr)) {
    // Reconnect passes a nameless BtDevice — don't clobber the stored bt.name.
    if (!d.name.empty()) ui.bt->savePaired(d);
    setStatus("Connected " LV_SYMBOL_OK);
  } else {
    setStatus("Could not connect");
  }
  rebuildPairedRow();
}

void deviceClicked(lv_event_t* e) {
  const BtDevice* d = static_cast<BtDevice*>(lv_event_get_user_data(e));
  connectTo(*d);
}

void scanClicked(lv_event_t*) {
  setStatus("Scanning ~8 s (speaker in pairing mode?)");
  lv_obj_clean(ui.list);
  auto found = ui.bt->scan();
  if (found.empty()) lv_list_add_text(ui.list, "Nothing found");
  for (auto& d : found) {
    lv_obj_t* btn = lv_list_add_btn(ui.list, LV_SYMBOL_BLUETOOTH, d.name.c_str());
    // Copy the device into button-owned memory (found dies with this scope).
    BtDevice* owned = new BtDevice(d);
    lv_obj_add_event_cb(btn, deviceClicked, LV_EVENT_CLICKED, owned);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* ev) {
          delete static_cast<BtDevice*>(lv_event_get_user_data(ev));
        },
        LV_EVENT_DELETE, owned);
  }
  setStatus("Tap a device to pair");
}

void reconnectClicked(lv_event_t*) {
  BtDevice d{"", ui.bt->pairedAddr()};
  connectTo(d);
}

void forgetClicked(lv_event_t*) {
  ui.bt->disconnect();
  ui.bt->forgetPaired();
  setStatus("Forgotten");
  rebuildPairedRow();
}

void rebuildPairedRow() {
  lv_obj_clean(ui.pairedRow);
  const std::string addr = ui.bt->pairedAddr();
  lv_obj_t* lbl = lv_label_create(ui.pairedRow);
  if (addr.empty()) {
    lv_label_set_text(lbl, "No speaker paired");
    return;
  }
  lv_label_set_text_fmt(lbl, "Paired: %s", addr.c_str());

  lv_obj_t* conn = lv_btn_create(ui.pairedRow);
  lv_label_set_text(lv_label_create(conn), "Connect");
  lv_obj_add_event_cb(conn, reconnectClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* forget = lv_btn_create(ui.pairedRow);
  lv_label_set_text(lv_label_create(forget), LV_SYMBOL_TRASH " Forget");
  lv_obj_add_event_cb(forget, forgetClicked, LV_EVENT_CLICKED, nullptr);
}

void bodyDeleted(lv_event_t*) {
  ui.bt->setSource(nullptr, nullptr);
  ui.radio->request(RadioMode::None);  // radio-while-open rule
}
}  // namespace

void buildBluetoothSection(lv_obj_t* parent, RadioManager& radio,
                           BluetoothAudioService& bt) {
  ui = {};
  ui.radio = &radio;
  ui.bt = &bt;
  ui.body = parent;

  ui.status = lv_label_create(parent);

  lv_obj_t* scanBtn = lv_btn_create(parent);
  lv_label_set_text(lv_label_create(scanBtn), LV_SYMBOL_REFRESH " Scan");
  lv_obj_add_event_cb(scanBtn, scanClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* toneBtn = lv_btn_create(parent);
  lv_label_set_text(lv_label_create(toneBtn), LV_SYMBOL_AUDIO " Play test tone");
  lv_obj_add_event_cb(toneBtn, toneClicked, LV_EVENT_CLICKED, nullptr);

  ui.pairedRow = lv_obj_create(parent);
  lv_obj_set_width(ui.pairedRow, LV_PCT(100));
  lv_obj_set_flex_flow(ui.pairedRow, LV_FLEX_FLOW_ROW_WRAP);

  ui.list = lv_list_create(parent);
  lv_obj_set_width(ui.list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.list, 1);

  rebuildPairedRow();
  lv_obj_add_event_cb(parent, bodyDeleted, LV_EVENT_DELETE, nullptr);

  if (radio.request(RadioMode::Bluetooth)) {
    setStatus("Tap Scan to find speakers");
  } else {
    setStatus("Bluetooth unavailable");
  }
}
```

- [ ] **Step 3: Verify device build**

Run: `cd /home/lucca/repos/danios && pio run -e cyd`
Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
cd /home/lucca/repos/danios && git add src/apps/settings/BluetoothSection.cpp \
  src/apps/settings/Sections.h src/apps/settings/SettingsApp.h \
  src/apps/settings/SettingsApp.cpp src/main.cpp && \
  git commit -m "feat: Settings -> Bluetooth section (scan, pair, forget, test tone)"
```

---

### Task 5: On-device verification — the XOR demo

**Files:** none (manual verification; delete `src/spike/` afterwards).

**Interfaces:**
- Consumes: everything F5 built. This is the roadmap §1 F5 E2E gate: *"Pair a
  speaker from Settings; hear a test tone through it."*

**Board note:** needs the **CYD** (touch UI) + a Bluetooth speaker; a WiFi
network must be saved (from F4) for the XOR half.

- [ ] **Step 1: Pair and hear the tone (the E2E gate)**

Flash `pio run -e cyd -t upload && pio device monitor`. Put the speaker in
pairing mode. Settings → Bluetooth → Scan → tap the speaker → "Connected ✓" →
"Play test tone". Expected: 2 s of 440 Hz through the speaker; serial shows
`[bt] connect AA:...: ok`.

- [ ] **Step 2: Persistence**

Reboot. Settings → Bluetooth shows "Paired: <addr>"; tap Connect (no re-scan).
Expected: connects and the tone plays again.

- [ ] **Step 3: WiFi-XOR-BT both directions, with heap logging**

With the speaker connected (glyph = BT), go home → Settings → WiFi → Scan →
join/verify your network. Expected serial order: `[bt] off, heap=...` then
`[radio] state=1 ...` — BT torn down before WiFi came up. Then back out and
re-enter Settings → Bluetooth → Connect. Expected: `[radio]` shows WiFi
stopped, then `[bt] connect ...: ok`. Record the three `heap=` figures
(idle / WiFi on / BT connected) in `docs/hardware.md` under the F5 spike note
— they are the Music app's (A4) RAM budget inputs. Rough expectation:
idle ≥ 200 KB, WiFi on ~150–200 KB, BT connected ~90–150 KB.

- [ ] **Step 4: Delete the spike and commit**

```bash
cd /home/lucca/repos/danios && git rm -r src/spike && \
  sed -i '/\[env:spike-a2dp\]/,+2d' platformio.ini && \
  pio run -e cyd && \
  git add platformio.ini docs/hardware.md && \
  git commit -m "chore: remove F5 A2DP spike; record radio heap figures"
```
Expected: `pio run -e cyd` still `SUCCESS` after removal.

---

## Definition of done (roadmap §6)

- [ ] `pio test -e native` — green, including the 4 bt_addr tests added here.
- [ ] `pio run -e cyd` — `SUCCESS` (spike env removed).
- [ ] Roadmap §1 F5 E2E outcome observed on the CYD: paired a speaker from
      Settings and heard the test tone through it.
- [ ] WiFi-XOR-BT demonstrated both directions on hardware (Task 5 Step 3),
      heap figures recorded in `docs/hardware.md`.
- [ ] Task 0's spike outcome (platform verified, or fallback taken + display
      re-verified) recorded in `docs/hardware.md`.
- [ ] `RadioManager::request(RadioMode::Bluetooth)` returns true and
      `request` transitions never leave both radios up (serial `[radio]`/`[bt]`
      ordering checked).
