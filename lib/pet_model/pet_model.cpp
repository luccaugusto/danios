#include "pet_model.h"

#include <date_utils.h>

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
