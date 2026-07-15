# Pomodoro App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A pomodoro timer app (Work/Break cycle, default 25/5 min, one Start/Stop button, NVS-persisted config, launcher badge while running) per `docs/superpowers/specs/2026-07-15-pomodoro-design.md`.

**Architecture:** House two-unit pattern: `lib/pomodoro_model/` is a pure-C++17 state machine driven by caller-supplied `millis()` timestamps (no Arduino/LVGL includes — the native test env compiles everything in `lib/`), and `src/apps/pomodoro/PomodoroApp` is a thin LVGL 8.4 UI. The app instance is a boot-time static, so the timer keeps counting while the app is closed; `main.cpp` polls it ~1 Hz for the launcher badge (same pattern as the Pet badge).

**Tech Stack:** C++17, LVGL 8.4.0, Unity native tests (`pio test -e native`), PlatformIO env `cyd` for device builds.

## Global Constraints

- `lib/` code must compile in the `native` env: **zero Arduino/LVGL includes** in `lib/pomodoro_model/`.
- LVGL is pinned at **8.4.0** — use v8 APIs only.
- UI copy is **PT-BR** ("Iniciar", "Parar", "Trabalho", "Pausa"). Default font `montserrat_pt_14` covers PT accents; the big countdown uses stock `lv_font_montserrat_48` (digits/colon are ASCII — never put accented text in that font).
- App id is `"pomodoro"` — an NVS/navigation key; never change it. Existing ids must not change either.
- Launcher/app contract: content area handed to `buildUI` is 240×288 (32 px top bar above it). `tick()` runs only while the app is active. The Launcher deletes the app's widgets after `onExit()`.
- Titles/icons live only in `src/apps/app_catalog.h`; icon path stays `nullptr` until hand-drawn art exists on the SD card.
- Settings must remain the **last** icon in the launcher grid.
- NVS keys: `pomo_work_min` (u32, default 25, clamp 5–60), `pomo_break_min` (u32, default 5, clamp 1–15).
- Run native tests with `pio test -e native --filter test_pomodoro_model`; device build with `pio run -e cyd`.

---

### Task 1: `lib/pomodoro_model` — PomoTimer state machine (TDD)

**Files:**
- Create: `lib/pomodoro_model/pomodoro_model.h`
- Create: `lib/pomodoro_model/pomodoro_model.cpp`
- Test: `test/test_pomodoro_model/test_main.cpp`

**Interfaces:**
- Consumes: nothing (pure logic, `<cstdint>` only).
- Produces (used by Task 2/3):
  - `enum class PomoPhase : uint8_t { Idle, Work, Break }`
  - `struct PomoConfig { uint16_t work_min = 25; uint16_t break_min = 5; }`
  - `class PomoTimer` with `void configure(const PomoConfig&)` (ignored unless Idle), `PomoConfig config() const`, `void start(uint32_t now_ms)`, `void stop()`, `PomoPhase phase(uint32_t now_ms)`, `uint32_t remainingMs(uint32_t now_ms)`, `bool running() const`.

- [ ] **Step 1: Write the failing tests**

Create `test/test_pomodoro_model/test_main.cpp`:

