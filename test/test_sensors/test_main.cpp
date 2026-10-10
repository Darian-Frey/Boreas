// Host-native tests for the pure parts of src/sensors/ (pio test -e native).

#include <unity.h>

#include "sensors/Battery.h"
#include "sensors/CivilTime.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- CivilTime

void test_epoch() {
  TEST_ASSERT_EQUAL_INT64(0, civil::to_utc({1970, 1, 1, 0, 0, 0}));
  civil::DateTime t = civil::from_utc(0);
  TEST_ASSERT_EQUAL(1970, t.year);
  TEST_ASSERT_EQUAL(1, t.month);
  TEST_ASSERT_EQUAL(1, t.day);
}

void test_known_instants() {
  // Reference values from `date -u -d ... +%s`.
  TEST_ASSERT_EQUAL_INT64(946684800, civil::to_utc({2000, 1, 1, 0, 0, 0}));
  TEST_ASSERT_EQUAL_INT64(1709164800, civil::to_utc({2024, 2, 29, 0, 0, 0}));  // leap day
  TEST_ASSERT_EQUAL_INT64(1791633600, civil::to_utc({2026, 10, 10, 12, 0, 0}));
  TEST_ASSERT_EQUAL_INT64(4102444799, civil::to_utc({2099, 12, 31, 23, 59, 59}));  // RTC range end
}

void test_round_trip_every_day_2000_to_2099() {
  for (time_t utc = 946684800; utc < 4102444800; utc += 86400 + 3661) {
    civil::DateTime t = civil::from_utc(utc);
    TEST_ASSERT_EQUAL_INT64(utc, civil::to_utc(t));
  }
}

void test_from_utc_fields() {
  civil::DateTime t = civil::from_utc(1791633600 + 3 * 3600 + 4 * 60 + 5);
  TEST_ASSERT_EQUAL(2026, t.year);
  TEST_ASSERT_EQUAL(10, t.month);
  TEST_ASSERT_EQUAL(10, t.day);
  TEST_ASSERT_EQUAL(15, t.hour);
  TEST_ASSERT_EQUAL(4, t.minute);
  TEST_ASSERT_EQUAL(5, t.second);
}

void test_weekday() {
  TEST_ASSERT_EQUAL(4, civil::weekday(0));                  // 1970-01-01 Thursday
  TEST_ASSERT_EQUAL(6, civil::weekday(946684800));          // 2000-01-01 Saturday
  TEST_ASSERT_EQUAL(6, civil::weekday(1791633600));         // 2026-10-10 Saturday
  TEST_ASSERT_EQUAL(0, civil::weekday(1791633600 + 86400)); // 2026-10-11 Sunday
}

// ---------------------------------------------------------------- Battery

void test_battery_gauge() {
  TEST_ASSERT_EQUAL(4, Battery::from_volts(4.12f).cells);
  TEST_ASSERT_EQUAL(4, Battery::from_volts(4.00f).cells);
  TEST_ASSERT_EQUAL(3, Battery::from_volts(3.97f).cells);
  TEST_ASSERT_EQUAL(2, Battery::from_volts(3.70f).cells);
  TEST_ASSERT_EQUAL(1, Battery::from_volts(3.45f).cells);
  TEST_ASSERT_EQUAL(0, Battery::from_volts(3.30f).cells);
}

void test_battery_flags() {
  BatteryModel ok = Battery::from_volts(3.97f);
  TEST_ASSERT_FALSE(ok.low);
  TEST_ASSERT_FALSE(ok.halt);
  BatteryModel low = Battery::from_volts(3.35f);
  TEST_ASSERT_TRUE(low.low);
  TEST_ASSERT_FALSE(low.halt);
  BatteryModel halt = Battery::from_volts(3.10f);
  TEST_ASSERT_TRUE(halt.low);
  TEST_ASSERT_TRUE(halt.halt);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_epoch);
  RUN_TEST(test_known_instants);
  RUN_TEST(test_round_trip_every_day_2000_to_2099);
  RUN_TEST(test_from_utc_fields);
  RUN_TEST(test_weekday);
  RUN_TEST(test_battery_gauge);
  RUN_TEST(test_battery_flags);
  return UNITY_END();
}
