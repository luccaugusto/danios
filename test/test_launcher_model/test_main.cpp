#include <unity.h>

#include "launcher_model.h"

void setUp() {}
void tearDown() {}

// Five grid apps in the pinned registration order + settings off-grid.
static LauncherModel makeFive() {
  LauncherModel m(3);
  m.registerApp("weather");
  m.registerApp("music");
  m.registerApp("calc");
  m.registerApp("oracle");
  m.registerApp("pet");
  m.registerApp("settings", /*inGrid=*/false);
  return m;
}

void test_registration_order_and_count() {
  LauncherModel m(3);
  TEST_ASSERT_EQUAL_INT(0, m.registerApp("weather"));
  TEST_ASSERT_EQUAL_INT(1, m.registerApp("music"));
  TEST_ASSERT_EQUAL_INT(2, m.count());
  TEST_ASSERT_EQUAL_INT(0, m.indexOf("weather"));
  TEST_ASSERT_EQUAL_INT(1, m.indexOf("music"));
  TEST_ASSERT_EQUAL_INT(-1, m.indexOf("nope"));
}

void test_duplicate_and_empty_ids_rejected() {
  LauncherModel m(3);
  m.registerApp("weather");
  TEST_ASSERT_EQUAL_INT(-1, m.registerApp("weather"));
  TEST_ASSERT_EQUAL_INT(-1, m.registerApp(""));
  TEST_ASSERT_EQUAL_INT(1, m.count());
}

void test_grid_excludes_non_grid_entries() {
  LauncherModel m = makeFive();
  TEST_ASSERT_EQUAL_INT(6, m.count());
  TEST_ASSERT_EQUAL_INT(5, m.gridCount());
  TEST_ASSERT_EQUAL_INT(-1, m.gridIndexOf("settings"));
  TEST_ASSERT_EQUAL_INT(0, m.gridIndexOf("weather"));
  TEST_ASSERT_EQUAL_INT(4, m.gridIndexOf("pet"));
  TEST_ASSERT_EQUAL_STRING("weather", m.idAtGrid(0).c_str());
  TEST_ASSERT_EQUAL_STRING("pet", m.idAtGrid(4).c_str());
}

void test_grid_slots_three_columns() {
  LauncherModel m = makeFive();
  GridSlot s0 = m.slotOf(0);
  TEST_ASSERT_EQUAL_INT(0, s0.row);
  TEST_ASSERT_EQUAL_INT(0, s0.col);
  GridSlot s2 = m.slotOf(2);
  TEST_ASSERT_EQUAL_INT(0, s2.row);
  TEST_ASSERT_EQUAL_INT(2, s2.col);
  GridSlot s3 = m.slotOf(3);
  TEST_ASSERT_EQUAL_INT(1, s3.row);
  TEST_ASSERT_EQUAL_INT(0, s3.col);
  GridSlot s4 = m.slotOf(4);
  TEST_ASSERT_EQUAL_INT(1, s4.row);
  TEST_ASSERT_EQUAL_INT(1, s4.col);
}

void test_badge_bookkeeping() {
  LauncherModel m = makeFive();
  TEST_ASSERT_FALSE(m.badgeAtGrid(4));               // pet, default off
  TEST_ASSERT_TRUE(m.setBadge("pet", true));
  TEST_ASSERT_TRUE(m.badgeAtGrid(4));
  TEST_ASSERT_TRUE(m.setBadge("pet", false));
  TEST_ASSERT_FALSE(m.badgeAtGrid(4));
  TEST_ASSERT_FALSE(m.setBadge("nope", true));       // unknown id rejected
}

void test_enabled_bookkeeping_and_can_open() {
  LauncherModel m = makeFive();
  TEST_ASSERT_TRUE(m.enabledAtGrid(1));              // music, default enabled
  TEST_ASSERT_TRUE(m.canOpen("music"));
  TEST_ASSERT_TRUE(m.setEnabled("music", false));
  TEST_ASSERT_FALSE(m.enabledAtGrid(1));
  TEST_ASSERT_FALSE(m.canOpen("music"));
  TEST_ASSERT_TRUE(m.setEnabled("music", true));
  TEST_ASSERT_TRUE(m.canOpen("music"));
  TEST_ASSERT_FALSE(m.canOpen("nope"));              // unknown id
  TEST_ASSERT_FALSE(m.setEnabled("nope", false));
  TEST_ASSERT_TRUE(m.canOpen("settings"));           // off-grid, still openable
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_registration_order_and_count);
  RUN_TEST(test_duplicate_and_empty_ids_rejected);
  RUN_TEST(test_grid_excludes_non_grid_entries);
  RUN_TEST(test_grid_slots_three_columns);
  RUN_TEST(test_badge_bookkeeping);
  RUN_TEST(test_enabled_bookkeeping_and_can_open);
  return UNITY_END();
}
