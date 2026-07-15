#include "pet_model.h"

#include <date_utils.h>

namespace {
// The spec's day-count -> level table, applied to any need's dateKey.
NeedLevel levelFromKey(uint32_t lastKey, uint32_t todayKey) {
  if (lastKey == 0 || todayKey == 0) return NeedLevel::Great;  // unknown: benign
  int32_t d = daysBetween(fromDateKey(lastKey), fromDateKey(todayKey));
  if (d <= 0) return NeedLevel::Great;   // same day or clock went backwards
  if (d >= petcfg::kDaysToCritical) return NeedLevel::Critical;
  if (d == 1) return NeedLevel::Okay;
  return NeedLevel::Neglected;           // 2 .. kDaysToCritical-1
}

// Advance a need's dateKey to today, but only the first time today. Returns
// true when it actually advanced (the day's first interaction with the need).
bool satisfyOncePerDay(uint32_t& key, uint32_t todayKey) {
  if (todayKey == 0 || key == todayKey) return false;
  key = todayKey;
  return true;
}

// Mark the current night as "disturbed" so tomorrow's dawn skips the energy
// award. 20:00-23:59 belongs to tonight (today); 00:00-06:59 belongs to the
// night that began yesterday. Daytime interactions do not mark anything.
void recordNightInteraction(PetState& st, uint32_t todayKey, int mins) {
  if (todayKey == 0 || mins < 0) return;
  if (mins >= petcfg::kNightStartMin) {
    st.nightint = todayKey;  // evening: this night starts today
  } else if (mins < petcfg::kDawnMin) {
    st.nightint = dateKey(addDays(fromDateKey(todayKey), -1));  // pre-dawn
  }
}

// The 2nd-oldest of the four satisfaction dates. Because needs only degrade
// during neglect, this is exactly the day the current "2+ Critical" streak
// began (its date + kDaysToCritical). Sorting four values inline.
uint32_t secondOldestSatisfied(const PetState& st) {
  uint32_t v[4] = {st.fed, st.played, st.cleaned, st.rested};
  for (int i = 1; i < 4; ++i) {
    const uint32_t k = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; --j; }
    v[j + 1] = k;
  }
  return v[1];
}

// Backdated Sick onset dateKey: the day the 2nd need crossed into Critical.
uint32_t sickOnsetKey(const PetState& st) {
  const uint32_t s2 = secondOldestSatisfied(st);
  if (s2 == 0) return 0;
  return dateKey(addDays(fromDateKey(s2), petcfg::kDaysToCritical));
}

// Advance rested to the most recent dawn that has passed, unless the night
// before that dawn was disturbed. Being away (no night interactions) keeps
// Energy Great.
void applyDawnAward(PetState& st, uint32_t todayKey, int mins) {
  if (todayKey == 0 || mins < 0) return;
  const LocalDate today = fromDateKey(todayKey);
  const LocalDate lastDawn = (mins >= petcfg::kDawnMin) ? today : addDays(today, -1);
  const uint32_t nightStartKey = dateKey(addDays(lastDawn, -1));
  if (st.nightint == nightStartKey) return;  // disturbed last night: no award
  const uint32_t dawnKey = dateKey(lastDawn);
  if (st.rested < dawnKey) st.rested = dawnKey;
}

int clampCare(int v) {
  if (v > petcfg::kCareMax) return petcfg::kCareMax;
  if (v < petcfg::kCareMin) return petcfg::kCareMin;
  return v;
}

// +1 if all four needs were satisfied exactly on dayKey; -1 if 2+ were
// Critical on dayKey; 0 otherwise. Reconstructed from the current need dates
// (see the care-score note in the plan) — coarse, but deterministic.
int careDeltaForDay(const PetState& st, uint32_t dayKey) {
  if (dayKey == 0) return 0;
  const uint32_t needs[4] = {st.fed, st.played, st.cleaned, st.rested};
  bool allGreat = true;
  int crit = 0;
  for (uint32_t k : needs) {
    if (k != dayKey) allGreat = false;
    if (k == 0) { ++crit; continue; }  // never satisfied counts as Critical
    if (daysBetween(fromDateKey(k), fromDateKey(dayKey)) >= petcfg::kDaysToCritical) {
      ++crit;
    }
  }
  if (allGreat) return 1;
  if (crit >= 2) return -1;
  return 0;
}

// Accumulate care over the completed days [lastKey, todayKey). Bounded so a
// pathological multi-year jump can't loop unboundedly.
int16_t scoreCareDays(int startCare, const PetState& st, uint32_t lastKey,
                      uint32_t todayKey) {
  if (lastKey == 0 || todayKey == 0) return static_cast<int16_t>(clampCare(startCare));
  int32_t span = daysBetween(fromDateKey(lastKey), fromDateKey(todayKey));
  if (span < 0) span = 0;
  if (span > petcfg::kCareCatchupCap) span = petcfg::kCareCatchupCap;
  int score = startCare;
  LocalDate d = fromDateKey(lastKey);
  for (int32_t i = 0; i < span; ++i) {  // scores lastKey .. todayKey-1
    score += careDeltaForDay(st, dateKey(d));
    d = addDays(d, 1);
  }
  return static_cast<int16_t>(clampCare(score));
}
}  // namespace

