#include "apps/pet/PetApp.h"

#include <date_utils.h>  // dateKey() in todayKey(); use Task 0's form

#include "services/StorageService.h"
#include "services/TimeService.h"

namespace {
// Food choice carried from a tray button's callback to onFoodChosen; owned by
// the button and freed on its LV_EVENT_DELETE (same pattern as WifiSection).
struct FoodCtx {
  PetApp* self;
  Food food;
  lv_obj_t* modal;
};

lv_color_t needColor(NeedLevel level) {
  switch (level) {
    case NeedLevel::Great:     return lv_palette_main(LV_PALETTE_GREEN);
    case NeedLevel::Okay:      return lv_palette_main(LV_PALETTE_LIGHT_GREEN);
    case NeedLevel::Neglected: return lv_palette_main(LV_PALETTE_ORANGE);
    default:                   return lv_palette_main(LV_PALETTE_RED);
  }
}

// One art slot: the SD image when present, else a flat colored placeholder box
// (roadmap §4.1 placeholder rule — pet art is hand-drawn later, and the pet
// stays fully interactive with no SD, spec §6.5). `lvglPath` is an "S:/..."
// path; StorageService::exists() wants the bare path (skip the "S:").
lv_obj_t* makePetArt(lv_obj_t* parent, StorageService& storage,
                     const char* lvglPath, lv_coord_t w, lv_coord_t h,
                     lv_color_t fallback) {
  lv_obj_t* img = lv_img_create(parent);
  lv_obj_set_size(img, w, h);
  if (lvglPath != nullptr && storage.exists(lvglPath + 2)) {
    lv_img_set_src(img, lvglPath);
  } else {
    lv_obj_set_style_bg_color(img, fallback, 0);
    lv_obj_set_style_bg_opa(img, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(img, 10, 0);
  }
  return img;
}

void addNeedRow(lv_obj_t* parent, const char* label, NeedLevel level) {
  lv_obj_t* row = lv_label_create(parent);
  lv_label_set_text_fmt(row, "%s: %s", label, needLevelLabelPt(level));
  lv_obj_set_style_text_color(row, needColor(level), 0);
  lv_obj_set_width(row, LV_PCT(46));
}

void addActionButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb,
                     void* userData) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
}

void bounceAnimCb(void* obj, int32_t v) {
  lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(obj), v, 0);
}
}  // namespace

void PetApp::setDeps(ISettingsStore& store, TimeService& time,
                     StorageService& storage) {
  store_ = &store;
  time_ = &time;
  storage_ = &storage;
}

uint32_t PetApp::todayKey() const {
  return time_->isTimeKnown() ? dateKey(time_->today()) : 0u;
}
int PetApp::nowMinutes() const {
  return time_->isTimeKnown() ? time_->minutesSinceMidnight() : -1;
}
void PetApp::save() { savePet(*store_, st_); }

void PetApp::onEnter() {
  st_ = loadPet(*store_);
  misbehavesThisVisit_ = false;
  scoldedThisVisit_ = false;
  const uint32_t today = todayKey();
  if (st_.alive && today != 0) {
    const bool died = petTick(st_, today, nowMinutes());
    if (!died) misbehavesThisVisit_ = onAppOpen(st_, today, nowMinutes());
    save();  // persist the tick/ritual (dawn award, death, mess, care)
  }
}

void PetApp::buildUI(lv_obj_t* parent) {
  root_ = parent;
  render();
}

void PetApp::onExit() {
  if (toastTimer_) {
    lv_timer_del(toastTimer_);
    toastTimer_ = nullptr;
  }
  if (toastLbl_) {
    lv_obj_del(toastLbl_);
    toastLbl_ = nullptr;
  }
  if (modal_) {
    lv_obj_del(modal_);  // orphaned naming/food modal (launcher never cleans lv_layer_top())
    modal_ = nullptr;
  }
  // Misbehaved this visit and never scolded -> missed the window (spec).
  if (misbehavesThisVisit_ && !scoldedThisVisit_ && st_.alive && todayKey() != 0) {
    petTick(st_, todayKey(), nowMinutes());  // re-sync before penalizing/saving
    if (st_.alive) {
      scoldPenalty(st_);
      save();
    }
  }
  petImg_ = nullptr;
}

void PetApp::render() {
  lv_obj_clean(root_);  // drop the previous screen's widgets
  petImg_ = nullptr;
  switch (currentScreen(st_)) {
    case Screen::Egg:      buildEgg(); break;
    case Screen::Memorial: buildMemorial(); break;
    default:               buildAlive(); break;
  }
}

void PetApp::buildEgg() {
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root_, 10, 0);

  lv_obj_t* title = lv_label_create(root_);
  lv_label_set_text(title, "Um ovo apareceu!");

  makePetArt(root_, *storage_, stageSprite(Stage::Egg), 120, 120,
             lv_palette_lighten(LV_PALETTE_GREY, 2));

  if (time_->isTimeKnown()) {
    addActionButton(root_, "Chocar", onHatchClicked, this);
  } else {
    lv_obj_t* hint = lv_label_create(root_);
    lv_label_set_text(hint,
                      "Preciso saber a data para\nnascer! Configure o relógio\n"
                      "em Config.");
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
  }
}

