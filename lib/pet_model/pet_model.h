// lib/pet_model/pet_model.h — the danios Pet state machine (A5, roadmap §3).
// Pure std C++17, zero Arduino/LVGL includes; the whole Tamagotchi is
// native-tested. Day-granularity and date-driven: every stage is a function
// of stored dateKeys and today, so a device off for weeks resyncs straight
// onto its correct (possibly dead) state in one recompute (spec: intentional).
// All thresholds and the night window are maker-tunable constants below.
#pragma once

#include <cstdint>
#include <string>

#include <settings_store.h>

// Maker-tunable constants (spec "maker-tunable constants in lib/pet_model/").
namespace petcfg {
constexpr int kDaysToCritical = 3;  // days since satisfied -> need is Critical
constexpr int kSickToIll = 3;       // consecutive Sick days -> Critically Ill
constexpr int kIllToDead = 3;       // consecutive Ill days  -> Dead (~9 total)
constexpr int kMessCap = 3;         // visible messes stack, capped
constexpr int kNightStartMin = 20 * 60;  // 20:00 — night window start
constexpr int kDawnMin = 7 * 60;          // 07:00 — dawn (energy award)
constexpr int kMisbehavePct = 30;   // ~% of days the pet may throw a tantrum
constexpr int kCareMax = 1000;      // pet.care clamp (i16, hidden)
constexpr int kCareMin = -1000;
constexpr int kDiscMax = 100;       // pet.disc clamp (i8)
constexpr int kDiscMin = -100;
constexpr int kChildFromDay = 3;    // growth: Baby 0-2, Child 3-9,
constexpr int kTeenFromDay = 10;    //         Teen 10-20, Adult 21+
constexpr int kAdultFromDay = 21;
constexpr int kCareCatchupCap = 60; // bound the care-scoring catch-up loop
}  // namespace petcfg

// Which top-level screen the UI should show (derived, never stored).
enum class Screen : uint8_t { Egg, Alive, Memorial };

// Food tray choices. Snack/Meal satisfy Hunger; Treat satisfies Hunger AND
// Happiness (the "different deltas, no downsides" of the spec at day scale).
enum class Food : uint8_t { Snack, Meal, Treat };

// A single need's care level, driven by days since last satisfied (spec
// table: 0 Great, 1 Okay, 2 Neglected, 3+ Critical).
enum class NeedLevel : uint8_t { Great, Okay, Neglected, Critical };

// Growth stage by total days alive (Egg is the pre-hatch state, not a
// days-alive stage). Thresholds in petcfg (Baby 0-2, Child 3-9, Teen 10-20).
enum class Stage : uint8_t { Egg, Baby, Child, Teen, Adult };

// The full persisted state, mirrored to/from the pinned NVS keys (roadmap
// §4.2). dateKey == 0 means "never/unknown" throughout (date_utils sentinel).
struct PetState {
  std::string name;
  uint32_t birth = 0;      // pet.birth   — hatch dateKey; 0 = unhatched egg
  bool alive = false;      // pet.alive   — false+birth==0 egg; false+birth!=0 dead
  uint32_t fed = 0;        // pet.fed     — last Hunger satisfied
  uint32_t played = 0;     // pet.played  — last Happiness satisfied
  uint32_t cleaned = 0;    // pet.cleaned — last Hygiene satisfied
  uint32_t rested = 0;     // pet.rested  — last Energy satisfied (dawn-driven)
  uint32_t sickday = 0;    // pet.sickday — derived Sick onset (0 = healthy)
  uint32_t illday = 0;     // pet.illday  — derived Critically-Ill onset
  int8_t disc = 0;         // pet.disc    — Discipline (scold outcomes)
  int16_t care = 0;        // pet.care    — hidden care-quality score
  uint8_t mess = 0;        // pet.mess    — visible mess count (0..kMessCap)
  uint32_t scold = 0;      // pet.scold   — last daily-open processing dateKey
  uint32_t nightint = 0;   // pet.nightint— last DISTURBED night's start dateKey
};

// --- Persistence (pinned NVS keys, namespace "danios") ---------------------
PetState loadPet(ISettingsStore& store);
void savePet(ISettingsStore& store, const PetState& st);

// --- Lifecycle -------------------------------------------------------------
// Hatch the egg: name it, stamp birth + all four need dates + scold to today.
// Refuses (returns false, no mutation) when todayKey == 0 (clock unknown).
bool hatch(PetState& st, const std::string& name, uint32_t todayKey);
// Death -> rebirth: wipe ALL state back to a fresh egg (spec: all resets).
void rebirth(PetState& st);
// Egg (birth==0) / Memorial (dead, birth!=0) / Alive.
Screen currentScreen(const PetState& st);

// --- Derived need levels (pure; todayKey==0 or backwards clock => Great) ----
NeedLevel hungerLevel(const PetState& st, uint32_t todayKey);
NeedLevel happyLevel(const PetState& st, uint32_t todayKey);
NeedLevel hygieneLevel(const PetState& st, uint32_t todayKey);
NeedLevel energyLevel(const PetState& st, uint32_t todayKey);

// How many of the four needs are Critical right now. Always 0 for an egg
// (birth==0) or an unknown clock (todayKey==0). A dead pet keeps its stale
// dates, so this stays >=2 for it (keeps the badge on -> memorial).
int criticalCount(const PetState& st, uint32_t todayKey);

// Launcher badge rule (spec): red dot whenever ANY need is Critical.
bool needsAttention(const PetState& st, uint32_t todayKey);

// Growth stage from total days alive.
Stage growthStage(const PetState& st, uint32_t todayKey);

// UI strings / art paths (Portuguese device UI; art under S:/art/pet/).
const char* needLevelLabelPt(NeedLevel level);
const char* stageSprite(Stage stage);
