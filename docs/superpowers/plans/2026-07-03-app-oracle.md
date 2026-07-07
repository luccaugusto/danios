# Oracle App (A2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Date:** 2026-07-06
**Spec:** [`docs/superpowers/specs/apps/oracle.md`](../specs/apps/oracle.md)
**Roadmap slot:** A2 (see [roadmap](2026-07-03-danios-roadmap.md) §1/§3/§4 — names/paths there are authoritative)

**Goal:** One stable wisdom entry per day from `/oracle/wisdom.txt` on SD, chosen by a date-seeded shuffle, with a random re-rolled-per-open fallback while the clock is unknown — app id `"oracle"` replaces the `oracleStub` registration in `src/main.cpp`.

**Architecture:** All picking logic lives in pure `lib/oracle_picker/` (std C++17 + `lib/date_utils` only), TDD'd on `pio test -e native`: the dateKey is converted to a civil-day serial via `date_utils`, each block of `entryCount` consecutive days ("a cycle") gets its own Fisher-Yates permutation of the list seeded deterministically from the cycle number, and the day's position in the cycle indexes that permutation — stable all day, changes at local midnight, unpredictable order, no repeats until the whole list cycles. `src/apps/oracle/OracleApp` is a thin LVGL wrapper: reload `wisdom.txt` on every open, typeset the entry centered over the oracle frame art (placeholder box when the art is missing), random pick via `esp_random()` while `TimeService` doesn't know the date.

**Tech Stack:** C++17 (both envs), LVGL 8.4 (v8 API), PlatformIO (`cyd` device env, `native` host-test env), Unity.