void PetApp::buildAlive() {
  const uint32_t today = todayKey();
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root_, 4, 0);
  lv_obj_set_style_pad_top(root_, 6, 0);

  lv_obj_t* nameLbl = lv_label_create(root_);
  lv_label_set_text(nameLbl, st_.name.c_str());

  petImg_ = makePetArt(root_, *storage_, stageSprite(growthStage(st_, today)),
                       104, 104, lv_palette_lighten(LV_PALETTE_BLUE, 2));

  // Mess sprites: tap one to clean (spec: uncleaned messes stack, cap 3).
  if (st_.mess > 0) {
    lv_obj_t* messRow = lv_obj_create(root_);
    lv_obj_remove_style_all(messRow);
    lv_obj_set_size(messRow, LV_PCT(100), 34);
    lv_obj_set_flex_flow(messRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(messRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < st_.mess; ++i) {
      lv_obj_t* m = makePetArt(messRow, *storage_, "S:/art/pet/mess.bin", 30, 30,
                               lv_palette_darken(LV_PALETTE_BROWN, 2));
      lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(m, onCleanClicked, LV_EVENT_CLICKED, this);
    }
  }

  lv_obj_t* health = lv_label_create(root_);
  const Health h = healthOf(st_, today);
  lv_label_set_text(health, healthLabelPt(h));
  lv_obj_set_style_text_color(
      health,
      h == Health::Healthy ? lv_palette_main(LV_PALETTE_GREEN)
                           : lv_palette_main(LV_PALETTE_RED),
      0);

  lv_obj_t* grid = lv_obj_create(root_);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  addNeedRow(grid, "Fome", hungerLevel(st_, today));
  addNeedRow(grid, "Alegria", happyLevel(st_, today));
  addNeedRow(grid, "Higiene", hygieneLevel(st_, today));
  addNeedRow(grid, "Energia", energyLevel(st_, today));

  if (!time_->isTimeKnown()) {
    lv_obj_t* hint = lv_label_create(root_);
    lv_label_set_text(hint, "Sem data - configure o relógio");
    lv_obj_set_style_text_color(hint, lv_palette_main(LV_PALETTE_RED), 0);
  }

  lv_obj_t* btnRow = lv_obj_create(root_);
  lv_obj_remove_style_all(btnRow);
  lv_obj_set_size(btnRow, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(btnRow, 4, 0);
  addActionButton(btnRow, "Alimentar", onFeedClicked, this);
  addActionButton(btnRow, "Brincar", onPlayClicked, this);
  if (misbehavesThisVisit_ && !scoldedThisVisit_) {
    addActionButton(btnRow, "Dar bronca", onScoldClicked, this);
    bounce();  // visible "tantrum" cue on a misbehaving open (spec)
  }
}

void PetApp::buildMemorial() {
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root_, 12, 0);

  lv_obj_t* msg = lv_label_create(root_);
  lv_label_set_text_fmt(msg, "%s virou uma\nestrelinha no céu.", st_.name.c_str());
  lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* sub = lv_label_create(root_);
  lv_label_set_text(sub, "Cuide bem do próximo ovo!");
  lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);

  addActionButton(root_, "Adeus", onAdeusClicked, this);
}

// --- Flows -----------------------------------------------------------------

void PetApp::askName() {
  if (modal_) return;  // already open: don't orphan the first modal
  lv_obj_t* modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* prompt = lv_label_create(modal);
  lv_label_set_text(prompt, "Qual é o nome do bichinho?");

  lv_obj_t* ta = lv_textarea_create(modal);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_max_length(ta, 12);
  lv_textarea_set_placeholder_text(ta, "Bichinho");
  lv_obj_set_width(ta, LV_PCT(100));

  lv_obj_t* kb = lv_keyboard_create(modal);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_add_event_cb(kb, onNameKeyboard, LV_EVENT_READY, this);
  lv_obj_add_event_cb(kb, onNameKeyboard, LV_EVENT_CANCEL, this);

  modal_ = modal;
}

void PetApp::doHatch(const std::string& name) {
  if (!hatch(st_, name, todayKey())) return;  // clock vanished mid-flow
  misbehavesThisVisit_ = false;  // never a tantrum on the hatch day itself
  scoldedThisVisit_ = false;
  save();
  render();  // -> Alive
}