```cpp
// Host-side tests for PomoTimer (pio test -e native).
//
// The timer is a pure state machine over caller-supplied millis() timestamps:
// Idle -> (start) -> Work <-> Break forever until stop(). Transitions resolve
// lazily in phase()/remainingMs() with a catch-up loop, so the timer is
// correct even when queried after several missed sections. All math is
// unsigned-wrap-safe (millis() wraps every ~49.7 days).
#include <unity.h>

#include "pomodoro_model.h"

void setUp() {}
void tearDown() {}

static constexpr uint32_t kMin = 60000u;  // ms per minute

static void test_starts_idle() {
  PomoTimer t;
  TEST_ASSERT_FALSE(t.running());
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Idle), static_cast<int>(t.phase(0)));
  TEST_ASSERT_EQUAL_UINT32(0, t.remainingMs(0));
}

static void test_default_config_25_5() {
  PomoTimer t;
  TEST_ASSERT_EQUAL_UINT16(25, t.config().work_min);
  TEST_ASSERT_EQUAL_UINT16(5, t.config().break_min);
}

static void test_start_enters_work_with_full_remaining() {
  PomoTimer t;
  t.start(1000);
  TEST_ASSERT_TRUE(t.running());
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work), static_cast<int>(t.phase(1000)));
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(1000));
}

static void test_remaining_counts_down() {
  PomoTimer t;
  t.start(0);
  TEST_ASSERT_EQUAL_UINT32(25 * kMin - 1500, t.remainingMs(1500));
}

static void test_work_to_break_at_boundary() {
  PomoTimer t;
  t.start(0);
  // One ms before the boundary: still Work.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(25 * kMin - 1)));
  // At the boundary: Break, with the full break remaining.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break),
                    static_cast<int>(t.phase(25 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(5 * kMin, t.remainingMs(25 * kMin));
}

static void test_break_back_to_work() {
  PomoTimer t;
  t.start(0);
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(30 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(30 * kMin));
}

static void test_catch_up_over_multiple_missed_sections() {
  PomoTimer t;
  t.start(0);
  // 65 min = 2 full 30-min cycles + 5 min into the 3rd Work section.
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work),
                    static_cast<int>(t.phase(65 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(20 * kMin, t.remainingMs(65 * kMin));
}

static void test_stop_returns_to_idle() {
  PomoTimer t;
  t.start(0);
  t.stop();
  TEST_ASSERT_FALSE(t.running());
  TEST_ASSERT_EQUAL_UINT32(0, t.remainingMs(10 * kMin));
}

static void test_start_while_running_is_ignored() {
  PomoTimer t;
  t.start(0);
  t.start(10 * kMin);  // must not restart the section
  TEST_ASSERT_EQUAL_UINT32(15 * kMin, t.remainingMs(10 * kMin));
}

static void test_configure_applies_when_idle() {
  PomoTimer t;
  t.configure({50, 10});
  t.start(0);
  TEST_ASSERT_EQUAL_UINT32(50 * kMin, t.remainingMs(0));
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break),
                    static_cast<int>(t.phase(50 * kMin)));
  TEST_ASSERT_EQUAL_UINT32(10 * kMin, t.remainingMs(50 * kMin));
}

static void test_configure_while_running_is_ignored() {
  PomoTimer t;
  t.start(0);
  t.configure({50, 10});
  TEST_ASSERT_EQUAL_UINT16(25, t.config().work_min);
  TEST_ASSERT_EQUAL_UINT32(25 * kMin, t.remainingMs(0));
  // After stop, configure works again.
  t.stop();
  t.configure({50, 10});
  TEST_ASSERT_EQUAL_UINT16(50, t.config().work_min);
}

static void test_millis_wrap_is_safe() {
  PomoTimer t;
  // Start 10 min before the uint32 wrap; query 20 min later (past the wrap).
  const uint32_t start = 0xFFFFFFFFu - 10 * kMin;
  t.start(start);
  const uint32_t later = start + 20 * kMin;  // wraps
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Work), static_cast<int>(t.phase(later)));
  TEST_ASSERT_EQUAL_UINT32(5 * kMin, t.remainingMs(later));
}

static void test_wrap_across_boundary() {
  PomoTimer t;
  const uint32_t start = 0xFFFFFFFFu - 10 * kMin;
  t.start(start);
  const uint32_t later = start + 27 * kMin;  // wraps, 2 min into Break
  TEST_ASSERT_EQUAL(static_cast<int>(PomoPhase::Break), static_cast<int>(t.phase(later)));
  TEST_ASSERT_EQUAL_UINT32(3 * kMin, t.remainingMs(later));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_idle);
  RUN_TEST(test_default_config_25_5);
  RUN_TEST(test_start_enters_work_with_full_remaining);
  RUN_TEST(test_remaining_counts_down);
  RUN_TEST(test_work_to_break_at_boundary);
  RUN_TEST(test_break_back_to_work);
  RUN_TEST(test_catch_up_over_multiple_missed_sections);
  RUN_TEST(test_stop_returns_to_idle);
  RUN_TEST(test_start_while_running_is_ignored);
  RUN_TEST(test_configure_applies_when_idle);
  RUN_TEST(test_configure_while_running_is_ignored);
  RUN_TEST(test_millis_wrap_is_safe);
  RUN_TEST(test_wrap_across_boundary);
  return UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `pio test -e native --filter test_pomodoro_model`
Expected: build FAILURE — `pomodoro_model.h: No such file or directory`.

- [ ] **Step 3: Write the model**

Create `lib/pomodoro_model/pomodoro_model.h`:

```cpp
// lib/pomodoro_model/pomodoro_model.h — pomodoro state machine (spec
// 2026-07-15-pomodoro-design). Pure C++17, no Arduino/LVGL: all time comes
// in as caller-supplied millis() values. Transitions resolve lazily in
// phase()/remainingMs() (catch-up loop), so the owner may stop querying for
// long stretches; the ~1 Hz badge poll in main.cpp bounds the gap well under
// the uint32 wrap.
#pragma once