**Prerequisites:** F4 (`2026-07-03-foundation-4-wifi-time.md`) is **merged**: this plan consumes `TimeService` (`isTimeKnown()`, `today()`) and `lib/date_utils/` (`LocalDate`, `dateKey`, `fromDateKey`, `daysBetween`). Task 0 verifies every anchor. (F3's `StorageService`, LVGL `S:` drive, and SD-missing launcher disabling are already in the tree.)

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
  UI code runs on the Arduino loop task only (LVGL is not thread-safe).
- **C++17** on both envs: `build_unflags = -std=gnu++11`,
  `build_flags = -std=gnu++17`.
- **Radio rule:** WiFi XOR Bluetooth, enforced only through `RadioManager`.
  Oracle needs no radio: `requiredRadio()` returns `RadioMode::None` — it reads
  SD and uses the **cached** date from `TimeService`, never triggering a sync.
- **TDD, native-first:** all pure logic lives in `lib/<module>/` with **zero
  Arduino/LVGL includes** and is unit-tested with `pio test -e native` (Unity).
  Services/UI wrap the pure logic thinly.
- **Commits:** small, frequent, conventional (`feat:`, `test:`, `fix:`, `docs:`).
- **SD layout & NVS keys:** Oracle owns SD paths **`/oracle/wisdom.txt`** and
  **`/art/oracle/`** and **no NVS keys**. Icon file (when drawn):
  `S:/art/icons/oracle.bin` — out of scope here (see facts below).

### Plan-specific facts (verified against the current tree + the F4 plan)

- The Launcher gives `App::buildUI(lv_obj_t* parent)` a style-stripped container
  below its own 32 px top bar: **240 wide × 288 tall**, origin (0,0) at the
  container's top-left (`src/core/Launcher.cpp:200-203`). The back arrow and the
  "Oráculo" title in the top bar are the launcher's — the app must not add its
  own.
- Device UI language is **Portuguese**. The default font is the custom
  `montserrat_pt_14` covering full **Latin-1 (0xA0–0xFF)** — `á ã ç é í ó ú`
  render fine — plus the FontAwesome symbol range. Curly quotes `“ ”`
  (U+201C/U+201D) are **NOT** in the font: use ASCII `"` or none. `wisdom.txt`
  must be **UTF-8**; entries should stick to ASCII + Latin-1 accents or they
  render as missing-glyph boxes.
- PlatformIO adds each `lib/<module>/` root to the include path — the repo
  convention is the flat form: `#include <settings_store.h>` (see
  `test/test_settings_store/test_main.cpp:2`), so this plan uses
  `#include <oracle_picker.h>` and assumes `#include <date_utils.h>`. The F4
  plan's text writes `<date_utils/date_utils.h>` — **Task 0 records the form F4
  actually landed**; use that exact form everywhere this plan says
  `<date_utils.h>`.
- PlatformIO's LDF resolves cross-`lib/` includes automatically:
  `oracle_picker.cpp` including the `date_utils` header pulls `lib/date_utils/`
  into both the native test build and the device build. No `library.json`
  needed.
- F4 lands the `TimeService` global in `src/main.cpp` as
  `static TimeService timeService(radioManager, wifiService, settings);`. Its
  position relative to the app statics is F4's choice — so `OracleApp` takes
  its deps via `setDeps(...)` called in `setup()` (same pattern as
  `settingsApp.setDeps(...)`, `src/main.cpp:119`), never via constructor.
- `TimeService::today()` returns `LocalDate{0,0,0}` while the clock is unknown,
  and `dateKey({0,0,0}) == 0` — **0 is the universal "unknown" sentinel**
  (roadmap §4.3/§4.8). The app never calls `isTimeKnown()` separately; it keys
  everything off `dateKey(today()) == 0`.
- `StorageService::exists()`/`readLines()` take bare SD paths
  (`"/art/oracle/frame.bin"`); LVGL image sources take the drive-letter form
  (`"S:/art/oracle/frame.bin"`). `readLines()` already trims `\r` and skips
  empty lines (roadmap §4.9) — no cleaning needed in the app.
- `esp_random()` (`#include <esp_random.h>`, IDF 5.x under arduino-esp32 3.x)
  is a no-seed hardware RNG — with radios off it degrades to a weaker PRNG,
  which is fine for picking a quote.
- Launcher label/icon come from `catalog::kOracle` in `src/apps/app_catalog.h`
  (`"Oráculo"`, icon `nullptr`). Drawing `S:/art/icons/oracle.bin` is **out of
  scope**; the launcher renders its colored-letter fallback. Do not edit the
  catalog.
- `main.cpp` already disables the oracle app in the launcher when the SD card
  is missing (`launcher.setAppEnabled("oracle", false)` in the F3 block) — the
  spec's "SD card missing" row needs **no change**.
- Spec non-goals: no online/hybrid quote sources, no history, no favorites.
  Do not add them.

## File Structure

| File | Task | Responsibility |
| --- | --- | --- |
| Create `lib/oracle_picker/oracle_picker.h` | 1–2 | Pure picker interface (grows one task at a time) |
| Create `lib/oracle_picker/oracle_picker.cpp` | 1–2 | Picker implementation — std C++17 + `date_utils` only |
| Create `test/test_oracle_picker/test_main.cpp` | 1–2 | Unity tests, one dir per lib module (repo convention) |
| Create `src/apps/oracle/OracleApp.h` | 3 | `App` subclass declaration |
| Create `src/apps/oracle/OracleApp.cpp` | 3 | LVGL frame + label, load/pick/typeset, midnight rollover |
| Modify `src/main.cpp` | 3 | Replace `oracleStub` with `OracleApp` + `setDeps` |
| Create `sd/oracle/wisdom.txt` | 3 | Staged starter entries (maker edits later) |
| Modify `sd/README.md` | 3 | Note the wisdom.txt format for the maker |

### Picker design (locked in here, implemented across Tasks 1–2)

Two-level API so the cycle math is natively testable with plain integers:

- `size_t oraclePickAt(int32_t daySerial, size_t entryCount)` — the core.
  `daySerial` is the civil-day number (1970-01-01 = 0, may be negative).
  `cycle = floorDiv(daySerial, entryCount)`, `pos = floorMod(daySerial,
  entryCount)`; a Fisher-Yates permutation of `[0, entryCount)` is generated
  with a splitmix64 stream seeded from `(cycle, entryCount)`; the result is
  `perm[pos]`. Every aligned `entryCount`-day cycle is therefore a full
  permutation (no repeats within a cycle), consecutive days walk it, and each
  cycle reshuffles (unpredictable order). O(entryCount) work + one small
  vector per call — called once per app open; a wisdom list is hundreds of
  lines at most.
- `size_t oraclePick(uint32_t key, size_t entryCount)` — the wrapper the app
  calls (the spec-pinned `(dateKey, entryCount) → index` shape). Converts
  `key` (YYYYMMDD) to the day serial via
  `daysBetween(LocalDate{1970,1,1}, fromDateKey(key))`.
- **Edges (pinned by the spec):** `entryCount == 0` → returns 0 (callers guard
  the empty list themselves); `entryCount == 1` → always 0; changing
  `entryCount` reshuffles — same date + same count still → same index
  (list-size-change behavior the spec tolerates). `key == 0` (clock-unknown
  sentinel) never reaches the picker in practice, but floor-div/mod keeps even
  that input in range — no UB path.

### App behavior (locked in here, implemented in Task 3)

- `onEnter()` reloads `/oracle/wisdom.txt` on **every open** — the maker edits
  the file between boots; the list may grow/shrink at any time.
- `buildUI()`: frame art `S:/art/oracle/frame.bin` (maker-tunable constant) as
  an `lv_img` when `/art/oracle/frame.bin` exists on SD; otherwise a
  **placeholder** plain styled container (roadmap §4.1). The entry is an
  `lv_label` created after (so on top of) the frame: width 184, wrapped,
  center-aligned, +4 line spacing, centered on the 240×288 container.
- Pick: `key = dateKey(time.today())`; `key != 0` → `oraclePick(key, n)`;
  `key == 0` → `esp_random() % n`, re-rolled on each open (spec fallback).
- `tick()` (1 s throttle): if `dateKey(today())` no longer matches the shown
  key, re-pick in place — covers **midnight rollover while the app is open**
  and the clock becoming known mid-session (0 → real key).
- Empty/missing file → friendly PT empty state (exact copy in Task 3) telling
  the maker where to put the file.

## Task Right-Sizing Overview

0. Preflight — verify the F4 baseline and record anchors
1. `oracle_picker`: `oraclePick` — determinism, range, count 0/1 edges (TDD)
2. `oracle_picker`: `oraclePickAt` — cycle permutation walk, reshuffle per
   cycle, boundary crossings (TDD)
3. `OracleApp` UI wrapper + registration + staged SD content
4. On-device verification (manual — needs the CYD + a microSD card)

---

### Task 0: Preflight — verify the F4 baseline

**Files:** none (verification only).

**Interfaces:**
- Consumes: the merged F1–F4 tree.
- Produces: confidence that this plan's graft points exist, plus one recorded
  fact later tasks need (the `date_utils` include form).

- [ ] **Step 1: Confirm builds and tests are green**

Run: `cd /home/lucca/repos/danios && pio test -e native && pio run -e cyd`
Expected: all native test dirs PASS; device build `SUCCESS`.

- [ ] **Step 2: Confirm the F4 anchors**

Run:

```bash
grep -n "isTimeKnown\|LocalDate today" src/services/TimeService.h && \
grep -n "static TimeService timeService" src/main.cpp && \
grep -n "daysBetween\|fromDateKey" lib/date_utils/*.h && \
grep -n "oracleStub" src/main.cpp
```

Expected: at least one hit per grep (`oracleStub` shows three: the static, the
registration, and the `setAppEnabled` line). If `TimeService.h` or
`lib/date_utils/` is missing, **stop — F4 is not merged.**

- [ ] **Step 3: Record the `date_utils` include form**

Run: `grep -rn "date_utils" test/test_date_utils/test_main.cpp src/services/TimeService.cpp | grep include | head -2`
Expected: the include lines show whether F4 landed `#include <date_utils.h>`
(the repo's flat convention) or `#include <date_utils/date_utils.h>` (the F4
plan's spelling). **Use that exact form** wherever Tasks 2–3 below write
`#include <date_utils.h>`.

---

### Task 1: `oracle_picker` — deterministic pick, range, count edges

**Files:**
- Create: `lib/oracle_picker/oracle_picker.h`
- Create: `lib/oracle_picker/oracle_picker.cpp`
- Test: `test/test_oracle_picker/test_main.cpp`

**Interfaces:**
- Consumes: nothing yet (std C++ only — the `date_utils` dependency arrives in
  Task 2).
- Produces: `size_t oraclePick(uint32_t key, size_t entryCount)` — the exact
  spec-pinned shape. Task 2 rewrites its internals (keeping every Task 1 test
  green) and adds `oraclePickAt`; Task 3's UI calls `oraclePick` only.

- [ ] **Step 1: Write the failing tests**

Create `test/test_oracle_picker/test_main.cpp`:

```cpp
// Host-side tests for oracle_picker (pio test -e native).
//
// The picker is A2's native-TDD module: one wisdom index per civil day,
// stable all day, walking a per-cycle shuffled permutation so nothing
// repeats until the whole list has been shown (spec §4.4).
#include <unity.h>

#include <oracle_picker.h>

void setUp() {}
void tearDown() {}

static void test_count_zero_returns_zero() {
  // 0 is a defined, harmless answer; the app never renders with an empty
  // list (it shows the empty state instead).
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260706u, 0));
}

static void test_count_one_always_zero() {
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260101u, 1));
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20260706u, 1));
  TEST_ASSERT_EQUAL_UINT(0u, oraclePick(20991231u, 1));
}

static void test_index_always_in_range() {
  const uint32_t kDates[] = {20260101u, 20260228u, 20260706u,
                             20261231u, 20270101u, 20991231u};
  const size_t kCounts[] = {2, 3, 7, 10, 137};
  for (uint32_t d : kDates)
    for (size_t c : kCounts)
      TEST_ASSERT_LESS_THAN_UINT(c, oraclePick(d, c));
}

static void test_same_inputs_same_index() {
  // "Stable all day": the pick is a pure function of (date, count).
  for (int i = 0; i < 5; ++i)
    TEST_ASSERT_EQUAL_UINT(oraclePick(20260706u, 42),
                           oraclePick(20260706u, 42));
}

static void test_unknown_date_sentinel_stays_in_range() {
  // dateKey 0 = "clock never synced" sentinel. The app handles it before
  // calling the picker, but the picker must not misbehave if it arrives.
  TEST_ASSERT_LESS_THAN_UINT(5u, oraclePick(0u, 5));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_count_zero_returns_zero);
  RUN_TEST(test_count_one_always_zero);
  RUN_TEST(test_index_always_in_range);
  RUN_TEST(test_same_inputs_same_index);
  RUN_TEST(test_unknown_date_sentinel_stays_in_range);
  return UNITY_END();
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_oracle_picker`
Expected: **build error** — `oracle_picker.h: No such file or directory`

- [ ] **Step 3: Write the minimal implementation**

Create `lib/oracle_picker/oracle_picker.h`:

```cpp
// lib/oracle_picker/oracle_picker.h — date-seeded daily shuffle (roadmap §3,
// A2). Pure logic: std C++17 + lib/date_utils only, zero Arduino/LVGL.
//
// One index per civil day, stable all day: each block of entryCount
// consecutive days ("a cycle") is a shuffled permutation of the wisdom list,
// so the order is not predictable and no entry repeats until the whole list
// has been shown. Changing entryCount reshuffles — tolerated by the spec
// (the maker's list may grow/shrink between boots).
#pragma once

#include <cstddef>
#include <cstdint>

// The shape pinned by the spec: (date_utils dateKey YYYYMMDD, list size) →
// index into the list. entryCount 0 → 0 (callers guard the empty list
// themselves); entryCount 1 → always 0.
size_t oraclePick(uint32_t key, size_t entryCount);
```

Create `lib/oracle_picker/oracle_picker.cpp`:

```cpp
#include "oracle_picker.h"

size_t oraclePick(uint32_t key, size_t entryCount) {
  if (entryCount < 2) return 0;  // 0 → sentinel 0; 1 → the only entry
  return static_cast<size_t>(key) % entryCount;  // placeholder walk — Task 2
                                                 // replaces with the shuffle
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_oracle_picker`
Expected: `5 Tests 0 Failures 0 Ignored` — PASSED

- [ ] **Step 5: Commit**

```bash
git add lib/oracle_picker test/test_oracle_picker
git commit -m "feat: oracle_picker skeleton — deterministic in-range pick, count edges"
```

---

### Task 2: `oracle_picker` — date-seeded shuffle (cycle permutation walk)

**Files:**
- Modify: `lib/oracle_picker/oracle_picker.h`
- Modify: `lib/oracle_picker/oracle_picker.cpp`
- Test: `test/test_oracle_picker/test_main.cpp`

**Interfaces:**
- Consumes: Task 1's `oraclePick` (rewritten here; Task 1 tests stay green);
  `LocalDate`, `fromDateKey`, `daysBetween` from `lib/date_utils/` (F4,
  roadmap §4.3) — **adjust the `<date_utils.h>` include to the form Task 0
  recorded**.
- Produces: `size_t oraclePickAt(int32_t daySerial, size_t entryCount)` — the
  testable core (`daySerial` = civil days since 1970-01-01). `oraclePick`
  becomes dateKey → daySerial → `oraclePickAt`. Task 3 keeps calling
  `oraclePick` only.

- [ ] **Step 1: Write the failing tests**

Add above `main()` in `test/test_oracle_picker/test_main.cpp` (and add
`#include <date_utils.h>` — Task 0's recorded form — under
`#include <oracle_picker.h>`):

```cpp
// --- Task 2: the shuffle itself, tested on plain day serials -------------

static void test_cycle_zero_is_a_full_permutation() {
  // Serials 0..9 with count 10 are one aligned cycle: every index appears
  // exactly once — "no repeats until the whole list cycles".
  const size_t n = 10;
  bool seen[10] = {false};
  for (int32_t day = 0; day < 10; ++day) {
    const size_t idx = oraclePickAt(day, n);
    TEST_ASSERT_LESS_THAN_UINT(n, idx);
    TEST_ASSERT_FALSE_MESSAGE(seen[idx], "index repeated within a cycle");
    seen[idx] = true;
  }
}

static void test_later_cycle_is_also_a_full_permutation() {
  // An arbitrary aligned cycle far from epoch (serials 20660..20669 —
  // 2026-era days) must also cover every index exactly once.
  const size_t n = 10;
  const int32_t base = 2066 * 10;
  bool seen[10] = {false};
  for (int32_t day = base; day < base + 10; ++day) {
    const size_t idx = oraclePickAt(day, n);
    TEST_ASSERT_FALSE(seen[idx]);
    seen[idx] = true;
  }
}

static void test_cycles_have_different_orders() {
  // The whole point of the *date-seeded* shuffle: cycle k and cycle k+1
  // walk the list in different orders.
  const size_t n = 10;
  bool anyDifferent = false;
  for (int32_t pos = 0; pos < 10; ++pos)
    if (oraclePickAt(pos, n) != oraclePickAt(10 + pos, n)) anyDifferent = true;
  TEST_ASSERT_TRUE(anyDifferent);
}

static void test_order_is_shuffled_not_sequential() {
  // Not a bare "index = day % count" walk: somewhere in the cycle, the next
  // day is NOT simply previous+1 (mod count).
  const size_t n = 10;
  bool anyJump = false;
  for (int32_t day = 0; day < 9; ++day) {
    const size_t a = oraclePickAt(day, n);
    const size_t b = oraclePickAt(day + 1, n);
    if (b != (a + 1) % n) anyJump = true;
  }
  TEST_ASSERT_TRUE_MESSAGE(anyJump, "walk is sequential — not shuffled");
}

static void test_count_change_reshuffles_but_stays_valid() {
  // The maker's list grows/shrinks between boots: any count gives an
  // in-range, deterministic answer for the same day.
  const int32_t day = 20660;
  for (size_t n : {size_t{9}, size_t{10}, size_t{11}, size_t{137}}) {
    TEST_ASSERT_LESS_THAN_UINT(n, oraclePickAt(day, n));
    TEST_ASSERT_EQUAL_UINT(oraclePickAt(day, n), oraclePickAt(day, n));
  }
}

static void test_negative_serial_stays_in_range() {
  // Pre-1970 serials can't happen on-device, but floor div/mod keeps the
  // math total — no UB path.
  for (int32_t day = -25; day < 0; ++day)
    TEST_ASSERT_LESS_THAN_UINT(7u, oraclePickAt(day, 7));
}

static void test_pick_walks_serials_across_month_and_year_boundaries() {
  // oraclePick(dateKey) must land on the same walk position as the civil-day
  // serial — proven with date_utils across the two boundary kinds where a
  // naive dateKey walk breaks (YYYYMMDD jumps by 70/71 and ~8870).
  const LocalDate epoch{1970, 1, 1};
  const uint32_t kKeys[] = {20260731u, 20260801u, 20261231u, 20270101u};
  for (uint32_t key : kKeys) {
    const int32_t serial = daysBetween(epoch, fromDateKey(key));
    TEST_ASSERT_EQUAL_UINT(oraclePickAt(serial, 10), oraclePick(key, 10));
  }
}
```

Add to `main()` before `return UNITY_END();`:

```cpp
  RUN_TEST(test_cycle_zero_is_a_full_permutation);
  RUN_TEST(test_later_cycle_is_also_a_full_permutation);
  RUN_TEST(test_cycles_have_different_orders);
  RUN_TEST(test_order_is_shuffled_not_sequential);
  RUN_TEST(test_count_change_reshuffles_but_stays_valid);
  RUN_TEST(test_negative_serial_stays_in_range);
  RUN_TEST(test_pick_walks_serials_across_month_and_year_boundaries);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `pio test -e native -f test_oracle_picker`
Expected: **build error** — `'oraclePickAt' was not declared in this scope`

- [ ] **Step 3: Write the implementation**

In `lib/oracle_picker/oracle_picker.h`, add above the `oraclePick`
declaration:

```cpp
// Core walk, exposed for native tests: daySerial is the civil-day number
// (1970-01-01 = 0; may be negative — handled with floor semantics). Each
// aligned entryCount-day cycle is a Fisher-Yates permutation seeded from
// (cycle number, entryCount).
size_t oraclePickAt(int32_t daySerial, size_t entryCount);
```

Replace the whole `lib/oracle_picker/oracle_picker.cpp` (adjust the
`date_utils` include to Task 0's recorded form):

```cpp
#include "oracle_picker.h"

#include <date_utils.h>

#include <utility>
#include <vector>

namespace {

// splitmix64 — tiny deterministic mixer; the state advances per call. Not
// cryptographic, just unpredictable-to-a-human quote ordering.
uint64_t splitmix64(uint64_t& s) {
  s += 0x9E3779B97F4A7C15ull;
  uint64_t z = s;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

}  // namespace

size_t oraclePickAt(int32_t daySerial, size_t entryCount) {
  if (entryCount < 2) return 0;  // 0 → sentinel 0; 1 → the only entry
  const int32_t n = static_cast<int32_t>(entryCount);
  int32_t cycle = daySerial / n;
  int32_t pos = daySerial % n;
  if (pos < 0) {  // floor semantics so pre-epoch serials still walk cleanly
    pos += n;
    cycle -= 1;
  }

  // Fresh Fisher-Yates permutation per (cycle, entryCount). O(n) time and
  // memory — runs once per app open on a list of at most a few hundred lines.
  std::vector<size_t> perm(entryCount);
  for (size_t i = 0; i < entryCount; ++i) perm[i] = i;
  uint64_t state = (static_cast<uint64_t>(static_cast<int64_t>(cycle))) ^
                   (static_cast<uint64_t>(entryCount) << 32);
  for (size_t i = entryCount - 1; i > 0; --i) {
    const size_t j = static_cast<size_t>(splitmix64(state) % (i + 1));
    std::swap(perm[i], perm[j]);
  }
  return perm[static_cast<size_t>(pos)];
}

size_t oraclePick(uint32_t key, size_t entryCount) {
  if (entryCount < 2) return 0;
  const int32_t serial = daysBetween(LocalDate{1970, 1, 1}, fromDateKey(key));
  return oraclePickAt(serial, entryCount);
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `pio test -e native -f test_oracle_picker`
Expected: `12 Tests 0 Failures 0 Ignored` — PASSED

Also run the full suite to confirm nothing else broke:

Run: `pio test -e native`
Expected: all test dirs PASSED.

- [ ] **Step 5: Commit**

```bash
git add lib/oracle_picker test/test_oracle_picker
git commit -m "feat: oracle_picker date-seeded shuffle — per-cycle permutation walk"
```

---

### Task 3: `OracleApp` UI wrapper + registration + staged SD content

**Files:**
- Create: `src/apps/oracle/OracleApp.h`
- Create: `src/apps/oracle/OracleApp.cpp`
- Modify: `src/main.cpp` (include + static + registration; anchors below)
- Create: `sd/oracle/wisdom.txt`
- Modify: `sd/README.md`

**Interfaces:**
- Consumes: `oraclePick` (Tasks 1–2); `App` from `src/core/App.h`;
  `catalog::kOracle` from `src/apps/app_catalog.h`;
  `StorageService::readLines/exists` (F3, roadmap §4.9);
  `TimeService::today()` (F4, roadmap §4.8); `dateKey` from `date_utils`
  (Task 0's include form); `esp_random()`.
- Produces: `class OracleApp : public App` with a default constructor and
  `void setDeps(StorageService&, TimeService&)` — `main.cpp` creates one
  static instance, wires deps in `setup()`, and registers it in the fourth
  grid slot.

No native test — this is thin LVGL glue over the tested picker; the device
build is the check and Task 4 verifies behavior on hardware.

- [ ] **Step 1: Write the header**

Create `src/apps/oracle/OracleApp.h`:

```cpp
// src/apps/oracle/OracleApp.h — Oracle app (A2, spec §4.4). Thin LVGL
// wrapper: reload /oracle/wisdom.txt on every open, typeset one entry over
// the oracle frame art (placeholder box when missing). The daily pick lives
// in lib/oracle_picker (native-tested); while the clock is unknown the entry
// is random, re-rolled per open. No radio, no NVS.
#pragma once

#include <lvgl.h>

#include <cstdint>
#include <string>
#include <vector>

#include "apps/app_catalog.h"
#include "core/App.h"

class StorageService;
class TimeService;

class OracleApp : public App {
 public:
  void setDeps(StorageService& storage, TimeService& time) {
    storage_ = &storage;
    time_ = &time;
  }

  const char* id() const override { return "oracle"; }
  const char* title() const override { return catalog::kOracle.title; }
  const char* iconPath() const override { return catalog::kOracle.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override;
  void buildUI(lv_obj_t* parent) override;
  void onExit() override { label_ = nullptr; }  // launcher deletes widgets
  void tick(uint32_t now_ms) override;

 private:
  void showEntry();

  StorageService* storage_ = nullptr;
  TimeService* time_ = nullptr;
  std::vector<std::string> lines_;
  lv_obj_t* label_ = nullptr;
  uint32_t shownKey_ = 0;   // dateKey of the shown entry; 0 = random fallback
  uint32_t lastCheck_ = 0;  // tick() throttle
};
```

- [ ] **Step 2: Write the implementation**

Create `src/apps/oracle/OracleApp.cpp` (adjust the `date_utils` include to
Task 0's recorded form):

```cpp
#include "apps/oracle/OracleApp.h"

#include <date_utils.h>
#include <esp_random.h>
#include <oracle_picker.h>

#include "services/StorageService.h"
#include "services/TimeService.h"

namespace {

constexpr const char* kWisdomPath = "/oracle/wisdom.txt";
// Maker-tunable frame art (roadmap §4.1: placeholder until hand-drawn art
// lands on the card). Same file, two path forms: StorageService takes bare
// SD paths, LVGL takes the 'S:' drive form.
constexpr const char* kFrameSdPath = "/art/oracle/frame.bin";
constexpr const char* kFrameLvglPath = "S:/art/oracle/frame.bin";

}  // namespace

void OracleApp::onEnter() {
  // Reload on every open: the maker edits wisdom.txt between boots and the
  // list may grow/shrink at any time (spec). readLines trims \r and skips
  // empty lines, so lines_ holds only real entries.
  lines_.clear();
  storage_->readLines(kWisdomPath, lines_);
}

void OracleApp::buildUI(lv_obj_t* parent) {
  // Parent is the launcher's style-stripped 240×288 container below the top
  // bar (back arrow + "Oráculo" title are the launcher's — none here).
  if (storage_->exists(kFrameSdPath)) {
    lv_obj_t* frame = lv_img_create(parent);
    lv_img_set_src(frame, kFrameLvglPath);
    lv_obj_center(frame);
  } else {
    // Placeholder frame: plain styled container (roadmap §4.1).
    lv_obj_t* frame = lv_obj_create(parent);
    lv_obj_remove_style_all(frame);
    lv_obj_set_size(frame, 216, 256);
    lv_obj_center(frame);
    lv_obj_set_style_bg_color(frame, lv_color_hex(0x252B54), 0);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(frame, 12, 0);
    lv_obj_set_style_border_width(frame, 2, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(0x8A7FD6), 0);
  }

  // The entry, typeset centered over the frame (created after it → on top).
  label_ = lv_label_create(parent);
  lv_label_set_long_mode(label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_, 184);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(label_, 4, 0);
  lv_obj_center(label_);

  if (lines_.empty()) {
    // Spec §6.5: file missing or empty → tell the maker where to put it.
    lv_label_set_text(label_,
                      "O oráculo ainda não tem sabedoria.\n\n"
                      "Coloque frases (uma por linha) em\n"
                      "/oracle/wisdom.txt no cartão SD.");
    return;
  }
  showEntry();
}

void OracleApp::showEntry() {
  // dateKey 0 = clock never synced (roadmap §4.3): random entry, re-rolled
  // on each open. Otherwise the date-seeded shuffle — stable all day.
  const uint32_t key = dateKey(time_->today());
  const size_t idx =
      (key != 0) ? oraclePick(key, lines_.size())
                 : static_cast<size_t>(esp_random() % lines_.size());
  shownKey_ = key;
  lv_label_set_text(label_, lines_[idx].c_str());
}

void OracleApp::tick(uint32_t now_ms) {
  if (label_ == nullptr || lines_.empty()) return;
  if (now_ms - lastCheck_ < 1000) return;  // once a second is plenty
  lastCheck_ = now_ms;
  // Re-pick in place at local midnight, or when the clock becomes known
  // mid-session (shownKey_ 0 → real key). While the clock stays unknown,
  // key == shownKey_ == 0, so the random entry holds for the whole open.
  if (dateKey(time_->today()) != shownKey_) showEntry();
}
```

- [ ] **Step 3: Replace the stub registration in main.cpp**

In `src/main.cpp` (anchors, not line numbers — F4 shifted the file):

Add the include after `#include "apps/app_catalog.h"`:

```cpp
#include "apps/oracle/OracleApp.h"
```

Replace the line `static StubApp oracleStub("oracle", catalog::kOracle);`
with:

```cpp
static OracleApp oracleApp;
```

In `setup()`, replace the line `launcher.registerApp(&oracleStub);` with —
**same position, fourth registration, so the grid order is unchanged**:

```cpp
  oracleApp.setDeps(storage, timeService);
  launcher.registerApp(&oracleApp);
```

Do not touch the `setAppEnabled("oracle", false)` block — the F3 SD-missing
behavior already covers the spec's "SD card missing" row.

- [ ] **Step 4: Stage the SD content**

Create `sd/oracle/wisdom.txt` (UTF-8, one entry per line — starter set; the
maker edits this on the card):

```text
A pressa é inimiga da perfeição.
Quem planta ternura colhe abraços.
Devagar também é progresso.
Hoje é um bom dia para começar de novo.
Até o café mais forte respeita quem dormiu cedo.
Confie no caminho, mesmo quando ele faz curvas.
Um passo pequeno ainda é um passo.
```

Append to `sd/README.md`:

```markdown

`oracle/wisdom.txt` holds the Oracle's entries — one per line, UTF-8,
edited freely (the list may grow or shrink). Stick to ASCII + Portuguese
accents; curly quotes render as missing glyphs on the device. The oracle
frame art goes in `art/oracle/frame.bin` (LVGL RGB565 .bin); until it
exists the app draws a styled placeholder box.
```

- [ ] **Step 5: Build for the device and re-run native tests**

Run: `pio run -e cyd`
Expected: `SUCCESS` (RAM/Flash usage printed; no warnings from new files)

Run: `pio test -e native`
Expected: all test dirs PASSED (including the 12 `test_oracle_picker` tests)

- [ ] **Step 6: Commit**

```bash
git add src/apps/oracle src/main.cpp sd/oracle sd/README.md
git commit -m "feat: Oracle app — daily wisdom from SD via date-seeded shuffle"
```

---

### Task 4: On-device verification (manual — needs the CYD + a microSD card)

**Files:** none (verification only).

**Interfaces:** consumes the flashed firmware from Task 3 and the staged
`sd/` tree on a FAT32 card.

- [ ] **Step 1: Prepare the card and flash**

Copy the **contents** of `sd/` to the root of the microSD card (so the card
has `/oracle/wisdom.txt` and `/art/icons/settings.bin`), insert it, then:

Run: `pio run -e cyd -t upload` (CYD enumerates as `/dev/ttyUSB0`)
Expected: upload completes; serial (115200) prints `danios: launcher up`

- [ ] **Step 2: Known-clock path — stable daily entry**

- Set the clock: Settings → Clock section (F4) → manual set to today ~10:00
  (no WiFi needed).
- Open **Oráculo** (fourth grid icon, colored-letter fallback — no icon art
  yet by design): one wisdom entry appears, wrapped and centered inside the
  purple-bordered placeholder frame (no `frame.bin` on the card yet).
- Back to the launcher, reopen three times: **the same entry every time**.
- Visit other apps and come back: still the same entry.

- [ ] **Step 3: Date walk — tomorrow is a different entry**

- Settings → Clock → set the date to tomorrow (same time). Open Oráculo:
  a **different** entry. (Rare legit exception: on a cycle boundary — every
  `entryCount` days — the new shuffle may repeat the last entry once. If the
  entry looks unchanged, advance one more day and re-check.)
- Set the date back to today: the original entry from Step 2 returns
  (determinism, not randomness).

- [ ] **Step 4: Unknown-clock fallback — random per open**

- Reboot the device (power-cycle) and do **not** set the clock; make sure no
  WiFi credentials are stored (Settings → WiFi → forget, if any) so NTP
  can't sync. The status bar shows `--:--`.
- Open Oráculo: an entry appears. Close and reopen ~5 times: the entry
  **changes between opens** (random re-roll; an occasional repeat out of the
  7 staged entries is normal).
- Settings → Clock → set today's date, reopen Oráculo: back to the stable
  daily entry (same one as Step 2 if the date matches).

- [ ] **Step 5: Empty state and SD-missing behavior**

- Power off, pull the card, rename `/oracle/wisdom.txt` to
  `/oracle/wisdom.bak` on a computer, reinsert, boot: Oráculo opens with the
  friendly empty state ("O oráculo ainda não tem sabedoria…" pointing at
  `/oracle/wisdom.txt`). Restore the filename afterwards.
- Boot once **without** the card: the Oráculo icon is greyed out; tapping it
  shows the F3 hint msgbox; the boot-time SD-missing msgbox lists Oráculo
  among the napping apps. (Existing F3 behavior — just confirm it survived.)

- [ ] **Step 6: Midnight rollover while open**

- Settings → Clock → manual set to today 23:59. Open Oráculo and wait ~90 s
  (screen sleep defaults to 60 s — tap once to wake if needed; the waking
  tap is swallowed by the F3 shield). When the clock passes 00:00 the entry
  **swaps in place** to the next day's pick — no navigation needed.

---

## Definition of done

- [ ] `pio test -e native` — all test dirs green (12 `test_oracle_picker`
      tests included)
- [ ] `pio run -e cyd` — device build green
- [ ] Roadmap §1 E2E outcome observed on hardware: one stable wisdom entry
      per day from SD; random fallback when the clock is unknown (Task 4
      checklist complete)
- [ ] No NVS keys introduced; SD paths limited to `/oracle/wisdom.txt` +
      `/art/oracle/` (+ staged `sd/` mirror)
- [ ] `lib/oracle_picker/` has zero Arduino/LVGL includes (std C++17 +
      `date_utils` only)
- [ ] `src/apps/app_catalog.h` untouched (icon stays `nullptr` until
      `S:/art/icons/oracle.bin` is drawn)
