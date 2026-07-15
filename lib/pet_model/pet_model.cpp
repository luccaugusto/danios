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
