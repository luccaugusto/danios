#include "core/Launcher.h"

#include <cctype>

#include "../services/LvglFs.h"

namespace {
constexpr lv_coord_t kCellW = 80;
constexpr lv_coord_t kCellH = 110;
constexpr lv_coord_t kIconSize = 64;
// Fallback icon colors, indexed by grid position (art arrives with F3).
constexpr uint32_t kIconColors[] = {0x4A90D9, 0x50B86C, 0xE0A030,
                                    0x9B59B6, 0xE05050, 0x6C7A89};
constexpr int kIconColorCount = 6;
}  // namespace

Launcher::Launcher(StatusBar& statusBar) : statusBar_(statusBar) {}

void Launcher::registerApp(App* app) {
  if (model_.registerApp(app->id(), /*inGrid=*/true) < 0) return;  // duplicate id: ignore
  apps_.push_back(app);
}

void Launcher::show() {
  if (!homeScreen_) buildHomeScreen();
  rebuildGrid();
  lv_scr_load(homeScreen_);
}

void Launcher::openApp(const char* id) {
  const int idx = model_.indexOf(id);
  if (idx < 0) return;
  if (active_) goHome();  // defensive: close whatever is open first
  App* app = apps_[static_cast<size_t>(idx)];
  if (!model_.canOpen(id)) {
    showDisabledHint(app->title());
    return;
  }
  // Pinned lifecycle: radioRequest → onEnter → buildUI(container).
  const RadioMode mode = app->requiredRadio();
  const bool granted = radioRequest_ ? radioRequest_(mode) : true;
  if (!granted) {
    lv_obj_t* mbox =
        lv_msgbox_create(NULL, app->title(), "Rádio indisponível no momento.", NULL, true);
    lv_obj_set_width(mbox, 220);
    lv_obj_center(mbox);
    return;
  }
  statusBar_.setRadio(mode);
  app->onEnter();
  buildAppScreen();
  lv_label_set_text(appTitleLabel_, app->title());
  app->buildUI(appContainer_);
  lv_scr_load(appScreen_);
  active_ = app;
}

void Launcher::goHome() {
  // Pinned lifecycle: onExit → delete container children → home → radio None.
  if (active_) {
    active_->onExit();
    lv_obj_clean(appContainer_);
    active_ = nullptr;
  }
  if (!homeScreen_) buildHomeScreen();
  lv_scr_load(homeScreen_);
  if (radioRequest_) radioRequest_(RadioMode::None);
  statusBar_.setRadio(RadioMode::None);
}

