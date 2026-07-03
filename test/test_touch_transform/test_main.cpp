// Native unit tests for lib/touch_transform — the raw CST820 → screen mapper.
// Raw space is landscape-native 320x240 (like the panel); screen is portrait
// 240x320 at rotation 7 (swap + mirror both). Flags are parameters because
// docs/DISPLAY.md requires verifying them against on-screen targets.
#include <unity.h>
#include <touch_transform.h>

void setUp() {}
void tearDown() {}

static TouchTransform base() {
  TouchTransform t;
  t.raw_w = 320;
  t.raw_h = 240;
  t.swap_xy = false;
  t.mirror_x = false;
  t.mirror_y = false;
  return t;
}

void test_identity_passes_through() {
  const TouchPoint p = transformTouch(base(), 10, 20);
  TEST_ASSERT_EQUAL_INT16(10, p.x);
  TEST_ASSERT_EQUAL_INT16(20, p.y);
}

void test_clamps_negative_raw_to_zero() {
  const TouchPoint p = transformTouch(base(), -5, -1);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_clamps_overrange_raw_to_edge() {
  const TouchPoint p = transformTouch(base(), 999, 400);
  TEST_ASSERT_EQUAL_INT16(319, p.x);  // raw_w - 1
  TEST_ASSERT_EQUAL_INT16(239, p.y);  // raw_h - 1
}

void test_swap_only_transposes_axes() {
  TouchTransform t = base();
  t.swap_xy = true;
  const TouchPoint p = transformTouch(t, 300, 10);
  TEST_ASSERT_EQUAL_INT16(10, p.x);   // output space is 240 wide
  TEST_ASSERT_EQUAL_INT16(300, p.y);  // ...and 320 tall
}

void test_mirror_x_only_flips_across_width() {
  TouchTransform t = base();
  t.mirror_x = true;
  const TouchPoint p = transformTouch(t, 0, 0);
  TEST_ASSERT_EQUAL_INT16(319, p.x);  // out_w (=raw_w, no swap) - 1
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_mirror_y_only_flips_across_height() {
  TouchTransform t = base();
  t.mirror_y = true;
  const TouchPoint p = transformTouch(t, 0, 0);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(239, p.y);  // out_h (=raw_h, no swap) - 1
}

// Rotation 7 = swap + mirror both (MADCTL MV|MX|MY), landscape 320x240 raw
// to portrait 240x320 screen — the danios default configuration.
static TouchTransform rotation7() {
  TouchTransform t = base();
  t.swap_xy = true;
  t.mirror_x = true;
  t.mirror_y = true;
  return t;
}

void test_rotation7_maps_raw_origin_to_screen_bottom_right() {
  const TouchPoint p = transformTouch(rotation7(), 0, 0);
  TEST_ASSERT_EQUAL_INT16(239, p.x);
  TEST_ASSERT_EQUAL_INT16(319, p.y);
}

void test_rotation7_maps_raw_far_corner_to_screen_origin() {
  const TouchPoint p = transformTouch(rotation7(), 319, 239);
  TEST_ASSERT_EQUAL_INT16(0, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

void test_rotation7_maps_raw_center_to_screen_center() {
  const TouchPoint p = transformTouch(rotation7(), 160, 120);
  TEST_ASSERT_EQUAL_INT16(119, p.x);  // 239 - 120
  TEST_ASSERT_EQUAL_INT16(159, p.y);  // 319 - 160
}

void test_rotation7_clamps_before_transforming() {
  const TouchPoint p = transformTouch(rotation7(), 999, -3);
  // clamp -> (319, 0); swap -> (0, 319); mirror -> (239, 0)
  TEST_ASSERT_EQUAL_INT16(239, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_identity_passes_through);
  RUN_TEST(test_clamps_negative_raw_to_zero);
  RUN_TEST(test_clamps_overrange_raw_to_edge);
  RUN_TEST(test_swap_only_transposes_axes);
  RUN_TEST(test_mirror_x_only_flips_across_width);
  RUN_TEST(test_mirror_y_only_flips_across_height);
  RUN_TEST(test_rotation7_maps_raw_origin_to_screen_bottom_right);
  RUN_TEST(test_rotation7_maps_raw_far_corner_to_screen_origin);
  RUN_TEST(test_rotation7_maps_raw_center_to_screen_center);
  RUN_TEST(test_rotation7_clamps_before_transforming);
  return UNITY_END();
}
