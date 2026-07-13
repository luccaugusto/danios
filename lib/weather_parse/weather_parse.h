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
