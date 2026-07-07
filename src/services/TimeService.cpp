#include "TimeService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "core/App.h"  // RadioMode
#include "RadioManager.h"
#include "WiFiService.h"

namespace {
// Any epoch after 2023-11 proves SNTP actually set the clock (the ESP32
// cold-boots believing it's 1970).
constexpr time_t kSaneEpoch = 1700000000;

// Brazil GMT-3, fixed offset, no DST (settings spec §2 "Fixed-offset timezone").
// Used offline and whenever the GeoIP lookup fails, so the clock is never UTC.
constexpr const char* kDefaultTz = "<-03>3";

// Build a POSIX TZ string from a UTC offset in seconds. POSIX inverts the sign
// of the numeric field (it's the seconds to ADD to local time to reach UTC), so
// -10800 (GMT-3) becomes "<-03>3". Half-hour zones (+19800 -> "<+0530>-5:30")
// work too. No DST rule is emitted — fixed offset by design.
std::string tzFromOffsetSeconds(int off) {
  const int a = off < 0 ? -off : off;
  const int oh = a / 3600;
  const int om = (a % 3600) / 60;
  char abbr[10];
  if (om)
    snprintf(abbr, sizeof(abbr), "<%c%02d%02d>", off < 0 ? '-' : '+', oh, om);
  else
    snprintf(abbr, sizeof(abbr), "<%c%02d>", off < 0 ? '-' : '+', oh);
  char num[12];
  const int nh = off < 0 ? oh : -oh;  // numeric field negates the offset sign
  if (om)
    snprintf(num, sizeof(num), "%d:%02d", nh, om);
  else
    snprintf(num, sizeof(num), "%d", nh);
  return std::string(abbr) + num;
}
}  // namespace

void TimeService::begin() {
  const std::string tz = store_.getString("tz", kDefaultTz);
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

LocalDate TimeService::today() const {
  if (!known_) return {0, 0, 0};
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return {static_cast<int16_t>(t.tm_year + 1900),
          static_cast<int8_t>(t.tm_mon + 1), static_cast<int8_t>(t.tm_mday)};
}

int TimeService::minutesSinceMidnight() const {
  if (!known_) return -1;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour * 60 + t.tm_min;
}

void TimeService::hhmm(char out[6]) const {
  if (!known_) {
    snprintf(out, 6, "--:--");
    return;
  }
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  snprintf(out, 6, "%02d:%02d", t.tm_hour, t.tm_min);
}

bool TimeService::syncNow() {
  const RadioState prev = radio_.current();
  if (prev != RadioState::WiFiOn && !radio_.request(RadioMode::WiFi)) {
    return false;
  }

  // Bounded connect: the boot path is already connected so this only caps the
  // interactive "Sincronizar agora" wait (avoids a ~15 s UI stall on failure).
  bool ok = wifi_.isConnected() || wifi_.connect(8000);
  if (ok) {
    const std::string tz = store_.getString("tz", kDefaultTz);
    configTzTime(tz.c_str(), "pool.ntp.org", "time.nist.gov");
    const uint32_t start = millis();
    ok = false;
    while (millis() - start < 15000) {
      if (time(nullptr) > kSaneEpoch) {
        ok = true;
        break;
      }
      delay(100);
    }
  }
  if (ok) {
    known_ = true;
    // Still connected here (radio restore is below); learn the local zone so
    // the clock isn't stuck on the GMT-3 fallback. Failure leaves tz untouched.
    std::string tz;
    if (detectTimezone(tz)) setTimezone(tz);
  }

  // F5: the failed-borrow early-return above does NOT restore prev, and the
  // BtOn branch below relies on request(Bluetooth) succeeding. Safe in F4 (the
  // radio never reaches BtOn while BT is stubbed, so prev is only Off/WiFiOn);
  // revisit this restore path when Bluetooth actually arms in F5.
  // Restore whatever the radio was doing before we borrowed it.
  if (prev == RadioState::Off) radio_.request(RadioMode::None);
  else if (prev == RadioState::BtOn) radio_.request(RadioMode::Bluetooth);
  Serial.printf("[time] syncNow %s\n", ok ? "ok" : "FAILED");
  return ok;
}

void TimeService::setManual(const LocalDate& d, int hour, int minute) {
  struct tm t = {};
  t.tm_year = d.year - 1900;
  t.tm_mon = d.month - 1;
  t.tm_mday = d.day;   // mktime normalizes out-of-range days (e.g. Feb 31)
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_isdst = -1;
  const time_t epoch = mktime(&t);  // interprets via the active TZ
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  known_ = true;
}

void TimeService::setTimezone(const std::string& posixTz) {
  store_.setString("tz", posixTz);
  setenv("TZ", posixTz.c_str(), 1);
  tzset();
}

bool TimeService::detectTimezone(std::string& out) {
  // ip-api.com over plain HTTP (no TLS/cert -> minimal heap on this no-PSRAM
  // board). `offset` is the current UTC offset in seconds, DST folded in.
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, "http://ip-api.com/json/?fields=status,offset")) {
    return false;
  }
  http.setConnectTimeout(4000);  // don't stall boot on a dead endpoint
  http.setTimeout(4000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    Serial.printf("[time] geoip HTTP %d\n", code);
    return false;
  }
  const String body = http.getString();
  http.end();

  // Body: {"status":"success","offset":-10800}. One integer field -> hand parse,
  // no ArduinoJson.
  if (body.indexOf("\"status\":\"success\"") < 0) return false;
  const int key = body.indexOf("\"offset\":");
  if (key < 0) return false;
  const long off = body.substring(key + 9).toInt();  // strlen("\"offset\":")
  out = tzFromOffsetSeconds(static_cast<int>(off));
  Serial.printf("[time] geoip offset=%ld tz=%s\n", off, out.c_str());
  return true;
}
