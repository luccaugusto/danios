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
  const bool has_frame = storage_->exists(kFrameSdPath);
  if (has_frame) {
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
  // Frame art has a white background; the placeholder container stays dark,
  // so the default light text is kept there.
  if (has_frame) lv_obj_set_style_text_color(label_, lv_color_black(), 0);
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, -20);

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
