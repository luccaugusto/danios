// lib/weather_parse/weather_parse.h — geolocation + forecast JSON -> plain
// structs (A3, roadmap §3). Uses ArduinoJson (platform-independent; runs on
// native). Callers never touch JSON: they get these structs or ok=false.
#pragma once

#include <cstdint>
#include <string>

// ip-api.com geolocation (fields=status,country,city,lat,lon,timezone,offset).
struct GeoResult {
  bool ok = false;
  float lat = 0.0f;
  float lon = 0.0f;
  std::string city;
  int32_t offsetSec = 0;  // seconds EAST of UTC (ip-api "offset")
};
GeoResult parseGeo(const char* json);

// Open-Meteo /v1/forecast, current + 3-day daily (spec-pinned query).
struct CurrentWx {
  float tempC = 0.0f;
  int humidity = 0;      // %
  bool isDay = true;
  int wmoCode = -1;      // WMO weather_code; -1 maps to Condition::Unknown
  float windKmh = 0.0f;
};

struct DayWx {
  int wmoCode = -1;
  float tmaxC = 0.0f;
  float tminC = 0.0f;
};

struct ForecastWx {
  bool ok = false;
  CurrentWx current{};
  DayWx days[3]{};       // days[0] = today
  int dayCount = 0;      // 0..3 actually filled
};
ForecastWx parseForecast(const char* json);

// The spec-pinned Open-Meteo request for these coordinates.
std::string forecastUrl(float lat, float lon);
