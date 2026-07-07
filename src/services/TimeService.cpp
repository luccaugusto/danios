#include "TimeService.h"

#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#include "core/App.h"  // RadioMode
#include "RadioManager.h"
#include "WiFiService.h"

namespace {
// Any epoch after 2023-11 proves SNTP actually set the clock (the ESP32
// cold-boots believing it's 1970).
constexpr time_t kSaneEpoch = 1700000000;
}  // namespace

void TimeService::begin() {
  const std::string tz = store_.getString("tz", "UTC0");
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
    const std::string tz = store_.getString("tz", "UTC0");
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
  if (ok) known_ = true;

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