void PetApp::openFoodTray() {
  if (modal_) return;  // already open: don't orphan the first modal
  lv_obj_t* modal = lv_obj_create(lv_layer_top());
  lv_obj_set_size(modal, 210, 250);
  lv_obj_center(modal);
  lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(modal, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* title = lv_label_create(modal);
  lv_label_set_text(title, "O que servir?");

  const Food foods[3] = {Food::Snack, Food::Meal, Food::Treat};
  for (Food f : foods) {
    lv_obj_t* btn = lv_btn_create(modal);
    lv_obj_set_width(btn, LV_PCT(88));
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, foodLabelPt(f));
    lv_obj_center(lbl);
    FoodCtx* ctx = new FoodCtx{this, f, modal};
    lv_obj_add_event_cb(btn, onFoodChosen, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(
        btn, [](lv_event_t* ev) { delete static_cast<FoodCtx*>(lv_event_get_user_data(ev)); },
        LV_EVENT_DELETE, ctx);
  }

  modal_ = modal;
}

void PetApp::doFeed(Food food) {
  if (petTick(st_, todayKey(), nowMinutes())) {  // re-sync; may report death
    save();
    render();  // -> Memorial
    return;
  }
  feed(st_, food, todayKey(), nowMinutes());
  save();
  render();
  bounce();
  toast("Nham nham!");
}

void PetApp::doPlay() {
  if (petTick(st_, todayKey(), nowMinutes())) {  // re-sync; may report death
    save();
    render();  // -> Memorial
    return;
  }
  play(st_, todayKey(), nowMinutes());
  save();
  render();
  bounce();
  toast("Uhuu!");
}

void PetApp::doClean() {
  if (petTick(st_, todayKey(), nowMinutes())) {  // re-sync; may report death
    save();
    render();  // -> Memorial
    return;
  }
  clean(st_, todayKey(), nowMinutes());
  save();
  render();
  toast("Limpou!");
}

void PetApp::doScold() {
  scoldReward(st_);
  scoldedThisVisit_ = true;
  save();
  render();  // drops the "Dar bronca" button for the rest of the visit
  toast("*snif* Desculpa...");
}

void PetApp::doRebirth() {
  rebirth(st_);
  save();
  render();  // -> Egg
}

void PetApp::toast(const char* msg) {
  // Replace any in-flight toast + its pending timer so they can't dismiss the
  // new one early (rapid taps) or outlive this visit.
  if (toastTimer_) {
    lv_timer_del(toastTimer_);
    toastTimer_ = nullptr;
  }
  if (toastLbl_) lv_obj_del(toastLbl_);
  toastLbl_ = lv_label_create(lv_layer_top());
  lv_label_set_text(toastLbl_, msg);
  lv_obj_set_style_bg_color(toastLbl_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(toastLbl_, LV_OPA_70, 0);
  lv_obj_set_style_text_color(toastLbl_, lv_color_white(), 0);
  lv_obj_set_style_pad_all(toastLbl_, 8, 0);
  lv_obj_set_style_radius(toastLbl_, 6, 0);
  lv_obj_align(toastLbl_, LV_ALIGN_BOTTOM_MID, 0, -60);
  toastTimer_ = lv_timer_create(onToastTimer, 1200, this);
  lv_timer_set_repeat_count(toastTimer_, 1);  // one-shot: LVGL frees it after it fires
}

void PetApp::bounce() {
  if (!petImg_) return;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, petImg_);  // deleting petImg_ (next render) also kills this anim
  lv_anim_set_exec_cb(&a, bounceAnimCb);
  lv_anim_set_values(&a, 0, -16);
  lv_anim_set_time(&a, 120);
  lv_anim_set_playback_time(&a, 120);
  lv_anim_start(&a);
}

// --- Static LVGL callbacks -------------------------------------------------

void PetApp::onHatchClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->askName();
}

void PetApp::onNameKeyboard(lv_event_t* e) {
  auto* self = static_cast<PetApp*>(lv_event_get_user_data(e));
  lv_obj_t* kb = lv_event_get_current_target(e);
  if (lv_event_get_code(e) == LV_EVENT_READY) {
    const char* txt = lv_textarea_get_text(lv_keyboard_get_textarea(kb));
    self->doHatch((txt != nullptr && txt[0] != '\0') ? std::string(txt)
                                                      : std::string("Bichinho"));
  }
  // READY or CANCEL: tear the modal down async (never delete an ancestor of
  // the object whose event is mid-dispatch).
  lv_obj_del_async(lv_obj_get_parent(kb));
  self->modal_ = nullptr;
}

void PetApp::onFeedClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->openFoodTray();
}
void PetApp::onPlayClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->doPlay();
}
void PetApp::onCleanClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->doClean();
}
void PetApp::onScoldClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->doScold();
}
void PetApp::onAdeusClicked(lv_event_t* e) {
  static_cast<PetApp*>(lv_event_get_user_data(e))->doRebirth();
}

void PetApp::onFoodChosen(lv_event_t* e) {
  auto* ctx = static_cast<FoodCtx*>(lv_event_get_user_data(e));
  PetApp* self = ctx->self;
  const Food food = ctx->food;
  lv_obj_del_async(ctx->modal);  // close the tray (frees ctx via LV_EVENT_DELETE)
  self->modal_ = nullptr;
  self->doFeed(food);
}

void PetApp::onToastTimer(lv_timer_t* t) {
  auto* self = static_cast<PetApp*>(t->user_data);
  if (self->toastLbl_) {
    lv_obj_del(self->toastLbl_);
    self->toastLbl_ = nullptr;
  }
  self->toastTimer_ = nullptr;  // LVGL auto-frees this one-shot after we return
}
