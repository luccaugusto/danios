// src/core/Layout.h — orientation-derived UI dimensions.
// Orientation is a runtime setting (NVS "disp.landscape", spec 2026-07-22):
// layout::init() runs once in setup() after settings.begin() and BEFORE any
// display/UI code, so every consumer reads post-init values. The `k` names
// are kept from the compile-time era so ~40 call sites stay unchanged —
// treat them as set-once-at-boot, never reassigned afterwards.
#pragma once

#include <lvgl.h>

namespace layout {

// Set once by init(); portrait defaults protect static-init readers.
extern bool kLandscape;
extern lv_coord_t kScreenW;   // 240 portrait, 320 landscape
extern lv_coord_t kScreenH;   // 320 portrait, 240 landscape

// App-screen back-bar height (orientation-independent).
constexpr lv_coord_t kTopBarH = 32;
extern lv_coord_t kAppW;      // container apps build into
extern lv_coord_t kAppH;      // 288 portrait, 208 landscape
extern int kGridCols;         // 3 portrait, 4 landscape (80 px cells)

// Call exactly once from setup(), after settings.begin() and before
// DisplayService::begin() or any lv_* call that sizes widgets.
void init(bool landscape);

}  // namespace layout
