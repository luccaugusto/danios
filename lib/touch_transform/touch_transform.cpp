#include "touch_transform.h"

namespace {
int16_t clampTo(int16_t v, int16_t max_exclusive) {
  if (v < 0) return 0;
  if (v >= max_exclusive) return static_cast<int16_t>(max_exclusive - 1);
  return v;
}
}  // namespace

TouchPoint transformTouch(const TouchTransform& t, int16_t raw_x, int16_t raw_y) {
  int16_t x = clampTo(raw_x, t.raw_w);
  int16_t y = clampTo(raw_y, t.raw_h);
  int16_t out_w = t.raw_w;
  int16_t out_h = t.raw_h;
  if (t.swap_xy) {
    const int16_t tmp = x;
    x = y;
    y = tmp;
    out_w = t.raw_h;
    out_h = t.raw_w;
  }
  if (t.mirror_x) x = static_cast<int16_t>(out_w - 1 - x);
  if (t.mirror_y) y = static_cast<int16_t>(out_h - 1 - y);
  return {x, y};
}