PetState loadPet(ISettingsStore& store) {
  PetState st;
  st.name = store.getString("pet.name", "");
  st.birth = store.getU32("pet.birth", 0);
  st.alive = store.getBool("pet.alive", false);
  st.fed = store.getU32("pet.fed", 0);
  st.played = store.getU32("pet.played", 0);
  st.cleaned = store.getU32("pet.cleaned", 0);
  st.rested = store.getU32("pet.rested", 0);
  st.sickday = store.getU32("pet.sickday", 0);
  st.illday = store.getU32("pet.illday", 0);
  st.disc = static_cast<int8_t>(store.getI32("pet.disc", 0));
  st.care = static_cast<int16_t>(store.getI32("pet.care", 0));
  st.mess = static_cast<uint8_t>(store.getU32("pet.mess", 0));
  st.scold = store.getU32("pet.scold", 0);
  st.nightint = store.getU32("pet.nightint", 0);
  return st;
}

void savePet(ISettingsStore& store, const PetState& st) {
  store.setString("pet.name", st.name);
  store.setU32("pet.birth", st.birth);
  store.setBool("pet.alive", st.alive);
  store.setU32("pet.fed", st.fed);
  store.setU32("pet.played", st.played);
  store.setU32("pet.cleaned", st.cleaned);
  store.setU32("pet.rested", st.rested);
  store.setU32("pet.sickday", st.sickday);
  store.setU32("pet.illday", st.illday);
  store.setI32("pet.disc", st.disc);
  store.setI32("pet.care", st.care);
  store.setU32("pet.mess", static_cast<uint32_t>(st.mess));
  store.setU32("pet.scold", st.scold);
  store.setU32("pet.nightint", st.nightint);
}

bool hatch(PetState& st, const std::string& name, uint32_t todayKey) {
  if (todayKey == 0) return false;  // day-driven logic needs a known clock
  st = PetState{};                  // start fresh, then stamp
  st.name = name;
  st.birth = todayKey;
  st.alive = true;
  st.fed = st.played = st.cleaned = st.rested = todayKey;
  st.scold = todayKey;  // daily-open ritual already "done" for the hatch day
  return true;
}

void rebirth(PetState& st) { st = PetState{}; }  // all defaults => fresh egg

Screen currentScreen(const PetState& st) {
  if (st.birth == 0) return Screen::Egg;
  if (!st.alive) return Screen::Memorial;
  return Screen::Alive;
}

NeedLevel hungerLevel(const PetState& st, uint32_t t) { return levelFromKey(st.fed, t); }
NeedLevel happyLevel(const PetState& st, uint32_t t) { return levelFromKey(st.played, t); }
NeedLevel hygieneLevel(const PetState& st, uint32_t t) { return levelFromKey(st.cleaned, t); }
NeedLevel energyLevel(const PetState& st, uint32_t t) { return levelFromKey(st.rested, t); }

int criticalCount(const PetState& st, uint32_t todayKey) {
  if (st.birth == 0 || todayKey == 0) return 0;  // egg / unknown clock: no needs
  int n = 0;
  if (hungerLevel(st, todayKey) == NeedLevel::Critical) ++n;
  if (happyLevel(st, todayKey) == NeedLevel::Critical) ++n;
  if (hygieneLevel(st, todayKey) == NeedLevel::Critical) ++n;
  if (energyLevel(st, todayKey) == NeedLevel::Critical) ++n;
  return n;
}

bool needsAttention(const PetState& st, uint32_t todayKey) {
  return criticalCount(st, todayKey) >= 1;
}

Stage growthStage(const PetState& st, uint32_t todayKey) {
  if (st.birth == 0) return Stage::Egg;
  int32_t days =
      (todayKey == 0) ? 0 : daysBetween(fromDateKey(st.birth), fromDateKey(todayKey));
  if (days < 0) days = 0;
  if (days < petcfg::kChildFromDay) return Stage::Baby;
  if (days < petcfg::kTeenFromDay) return Stage::Child;
  if (days < petcfg::kAdultFromDay) return Stage::Teen;
  return Stage::Adult;
}

const char* needLevelLabelPt(NeedLevel level) {
  switch (level) {
    case NeedLevel::Great:     return "Ótimo";
    case NeedLevel::Okay:      return "Bem";
    case NeedLevel::Neglected: return "Carente";
    default:                   return "Crítico";
  }
}

const char* stageSprite(Stage stage) {
  switch (stage) {
    case Stage::Egg:   return "S:/art/pet/egg.bin";
    case Stage::Baby:  return "S:/art/pet/baby.bin";
    case Stage::Child: return "S:/art/pet/child.bin";
    case Stage::Teen:  return "S:/art/pet/teen.bin";
    default:           return "S:/art/pet/adult.bin";
  }
}