#include <cstdint>

enum class PomoPhase : uint8_t { Idle, Work, Break };

struct PomoConfig {
  uint16_t work_min = 25;
  uint16_t break_min = 5;
};

class PomoTimer {
 public:
  void configure(const PomoConfig& c);  // ignored unless Idle
  PomoConfig config() const { return cfg_; }

  void start(uint32_t now_ms);  // Idle -> Work; ignored while running
  void stop();                  // any -> Idle

  PomoPhase phase(uint32_t now_ms);       // resolves due transitions
  uint32_t remainingMs(uint32_t now_ms);  // 0 when Idle
  bool running() const { return phase_ != PomoPhase::Idle; }

 private:
  void catchUp(uint32_t now_ms);
  uint32_t phaseLenMs(PomoPhase p) const;

  PomoConfig cfg_{};
  PomoPhase phase_ = PomoPhase::Idle;
  uint32_t phaseStart_ = 0;  // millis at which the current phase began
};
```

Create `lib/pomodoro_model/pomodoro_model.cpp`:

```cpp
#include "pomodoro_model.h"

void PomoTimer::configure(const PomoConfig& c) {
  if (phase_ == PomoPhase::Idle) cfg_ = c;
}

void PomoTimer::start(uint32_t now_ms) {
  if (phase_ != PomoPhase::Idle) return;
  phase_ = PomoPhase::Work;
  phaseStart_ = now_ms;
}

void PomoTimer::stop() { phase_ = PomoPhase::Idle; }

uint32_t PomoTimer::phaseLenMs(PomoPhase p) const {
  const uint16_t mins = (p == PomoPhase::Work) ? cfg_.work_min : cfg_.break_min;
  return static_cast<uint32_t>(mins) * 60000u;
}

// Unsigned subtraction makes `now - phaseStart_` wrap-safe as long as the
// timer is queried at least once per millis() wrap (~49.7 days).
void PomoTimer::catchUp(uint32_t now_ms) {
  if (phase_ == PomoPhase::Idle) return;
  while (now_ms - phaseStart_ >= phaseLenMs(phase_)) {
    phaseStart_ += phaseLenMs(phase_);
    phase_ = (phase_ == PomoPhase::Work) ? PomoPhase::Break : PomoPhase::Work;
  }
}

PomoPhase PomoTimer::phase(uint32_t now_ms) {
  catchUp(now_ms);
  return phase_;
}

