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

// Advance `today` this many days past a pet's hatch day (2026-07-06).
static uint32_t dayPlus(int n) { return dateKey(addDays({2026, 7, 6}, n)); }

static void test_need_levels_by_days_since_satisfied() {
  PetState st = hatched();  // all needs satisfied on day 0
  // day 0 = Great, +1 = Okay, +2 = Neglected, +3 and beyond = Critical.
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Great, (int)hungerLevel(st, dayPlus(0)));
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Okay, (int)hungerLevel(st, dayPlus(1)));
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Neglected, (int)hungerLevel(st, dayPlus(2)));
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Critical, (int)hungerLevel(st, dayPlus(3)));
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Critical, (int)hungerLevel(st, dayPlus(9)));
}

static void test_need_level_unknown_clock_is_great() {
  PetState st = hatched();
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Great, (int)hungerLevel(st, 0u));
}

static void test_need_level_ignores_backwards_clock() {
  PetState st = hatched(20260706u);  // satisfied 2026-07-06
  // "today" earlier than satisfied (clock went back): benign Great, no crash.
  TEST_ASSERT_EQUAL_INT((int)NeedLevel::Great, (int)hungerLevel(st, 20260701u));
}

static void test_critical_count_and_badge() {
  PetState st = hatched();
  // day+2: all needs Neglected, none Critical -> no badge.
  TEST_ASSERT_EQUAL_INT(0, criticalCount(st, dayPlus(2)));
  TEST_ASSERT_FALSE(needsAttention(st, dayPlus(2)));
  // day+3: all four Critical -> badge on.
  TEST_ASSERT_EQUAL_INT(4, criticalCount(st, dayPlus(3)));
  TEST_ASSERT_TRUE(needsAttention(st, dayPlus(3)));
  // Satisfy three needs on day+3; only one stays Critical -> still badge.
  st.fed = st.played = st.cleaned = dayPlus(3);
  TEST_ASSERT_EQUAL_INT(1, criticalCount(st, dayPlus(3)));
  TEST_ASSERT_TRUE(needsAttention(st, dayPlus(3)));
}

static void test_badge_off_for_egg_and_unknown_clock() {
  PetState egg;  // unhatched
  TEST_ASSERT_EQUAL_INT(0, criticalCount(egg, dayPlus(3)));
  TEST_ASSERT_FALSE(needsAttention(egg, dayPlus(3)));
  PetState st = hatched();
  TEST_ASSERT_FALSE(needsAttention(st, 0u));  // clock unknown -> no badge
}

static void test_badge_on_for_dead_pet() {
  PetState st = hatched();
  st.alive = false;  // died; needs stay stale-Critical -> draws user to memorial
  TEST_ASSERT_TRUE(needsAttention(st, dayPlus(9)));
}

