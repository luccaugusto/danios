// src/core/Layout.h — orientation-derived UI dimensions (landscape spike,
// spec 2026-07-17). This header is the ONLY place DANIOS_LANDSCAPE is read;
// everything else branches on layout::kLandscape or uses these constants so
// both orientation paths compile in every build.
#pragma once

#include <lvgl.h>

namespace layout {

#ifdef DANIOS_LANDSCAPE
constexpr bool kLandscape = true;
#else
constexpr bool kLandscape = false;
#endif

constexpr lv_coord_t kScreenW = kLandscape ? 320 : 240;
constexpr lv_coord_t kScreenH = kLandscape ? 240 : 320;

// App-screen back-bar height (owned here so apps and Launcher agree).
constexpr lv_coord_t kTopBarH = 32;
// The container apps build into: full width, below the top bar.
constexpr lv_coord_t kAppW = kScreenW;
constexpr lv_coord_t kAppH = kScreenH - kTopBarH;  // 288 portrait, 208 landscape

// Home grid: 80 px cells — 3x80 = 240, 4x80 = 320, both exact.
constexpr int kGridCols = kLandscape ? 4 : 3;

}  // namespace layout
