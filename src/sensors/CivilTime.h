// Calendar <-> UTC conversion for the RTC, without timegm() (not in the
// ESP32 newlib). Pure functions, so they're unit-tested on the host
// (test/test_civil_time). Algorithms: Howard Hinnant, "chrono-Compatible
// Low-Level Date Algorithms".
#pragma once

#include <cstdint>
#include <ctime>

namespace civil {

struct DateTime {
  int year;    // e.g. 2026
  int month;   // 1-12
  int day;     // 1-31
  int hour;    // 0-23
  int minute;  // 0-59
  int second;  // 0-59
};

// Days since 1970-01-01 for a proleptic Gregorian date.
constexpr int64_t days_from_civil(int64_t y, int m, int d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline time_t to_utc(const DateTime& t) {
  return static_cast<time_t>(days_from_civil(t.year, t.month, t.day) * 86400 + t.hour * 3600 +
                             t.minute * 60 + t.second);
}

inline DateTime from_utc(time_t utc) {
  int64_t secs = static_cast<int64_t>(utc);
  int64_t days = secs >= 0 ? secs / 86400 : (secs - 86399) / 86400;
  int64_t rem = secs - days * 86400;

  int64_t z = days + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  const int y = static_cast<int>(yoe + era * 400 + (m <= 2));

  return DateTime{y, m, d, static_cast<int>(rem / 3600), static_cast<int>(rem % 3600 / 60),
                  static_cast<int>(rem % 60)};
}

// 0 = Sunday ... 6 = Saturday (the PCF85063 weekday convention).
inline int weekday(time_t utc) {
  int64_t secs = static_cast<int64_t>(utc);
  int64_t days = secs >= 0 ? secs / 86400 : (secs - 86399) / 86400;
  int64_t w = (days + 4) % 7;  // 1970-01-01 was a Thursday
  return static_cast<int>(w < 0 ? w + 7 : w);
}

}  // namespace civil
