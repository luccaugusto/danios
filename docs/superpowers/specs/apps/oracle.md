# danios app spec — Oracle

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §4.4, §6.2, §6.3, §6.5, §8.
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative — never rename its names/paths.
**Roadmap slot:** A2 (`lib/oracle_picker/` reserved in roadmap §3). Depends on F3 (SD) + F4 (TimeService).

---

## What it is

A daily "oracle": one piece of wisdom per day from a curated list on the SD
card. App id `"oracle"` (pinned), replaces the `oracleStub` registration in
`src/main.cpp`. `requiredRadio()` = `None` — reads SD; uses the cached date
from `TimeService`.

## Requirements

- Reads **`/oracle/wisdom.txt`** on SD — **one entry per line** (real wisdom,
  inside jokes, sweet notes — all mixed). The maker edits this file to add or
  change entries; the app must tolerate the list growing/shrinking between
  boots. Use `StorageService::readLines()` (trims `\r`, skips empty lines).
- **One entry per day, stable all day, changing at local midnight.** Chosen
  **deterministically from the current date** via a **date-seeded shuffle**, so
  the order is not predictable and entries do not repeat until the whole list
  cycles.
- **Fallback when the clock was never synced** (`TimeService::isTimeKnown()`
  false / `today()` == `{0,0,0}`): pick a **random** entry, re-rolled on each
  open, until the time is known.
- Displays the entry nicely typeset within the oracle character framing — art
  frame from `S:/art/oracle/` (LVGL `S:` drive), with a **placeholder** (plain
  styled container) when the art file is missing.

## Architecture (roadmap conventions)

- **Pure logic:** `lib/oracle_picker/` — std C++17 only. Given
  (`uint32_t dateKey`, `size_t entryCount`) → entry index, implementing the
  date-seeded shuffle (deterministic permutation over the list; same date +
  same count → same index; consecutive dates walk the permutation so nothing
  repeats within a cycle). Native-tested in `test/test_oracle_picker/`:
  determinism, full-cycle-no-repeat, list-size-change behavior, count 0/1
  edge cases.
- **Thin UI wrapper:** `src/apps/oracle/OracleApp.{h,cpp}` — an `App` (roadmap
  §4.5) taking `StorageService&` and `TimeService&`. Pure logic takes plain
  `dateKey` values so it tests natively (roadmap §4.8).

## Errors (spec §6.5)

| Situation | Behavior |
| --- | --- |
| SD card missing | App is disabled in the launcher (greyed, hint msgbox — F3 behavior, already wired via `setAppEnabled`). |
| `/oracle/wisdom.txt` missing or empty | Friendly in-app empty state telling the maker where to put the file. |
| Clock never synced | Random entry per open (see above). |

## Name & icon

Launcher label and icon come from `catalog::kOracle` in
`src/apps/app_catalog.h`. Icon file (when drawn): `S:/art/icons/oracle.bin`;
`nullptr` until then.

## E2E outcome (roadmap §1)

One stable wisdom entry per day from SD; random fallback when clock unknown.
