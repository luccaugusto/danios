# danios app spec — Pet

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §4.5, §3.3, §6.1, §6.5, §8.
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative — never rename its names/paths/keys.
**Roadmap slot:** A5 (`lib/pet_model/` reserved in roadmap §3). Depends on F3 (NVS/SD) + F4 (time). Also delivers the **launcher badge** wiring.

---

## What it is

A Tamagotchi-style virtual pet needing daily feeding, play, cleaning, and
rest, with real stakes (death) for sustained neglect. App id `"pet"` (pinned),
replaces the `petStub` registration in `src/main.cpp`. `requiredRadio()` =
`None` — state in NVS, art from SD, date from `TimeService`.

## Needs model (day-granularity, date-driven — same determinism as Oracle)

Four directly-tended needs — **Hunger, Happiness, Hygiene, Energy** — each a
discrete care level driven by **days since last satisfied** (per-need
`lastSatisfiedDate`, stored as `dateKey`):

| Days since satisfied | Level |
| --- | --- |
| 0 | Great |
| 1 | Okay |
| 2 | Neglected |
| 3+ | Critical |

Within a single visit, unlimited feed/play/clean taps are allowed for
animation/fun, but **only the day's first interaction with a need advances its
`lastSatisfiedDate`** — no stat-maxing by spam-tapping.

A fifth track, **Discipline**, comes from scold events and does **not** feed
into health/death — tracked for a future growth-branching update.

## Interactions

| Need | Interaction | Notes |
| --- | --- | --- |
| Hunger | **Feed** — tray of 3 food icons (Snack, Meal, Treat) | Different hunger/happiness deltas; no downsides. |
| Happiness | **Play** — single button | Short happy animation per tap. |
| Hygiene | **Clean** — tap a mess icon | Mess appears ~once/day; uncleaned messes stack, **capped at 3 visible**. |
| Energy | *(passive)* — don't open the app at night | Night hours default **8pm–7am** (tunable). Interacting during night hours means Energy does **not** advance that night (only the first night interaction per night counts, `pet.nightint`); staying away lets it advance at dawn. |
| Discipline | **Scold** — button appears on misbehavior | On the day's first app-open the pet may "misbehave" (tantrum animation); Scold button appears **for that visit only**. Tap in time → discipline up; miss the window → down. |

## Health (derived, never stored directly)

Computed from how many needs are currently Critical:
`0–1 Critical = Healthy`; `2+ Critical = Sick`; `Sick 3+ consecutive days =
Critically Ill`; `Critically Ill 3+ more consecutive days (~9 days sustained
neglect) = Dead`. Track onset days in `pet.sickday` / `pet.illday`.
All thresholds and the night window are **maker-tunable constants** in
`lib/pet_model/`.

**Clock-jump behavior (intentional):** every stage is computed from
elapsed-day counts, so a device off for weeks that resyncs via NTP can land
straight on a late stage — including Dead — in one recompute. Do not "protect"
against this; it's the designed semantics.

## Growth

Egg → Baby → Child → Teen → Adult on **total-days-alive** thresholds (defaults:
Baby 0–2, Child 3–9, Teen 10–20, Adult 21+; tunable). One sprite per stage,
single fixed path. **Egg is a one-time hatch moment** (not a days-alive
stage): naming happens here via the LVGL keyboard widget (same widget as WiFi
passwords). A hidden **care-quality score** (`pet.care`) ticks up on days all
4 needs stayed Great and down on days with 2+ Critical — persisted from day
one but **not yet used** (future growth branching; don't build UI for it).

## Death & rebirth

When Critically Ill persists past the threshold, the pet dies. Brief memorial
screen with its name → auto-reset to a fresh Egg (same first-run flow incl.
re-naming). **All** pet state resets.

## Launcher badge

`Launcher::setBadge("pet", on)` (already implemented, F2) — red dot on the Pet
icon whenever any need is **Critical**. Recompute on boot and on day change
(main-loop tick), not just when the app is open.

## Storage — NVS keys owned (A5, namespace `"danios"`)

`pet.name` (str), `pet.birth` (u32 dateKey), `pet.alive` (bool),
`pet.fed`/`pet.played`/`pet.cleaned`/`pet.rested` (u32 dateKey each),
`pet.sickday` (u32), `pet.illday` (u32), `pet.disc` (i8), `pet.care` (i16),
`pet.mess` (u8), `pet.scold` (u32 dateKey), `pet.nightint` (u32 dateKey).

State lives in **NVS, not SD** — survives SD swap/corruption and tolerates
power loss better; a storage hiccup must never look like neglect. Pet **art**
(stage sprites, food icons, mess icon) lives on SD under `S:/art/pet/`.

## Architecture (roadmap conventions)

- **Pure logic:** `lib/pet_model/` — std C++17 only, the whole state machine:
  need staging from dates, first-interaction-per-day rule, health derivation,
  sick/ill/death day tracking, growth stage from days alive, scold-window
  logic, mess spawn/cap, night-window energy rule, care-quality scoring,
  death→rebirth transition. Takes plain `LocalDate`/`dateKey` values
  (`lib/date_utils/`) and an `ISettingsStore&` (or plain struct in/out) so
  everything tests natively — including **large elapsed-day jumps**.
  Tests in `test/test_pet_model/`.
- **Thin UI wrapper:** `src/apps/pet/PetApp.{h,cpp}` — an `App` (roadmap
  §4.5) consuming `TimeService`, `SettingsService`, `StorageService`.

## Errors (spec §6.5 — Pet is the exception)

| Situation | Behavior |
| --- | --- |
| SD missing/corrupt | Pet stays **fully alive and interactive** (state is NVS); art renders as **placeholder shapes** instead of sprites. Do NOT disable the app on SD-missing (unlike Weather/Music/Oracle). |
| Clock never synced | Day-driven logic can't advance; interactions still animate. Hint to Settings → Clock. |

## Name & icon

Launcher label and icon come from `catalog::kPet` in
`src/apps/app_catalog.h`. Icon file (when drawn): `S:/art/icons/pet.bin`;
`nullptr` until then.

## E2E outcome (roadmap §1)

Hatch, name, feed/play/clean/scold the pet; state survives reboot; badge on
Critical.
