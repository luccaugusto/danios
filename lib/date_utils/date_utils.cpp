#include "date_utils.h"

bool operator==(const LocalDate& a, const LocalDate& b) {
  return a.year == b.year && a.month == b.month && a.day == b.day;
}

uint32_t dateKey(const LocalDate& d) {
  if (d.year == 0 && d.month == 0 && d.day == 0) return 0;
  return static_cast<uint32_t>(d.year) * 10000u +
         static_cast<uint32_t>(d.month) * 100u + static_cast<uint32_t>(d.day);
}

LocalDate fromDateKey(uint32_t key) {
  if (key == 0) return {0, 0, 0};
  return {static_cast<int16_t>(key / 10000u),
          static_cast<int8_t>((key / 100u) % 100u),
          static_cast<int8_t>(key % 100u)};
}

namespace {
// Howard Hinnant's days_from_civil: serial day number from 1970-01-01.
// http://howardhinnant.github.io/date_algorithms.html — proleptic Gregorian.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);           // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
  return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}
}  // namespace

int32_t daysBetween(const LocalDate& a, const LocalDate& b) {
  return static_cast<int32_t>(
      daysFromCivil(b.year, static_cast<unsigned>(b.month),
                    static_cast<unsigned>(b.day)) -
      daysFromCivil(a.year, static_cast<unsigned>(a.month),
                    static_cast<unsigned>(a.day)));
}
