# danios app spec — Settings

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §5, §6.1, §6.2 (kept intact — it stays authoritative for anything not repeated here).
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative for every name, signature, path, and NVS key below — never rename.
**Implementation plans:** Settings is built by the three foundation plans, in order:
[F3 storage+settings](../../plans/2026-07-03-foundation-3-storage-settings.md) →
[F4 wifi+time](../../plans/2026-07-03-foundation-4-wifi-time.md) →
[F5 bluetooth](../../plans/2026-07-03-foundation-5-bluetooth-audio.md).
The Weather-location section arrives later with the Weather app (A3).
**Start here:** [`docs/HANDOVER-settings.md`](../../../HANDOVER-settings.md).

---

## What it is

The sixth launcher icon (id `"settings"`, already registered — the F2 shell in
`src/apps/settings/SettingsApp.{h,cpp}` shows an `lv_list` menu of sections).
Everything on the device is configurable on-device — no reflashing to change
WiFi, Bluetooth pairing, units, brightness (spec goal). `requiredRadio()` is
`None`; the WiFi/Bluetooth sections request their radio themselves while open.

## Sections

| Section | File in `src/apps/settings/` | Built by | Behavior |
| --- | --- | --- | --- |
| Display | `DisplaySection.cpp` | F3 | Backlight brightness slider (PWM, BL pin 27) + screen-sleep timeout dropdown (battery saving). |
| Units | `UnitsSection.cpp` | F3 | °C / °F switch, default °C. |
| About | `AboutSection.cpp` | F3 | Firmware version, free heap, SD status. (No battery % — see deviations.) |
| WiFi | `WifiSection.cpp` | F4 | Scan and list nearby networks → tap one → on-screen `lv_keyboard` for the password → connect and save. Forget button. Fully on-device, no phone/web. |
| Clock | `ClockSection.cpp` | F4 | **Sync now** button (forces NTP re-sync, bringing WiFi up briefly if needed) + manual date/time rollers for when there's no WiFi (gives Oracle/Pet a correct day). |
| Bluetooth | `BluetoothSection.cpp` | F5 | Scan, pick, connect, forget a device; plays a test tone. The chosen device's address is saved for Music's auto-connect. *Music redirects here when nothing is paired.* |
| Weather location | `WeatherLocationSection.cpp` | A3 (not settings scope) | Auto (geolocation, default) / manual override. |

Shell API each section implements:
`void buildSection(lv_obj_t* parent, /* deps by reference */)`.

## Storage — NVS keys owned (namespace `"danios"`, via `SettingsService`)

| Owner | Keys |
| --- | --- |
| F3 | `disp.bright` (u8 0–255, default 160), `disp.sleep_s` (u16 seconds, 0 = never, default 60), `units.f` (bool, default false = °C) |
| F4 | `wifi.ssid` (str), `wifi.pass` (str), `tz` (str, POSIX TZ e.g. `"<-03>3"`, default `"UTC0"`) |
| F5 | `bt.addr` (str, `AA:BB:CC:DD:EE:FF`), `bt.name` (str) |

## Services delivered alongside (used by every other app)

F3–F5 also deliver the system services the other apps consume — that's why
settings comes first: `StorageService` (SD, roadmap §4.9), `SettingsService`
(NVS, §4.4), LVGL FS driver on drive `S`, `RadioManager` (WiFi-XOR-BT, §4.6),
`WiFiService` (§4.7), `TimeService` (§4.8), `BluetoothAudioService` (§4.10).

## Radio rule

WiFi XOR Bluetooth — no PSRAM, both stacks don't fit in RAM. Only
`RadioManager` touches radio power state. WiFi section calls
`radio.request(WiFi)` on open; Bluetooth section `radio.request(Bluetooth)`;
both drop to `None` when leaving Settings (launcher `goHome()` does this).

## Agreed deviations (roadmap §5)

1. **No battery %** — this board has no battery-sense ADC. About section shows
   no battery; `StatusBar::setBatteryText()` hook exists for a future revision.
2. **Fixed-offset timezone** — `tz` is a fixed POSIX offset string derived from
   geolocation, no DST table. Manual clock set covers drift.
3. **A2DP on arduino-esp32 core 3.x is a risk** — F5 Task 0 is a de-risk spike
   with a documented fallback (pin platform to core 2.0.x, re-verify display).

## Name & icon

Launcher label and icon come from `catalog::kSettings` in
`src/apps/app_catalog.h` — edit only there. Icon stays `nullptr` (colored-letter
fallback) until F3's FS driver lands and `S:/art/icons/settings.bin` exists.

## E2E outcomes (from roadmap §1 — the "done" tests)

- F3: change brightness, reboot, it persists; icons load from SD; SD-missing boot error.
- F4: join a WiFi network from the device with the on-screen keyboard; status bar shows correct local time.
- F5: pair a speaker from Settings; hear a test tone through it.