uint32_t PomoTimer::remainingMs(uint32_t now_ms) {
  catchUp(now_ms);
  if (phase_ == PomoPhase::Idle) return 0;
  return phaseLenMs(phase_) - (now_ms - phaseStart_);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `pio test -e native --filter test_pomodoro_model`
Expected: `13 Tests 0 Failures 0 Ignored` — PASSED.

- [ ] **Step 5: Run the full native suite (no regressions)**

Run: `pio test -e native`
Expected: all test dirs pass, including the pre-existing ones.

- [ ] **Step 6: Commit**

```bash
git add lib/pomodoro_model test/test_pomodoro_model
git commit -m "feat: pomodoro_model — Idle/Work/Break timer with lazy catch-up"
```

---

### Task 2: PomodoroApp UI + catalog entry + big countdown font

**Files:**
- Modify: `src/apps/app_catalog.h` (add `kPomodoro` after `kPet`)
- Modify: `include/lv_conf.h` (enable `LV_FONT_MONTSERRAT_48`, next to the `LV_FONT_MONTSERRAT_14 1` line)
- Create: `src/apps/pomodoro/PomodoroApp.h`
- Create: `src/apps/pomodoro/PomodoroApp.cpp`

**Interfaces:**
- Consumes: Task 1's `PomoTimer`/`PomoPhase`/`PomoConfig`; `ISettingsStore` (`getU32`/`setU32`); `StorageService::exists(const char* barePath)` (LVGL `"S:/x"` path minus the leading `"S:"`); `catalog::kPomodoro`.
- Produces (used by Task 3): `class PomodoroApp : public App` with `void setDeps(ISettingsStore&, StorageService&)` and `bool badgeOn(uint32_t now_ms)`.

- [ ] **Step 1: Add the catalog entry**

In `src/apps/app_catalog.h`, after the `kPet` line add:

```cpp
inline constexpr AppInfo kPomodoro{"Pomodoro", nullptr};
```

(`nullptr` icon until `sd/art/icons/pomodoro.bin` is drawn — file-header rule.)

- [ ] **Step 2: Enable the 48 px stock font**

In `include/lv_conf.h`, directly below `#define LV_FONT_MONTSERRAT_14 1` add:

```c
#define LV_FONT_MONTSERRAT_48 1  /* pomodoro countdown; ASCII digits only */
```

- [ ] **Step 3: Write the app header**

Create `src/apps/pomodoro/PomodoroApp.h`:

```cpp
// src/apps/pomodoro/PomodoroApp.h — Pomodoro timer (spec 2026-07-15). Thin
// LVGL wrapper over lib/pomodoro_model: status art (120x120, colored
// placeholder until S:/art/pomo/*.bin exist), 48 px mm:ss countdown, one
// Start/Stop button, work/break steppers (NVS, editable only while Idle),
// full-screen flash on a phase flip. The timer lives in this boot-time
// static instance, so it keeps counting while the app is closed; main.cpp
// polls badgeOn(millis()) ~1 Hz for the launcher badge. No radio.
#pragma once

#include <lvgl.h>

#include <vector>

#include <pomodoro_model.h>
#include <settings_store.h>

#include "apps/app_catalog.h"
#include "core/App.h"

class StorageService;

class PomodoroApp : public App {
 public:
  // Call once from main.cpp before registerApp (loads config from NVS).
  void setDeps(ISettingsStore& store, StorageService& storage);

  const char* id() const override { return "pomodoro"; }
  const char* title() const override { return catalog::kPomodoro.title; }
  const char* iconPath() const override { return catalog::kPomodoro.icon; }
  RadioMode requiredRadio() const override { return RadioMode::None; }
  void onEnter() override {}
  void buildUI(lv_obj_t* parent) override;
  void onExit() override;
  void tick(uint32_t now_ms) override;

  // main.cpp badge poll; also resolves transitions that fell due while the
  // app was closed (keeps the wrap-safety invariant of pomodoro_model).
  bool badgeOn(uint32_t now_ms) {
    timer_.phase(now_ms);
    return timer_.running();
  }

 private:
  void buildStepperRow(lv_obj_t* parent, lv_coord_t y, const char* name,
                       lv_obj_t** valLbl, lv_event_cb_t minusCb,
                       lv_event_cb_t plusCb);
  void syncAll(uint32_t now_ms);  // art + countdown + button + steppers
  void updateCountdown(uint32_t now_ms);
  void applyPhaseArt(PomoPhase p);
  void adjust(int dWork, int dBreak);  // stepper deltas, clamps + saves
  void saveConfig();
  void flash();      // 3 quick white blinks on lv_layer_top()
  void stopFlash();

  // LVGL C callbacks (each gets `this` via user_data).
  static void onStartStop(lv_event_t* e);
  static void onWorkMinus(lv_event_t* e);
  static void onWorkPlus(lv_event_t* e);
  static void onBreakMinus(lv_event_t* e);
  static void onBreakPlus(lv_event_t* e);
  static void onFlashTimer(lv_timer_t* t);

  ISettingsStore* store_ = nullptr;
  StorageService* storage_ = nullptr;

  PomoTimer timer_;
  PomoPhase shownPhase_ = PomoPhase::Idle;
  uint32_t lastLblMs_ = 0;

  lv_obj_t* box_ = nullptr;        // placeholder art (no SD file)
  lv_obj_t* boxLbl_ = nullptr;
  lv_obj_t* img_ = nullptr;        // real art (SD file present)
  lv_obj_t* countdown_ = nullptr;
  lv_obj_t* btnLbl_ = nullptr;
  lv_obj_t* workVal_ = nullptr;
  lv_obj_t* breakVal_ = nullptr;
  std::vector<lv_obj_t*> stepperBtns_;
  lv_obj_t* flashOv_ = nullptr;
  lv_timer_t* flashTimer_ = nullptr;
  uint8_t flashCount_ = 0;
};
```

- [ ] **Step 4: Write the app implementation**

Create `src/apps/pomodoro/PomodoroApp.cpp`:

```cpp
// src/apps/pomodoro/PomodoroApp.cpp — see PomodoroApp.h.
#include "apps/pomodoro/PomodoroApp.h"

#include <Arduino.h>

#include <cstdio>

#include "services/StorageService.h"

namespace {

constexpr char kArtWork[] = "S:/art/pomo/work.bin";
constexpr char kArtBreak[] = "S:/art/pomo/break.bin";

uint16_t clampU16(int v, int lo, int hi) {
  if (v < lo) return static_cast<uint16_t>(lo);
  if (v > hi) return static_cast<uint16_t>(hi);
  return static_cast<uint16_t>(v);
}

}  // namespace

void PomodoroApp::setDeps(ISettingsStore& store, StorageService& storage) {
  store_ = &store;
  storage_ = &storage;
  PomoConfig c;
  c.work_min = clampU16(static_cast<int>(store.getU32("pomo_work_min", 25)), 5, 60);
  c.break_min = clampU16(static_cast<int>(store.getU32("pomo_break_min", 5)), 1, 15);
  timer_.configure(c);
}

void PomodoroApp::saveConfig() {
  store_->setU32("pomo_work_min", timer_.config().work_min);
  store_->setU32("pomo_break_min", timer_.config().break_min);
}

void PomodoroApp::buildUI(lv_obj_t* parent) {
  lv_obj_set_style_pad_all(parent, 0, 0);
  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  stepperBtns_.clear();

  // Art slot: SD image when the file exists, flat colored box otherwise
  // (roadmap §4.1 placeholder rule). Both children exist; applyPhaseArt()
  // toggles visibility per phase.
  lv_obj_t* slot = lv_obj_create(parent);
  lv_obj_remove_style_all(slot);
  lv_obj_set_size(slot, 120, 120);
  lv_obj_align(slot, LV_ALIGN_TOP_MID, 0, 6);
  box_ = lv_obj_create(slot);
  lv_obj_remove_style_all(box_);
  lv_obj_set_size(box_, 120, 120);
  lv_obj_set_style_bg_opa(box_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box_, 10, 0);
  boxLbl_ = lv_label_create(box_);
  lv_obj_center(boxLbl_);
  img_ = lv_img_create(slot);
  lv_obj_center(img_);

  countdown_ = lv_label_create(parent);
  lv_obj_set_style_text_font(countdown_, &lv_font_montserrat_48, 0);
  lv_obj_align(countdown_, LV_ALIGN_TOP_MID, 0, 132);

  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 150, 44);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 186);
  btnLbl_ = lv_label_create(btn);
  lv_obj_center(btnLbl_);
  lv_obj_add_event_cb(btn, onStartStop, LV_EVENT_CLICKED, this);

  buildStepperRow(parent, 238, "Trabalho", &workVal_, onWorkMinus, onWorkPlus);
  buildStepperRow(parent, 264, "Pausa", &breakVal_, onBreakMinus, onBreakPlus);

  syncAll(millis());
  lastLblMs_ = millis();
}

void PomodoroApp::buildStepperRow(lv_obj_t* parent, lv_coord_t y,
                                  const char* name, lv_obj_t** valLbl,
                                  lv_event_cb_t minusCb, lv_event_cb_t plusCb) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, name);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 8, y + 6);

  lv_obj_t* minus = lv_btn_create(parent);
  lv_obj_set_size(minus, 32, 24);
  lv_obj_align(minus, LV_ALIGN_TOP_RIGHT, -88, y);
  lv_obj_t* ml = lv_label_create(minus);
  lv_label_set_text(ml, "-");
  lv_obj_center(ml);
  lv_obj_add_event_cb(minus, minusCb, LV_EVENT_CLICKED, this);

  *valLbl = lv_label_create(parent);
  lv_obj_set_width(*valLbl, 44);
  lv_obj_set_style_text_align(*valLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(*valLbl, LV_ALIGN_TOP_RIGHT, -42, y + 6);

  lv_obj_t* plus = lv_btn_create(parent);
  lv_obj_set_size(plus, 32, 24);
  lv_obj_align(plus, LV_ALIGN_TOP_RIGHT, -8, y);
  lv_obj_t* pl = lv_label_create(plus);
  lv_label_set_text(pl, "+");
  lv_obj_center(pl);
  lv_obj_add_event_cb(plus, plusCb, LV_EVENT_CLICKED, this);

  stepperBtns_.push_back(minus);
  stepperBtns_.push_back(plus);
}

void PomodoroApp::syncAll(uint32_t now_ms) {
  shownPhase_ = timer_.phase(now_ms);
  applyPhaseArt(shownPhase_);
  updateCountdown(now_ms);
  lv_label_set_text(btnLbl_, timer_.running() ? "Parar" : "Iniciar");
  char buf[12];
  snprintf(buf, sizeof buf, "%u min", timer_.config().work_min);
  lv_label_set_text(workVal_, buf);
  snprintf(buf, sizeof buf, "%u min", timer_.config().break_min);
  lv_label_set_text(breakVal_, buf);
  for (lv_obj_t* b : stepperBtns_) {
    if (timer_.running()) lv_obj_add_state(b, LV_STATE_DISABLED);
    else lv_obj_clear_state(b, LV_STATE_DISABLED);
  }
}

void PomodoroApp::updateCountdown(uint32_t now_ms) {
  const uint32_t ms =
      timer_.running() ? timer_.remainingMs(now_ms)
                       : static_cast<uint32_t>(timer_.config().work_min) * 60000u;
  const uint32_t totalS = (ms + 999) / 1000;
  char buf[8];
  snprintf(buf, sizeof buf, "%02u:%02u", static_cast<unsigned>(totalS / 60),
           static_cast<unsigned>(totalS % 60));
  lv_label_set_text(countdown_, buf);
}

void PomodoroApp::applyPhaseArt(PomoPhase p) {
  // Idle shows the work art dimmed (placeholder: grey box).
  const char* path = (p == PomoPhase::Break) ? kArtBreak : kArtWork;
  if (storage_->exists(path + 2)) {  // "S:" stripped for StorageService
    lv_img_set_src(img_, path);
    lv_obj_set_style_img_opa(img_, p == PomoPhase::Idle ? LV_OPA_40 : LV_OPA_COVER, 0);
    lv_obj_clear_flag(img_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(box_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(img_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(box_, LV_OBJ_FLAG_HIDDEN);
    lv_color_t c = (p == PomoPhase::Work)    ? lv_palette_main(LV_PALETTE_RED)
                   : (p == PomoPhase::Break) ? lv_palette_main(LV_PALETTE_GREEN)
                                             : lv_palette_main(LV_PALETTE_GREY);
    lv_obj_set_style_bg_color(box_, c, 0);
    lv_label_set_text(boxLbl_, (p == PomoPhase::Work)    ? "Trabalho"
                               : (p == PomoPhase::Break) ? "Pausa"
                                                         : "Pronto");
  }
}

void PomodoroApp::tick(uint32_t now_ms) {
  const PomoPhase p = timer_.phase(now_ms);
  if (p != shownPhase_) {
    // Work<->Break flip while watching gets the flash; start/stop via the
    // button already re-rendered and shouldn't blink.
    const bool flipped = (p != PomoPhase::Idle) && (shownPhase_ != PomoPhase::Idle);
    syncAll(now_ms);
    if (flipped) flash();
  }
  if (now_ms - lastLblMs_ >= 250) {
    lastLblMs_ = now_ms;
    updateCountdown(now_ms);
  }
}

void PomodoroApp::onExit() {
  stopFlash();
  box_ = boxLbl_ = img_ = countdown_ = btnLbl_ = workVal_ = breakVal_ = nullptr;
  stepperBtns_.clear();
  // timer_ deliberately untouched: it keeps counting while the app is closed.
}

void PomodoroApp::adjust(int dWork, int dBreak) {
  if (timer_.running()) return;  // steppers are disabled, belt and braces
  PomoConfig c = timer_.config();
  c.work_min = clampU16(static_cast<int>(c.work_min) + dWork, 5, 60);
  c.break_min = clampU16(static_cast<int>(c.break_min) + dBreak, 1, 15);
  timer_.configure(c);
  saveConfig();
  syncAll(millis());
}

void PomodoroApp::flash() {
  if (flashTimer_ != nullptr) return;
  flashOv_ = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(flashOv_);
  lv_obj_set_size(flashOv_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(flashOv_, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(flashOv_, LV_OPA_COVER, 0);
  flashCount_ = 0;
  flashTimer_ = lv_timer_create(onFlashTimer, 120, this);
}

void PomodoroApp::stopFlash() {
  if (flashTimer_ != nullptr) {
    lv_timer_del(flashTimer_);
    flashTimer_ = nullptr;
  }
  if (flashOv_ != nullptr) {
    lv_obj_del(flashOv_);
    flashOv_ = nullptr;
  }
}

void PomodoroApp::onStartStop(lv_event_t* e) {
  auto* self = static_cast<PomodoroApp*>(lv_event_get_user_data(e));
  if (self->timer_.running()) self->timer_.stop();
  else self->timer_.start(millis());
  self->syncAll(millis());
}

void PomodoroApp::onWorkMinus(lv_event_t* e) {
  static_cast<PomodoroApp*>(lv_event_get_user_data(e))->adjust(-5, 0);
}
void PomodoroApp::onWorkPlus(lv_event_t* e) {
  static_cast<PomodoroApp*>(lv_event_get_user_data(e))->adjust(5, 0);
}
void PomodoroApp::onBreakMinus(lv_event_t* e) {
  static_cast<PomodoroApp*>(lv_event_get_user_data(e))->adjust(0, -1);
}
void PomodoroApp::onBreakPlus(lv_event_t* e) {
  static_cast<PomodoroApp*>(lv_event_get_user_data(e))->adjust(0, 1);
}

void PomodoroApp::onFlashTimer(lv_timer_t* t) {
  auto* self = static_cast<PomodoroApp*>(t->user_data);
  ++self->flashCount_;
  if (self->flashCount_ >= 6) {  // 3 blinks
    self->stopFlash();
    return;
  }
  if (lv_obj_has_flag(self->flashOv_, LV_OBJ_FLAG_HIDDEN))
    lv_obj_clear_flag(self->flashOv_, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(self->flashOv_, LV_OBJ_FLAG_HIDDEN);
}
```

- [ ] **Step 5: Verify the device build compiles**

Run: `pio run -e cyd`
Expected: SUCCESS. Note the flash-usage delta in the output — montserrat_48 adds tens of KB; anything under ~100 KB growth is fine on `huge_app.csv`.

(The new app isn't registered yet, so `pio run` proves compilation only; behavior lands in Task 3.)

- [ ] **Step 6: Commit**

```bash
git add src/apps/app_catalog.h include/lv_conf.h src/apps/pomodoro
git commit -m "feat: PomodoroApp UI — status art, 48px countdown, steppers, flash"
```

---

### Task 3: Register the app + launcher badge poll

**Files:**
- Modify: `src/main.cpp` (instance near the other `static XxxApp` lines ~44–48; registration in `setup()` after the pet block ~line 152; badge poll in the 1 Hz block in `loop()` ~line 236 and after `updatePetBadge()` in `setup()` ~line 215)

**Interfaces:**
- Consumes: Task 2's `PomodoroApp::setDeps(ISettingsStore&, StorageService&)` and `badgeOn(uint32_t)`; `Launcher::registerApp/setBadge`.
- Produces: the app on the home grid (before Settings), badge dot live while a timer runs.

- [ ] **Step 1: Wire main.cpp**

Add the include next to the other app includes:

```cpp
#include "apps/pomodoro/PomodoroApp.h"
```

Add the instance after `static PetApp petApp;`:

```cpp
static PomodoroApp pomodoroApp;
```

In `setup()`, after `launcher.registerApp(&petApp);` and **before** the `settingsApp` lines (Settings stays last):

```cpp
pomodoroApp.setDeps(settings, storage);
launcher.registerApp(&pomodoroApp);
```

In `setup()`, right after `updatePetBadge();`:

```cpp
launcher.setBadge("pomodoro", pomodoroApp.badgeOn(millis()));  // Idle on boot
```

In `loop()`, inside the once-per-second block right after `updatePetBadge();`:

```cpp
launcher.setBadge("pomodoro", pomodoroApp.badgeOn(millis()));
```

- [ ] **Step 2: Build**

Run: `pio run -e cyd`
Expected: SUCCESS.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: register Pomodoro app + launcher badge poll"
```

---

### Task 4: README art TODOs + on-device verification

**Files:**
- Modify: `README.md` (append to the `## TODO` section)

**Interfaces:**
- Consumes: nothing new.
- Produces: art work-items for the user; a verified app.

- [ ] **Step 1: Append to the README `## TODO` list**

Add after the pet-art block (keep the existing checkbox style):

```markdown
- [ ] draw the images for the pomodoro app — 2 sprites + 1 icon. Same workflow
      as weather/pet: `assets/icons/svg_to_lvgl_bin.py <png> sd/art/pomo/<name>.bin`,
      then copy `sd/` onto the card. The app shows colored placeholder boxes
      (red = trabalho, green = pausa) until the files exist.
  - Status sprites (rendered 120×120 in `src/apps/pomodoro/PomodoroApp.cpp`):
    - [ ] `sd/art/pomo/work.bin` — work-phase sprite
    - [ ] `sd/art/pomo/break.bin` — break-phase sprite
  - Launcher icon (wire in `app_catalog.h` like oracle's once drawn):
    - [ ] `sd/art/icons/pomodoro.bin`
```

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: pomodoro sprite TODOs"
```

- [ ] **Step 3: Flash and verify on device**

Flash: `pio run -e cyd -t upload` (CYD on `/dev/ttyUSB0`; free the serial port first if a reader is attached).

Checklist (all must pass):
1. Launcher shows "Pomodoro" (colored-letter fallback icon) before Settings.
2. Open app: grey "Pronto" box, `25:00`, "Iniciar", steppers enabled.
3. Steppers clamp (work 5–60 step 5, break 1–15 step 1); values survive a reboot.
4. Start: box turns red "Trabalho", countdown runs, button reads "Parar", steppers disabled.
5. Leave the app: red badge dot on the launcher icon; clock/status bar still ticks.
6. Re-enter: countdown shows the correct remaining time.
7. Set work=5/break=1 for a fast test: at the flip the screen flashes ~3× and the box turns green "Pausa"; cycle continues Work↔Break.
8. "Parar" returns to Idle; badge clears within ~1 s of leaving.
9. Reboot mid-timer: app is Idle, no badge (by design).

- [ ] **Step 4: Record the result**

Commit any fixes found during verification, then:

```bash
git commit --allow-empty -m "test: Pomodoro on-device verification passed"
```
