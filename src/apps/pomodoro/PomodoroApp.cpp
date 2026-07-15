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
  lv_obj_clear_flag(flashOv_, LV_OBJ_FLAG_CLICKABLE);  // don't steal taps
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
