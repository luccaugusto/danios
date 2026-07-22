#include "apps/weather/WeatherApp.h"

#include <Arduino.h>

#include <cmath>
#include <cstring>

#include <weather_model.h>

#include "apps/weather/WeatherFetch.h"
#include "core/Layout.h"
#include "services/StorageService.h"
#include "services/WiFiService.h"

namespace {
constexpr uint32_t kRefreshMs = 20u * 60u * 1000u;  // spec: ~15-30 min timer
constexpr uint32_t kFirstFetchDelayMs = 400;  // let the cached frame paint

// Base character sprite; outfit + accessory stack on top of her.
const char* kCharacterPath = "S:/art/weather/gata-coco.bin";

const char* kDayNames[3] = {"Hoje", "Amanhã", "Depois"};

// One art slot: the SD image when present, else a flat colored box (roadmap
// §4.1 placeholder rule — hand-drawn art arrives incrementally). A nullptr
// path means the condition has no such slot: render nothing. hideIfMissing
// skips the placeholder box for slots that would cover other art (the
// full-canvas accessory overlay sits on top of the outfit).
lv_obj_t* makeArtSlot(lv_obj_t* parent, StorageService& storage,
                      const char* lvglPath, lv_coord_t w, lv_coord_t h,
                      lv_color_t fallback, bool hideIfMissing = false) {
  lv_obj_t* img = lv_img_create(parent);
  lv_obj_set_size(img, w, h);
  if (lvglPath != nullptr && storage.exists(lvglPath + 2)) {  // "S:/x" -> "/x"
    lv_img_set_src(img, lvglPath);
  } else if (lvglPath != nullptr && !hideIfMissing) {
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

  if (layout::kLandscape) {
    renderLandscape(f, stale);
    return;
  }

  const bool useF = store_->getBool("units.f", false);
  const Condition cond = conditionFromWmo(f.current.wmoCode);
  const ArtSlots art =
      artSlots(tempBand(f.current.tempC), cond, f.current.isDay);

  // Background fills the whole 240x288 app container.
  lv_obj_t* bg = makeArtSlot(root_, *storage_, art.background, 240, 288,
                             lv_color_white());
  lv_obj_set_pos(bg, 0, 0);
  lv_obj_set_style_radius(bg, 0, 0);

  // Character: base sprite, outfit, and accessory overlay share one canvas,
  // exported pre-positioned relative to the character — all three stack at
  // the same anchor and no per-sprite offsets live in code. The 198x234 PNGs
  // are converted at 95% (188x222); the y offset keeps her feet on the
  // ground line the backgrounds draw at the old canvas bottom (27 = tuned
  // on device 2026-07-22, +15 from the original 12).
  lv_obj_t* character = makeArtSlot(root_, *storage_, kCharacterPath, 188, 222,
                                    lv_palette_main(LV_PALETTE_GREY));
  lv_obj_align(character, LV_ALIGN_TOP_MID, 0, 27);
  lv_obj_t* outfit =
      makeArtSlot(root_, *storage_, art.outfit, 188, 222,
                  lv_palette_main(LV_PALETTE_GREY), /*hideIfMissing=*/true);
  lv_obj_align(outfit, LV_ALIGN_TOP_MID, 0, 27);
  lv_obj_t* overlay =
      makeArtSlot(root_, *storage_, art.overlay, 188, 222,
                  lv_palette_main(LV_PALETTE_ORANGE), /*hideIfMissing=*/true);
  lv_obj_align(overlay, LV_ALIGN_TOP_MID, 0, 27);

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
    lv_obj_align(hilo, LV_ALIGN_TOP_RIGHT, -8, 8);
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

// Landscape (spec 2026-07-17): split screen. Left half is a pure art panel —
// the portrait-drawn art center-cropped/zoomed, no text over it. Right half
// is a clean data panel on the app background.
void WeatherApp::renderLandscape(const ForecastWx& f, bool stale) {
  const bool useF = store_->getBool("units.f", false);
  const Condition cond = conditionFromWmo(f.current.wmoCode);
  const ArtSlots art =
      artSlots(tempBand(f.current.tempC), cond, f.current.isDay);

  // Maps "S:/art/weather/<file>.bin" -> "S:/art/weather/ls/<file>.bin" by
  // inserting "ls/" after the final '/', writing into a caller-provided
  // buffer. Passes nullptr through unchanged (art.outfit/art.overlay can be
  // nullptr — makeArtSlot treats nullptr as "no slot").
  auto lsPath = [](const char* path, char* buf, size_t bufSize) -> const char* {
    if (path == nullptr) return nullptr;
    const char* slash = strrchr(path, '/');
    if (slash == nullptr) {
      snprintf(buf, bufSize, "%s", path);
      return buf;
    }
    const int prefixLen = static_cast<int>(slash - path) + 1;  // include '/'
    snprintf(buf, bufSize, "%.*sls/%s", prefixLen, path, slash + 1);
    return buf;
  };

  const lv_coord_t kArtW = layout::kAppW / 2;  // 160
  const lv_coord_t kArtH = layout::kAppH;      // 208

  // Art panel clips its children, so the oversized portrait art is
  // center-cropped by parking it at negative offsets.
  lv_obj_t* artPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(artPanel);
  lv_obj_set_pos(artPanel, 0, 0);
  lv_obj_set_size(artPanel, kArtW, kArtH);
  lv_obj_clear_flag(artPanel, LV_OBJ_FLAG_SCROLLABLE);

  char bgBuf[64];
  lv_obj_t* bg = makeArtSlot(artPanel, *storage_,
                             lsPath(art.background, bgBuf, sizeof bgBuf), 216,
                             259, lv_color_white());
  lv_obj_set_pos(bg, (kArtW - 216) / 2, (kArtH - 259) / 2);  // center-crop
  lv_obj_set_style_radius(bg, 0, 0);

  // Art is pre-scaled to 90% for landscape (ls/ variants) because LVGL 8
  // can't zoom file-backed images: the FS decoder reads the file
  // line-by-line, but a transformed draw needs the whole image available at
  // once, so a zoomed file-backed image silently renders nothing. The
  // 169x200 art (90% of the 188x222 portrait render) fits the 208-high
  // panel with ~5 px side clip.
  const struct {
    const char* path;
    lv_color_t fallback;
    bool hideIfMissing;
  } kLayers[] = {
      {kCharacterPath, lv_palette_main(LV_PALETTE_GREY), false},
      {art.outfit, lv_palette_main(LV_PALETTE_GREY), true},
      {art.overlay, lv_palette_main(LV_PALETTE_ORANGE), true},
  };
  for (const auto& layer : kLayers) {
    char pathBuf[64];
    const char* path = lsPath(layer.path, pathBuf, sizeof pathBuf);
    lv_obj_t* img = makeArtSlot(artPanel, *storage_, path, 169, 200,
                                layer.fallback, layer.hideIfMissing);
    lv_obj_align(img, LV_ALIGN_BOTTOM_MID, 0, 0);
  }

  // Data panel: flex column of plain labels on the dark app background —
  // no white readout pills needed, nothing sits over art here.
  lv_obj_t* dataPanel = lv_obj_create(root_);
  lv_obj_remove_style_all(dataPanel);
  lv_obj_set_pos(dataPanel, kArtW, 0);
  lv_obj_set_size(dataPanel, layout::kAppW - kArtW, kArtH);
  lv_obj_set_style_pad_all(dataPanel, 8, 0);
  lv_obj_set_style_pad_row(dataPanel, 4, 0);
  lv_obj_set_flex_flow(dataPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(dataPanel, LV_OBJ_FLAG_SCROLLABLE);

  auto addLine = [dataPanel](const char* txt) {
    lv_obj_t* l = lv_label_create(dataPanel);
    lv_label_set_text(l, txt);
    return l;
  };

  char buf[80];
  snprintf(buf, sizeof buf, "%d°%c  %s", shownTemp(f.current.tempC, useF),
           useF ? 'F' : 'C', conditionLabelPt(cond));
  addLine(buf);
  addLine(store_->getString("loc.city", "").c_str());
  addLine("");  // spacer row between readings and the forecast list
  for (int i = 0; i < f.dayCount; ++i) {
    snprintf(buf, sizeof buf, "%s\n" LV_SYMBOL_UP "%d°  " LV_SYMBOL_DOWN "%d°",
             kDayNames[i], shownTemp(f.days[i].tmaxC, useF),
             shownTemp(f.days[i].tminC, useF));
    addLine(buf);
  }

  if (stale) setStatus(LV_SYMBOL_WARNING " desatualizado");
}

void WeatherApp::renderEmpty() {
  lv_obj_clean(root_);
  statusLbl_ = nullptr;
  lv_obj_t* lbl = lv_label_create(root_);
  lv_label_set_text(lbl,
                    "Não consegui ver o céu agora.\n\n"
                    "Verifique o WiFi em Config.");
  lv_obj_set_width(lbl, layout::kAppW - 16);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
}

void WeatherApp::setStatus(const char* msg) {
  if (root_ == nullptr) return;
  if (statusLbl_ == nullptr) {
    statusLbl_ = makeReadout(root_);
    // Portrait: below the hi/lo readout (top-right corner). Landscape: the
    // data panel's bottom-right, which the forecast list never reaches.
    if (layout::kLandscape)
      lv_obj_align(statusLbl_, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    else
      lv_obj_align(statusLbl_, LV_ALIGN_TOP_RIGHT, -8, 30);
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
