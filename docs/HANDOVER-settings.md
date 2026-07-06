# danios — Settings App Handover

**Written:** 2026-07-06. **Read this first if your job is: make Settings work
(SD + persistence, WiFi + clock, Bluetooth pairing).**

The goal of this phase: from the launcher's Settings icon, the user can set
brightness/sleep/units, **join a WiFi network with the on-screen keyboard**,
see correct local time in the status bar, and **pair a Bluetooth speaker and
hear a test tone**. That unblocks every remaining app (Weather, Music, Oracle,
Pet).

---

## The plans already exist — execute, don't re-plan

Settings is not one plan; it is the three remaining foundation plans, written
in full bite-sized TDD detail. Execute them **strictly in this order** (each
assumes the previous is merged):

| Order | Plan | Delivers | E2E outcome (the "done" test) |
| --- | --- | --- | --- |
| 1 | [`docs/superpowers/plans/2026-07-03-foundation-3-storage-settings.md`](superpowers/plans/2026-07-03-foundation-3-storage-settings.md) | StorageService (SD), SettingsService (NVS), LVGL FS driver `S:`, Settings→Display/Units/About, screen sleep | Change brightness, reboot, it persists; icons load from SD; SD-missing boot error |
| 2 | [`docs/superpowers/plans/2026-07-03-foundation-4-wifi-time.md`](superpowers/plans/2026-07-03-foundation-4-wifi-time.md) | RadioManager, WiFiService, TimeService (NTP + manual), Settings→WiFi/Clock, boot-time sync | Join a WiFi network from the device with the on-screen keyboard; status bar shows correct local time |
| 3 | [`docs/superpowers/plans/2026-07-03-foundation-5-bluetooth-audio.md`](superpowers/plans/2026-07-03-foundation-5-bluetooth-audio.md) | BluetoothAudioService (A2DP source), Settings→Bluetooth, WiFi-XOR-BT complete | Pair a speaker from Settings; hear a test tone through it |

Use `superpowers:executing-plans` (or `superpowers:subagent-driven-development`)
per plan. Interface names, signatures, file paths, and NVS keys in the
[roadmap](superpowers/plans/2026-07-03-danios-roadmap.md) §4 are
**authoritative** — plans and code must not rename them.

Condensed requirements (if you need the product view, not the task view):
[`docs/superpowers/specs/apps/settings.md`](superpowers/specs/apps/settings.md).

## Where the repo is right now

- **F1 + F2 are done and hardware-verified.** Working: display (LVGL 8.4 over
  LovyanGFX, portrait via rotation 7), resistive XPT2046 touch, launcher grid
  with six icons (5 stub apps + Settings shell), status bar, app lifecycle
  with back navigation, badge + enabled/disabled plumbing.
- `src/apps/settings/SettingsApp.{h,cpp}` is the F2 **shell** (an `lv_list` of
  sections); F3/F4/F5 each append their section files
  (`DisplaySection.cpp`, … — roadmap §4.11).
- `main.cpp` marks exactly where F3/F4 inits go (comments at the top).
- **Nothing SD/NVS/radio exists yet** — F3 Task 1 starts from a clean slate.
- General repo/hardware context: [`docs/HANDOVER.md`](HANDOVER.md).

## Conventions you must keep (roadmap §2, §6)

- **App names/icons:** launcher label + icon path live ONLY in
  `src/apps/app_catalog.h` (`catalog::kSettings` etc.). When F3's `S:` FS
  driver lands and an icon `.bin` exists on the card, flip that entry's `icon`
  from `nullptr` to `"S:/art/icons/<id>.bin"` — no other file changes.
- **TDD, native-first:** pure logic in `lib/<module>/` (std C++17, zero
  Arduino/LVGL includes), tested with `pio test -e native`. Services/UI wrap
  thinly. Small conventional commits.
- **Radio rule:** WiFi XOR Bluetooth, only through `RadioManager`. No direct
  `WiFi.*` / `esp_bt_*` power calls anywhere else.
- **RAM:** no PSRAM — WiFi ~50 KB or BT ~64 KB, never both.
- **Display work:** read [`docs/DISPLAY.md`](DISPLAY.md) first; never touch
  panel dims / `offset_rotation` in `include/LGFX_ESP32_2432S024.hpp`.

## Hardware gotchas for this phase

- Board is the **resistive** 2432S024 (XPT2046 on the display SPI bus, CS 33)
  — not the capacitive C variant. Pin tables: [`docs/hardware.md`](hardware.md).
- **SD is untouched so far** — F3 proves the SD bus (VSPI: SCK 18, MISO 19,
  MOSI 23, CS 5). Needs a FAT32 card ≤32 GB in the slot for verification.
- **A2DP on arduino-esp32 core 3.x is the phase's main risk.** F5 Task 0 is a
  deliberate de-risk spike (sine tone) with a documented fallback: pin the
  platform back to core 2.0.x and re-verify the display config. Do the spike
  before anything else in F5.
- Device is `/dev/ttyUSB0` (CH340). Flash: `pio run -e cyd -t upload`.
  `pio device monitor` dies without a TTY — capture serial with
  `python3 .superpowers/sdd/serial_capture.py --reset --seconds 10`, started
  **before** interacting.

## Definition of done (whole phase)

- `pio test -e native` green; `pio run -e cyd` green.
- The three E2E outcomes above observed on the device.
- Settings survives reboot: brightness, sleep timeout, units, WiFi creds,
  timezone, paired BT device all persist (NVS namespace `"danios"`, keys per
  roadmap §4.2).
- `docs/HANDOVER.md` refreshed to "settings phase done, apps next" and the
  next handover written (Calculator is the first app — see
  [`docs/superpowers/specs/apps/calculator.md`](superpowers/specs/apps/calculator.md)).