void Launcher::setBadge(const char* appId, bool on) {
  if (!model_.setBadge(appId, on)) return;
  const int g = model_.gridIndexOf(appId);
  if (g < 0 || static_cast<size_t>(g) >= badges_.size()) return;
  if (on) {
    lv_obj_clear_flag(badges_[static_cast<size_t>(g)], LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(badges_[static_cast<size_t>(g)], LV_OBJ_FLAG_HIDDEN);
  }
}

void Launcher::setAppEnabled(const char* appId, bool en) {
  if (!model_.setEnabled(appId, en)) return;
  const int g = model_.gridIndexOf(appId);
  if (g < 0 || static_cast<size_t>(g) >= cells_.size()) return;
  lv_obj_set_style_opa(cells_[static_cast<size_t>(g)], en ? LV_OPA_COVER : LV_OPA_40, 0);
}

void Launcher::setRadioRequest(std::function<bool(RadioMode)> fn) {
  radioRequest_ = std::move(fn);
}

void Launcher::tick(uint32_t now_ms) {
  if (active_) active_->tick(now_ms);
}

void Launcher::buildHomeScreen() {
  homeScreen_ = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(homeScreen_, lv_color_hex(0x101418), 0);
  lv_obj_set_style_text_color(homeScreen_, lv_color_white(), 0);

  statusBar_.build(homeScreen_);

  gridContainer_ = lv_obj_create(homeScreen_);
  lv_obj_remove_style_all(gridContainer_);
  lv_obj_set_pos(gridContainer_, 0, StatusBar::kHeight);
  lv_obj_set_size(gridContainer_, 240, 320 - StatusBar::kHeight);
  lv_obj_clear_flag(gridContainer_, LV_OBJ_FLAG_SCROLLABLE);
}

void Launcher::rebuildGrid() {
  lv_obj_clean(gridContainer_);
  cells_.clear();
  badges_.clear();
  iconCtxs_.clear();

  for (int g = 0; g < model_.gridCount(); ++g) {
    App* app = apps_[static_cast<size_t>(model_.indexOf(model_.idAtGrid(g)))];
    const GridSlot slot = model_.slotOf(g);

    lv_obj_t* cell = lv_obj_create(gridContainer_);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, kCellW, kCellH);
    lv_obj_set_pos(cell, slot.col * kCellW, slot.row * kCellH);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn = lv_btn_create(cell);
    lv_obj_set_size(btn, kIconSize, kIconSize);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 8);
    // Badge overflows the button; don't clip it.
    lv_obj_add_flag(btn, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    const char* icon = app->iconPath();
    if (icon && !lvglFsExists(icon)) icon = nullptr;  // missing art -> fallback tile
    if (icon != nullptr) {
      // F3+: hand-drawn icon from SD via the LVGL FS driver (drive 'S').
      lv_obj_t* img = lv_img_create(btn);
      lv_img_set_src(img, icon);
      lv_obj_center(img);
    } else {
      // Fallback: colored rounded box + first letter (art/SD arrive in F3).
      lv_obj_set_style_bg_color(btn, lv_color_hex(kIconColors[g % kIconColorCount]), 0);
      lv_obj_set_style_radius(btn, 12, 0);
      lv_obj_t* letter = lv_label_create(btn);
      lv_label_set_text_fmt(letter, "%c",
                            std::toupper(static_cast<unsigned char>(app->title()[0])));
      lv_obj_center(letter);
    }
    auto ctx = std::make_unique<IconCtx>(IconCtx{this, app->id()});
    lv_obj_add_event_cb(btn, onIconClicked, LV_EVENT_CLICKED, ctx.get());
    iconCtxs_.push_back(std::move(ctx));

    lv_obj_t* title = lv_label_create(cell);
    lv_label_set_text(title, app->title());
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -8);

    lv_obj_t* badge = lv_obj_create(btn);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 12, 12);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, 4, -4);
    // Don't steal taps meant for the icon underneath it.
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);
    if (!model_.badgeAtGrid(g)) lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    badges_.push_back(badge);

    if (!model_.enabledAtGrid(g)) lv_obj_set_style_opa(cell, LV_OPA_40, 0);
    cells_.push_back(cell);
  }
}

void Launcher::buildAppScreen() {
  if (appScreen_) return;
  appScreen_ = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(appScreen_, lv_color_hex(0x101418), 0);
  lv_obj_set_style_text_color(appScreen_, lv_color_white(), 0);

  // Top bar with back arrow — provided by the Launcher; apps build below it.
  lv_obj_t* topBar = lv_obj_create(appScreen_);
  lv_obj_remove_style_all(topBar);
  lv_obj_set_size(topBar, 240, kTopBarH);
  lv_obj_set_pos(topBar, 0, 0);
  lv_obj_set_style_bg_color(topBar, lv_color_hex(0x1B2026), 0);
  lv_obj_set_style_bg_opa(topBar, LV_OPA_COVER, 0);
  lv_obj_clear_flag(topBar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* backBtn = lv_btn_create(topBar);
  lv_obj_set_size(backBtn, 40, 28);
  lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 2, 0);
  lv_obj_set_ext_click_area(backBtn, 6);
  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT);
  lv_obj_center(backLabel);
  lv_obj_add_event_cb(backBtn, onBackClicked, LV_EVENT_CLICKED, this);

  appTitleLabel_ = lv_label_create(topBar);
  lv_obj_align(appTitleLabel_, LV_ALIGN_CENTER, 0, 0);

  appContainer_ = lv_obj_create(appScreen_);
  lv_obj_remove_style_all(appContainer_);
  lv_obj_set_pos(appContainer_, 0, kTopBarH);
  lv_obj_set_size(appContainer_, 240, 320 - kTopBarH);
}

void Launcher::showDisabledHint(const char* title) {
  lv_obj_t* mbox = lv_msgbox_create(
      NULL, title, "Indisponível - insira o cartão SD e reinicie.", NULL, true);
  lv_obj_set_width(mbox, 220);
  lv_obj_center(mbox);
}

void Launcher::onIconClicked(lv_event_t* e) {
  auto* ctx = static_cast<IconCtx*>(lv_event_get_user_data(e));
  ctx->self->openApp(ctx->appId);
}

void Launcher::onBackClicked(lv_event_t* e) {
  static_cast<Launcher*>(lv_event_get_user_data(e))->goHome();
}
