# F1 close-out: final whole-branch review + cleanup

**Written:** 2026-07-04, right after Task 6 completed on hardware.
**Goal:** run F1's final whole-branch review, resolve the queued findings,
and clean up the code/docs left stale by the resistive-variant discovery.
When this doc's Definition of Done is met, F1 (plan
`2026-07-03-foundation-1-lvgl-touch.md`) is closed and F2 (launcher) can start.

## Context in three sentences

F1 Tasks 0–6 are complete and committed on `main` (`27ba67d..6cf37b0`); every
task was individually reviewed, but the plan's **final whole-branch review has
not run yet**. Task 6 revealed the board is the **resistive** 2432S024 variant
(XPT2046 on shared display SPI), so touch was rewritten late (commit
`6cf37b0`) and some earlier code/docs now describe hardware we don't have.
The debugging trail lives in `.superpowers/sdd/progress.md`; hardware truth is
`docs/hardware.md` + `docs/VENDOR-NOTES.md`.

## Part 1 — final whole-branch review

Review the full F1 diff (`git diff 27ba67d..HEAD`, or root-to-HEAD as one
branch) with superpowers:requesting-code-review (or `/code-review` at high
effort). Pay extra attention to `6cf37b0` — it's the only commit that was NOT
task-reviewed before landing (written mid-debug, verified on hardware only).

Queued Minors from the per-task reviews (ledger lines, restated):

| Origin | Finding | Status now |
| --- | --- | --- |
| Task 2 | no test for degenerate `raw_w`/`raw_h == 0` underflow in `clampTo` (`lib/touch_transform`) | **moot if the lib is deleted** (Part 2); fix only if kept |
| Task 3 | `~28.8 KB` comment in `DisplayService.h` uses decimal KB, not KiB | trivial; fix or waive |
| Task 4 | `writeRegister` discarded `endTransmission` result; I2C hiccup mid-press reads as release+repress | **MOOT** — that CST820 code was deleted in `6cf37b0` |

Things `6cf37b0` deserves scrutiny on (never formally reviewed):
- `TouchService::readTouch` calls `getTouchRaw` then `convertRawXY` (two
  calls instead of one `getTouch`) to keep raw values for the serial print —
  fine? or simplify now that calibration is proven?
- Touch cal constants live in `include/LGFX_ESP32_2432S024C.hpp` with
  min>max inversion — comment-heavy but magic-number-y. Acceptable for
  hardware constants, but reviewer should sanity-check the explanation
  against `.superpowers/sdd/progress.md` steps 4–6.
- `DisplayService::gfx()` exposes the whole LGFX device — narrowest
  interface that works, or should TouchService take it at construction?

## Part 2 — cleanup decisions (each is small; decide, do, commit)

1. **`lib/touch_transform/` + `test/test_touch_transform/` are dead product
   code.** LovyanGFX's `convertRawXY` now does all mapping. BUT these tests
   are the repo's *only* native tests — deleting them leaves `[env:native]`
   unproven and breaks the "pio test -e native" habit until F2 adds its own.
   **Recommendation: delete the lib and its tests** (git history keeps them;
   the CST820 raw-space assumption baked into its docs is wrong anyway), and
   in the same commit add a trivial placeholder native test (or accept the
   empty env and note it in the F2 plan, which schedules native tests of its
   own). Don't keep dead code as scaffolding.
2. **Stale "capacitive/CST820" claims in living docs** (historical plan/spec
   docs are fine as-is — they're records, not references):
   - `README.md:7` ("2.4\" capacitive CYD") and `README.md:59` (edmasini
     repo billed as "our exact variant" — it isn't; it's the C variant).
   - `docs/DISPLAY.md:27,31,43-45,61-63` — board table says capacitive,
     touch row lists CST820/I2C pins, "expect touch to need the same
     swap+mirror" note, and the architecture summary says CST820 +
     `lib/touch_transform`. Rewrite those lines to XPT2046 reality (point at
     `docs/hardware.md` instead of duplicating pin tables).
   - `docs/hardware.md` "Board currently on hand: bare ESP32 devkit" section
     is from before the CYD arrived — outdated header; the CYD is on hand
     and flashed. Restructure so the CYD is primary. (The C-variant CST820
     section is already marked as not-this-unit — keep.)
3. **`include/LGFX_ESP32_2432S024C.hpp` filename says C-variant.**
   Recommendation: rename to `LGFX_ESP32_2432S024.hpp` (grep says includers
   are `DisplayService.h`, `TouchService.h`, plus mentions in docs/HANDOVER,
   docs/hardware.md, VENDOR-NOTES, ledger). Mechanical rename + doc sweep,
   or explicitly decide to keep the name with its existing header-comment
   disclaimer. Either is fine; don't leave it undecided.
4. **`docs/HANDOVER.md`** still has display-era structure with touch bolted
   on. After parts 1–3, refresh it to "F1 done, F2 next" shape (it's the
   living doc every session reads first).
5. **`.superpowers/sdd/progress.md`** — once the final review passes, add a
   closing line ("F1 closed at <commit>") so a future reader knows the
   ledger is historical, not active.

## Verification (before claiming done)

```sh
pio run                  # cyd build
pio test -e native       # whatever native tests remain must pass
pio run -t upload        # board on /dev/ttyUSB0 (pio device list | grep USB0)
python3 .superpowers/sdd/serial_capture.py --reset --seconds 10
                         # boot lines clean, then tap → counter + [touch] lines
```

Serial gotcha: `pio device monitor` dies without a TTY — use the capture
script. Serial prints are lost when nothing is listening; start the capture
BEFORE asking for taps.

## Definition of Done

- [ ] Whole-branch review run; all findings fixed or explicitly waived
- [ ] touch_transform decision executed (delete + native-test story, per rec)
- [ ] README.md / DISPLAY.md / hardware.md say resistive-XPT2046 everywhere a
      reader could act on it
- [ ] LGFX header rename decided and executed (or waived, recorded here)
- [ ] HANDOVER.md refreshed to F1-done/F2-next
- [ ] Ledger closed out; memory updated (danios-f1-stopping-point → closed)
- [ ] `pio run` + native tests green, smoke screen re-verified on hardware
      after any code change
