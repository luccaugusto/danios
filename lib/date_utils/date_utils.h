// danios date_utils — civil-date math shared by Oracle, Weather, and Pet.
// Pure std C++17; zero Arduino/LVGL includes (roadmap §2, tested on native).
#pragma once
#include <cstdint>

struct LocalDate {
  int16_t year;   // e.g. 2026; 0 = unknown
  int8_t month;   // 1..12;    0 = unknown
  int8_t day;     // 1..31;    0 = unknown
};

bool operator==(const LocalDate& a, const LocalDate& b);

// YYYYMMDD as u32 ({0,0,0} -> 0). 0 is the universal "never/unknown" sentinel.
uint32_t dateKey(const LocalDate& d);
LocalDate fromDateKey(uint32_t key);  // inverse; 0 -> {0,0,0}

// b - a in civil days (negative if b is before a).
int32_t daysBetween(const LocalDate& a, const LocalDate& b);
