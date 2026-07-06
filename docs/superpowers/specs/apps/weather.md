# danios app spec — Weather

**Extracted:** 2026-07-06 from the [master spec](../2026-06-03-esp32-gift-device-design.md) §4.1, §5 (location), §6.5, §7.1–7.3, §8.
**Interfaces:** the [roadmap](../../plans/2026-07-03-danios-roadmap.md) §4 is authoritative — never rename its names/paths/keys.
**Roadmap slot:** A3 (`lib/weather_model/` + `lib/weather_parse/` reserved in roadmap §3). Depends on F4 (WiFi/time; F3 SD for art). Also delivers **Settings → Weather location** and the **boot-time prefetch**.

---

## What it is

Weather forecast shown via a character dressed for the conditions. App id
`"weather"` (pinned), replaces the `weatherStub` registration in
`src/main.cpp`. `requiredRadio()` = `WiFi`.

## Flow on open

1. Launcher already requested WiFi via `RadioManager`; `WiFiService.connect()`.
2. **Geolocation:** if no fresh cached location, call **ip-api.com** (free, no
   key, HTTP-only, ~45 req/min limit — call rarely, cache the result):
   `GET http://ip-api.com/json/?fields=status,country,city,lat,lon,timezone,offset`
   → `lat`, `lon`, `city`, `offset`. Cache in settings (NVS keys below).
   Convert `offset` (seconds from UTC) to a fixed-offset POSIX string
   (e.g. `-10800` → `"<-03>3"`) and pass to `TimeService::setTimezone()` —
   we do NOT map IANA names (roadmap §4.8 + deviation 4).
3. **Weather:** fetch from **Open-Meteo** (free, no key, HTTPS —
   `WiFiClientSecure` + `setInsecure()`, no cert pinning):
   ```
   GET https://api.open-meteo.com/v1/forecast
         ?latitude={lat}&longitude={lon}
         &current=temperature_2m,relative_humidity_2m,is_day,weather_code,wind_speed_10m
         &daily=weather_code,temperature_2m_max,temperature_2m_min
         &timezone=auto&forecast_days=3
   ```
   Parse with ArduinoJson (`bblanchon/ArduinoJson@^7.4.0`, add to both envs).
4. Map data → art slots, render character + readings.
5. Cache the last successful result (NVS `wx.json` + `wx.day`) for instant
   display and offline fallback.

**Refresh:** on open, on a timer while open (~15–30 min), and **prefetched at
boot** (boot flow step 4: if WiFi creds saved, sync time + prefetch weather,
then drop radio to idle — this plan adds the prefetch half).

## Data → art bridge

Outfit chosen by **temperature band**; condition overlay + background by
**weather condition**. Defaults (maker-tunable constants in `lib/weather_model/`):

*Temperature bands (°C):*

| Band | Range (°C) | Example outfit |
| --- | --- | --- |
| Freezing | < 0 | Heavy coat, hat, gloves |
| Cold | 0–9 | Jacket |
| Mild | 10–19 | Long sleeves |
| Warm | 20–27 | T-shirt |
| Hot | ≥ 28 | Shorts / tank top |

*Condition groups (Open-Meteo WMO `weather_code`):*

| Condition | WMO codes | Overlay / background |
| --- | --- | --- |
| Clear / Sunny | 0, 1 | Sunglasses, sunny background |
| Cloudy | 2, 3 | Clouds |
| Fog | 45, 48 | Misty background |
| Rain | 51–57, 61–67, 80–82 | Umbrella, rain background |
| Snow | 71–77, 85, 86 | Scarf, snow background |
| Storm | 95, 96, 99 | Umbrella, dark/lightning background |

`is_day` may select a day/night background variant (optional polish). ~5
outfits + ~6 overlays/backgrounds — art lives in `S:/art/weather/`, each slot
rendering a **placeholder** (colored box / `LV_SYMBOL_*`) when its file is
missing (art arrives incrementally).

**On-screen readings:** character; current temperature + condition text; city
name; today's high/low; a 2–3 day mini-forecast row. Respect `units.f` (°C/°F,
convert for display only — storage/API stay metric).

## Settings → Weather location (this plan builds it)

`src/apps/settings/WeatherLocationSection.cpp` — auto (default, geolocation) /
manual override (lat/lon/city entry). Shell API:
`void buildSection(lv_obj_t* parent, /* deps by reference */)`.

## Storage — NVS keys owned (A3, namespace `"danios"`)

`loc.mode` (u8 0=auto 1=manual), `loc.lat`/`loc.lon` (float), `loc.city` (str),
`wx.json` (str, last snapshot), `wx.day` (u32 dateKey).

## Architecture (roadmap conventions)

- **Pure logic, native-tested:**
  - `lib/weather_model/` — temperature→band, WMO code→condition group, °C/°F
    conversion, data→art-slot selection. std C++17 only.
  - `lib/weather_parse/` — geolocation + forecast JSON → plain structs, via
    ArduinoJson (platform-independent, fine on native — roadmap §3 footnote).
  - Tests: `test/test_weather_model/`, `test/test_weather_parse/` (feed
    canned JSON, band/code boundary values: -1/0, 9/10, 27/28, each WMO group).
- **Thin UI wrapper:** `src/apps/weather/WeatherApp.{h,cpp}` — an `App`
  (roadmap §4.5) consuming `WiFiService`, `TimeService`, `SettingsService`,
  `StorageService` (art existence checks).

## Errors (spec §4.1, §6.5)

| Situation | Behavior |
| --- | --- |
| No WiFi / API failure | Show last cached weather marked **stale**; hint to Settings → WiFi. |
| No cache either | Friendly "can't reach weather" state + Settings → WiFi hint. |
| SD missing | App disabled in launcher (F3 wiring); weather data itself is NVS-cached, but art needs SD. |

## Name & icon

Launcher label and icon come from `catalog::kWeather` in
`src/apps/app_catalog.h`. Icon file (when drawn): `S:/art/icons/weather.bin`;
`nullptr` until then.

## E2E outcome (roadmap §1)

Character dressed for live local weather; stale-cache fallback.
