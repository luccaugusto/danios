#include "apps/calculator/CalculatorApp.h"

#include <cstring>
#include <string>

namespace {

// 4 columns × 5 rows. × (U+00D7) and ÷ (U+00F7) exist in montserrat_pt_14
// (full Latin-1 range); the backspace glyph comes from its FontAwesome range.
// Minus must stay ASCII '-' — U+2212 is not in the font.
const char* kKeypadMap[] = {
    "C", LV_SYMBOL_BACKSPACE, "%", "÷", "\n",
    "7", "8", "9", "×", "\n",
    "4", "5", "6", "-", "\n",
    "1", "2", "3", "+", "\n",
    "()", "0", ".", "=", ""};

constexpr lv_coord_t kDisplayH = 56;  // readout strip; keypad fills the rest

}  // namespace

void CalculatorApp::buildUI(lv_obj_t* parent) {
  // Parent is the launcher's style-stripped 240×288 container below the top
  // bar (back arrow is the launcher's — none here).
  displayLabel_ = lv_label_create(parent);
  lv_label_set_long_mode(displayLabel_, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(displayLabel_, 240 - 16);
  lv_obj_set_style_text_align(displayLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(displayLabel_, LV_ALIGN_TOP_MID, 0, (kDisplayH - 16) / 2);

  lv_obj_t* pad = lv_btnmatrix_create(parent);
  lv_btnmatrix_set_map(pad, kKeypadMap);
  lv_obj_set_pos(pad, 0, kDisplayH);
  lv_obj_set_size(pad, 240, 288 - kDisplayH);  // 5 rows ≈ 46 px — good targets
  lv_obj_add_event_cb(pad, keyPressed, LV_EVENT_VALUE_CHANGED, this);

  refresh();
}

void CalculatorApp::keyPressed(lv_event_t* e) {
  auto* self = static_cast<CalculatorApp*>(lv_event_get_user_data(e));
  lv_obj_t* pad = lv_event_get_target(e);
  const uint16_t id = lv_btnmatrix_get_selected_btn(pad);
  if (id == LV_BTNMATRIX_BTN_NONE) return;
  self->handleKey(lv_btnmatrix_get_btn_text(pad, id));
}

void CalculatorApp::handleKey(const char* t) {
  if (t[0] >= '0' && t[0] <= '9' && t[1] == '\0') engine_.digit(t[0]);
  else if (std::strcmp(t, ".") == 0) engine_.dot();
  else if (std::strcmp(t, "+") == 0) engine_.op('+');
  else if (std::strcmp(t, "-") == 0) engine_.op('-');
  else if (std::strcmp(t, "×") == 0) engine_.op('*');
  else if (std::strcmp(t, "÷") == 0) engine_.op('/');
  else if (std::strcmp(t, "=") == 0) engine_.equals();
  else if (std::strcmp(t, "C") == 0) engine_.clear();
  else if (std::strcmp(t, LV_SYMBOL_BACKSPACE) == 0) engine_.backspace();
  else if (std::strcmp(t, "()") == 0) engine_.paren();
  else if (std::strcmp(t, "%") == 0) engine_.percent();
  refresh();
}

void CalculatorApp::refresh() {
  if (displayLabel_ == nullptr) return;
  // Engine strings are ASCII; render '*' and '/' with the keypad's glyphs
  // (U+00D7 / U+00F7, both in montserrat_pt_14's Latin-1 range).
  const std::string raw = engine_.display();
  std::string out;
  out.reserve(raw.size() * 2);
  for (char c : raw) {
    if (c == '*') out += "\xC3\x97";       // ×
    else if (c == '/') out += "\xC3\xB7";  // ÷
    else out += c;
  }
  lv_label_set_text(displayLabel_, out.c_str());
}
