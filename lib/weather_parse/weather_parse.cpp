#include "weather_parse.h"

#include <ArduinoJson.h>
#include <cstdio>

GeoResult parseGeo(const char* json) {
  GeoResult r;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return r;
  if (doc["status"] != "success") return r;
  if (!doc["lat"].is<float>() || !doc["lon"].is<float>()) return r;
  r.lat = doc["lat"].as<float>();
  r.lon = doc["lon"].as<float>();
  r.city = doc["city"] | "";
  r.offsetSec = doc["offset"] | 0;
  r.ok = true;
  return r;
}

ForecastWx parseForecast(const char* json) {
  ForecastWx f;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return f;

  JsonObjectConst cur = doc["current"];
  if (cur.isNull() || !cur["temperature_2m"].is<float>()) return f;
  f.current.tempC = cur["temperature_2m"].as<float>();
  f.current.humidity = cur["relative_humidity_2m"] | 0;
  f.current.isDay = (cur["is_day"] | 1) != 0;
  f.current.wmoCode = cur["weather_code"] | -1;
  f.current.windKmh = cur["wind_speed_10m"] | 0.0f;

  JsonArrayConst codes = doc["daily"]["weather_code"];
  JsonArrayConst tmax = doc["daily"]["temperature_2m_max"];
  JsonArrayConst tmin = doc["daily"]["temperature_2m_min"];
  const size_t n = codes.size() < tmax.size() ? codes.size() : tmax.size();
  for (size_t i = 0; i < n && i < tmin.size() && f.dayCount < 3; ++i) {
    f.days[f.dayCount].wmoCode = codes[i] | -1;
    f.days[f.dayCount].tmaxC = tmax[i] | 0.0f;
    f.days[f.dayCount].tminC = tmin[i] | 0.0f;
    ++f.dayCount;
  }
  f.ok = true;
  return f;
}

std::string forecastUrl(float lat, float lon) {
  char buf[256];
  std::snprintf(
      buf, sizeof(buf),
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=%.4f&longitude=%.4f"
      "&current=temperature_2m,relative_humidity_2m,is_day,weather_code,"
      "wind_speed_10m"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min"
      "&timezone=auto&forecast_days=3",
      static_cast<double>(lat), static_cast<double>(lon));
  return buf;
}
