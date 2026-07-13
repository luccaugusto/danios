#include "weather_parse.h"

#include <ArduinoJson.h>

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
