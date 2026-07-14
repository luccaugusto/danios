// lib/weather_model/weather_model.h — the spec's data->art bridge (A3,
// roadmap §3). Pure std C++17, zero Arduino/LVGL includes; all mapping is
// native-tested. Maker-tunable constants (band edges, WMO groups, art file
// names) live here and in the .cpp — tweak them, rerun the tests.
#pragma once

#include <cstdint>
#include <string>

// Outfit is chosen by temperature band (README table: <0, 0-14, 15-23, 24-27, >=28 °C).
enum class TempBand : uint8_t { Freezing, Cold, Mild, Warm, Hot };

// Overlay + background are chosen by WMO weather_code group (spec table).
enum class Condition : uint8_t { Clear, Cloudy, Fog, Rain, Snow, Storm, Unknown };

TempBand tempBand(float celsius);
Condition conditionFromWmo(int code);

// Display-only conversion (spec: storage and API stay metric). Pass the F3
// NVS flag units.f as `fahrenheit`.
float toDisplayTemp(float celsius, bool fahrenheit);

// Portuguese condition label for on-screen text (device UI language).
const char* conditionLabelPt(Condition c);

// Which art files to show. overlay/background are nullptr when the condition
// has no such slot (Cloudy/Fog/Unknown overlay; Unknown background). Files
// live on the SD card; the UI renders a placeholder box when one is missing.
struct ArtSlots {
  const char* outfit;      // never nullptr
  const char* overlay;     // accessory (sunglasses/umbrella/scarf/hat) or nullptr
  const char* background;  // scene behind the character, or nullptr = plain
};
ArtSlots artSlots(TempBand band, Condition cond, bool isDay);

// ip-api `offset` (seconds EAST of UTC) -> fixed-offset POSIX TZ string for
// TimeService::setTimezone(), e.g. -10800 -> "<-03>3" (roadmap §4.8 +
// deviation 4: no IANA names, no DST — the offset refreshes on the next
// geolocation).
std::string posixTzFromOffset(int32_t offsetSeconds);
