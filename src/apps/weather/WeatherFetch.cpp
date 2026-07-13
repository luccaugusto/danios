#include "apps/weather/WeatherFetch.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <date_utils.h>
#include <weather_model.h>
#include <weather_parse.h>

#include "services/TimeService.h"

namespace {
// Out-of-range latitude = "no stored location yet" (valid range is ±90).
constexpr float kNoCoord = 999.0f;
// NVS string values cap out near 4000 bytes; a 3-day forecast is ~800.
constexpr size_t kMaxCacheLen = 3500;

// One geolocation call: ip-api free tier is HTTP-only, ~45 req/min — the
// caller's freshness rule keeps this rare. URL is spec-pinned.
bool geolocate(ISettingsStore& store, TimeService& time) {
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(
      "http://ip-api.com/json/"
      "?fields=status,country,city,lat,lon,timezone,offset");
  const int code = http.GET();
  if (code != 200) {
    http.end();
    Serial.printf("[wx] geo http %d\n", code);
    return false;
  }
  const String body = http.getString();
  http.end();

  const GeoResult geo = parseGeo(body.c_str());
  if (!geo.ok) {
    Serial.println("[wx] geo parse failed");
    return false;
  }

  store.setFloat("loc.lat", geo.lat);
  store.setFloat("loc.lon", geo.lon);
  store.setString("loc.city", geo.city);
  // Fixed-offset TZ from the geolocation (roadmap §4.8 + deviation 4).
  time.setTimezone(posixTzFromOffset(geo.offsetSec));
  Serial.printf("[wx] geo %s %.4f,%.4f offset=%d\n", geo.city.c_str(),
                static_cast<double>(geo.lat), static_cast<double>(geo.lon),
                static_cast<int>(geo.offsetSec));
  return true;
}
}  // namespace

bool weatherRefresh(ISettingsStore& store, TimeService& time) {
  const bool manual = store.getU32("loc.mode", 0) == 1;
  float lat = store.getFloat("loc.lat", kNoCoord);
  float lon = store.getFloat("loc.lon", kNoCoord);

  // Auto mode re-geolocates when there are no coords yet, or on the first
  // fetch of a new local day (wx.day is the last successful fetch's dateKey).
  const uint32_t todayKey = dateKey(time.today());
  const bool newDay = todayKey != 0 && store.getU32("wx.day", 0) != todayKey;
  if (!manual && (lat == kNoCoord || lon == kNoCoord || newDay)) {
    if (geolocate(store, time)) {
      lat = store.getFloat("loc.lat", kNoCoord);
      lon = store.getFloat("loc.lon", kNoCoord);
    }
  }
  if (lat == kNoCoord || lon == kNoCoord) return false;  // nowhere to look up

  Serial.printf("[wx] heap before fetch %u\n", ESP.getFreeHeap());
  WiFiClientSecure client;
  client.setInsecure();  // spec: HTTPS without cert pinning
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(client, forecastUrl(lat, lon).c_str());
  const int code = http.GET();
  if (code != 200) {
    http.end();
    Serial.printf("[wx] forecast http %d\n", code);
    return false;
  }
  const String body = http.getString();
  http.end();
  Serial.printf("[wx] heap after fetch %u\n", ESP.getFreeHeap());

  if (!parseForecast(body.c_str()).ok) {
    Serial.println("[wx] forecast parse failed");
    return false;
  }
  if (body.length() >= kMaxCacheLen) {
    Serial.println("[wx] body too large to cache");
    return false;
  }
  store.setString("wx.json", body.c_str());
  store.setU32("wx.day", todayKey);
  Serial.printf("[wx] forecast cached (%u bytes)\n", body.length());
  return true;
}
