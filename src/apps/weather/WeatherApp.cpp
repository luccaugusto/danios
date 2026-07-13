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

// White-backed label so readings stay legible over any background art
// (black text — the dark theme's white default vanishes on the white pill).
lv_obj_t* makeReadout(lv_obj_t* parent) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_obj_set_style_bg_color(lbl, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(lbl, LV_OPA_70, 0);
  lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
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
                             lv_color_white());
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
    lv_obj_set_style_text_color(day, lv_color_black(), 0);
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
  const bool online = wifi_->isConnected() || wifi_->connect(8000);
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
