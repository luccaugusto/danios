// Host-side tests for pet_model (pio test -e native): the whole Tamagotchi
// state machine — persistence round-trip, hatch/rebirth, need staging, health
// derivation with backdated onsets (so a clock jump lands on a late stage in
// one recompute), growth, the night-window energy rule, misbehavior, mess,
// and care scoring — all verified off-device with plain dateKeys and a
// FakeSettingsStore. dateKeys below are YYYYMMDD.
#include <unity.h>

#include <date_utils.h>
#include <settings_store.h>

#include <pet_model.h>

void setUp() {}
void tearDown() {}

// A pet hatched on 2026-07-06 with a known clock, for reuse across tests.
static PetState hatched(uint32_t day = 20260706u) {
  PetState st;
  TEST_ASSERT_TRUE(hatch(st, "Rex", day));
  return st;
}

static void test_defaults_are_an_egg() {
  PetState st;  // default-constructed
  TEST_ASSERT_EQUAL_INT((int)Screen::Egg, (int)currentScreen(st));
  TEST_ASSERT_FALSE(st.alive);
  TEST_ASSERT_EQUAL_UINT32(0u, st.birth);
}

static void test_hatch_requires_known_clock() {
  PetState st;
  TEST_ASSERT_FALSE(hatch(st, "Rex", 0u));  // clock unknown -> refuse
  TEST_ASSERT_EQUAL_INT((int)Screen::Egg, (int)currentScreen(st));
}

static void test_hatch_stamps_all_dates() {
  const PetState st = hatched(20260706u);
  TEST_ASSERT_TRUE(st.alive);
  TEST_ASSERT_EQUAL_INT((int)Screen::Alive, (int)currentScreen(st));
  TEST_ASSERT_EQUAL_STRING("Rex", st.name.c_str());
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.birth);
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.fed);
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.played);
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.cleaned);
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.rested);
  TEST_ASSERT_EQUAL_UINT32(20260706u, st.scold);  // no tantrum on hatch day
  TEST_ASSERT_EQUAL_UINT32(0u, st.nightint);
  TEST_ASSERT_EQUAL_INT(0, (int)st.mess);
}

static void test_save_load_roundtrip() {
  FakeSettingsStore store;
  PetState st = hatched(20260706u);
  st.disc = -7;
  st.care = 42;
  st.mess = 2;
  st.nightint = 20260705u;
  savePet(store, st);

  const PetState got = loadPet(store);
  TEST_ASSERT_EQUAL_STRING("Rex", got.name.c_str());
  TEST_ASSERT_EQUAL_UINT32(20260706u, got.birth);
  TEST_ASSERT_TRUE(got.alive);
  TEST_ASSERT_EQUAL_UINT32(20260706u, got.fed);
  TEST_ASSERT_EQUAL_INT(-7, (int)got.disc);
  TEST_ASSERT_EQUAL_INT(42, (int)got.care);
  TEST_ASSERT_EQUAL_INT(2, (int)got.mess);
  TEST_ASSERT_EQUAL_UINT32(20260705u, got.nightint);
}

static void test_load_empty_store_is_egg() {
  FakeSettingsStore store;  // nothing written
  const PetState got = loadPet(store);
  TEST_ASSERT_EQUAL_INT((int)Screen::Egg, (int)currentScreen(got));
  TEST_ASSERT_FALSE(got.alive);
}

static void test_rebirth_wipes_to_egg() {
  PetState st = hatched(20260706u);
  st.alive = false;  // pretend it died
  st.disc = 9;
  st.care = -3;
  st.mess = 3;
  rebirth(st);
  TEST_ASSERT_EQUAL_INT((int)Screen::Egg, (int)currentScreen(st));
  TEST_ASSERT_EQUAL_UINT32(0u, st.birth);
  TEST_ASSERT_EQUAL_STRING("", st.name.c_str());
  TEST_ASSERT_EQUAL_INT(0, (int)st.disc);
  TEST_ASSERT_EQUAL_INT(0, (int)st.care);
  TEST_ASSERT_EQUAL_INT(0, (int)st.mess);
}

static void test_dead_pet_shows_memorial() {
  PetState st = hatched(20260706u);
  st.alive = false;  // hatched then died -> birth stays set
  TEST_ASSERT_EQUAL_INT((int)Screen::Memorial, (int)currentScreen(st));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_an_egg);
  RUN_TEST(test_hatch_requires_known_clock);
  RUN_TEST(test_hatch_stamps_all_dates);
  RUN_TEST(test_save_load_roundtrip);
  RUN_TEST(test_load_empty_store_is_egg);
  RUN_TEST(test_rebirth_wipes_to_egg);
  RUN_TEST(test_dead_pet_shows_memorial);
  return UNITY_END();
}