bool isNightMinute(int minutesSinceMidnight) {
  if (minutesSinceMidnight < 0) return false;
  return minutesSinceMidnight >= petcfg::kNightStartMin ||
         minutesSinceMidnight < petcfg::kDawnMin;
}

bool feed(PetState& st, Food food, uint32_t todayKey, int mins) {
  recordNightInteraction(st, todayKey, mins);
  const bool advanced = satisfyOncePerDay(st.fed, todayKey);
  if (food == Food::Treat) satisfyOncePerDay(st.played, todayKey);  // also cheers
  return advanced;
}

bool play(PetState& st, uint32_t todayKey, int mins) {
  recordNightInteraction(st, todayKey, mins);
  return satisfyOncePerDay(st.played, todayKey);
}

bool clean(PetState& st, uint32_t todayKey, int mins) {
  if (todayKey == 0) return false;  // clock unknown: no mutation
  recordNightInteraction(st, todayKey, mins);
  if (st.mess > 0) --st.mess;  // remove the tapped mess sprite (cosmetic)
  return satisfyOncePerDay(st.cleaned, todayKey);  // first clean satisfies Hygiene
}

void scoldReward(PetState& st) {
  if (st.disc < petcfg::kDiscMax) ++st.disc;
}

void scoldPenalty(PetState& st) {
  if (st.disc > petcfg::kDiscMin) --st.disc;
}

const char* foodLabelPt(Food food) {
  switch (food) {
    case Food::Snack: return "Lanche";
    case Food::Meal:  return "Refeição";
    default:          return "Docinho";
  }
}

const char* foodSprite(Food food) {
  switch (food) {
    case Food::Snack: return "S:/art/pet/food_snack.bin";
    case Food::Meal:  return "S:/art/pet/food_meal.bin";
    default:          return "S:/art/pet/food_treat.bin";
  }
}

Health healthOf(const PetState& st, uint32_t todayKey) {
  if (criticalCount(st, todayKey) < 2) return Health::Healthy;
  const uint32_t onset = sickOnsetKey(st);
  if (onset == 0 || todayKey == 0) return Health::Sick;
  int32_t sickDays = daysBetween(fromDateKey(onset), fromDateKey(todayKey));
  if (sickDays < 0) sickDays = 0;
  return (sickDays >= petcfg::kSickToIll) ? Health::CriticallyIll : Health::Sick;
}

const char* healthLabelPt(Health h) {
  switch (h) {
    case Health::Healthy: return "Saudável";
    case Health::Sick:    return "Doente";
    default:              return "Muito doente";
  }
}

bool petTick(PetState& st, uint32_t todayKey, int mins) {
  if (todayKey == 0 || !st.alive) return false;  // clock unknown / egg / dead
  applyDawnAward(st, todayKey, mins);
  if (criticalCount(st, todayKey) < 2) {  // healthy: clear any recorded onsets
    st.sickday = 0;
    st.illday = 0;
    return false;
  }
  const uint32_t onset = sickOnsetKey(st);
  st.sickday = onset;
  int32_t sickDays =
      (onset == 0) ? 0 : daysBetween(fromDateKey(onset), fromDateKey(todayKey));
  if (sickDays < 0) sickDays = 0;
  st.illday = (sickDays >= petcfg::kSickToIll)
                  ? dateKey(addDays(fromDateKey(onset), petcfg::kSickToIll))
                  : 0;
  // Sustained illness reaches the death threshold (~9 days total neglect).
  if (sickDays >= petcfg::kSickToIll + petcfg::kIllToDead) {
    st.alive = false;  // dates are kept so the memorial can show the name
    return true;       // just died -> UI shows the memorial screen
  }
  return false;
}

bool misbehavesOn(uint32_t dayKey, uint32_t birthKey) {
  if (dayKey == 0 || birthKey == 0) return false;
  uint32_t h = dayKey * 2654435761u + birthKey * 40503u;  // date-seeded
  h ^= h >> 13;
  h *= 0x85ebca6bu;
  h ^= h >> 16;
  return (h % 100u) < static_cast<uint32_t>(petcfg::kMisbehavePct);
}

bool onAppOpen(PetState& st, uint32_t todayKey, int mins) {
  if (todayKey == 0 || !st.alive) return false;
  recordNightInteraction(st, todayKey, mins);  // opening at night disturbs the pet
  if (st.scold != todayKey) {                   // first open of a new day
    const uint32_t last = (st.scold != 0) ? st.scold : st.birth;
    int32_t elapsed = daysBetween(fromDateKey(last), fromDateKey(todayKey));
    if (elapsed < 0) elapsed = 0;
    int newMess = static_cast<int>(st.mess) + elapsed;  // ~one mess per elapsed day
    if (newMess > petcfg::kMessCap) newMess = petcfg::kMessCap;
    st.mess = static_cast<uint8_t>(newMess);
    st.care = scoreCareDays(st.care, st, last, todayKey);
    st.scold = todayKey;
    return misbehavesOn(todayKey, st.birth);  // only the day's first open rolls the tantrum
  }
  return false;
}