static void test_growth_stage_thresholds() {
  PetState egg;
  TEST_ASSERT_EQUAL_INT((int)Stage::Egg, (int)growthStage(egg, dayPlus(0)));
  PetState st = hatched();
  TEST_ASSERT_EQUAL_INT((int)Stage::Baby, (int)growthStage(st, dayPlus(0)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Baby, (int)growthStage(st, dayPlus(2)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Child, (int)growthStage(st, dayPlus(3)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Child, (int)growthStage(st, dayPlus(9)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Teen, (int)growthStage(st, dayPlus(10)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Teen, (int)growthStage(st, dayPlus(20)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Adult, (int)growthStage(st, dayPlus(21)));
  TEST_ASSERT_EQUAL_INT((int)Stage::Adult, (int)growthStage(st, dayPlus(400)));
}

static void test_labels_and_sprites() {
  TEST_ASSERT_EQUAL_STRING("Ótimo", needLevelLabelPt(NeedLevel::Great));
  TEST_ASSERT_EQUAL_STRING("Bem", needLevelLabelPt(NeedLevel::Okay));
  TEST_ASSERT_EQUAL_STRING("Carente", needLevelLabelPt(NeedLevel::Neglected));
  TEST_ASSERT_EQUAL_STRING("Crítico", needLevelLabelPt(NeedLevel::Critical));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/egg.bin", stageSprite(Stage::Egg));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/baby.bin", stageSprite(Stage::Baby));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/child.bin", stageSprite(Stage::Child));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/teen.bin", stageSprite(Stage::Teen));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/adult.bin", stageSprite(Stage::Adult));
}

static void test_night_window_boundaries() {
  TEST_ASSERT_FALSE(isNightMinute(7 * 60));       // 07:00 exactly -> day
  TEST_ASSERT_FALSE(isNightMinute(12 * 60));      // noon -> day
  TEST_ASSERT_FALSE(isNightMinute(19 * 60 + 59)); // 19:59 -> day
  TEST_ASSERT_TRUE(isNightMinute(20 * 60));       // 20:00 -> night
  TEST_ASSERT_TRUE(isNightMinute(23 * 60 + 59));  // 23:59 -> night
  TEST_ASSERT_TRUE(isNightMinute(0));             // 00:00 -> night
  TEST_ASSERT_TRUE(isNightMinute(6 * 60 + 59));   // 06:59 -> night
  TEST_ASSERT_FALSE(isNightMinute(-1));           // unknown clock -> not night
}

static void test_feed_first_of_day_advances_then_spam_no_op() {
  PetState st = hatched(20260706u);       // fed = day 0
  const uint32_t d1 = dayPlus(1);
  TEST_ASSERT_TRUE(feed(st, Food::Meal, d1, 12 * 60));   // first feed today
  TEST_ASSERT_EQUAL_UINT32(d1, st.fed);
  TEST_ASSERT_FALSE(feed(st, Food::Meal, d1, 12 * 60));  // spam: no advance
  TEST_ASSERT_EQUAL_UINT32(d1, st.fed);                  // still day 1
}

static void test_treat_also_cheers() {
  PetState st = hatched(20260706u);
  const uint32_t d1 = dayPlus(1);
  st.played = 0;  // pretend Happiness never satisfied
  TEST_ASSERT_TRUE(feed(st, Food::Treat, d1, 12 * 60));
  TEST_ASSERT_EQUAL_UINT32(d1, st.fed);
  TEST_ASSERT_EQUAL_UINT32(d1, st.played);  // Treat satisfied Happiness too
}

static void test_snack_and_meal_do_not_cheer() {
  PetState st = hatched(20260706u);
  const uint32_t d1 = dayPlus(1);
  st.played = 0;
  feed(st, Food::Snack, d1, 12 * 60);
  TEST_ASSERT_EQUAL_UINT32(0u, st.played);  // Snack: Hunger only
  feed(st, Food::Meal, d1, 12 * 60);
  TEST_ASSERT_EQUAL_UINT32(0u, st.played);  // Meal: Hunger only
}

static void test_play_and_clean() {
  PetState st = hatched(20260706u);
  const uint32_t d1 = dayPlus(1);
  TEST_ASSERT_TRUE(play(st, d1, 12 * 60));
  TEST_ASSERT_EQUAL_UINT32(d1, st.played);

  st.mess = 2;
  TEST_ASSERT_TRUE(clean(st, d1, 12 * 60));  // first clean of day satisfies Hygiene
  TEST_ASSERT_EQUAL_UINT32(d1, st.cleaned);
  TEST_ASSERT_EQUAL_INT(1, (int)st.mess);    // removed one mess sprite
  TEST_ASSERT_FALSE(clean(st, d1, 12 * 60)); // second clean: Hygiene already done
  TEST_ASSERT_EQUAL_INT(0, (int)st.mess);    // but still clears a mess
}

static void test_interactions_noop_on_unknown_clock() {
  PetState st = hatched(20260706u);
  const uint32_t before = st.fed;
  TEST_ASSERT_FALSE(feed(st, Food::Meal, 0u, 12 * 60));  // clock unknown
  TEST_ASSERT_EQUAL_UINT32(before, st.fed);
}

static void test_daytime_interaction_leaves_nightint() {
  PetState st = hatched(20260706u);
  st.nightint = 0;
  feed(st, Food::Meal, dayPlus(1), 12 * 60);  // noon: not a night interaction
  TEST_ASSERT_EQUAL_UINT32(0u, st.nightint);
}

static void test_evening_interaction_marks_tonight() {
  PetState st = hatched(20260706u);
  const uint32_t d1 = dayPlus(1);          // 2026-07-07
  play(st, d1, 21 * 60);                   // 21:00 -> tonight starts today
  TEST_ASSERT_EQUAL_UINT32(d1, st.nightint);
}

static void test_early_morning_interaction_marks_previous_night() {
  PetState st = hatched(20260706u);
  const uint32_t d1 = dayPlus(1);          // 2026-07-07
  play(st, d1, 2 * 60);                    // 02:00 -> belongs to night of 07-06
  TEST_ASSERT_EQUAL_UINT32(dayPlus(0), st.nightint);
}

static void test_scold_reward_and_penalty_clamp() {
  PetState st = hatched();
  st.disc = petcfg::kDiscMax;
  scoldReward(st);
  TEST_ASSERT_EQUAL_INT(petcfg::kDiscMax, (int)st.disc);  // clamped at ceiling
  st.disc = petcfg::kDiscMin;
  scoldPenalty(st);
  TEST_ASSERT_EQUAL_INT(petcfg::kDiscMin, (int)st.disc);  // clamped at floor
  st.disc = 0;
  scoldReward(st);
  TEST_ASSERT_EQUAL_INT(1, (int)st.disc);
  scoldPenalty(st);
  scoldPenalty(st);
  TEST_ASSERT_EQUAL_INT(-1, (int)st.disc);
}

static void test_food_strings() {
  TEST_ASSERT_EQUAL_STRING("Lanche", foodLabelPt(Food::Snack));
  TEST_ASSERT_EQUAL_STRING("Refeição", foodLabelPt(Food::Meal));
  TEST_ASSERT_EQUAL_STRING("Docinho", foodLabelPt(Food::Treat));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/food_snack.bin", foodSprite(Food::Snack));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/food_meal.bin", foodSprite(Food::Meal));
  TEST_ASSERT_EQUAL_STRING("S:/art/pet/food_treat.bin", foodSprite(Food::Treat));
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
  RUN_TEST(test_need_levels_by_days_since_satisfied);
  RUN_TEST(test_need_level_unknown_clock_is_great);
  RUN_TEST(test_need_level_ignores_backwards_clock);
  RUN_TEST(test_critical_count_and_badge);
  RUN_TEST(test_badge_off_for_egg_and_unknown_clock);
  RUN_TEST(test_badge_on_for_dead_pet);
  RUN_TEST(test_growth_stage_thresholds);
  RUN_TEST(test_labels_and_sprites);
  RUN_TEST(test_night_window_boundaries);
  RUN_TEST(test_feed_first_of_day_advances_then_spam_no_op);
  RUN_TEST(test_treat_also_cheers);
  RUN_TEST(test_snack_and_meal_do_not_cheer);
  RUN_TEST(test_play_and_clean);
  RUN_TEST(test_interactions_noop_on_unknown_clock);
  RUN_TEST(test_daytime_interaction_leaves_nightint);
  RUN_TEST(test_evening_interaction_marks_tonight);
  RUN_TEST(test_early_morning_interaction_marks_previous_night);
  RUN_TEST(test_scold_reward_and_penalty_clamp);
  RUN_TEST(test_food_strings);
  return UNITY_END();
}
