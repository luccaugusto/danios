# danios — ESP32 Touchscreen Companion Device

**Date:** 2026-06-03
**Status:** Design approved — ready for implementation planning

A small, portable, battery-powered touchscreen "app phone" built on an ESP32 LCD
module. A home-screen launcher hosts five apps — Weather, Music, Calculator,
Oracle, and Pet — plus a Settings screen. Built as a personal gift; the
character artwork is hand-drawn by the maker and is out of scope for this spec.

---

## 1. Goals & Non-Goals

**Goals**
- Useful and fun handheld device with five small apps and a settings screen.
- Weather forecast shown via a character dressed for the conditions.
- Music playback from the SD card, streamed over Bluetooth to a speaker/earbuds.
- Basic touchscreen calculator.
- A daily "oracle" that shows one piece of wisdom per day from a curated list.
- A Tamagotchi-style virtual pet that needs daily feeding, play, cleaning, and
  rest, with real stakes for sustained neglect.
- Portable and battery-powered; everything configurable on-device (no reflashing
  to change WiFi, Bluetooth pairing, units, etc.).

**Non-Goals (explicitly out of scope)**
- AVRCP (controlling playback from the speaker's own buttons).
- DS3231 / external RTC for offline timekeeping.
- Online or hybrid oracle quote sources (curated list only).
- A fancy multi-city weather picker (auto-geolocation + a simple manual override
  is enough).
- OTA firmware updates, multi-language UI, and enclosure/case design.
- The character artwork itself (drawn separately by the maker).

---

## 2. Hardware

### 2.1 Board: ESP32-2432S024C ("Cheap Yellow Display", 2.4" capacitive)

| Property | Value | Why it matters |
| --- | --- | --- |
| Chip | **ESP32-WROOM-32** (original ESP32) | Has **Bluetooth Classic / A2DP** — required for streaming music to a Bluetooth speaker. The newer ESP32-S3/C3 **cannot** do this. |
| Display | 2.4" TFT, **240×320**, ILI9341 | Fits a launcher + app UIs. |
| Touch | **Capacitive** (`...S024C` variant) | Nicer feel than the resistive `...S024R`. |
| Storage | microSD (TF) slot, ≤32 GB | Holds music, oracle quotes, and art assets. |
| RAM | 520 KB SRAM, **no PSRAM** | **Key constraint** — see §2.2. |
| Power | JST 1.25mm battery connector + **IP5603** power-bank/charging chip; USB-C | Built-in LiPo charging & protection, no soldering. |
| Clock | 240 MHz dual-core | Enough for MP3 decode + A2DP. |

### 2.2 Central constraint: one radio at a time

With no PSRAM, running **Bluetooth Classic and WiFi simultaneously will not fit in
RAM**. Since the apps are used one at a time, the firmware enforces **"WiFi or
Bluetooth, never both"** via the RadioManager (§3.2). Weather uses WiFi; Music uses
Bluetooth; Calculator and Oracle use no radio (saving battery).

### 2.3 Accessories / shopping list

- ✅ The board: **ESP32-2432S024C** (capacitive variant).
- **microSD card**, 8–32 GB, formatted **FAT32** (board supports ≤32 GB).
- **3.7V LiPo battery** with a **JST 1.25mm 2-pin** connector.
  - ⚠️ **Check polarity matches the board before plugging in** — vendors sometimes
    wire the 1.25mm connector reversed. The board ships with a matching 1.25mm cable.
- **USB-C data cable** (for flashing + charging).
- A **Bluetooth speaker or earbuds** (likely already owned).
- *Optional:* 3D-printed / laser-cut case.

---

## 3. System Architecture

The firmware is layered so each unit has one clear job and a well-defined interface.

### 3.1 Layers

**Layer 1 — Drivers (board-specific)**
- Display driver (LovyanGFX bound to LVGL).
- Touch driver (capacitive, I²C).
- microSD (SPI).
- Battery voltage (ADC / IP5603 reading).

**Layer 2 — System services** (each one job, testable in isolation)
- **DisplayService** — LVGL init, tick, flush to the panel.
- **TouchService** — feeds touch input into LVGL.
- **StorageService** — mount SD; read quotes, music files, config fallback, art.
- **RadioManager** — owns a single radio state, enforces WiFi-XOR-Bluetooth.
  `useWiFi()` tears down Bluetooth first; `useBluetooth()` tears down WiFi first;
  `radioOff()` for power saving.
- **WiFiService** — scan networks, connect with stored credentials, report status.
- **TimeService** — NTP sync when WiFi is up; provides current date/time + timezone;
  supports a manual "sync now" and manual set (§5, §6.2).
- **BluetoothAudioService** — A2DP source: connect to a paired device, stream
  decoded audio.
- **SettingsService** — load/save settings in flash (NVS).

**Layer 3 — App framework**
- A common `App` interface: `icon`, `title`, `requiredRadio`, `onEnter()`,
  `onExit()`, `buildUI(parent)`, `tick()`.
- The **Launcher** owns the lifecycle: only one app is active at a time, and on
  entry it asks the RadioManager to switch to the app's `requiredRadio`.

### 3.2 Radio needs per app

| App | Radio | Notes |
| --- | --- | --- |
| Weather | **WiFi** | Geolocation + Open-Meteo fetch. |
| Music | **Bluetooth** | A2DP streaming. |
| Calculator | none | Radios off. |
| Oracle | none | Reads SD; uses cached date from TimeService. |
| Pet | none | Reads/writes NVS for state; reads SD for art; uses cached date from TimeService. |
| Settings | as needed | WiFi section uses WiFi; Bluetooth section uses Bluetooth. |

### 3.3 Launcher (home screen)

- Grid of **five app icons** (hand-drawn art) + a **status bar** showing time,
  battery %, and current radio status, plus a **gear icon** opening Settings.
- Tapping an icon opens the app; each app has a back/home affordance to return.
- The **Pet** icon shows a small badge/dot whenever one of the pet's needs has
  gone Critical (§4.5), nudging a daily check-in without any always-on animation
  on the home screen.

### 3.4 Boot flow

1. Mount SD (StorageService).
2. Load settings from NVS (SettingsService).
3. Init display, touch, and LVGL.
4. If a WiFi network is already saved: bring WiFi up briefly to sync time (NTP) and
   prefetch today's weather, then drop the radio to idle.
   If no network is saved: skip; WiFi-dependent apps point the user to
   Settings → WiFi.
5. Show the launcher.

### 3.5 Main loop

LVGL handler tick + active app `tick()` + service ticks (e.g., TimeService refresh,
BluetoothAudioService streaming pump).

---

## 4. Apps

### 4.1 Weather app (WiFi)

**Flow on open**
1. RadioManager → `useWiFi()`, connect.
2. **Geolocation:** if no fresh cached location, call **ip-api.com** (free, no key,
   HTTP) to get `lat`, `lon`, `city`, `timezone`; cache in settings.
3. **Weather:** fetch from **Open-Meteo** (free, no key); parse with ArduinoJson.
4. Map data → art slots, render the character + readings.
5. Cache the last successful result for instant display and offline fallback.

**Refresh:** on open, on a timer while open (~15–30 min), and prefetched at boot.

**Data → art bridge (decomposed to minimize drawing):**
The character's **outfit is chosen by temperature band**, and a **condition overlay
+ background** is chosen by the weather condition. Defaults (tunable by the maker):

*Temperature bands (°C):*

| Band | Range (°C) | Example outfit |
| --- | --- | --- |
| Freezing | < 0 | Heavy coat, hat, gloves |
| Cold | 0–9 | Jacket |
| Mild | 10–19 | Long sleeves |
| Warm | 20–27 | T-shirt |
| Hot | ≥ 28 | Shorts / tank top |

*Condition groups (from Open-Meteo WMO `weather_code`):*

| Condition | WMO codes | Overlay / background |
| --- | --- | --- |
| Clear / Sunny | 0, 1 | Sunglasses, sunny background |
| Cloudy | 2, 3 | Clouds |
| Fog | 45, 48 | Misty background |
| Rain | 51–57, 61–67, 80–82 | Umbrella, rain background |
| Snow | 71–77, 85, 86 | Scarf, snow background |
| Storm | 95, 96, 99 | Umbrella, dark/lightning background |

The `is_day` flag may select a day/night background variant (optional polish).
This yields ~5 outfits + ~6 condition overlays/backgrounds rather than a drawing
per combination.

**On-screen readings:** character; current temperature + condition text; city name;
today's high/low; a 2–3 day mini-forecast row.

**Errors:** no WiFi or API failure → show last cached weather marked **stale**; if
no cache exists, show a friendly "can't reach weather" state and a hint to check
Settings → WiFi.

### 4.2 Music app (Bluetooth)

**Flow on open**
1. RadioManager → `useBluetooth()`.
2. Auto-connect to the **last paired device** (address stored in NVS).
3. **If nothing is paired → redirect to Settings → Bluetooth** to pick a device.
4. Scan `/music` on the SD card to build the playlist.

**Playback:** decode MP3 (libhelix via arduino-audio-tools) → PCM → feed the A2DP
source callback → streamed to the speaker.

**Controls (touch UI):** play/pause, next, previous, scrollable track list, volume,
and now-playing title (from filename, or ID3 tag if straightforward). A progress
bar is optional polish.

**Constraints:** keep MP3s at a moderate, constant bitrate — comfortable within the
no-PSRAM RAM budget alongside SD reads and the A2DP stack.

**Errors:** no music on card → friendly empty state; no paired device → redirect to
Settings; unreadable/bad file → skip to next track.

### 4.3 Calculator app (no radio)

- LVGL keypad: `0–9  .  +  −  ×  ÷  =  C  ⌫  +/−  %`.
- Four-function arithmetic with operation chaining.
- Graceful divide-by-zero handling; sensible number formatting; overflow handled
  without crashing.
- No history or memory keys (kept intentionally simple).

### 4.4 Oracle app (no radio)

- Reads a plain text file on SD: **`/oracle/wisdom.txt`**, **one entry per line**
  (real wisdom, inside jokes, sweet notes — all mixed). The maker edits this file to
  add or change entries.
- **One entry per day, stable all day, changing at local midnight.** The entry is
  chosen **deterministically from the current date** via a date-seeded shuffle, so
  the order is not predictable and entries do not repeat until the whole list cycles.
- **Fallback when the clock was never synced:** pick a **random** entry (re-rolled on
  each open) until the time is known.
- Displays the entry nicely typeset within the oracle character framing.

### 4.5 Pet app (no radio)

A Tamagotchi-style virtual pet that lives on the device and needs daily care.
Reads/writes NVS for its state; art assets come from SD.

**Needs:** four directly-tended needs — **Hunger**, **Happiness**, **Hygiene**,
and **Energy** — each held as a discrete care level (`Great → Okay → Neglected →
Critical`) driven by days since last satisfied, not a numeric decay curve (the
same date-driven determinism as the Oracle's picker). A fifth track,
**Discipline**, is built from scold events (below) and does **not** feed into
health/death — it's tracked for a future growth-branching update (see Growth).

**Day-granularity + session feedback:** each need has a `lastSatisfiedDate`. At
each day boundary (TimeService's current date): 0 days since = Great, 1 day =
Okay, 2 days = Neglected, 3+ days = Critical. Within a single visit, unlimited
feed/play/clean taps are allowed for animation and fun, but only the day's
*first* interaction with a need advances its `lastSatisfiedDate` — this keeps
the model driven purely by day counts and prevents stat-maxing by spam-tapping.

**Interactions:**

| Need | Interaction | Notes |
| --- | --- | --- |
| Hunger | **Feed** — tray of 3 food icons (Snack, Meal, Treat) | Different hunger/happiness deltas; no downsides. |
| Happiness | **Play** — single button | Short happy animation per tap. |
| Hygiene | **Clean** — tap a mess icon | Mess appears ~once/day; uncleaned messes stack, capped at 3 visible. |
| Energy | *(passive)* — don't open the app at night | Night hours default 8pm–7am (tunable). Interacting during night hours means Energy does **not** advance that night (only the first night interaction per night counts); staying away lets it advance at dawn. |
| Discipline | **Scold** — button appears on misbehavior | On the day's first app-open, the pet may "misbehave" (tantrum animation) and a Scold button appears for that visit only. Tapping it in time raises discipline; missing the window lowers it. |

**Health (derived):** not fed directly — computed from how many needs are
currently Critical. `0–1 Critical = Healthy`, `2+ Critical = Sick`, `Sick for
3+ consecutive days = Critically Ill`, `Critically Ill for 3+ more consecutive
days (~9 days of sustained neglect total) = Dead`. All defaults above (day
thresholds, night window) are maker-tunable, same spirit as Weather's
temperature bands (§4.1).

**Clock-jump behavior:** since every stage is computed from elapsed-day
counts, a device that's been off for a long stretch (dead battery, no charger
during a trip) and then resyncs via NTP can land straight on a late stage —
including Dead — in one recompute, rather than being watched day-by-day. This
is intentional: if that many days genuinely passed with zero interaction, a
real Tamagotchi would be in the same state by the time you checked back. The
escalating stages mainly protect the common case — checking in every few days
but missing some needs — where decline is always visible and gives a clear
chance to intervene before it's fatal.

**Growth:** Egg → Baby → Child → Teen → Adult, on total-days-alive thresholds
(e.g., Baby 0–2 days, Child 3–9, Teen 10–20, Adult 21+; maker-tunable). One
sprite per stage — a single fixed path for now. Egg is a one-time hatch moment
(not a days-alive stage): naming happens here via the LVGL keyboard widget
(same widget used for WiFi passwords, §5). A hidden **care-quality score**
ticks up on days all 4 needs stayed Great and down on days with 2+ Critical —
persisted from day one but not yet used, so a future update can branch growth
forms on it without a data-model change.

**Death & rebirth:** when Critically Ill persists past the threshold above,
the pet dies. A brief memorial screen shows its name, then the app
auto-resets to a fresh Egg — the same first-run flow as initial setup,
including re-naming. All pet state (needs, discipline, care-quality score,
birth date) resets.

**Storage:** pet *state* — name, birth date, life stage, alive flag, per-need
`lastSatisfiedDate` ×4, discipline score, care-quality score, active mess
count — lives in **NVS**, not SD (§6.1). This differs from the other
no-radio/low-radio apps' content and matters here specifically: it survives an
SD card swap or corruption, and tolerates power loss mid-write better than
repeated small rewrites to a FAT32 file — important given neglect has real
(death) consequences, so a storage hiccup should never be indistinguishable
from neglect. Pet **art** (sprites, food icons, mess icon) still lives on SD
under `/art/pet/`, same as every other app's art.

**Errors:** SD card missing or corrupt → pet state and interactions still
work (state is in NVS); art renders as placeholder shapes instead of sprites.
This is different from Weather/Music/Oracle, which are fully SD-dependent and
disable on a missing card (§6.5).

---

## 5. Settings screen

Reached via the **gear icon** in the launcher's status bar.

- **Bluetooth** — scan, pick, connect, and forget a device. *This is where the Music
  app sends the user when nothing is paired.* The chosen device's address is saved
  in NVS for auto-connect.
- **WiFi** — fully on-device: the device **scans and lists nearby networks**; the
  user **taps a network**, an **on-screen keyboard** (LVGL keyboard widget) appears,
  she types the password, and it connects and saves. No phone or web page involved.
- **Display** — backlight brightness + screen-sleep timeout (battery saving).
- **Units** — °C / °F toggle (default °C).
- **Weather location** — auto (default, via geolocation) or a manual override.
- **Clock** — a **"Sync now"** button that forces an NTP re-sync (bringing WiFi up
  briefly if needed), plus **manual date/time set** for when there's no WiFi (which
  also gives the Oracle a correct day to work from).
- **About** — battery %, firmware version, free memory.

---

## 6. Cross-Cutting Concerns

### 6.1 Configuration storage

- **Flash (NVS, via the Preferences library):** WiFi credentials, paired-Bluetooth
  address, units, brightness, sleep timeout, weather-location mode/override, last
  known geolocation, last weather snapshot, pet state (name, birth date, life
  stage, alive flag, per-need `lastSatisfiedDate` ×4, discipline score,
  care-quality score, active mess count). Survives an SD swap.
- **SD card:** bulky content (music, quotes, art).

### 6.2 Time / date

- NTP sync whenever WiFi is up; **timezone derived from geolocation**.
- The ESP32 internal clock holds time while powered.
- Manual "Sync now" and manual set available in Settings → Clock.
- Cold boot with no WiFi and no manual set → time unknown → Oracle uses its random
  fallback until the clock is set or synced.

### 6.3 SD card layout

```
/music/*.mp3              ← songs
/oracle/wisdom.txt        ← curated quotes, one entry per line
/art/weather/             ← outfits, condition overlays, backgrounds
/art/icons/               ← the 5 app icons + gear
/art/oracle/              ← oracle frame
/art/pet/                 ← egg/baby/child/teen/adult sprites, food icons, mess icon
```

### 6.4 Power & battery

- Onboard IP5603 charges the LiPo over USB-C and provides over-charge / over-discharge
  protection.
- PWM backlight dimming + a configurable screen-sleep timeout to save battery.
- Battery % shown in the status bar; a low-battery warning dims the screen.

### 6.5 Error-handling summary

| Situation | Behavior |
| --- | --- |
| SD card missing | Friendly boot error; SD-dependent apps disabled. Pet is the exception — its state lives in NVS, so it stays fully alive/interactive with placeholder art instead of sprites. |
| WiFi connect fails | Weather shows cached/stale data; hint to Settings → WiFi. |
| Bluetooth not paired | Music app redirects to Settings → Bluetooth. |
| Bad/unreadable MP3 | Skip to next track. |
| Clock never synced | Oracle picks a random entry. |
| Pet neglected past the Critically Ill threshold | Pet dies; memorial screen, then auto-resets to a fresh Egg (§4.5). |
| Low battery | Warn + dim screen. |

---

## 7. Data Sources & APIs

### 7.1 IP geolocation — ip-api.com (free, no key, HTTP)

```
GET http://ip-api.com/json/?fields=status,country,city,lat,lon,timezone
```
Returns `lat`, `lon`, `city`, `timezone`. Free tier is HTTP-only and rate-limited
(~45 req/min) — we call it rarely and cache the result.

### 7.2 Weather — Open-Meteo (free, no key, metric/°C by default)

```
GET https://api.open-meteo.com/v1/forecast
      ?latitude={lat}&longitude={lon}
      &current=temperature_2m,relative_humidity_2m,is_day,weather_code,wind_speed_10m
      &daily=weather_code,temperature_2m_max,temperature_2m_min
      &timezone=auto&forecast_days=3
```
`weather_code` follows WMO codes (mapping in §4.1). Open-Meteo is HTTPS; on the
ESP32 use `WiFiClientSecure` with `setInsecure()` for simplicity (no cert pinning).

### 7.3 Libraries

- **Arduino-ESP32 core** — WiFi, HTTPClient, `Preferences` (NVS), `SD`, time/NTP.
- **LVGL** — UI toolkit (launcher, apps, keyboard widget).
- **LovyanGFX** — display + touch driver, bound to LVGL.
- **ArduinoJson** — parse geolocation + weather responses.
- **ESP32-A2DP** (pschatzmann) — A2DP source (Bluetooth audio out).
- **arduino-audio-tools** + **libhelix** (pschatzmann) — MP3 decode → PCM.

---

## 8. Testing Strategy

Split by what's testable where:

- **Pure logic → unit-tested off-device** (host/native build, TDD):
  - Calculator engine (arithmetic, chaining, divide-by-zero, formatting).
  - Oracle date→entry picker (deterministic by date; random fallback).
  - Weather mapping (WMO code → condition; temperature → band).
  - Geolocation + weather JSON parsing.
  - Playlist scanning logic.
  - Pet need-level staging from dates, health derivation, growth-stage-from-
    days-alive, discipline scold-window logic, mess-cap behavior, and the
    death → rebirth transition (including large elapsed-day jumps).
- **Hardware-dependent parts → incremental on-device bring-up + manual verification**,
  in order: display → touch → SD → WiFi/weather → Bluetooth/audio. Each proven
  working before stacking the next. App UIs are built against the service interfaces
  so app logic can be tested with hardware mocked. Pet-specific: sprite
  rendering/animation and touch-hit-testing for mess icons.

---

## 9. References & Links

**Board**
- ESP32-2432S024C user manual — https://manuals.plus/ae/1005009952519844
- ESP32-2432S024 user manual — https://manuals.plus/ae/1005008690761836
- DIYmalls product page (Amazon) — https://www.amazon.com/DIYmalls-ESP32-2432S024C-Capacitive-ESP-WROOM-32-Development/dp/B0CLGD2DG6
- ESP32-2432S024C (eBay listing) — https://www.ebay.com/itm/286508001291
- Board support / examples (esp32-2432S024-Capacitive) — https://github.com/edmasini/esp32-2432S024-Capacitive

**Cheap Yellow Display (general resources)**
- ESP32-Cheap-Yellow-Display project (witnessmenow) — https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
- CYD guide — Random Nerd Tutorials — https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/
- CYD pinout — Random Nerd Tutorials — https://randomnerdtutorials.com/esp32-cheap-yellow-display-cyd-pinout-esp32-2432s028r/
- Sunton board definitions (PlatformIO) — https://github.com/rzeldent/platformio-espressif32-sunton

**APIs**
- Open-Meteo docs — https://open-meteo.com/en/docs
- ip-api docs — https://ip-api.com/docs

**Libraries**
- LVGL — https://lvgl.io
- LovyanGFX — https://github.com/lovyan03/LovyanGFX
- ArduinoJson — https://arduinojson.org
- ESP32-A2DP (pschatzmann) — https://github.com/pschatzmann/ESP32-A2DP
- arduino-audio-tools (pschatzmann) — https://github.com/pschatzmann/arduino-audio-tools
