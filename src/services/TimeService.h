// danios TimeService — NTP sync + manual set + local date/clock queries
// (spec §3.1, §6.2). Time is "known" once NTP succeeds or the user sets it
// manually; until then Oracle falls back to random picks (spec §4.4).
#pragma once
#include <date_utils.h>
#include <settings_store.h>

#include <string>

class RadioManager;
class WiFiService;

class TimeService {
 public:
  TimeService(RadioManager& radio, WiFiService& wifi, ISettingsStore& store)
      : radio_(radio), wifi_(wifi), store_(store) {}

  void begin();                          // apply persisted TZ (call in setup())

  bool isTimeKnown() const { return known_; }
  LocalDate today() const;               // {0,0,0} if unknown
  int minutesSinceMidnight() const;      // -1 if unknown
  void hhmm(char out[6]) const;          // "14:07" or "--:--"

  // NTP; asks RadioManager for WiFi, restores the previous radio state after.
  bool syncNow();

  void setManual(const LocalDate& d, int hour, int minute);
  void setTimezone(const std::string& posixTz);  // persists "tz" + applies

 private:
  RadioManager& radio_;
  WiFiService& wifi_;
  ISettingsStore& store_;
  bool known_ = false;
};
