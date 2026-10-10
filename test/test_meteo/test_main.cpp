// Host-native tests for src/net/MeteoParse.h (pio test -e native).
// The fixture is a real Open-Meteo response for the public example
// location in locations.example.h, fetched 2026-10-10.

#include <unity.h>

#include <string>

#include "net/MeteoParse.h"

static const char* kFixture = R"json({"latitude":51.716003,"longitude":-0.4683838,"generationtime_ms":0.7022619247436523,"utc_offset_seconds":3600,"timezone":"Europe/London","timezone_abbreviation":"GMT+1","elevation":118.0,"current_units":{"time":"unixtime","interval":"seconds","temperature_2m":"°C","apparent_temperature":"°C","relative_humidity_2m":"%","weather_code":"wmo code","wind_speed_10m":"km/h","wind_direction_10m":"°","wind_gusts_10m":"km/h","surface_pressure":"hPa","uv_index":""},"current":{"time":1791653400,"interval":900,"temperature_2m":11.9,"apparent_temperature":7.8,"relative_humidity_2m":60,"weather_code":1,"wind_speed_10m":18.7,"wind_direction_10m":273,"wind_gusts_10m":35.3,"surface_pressure":999.5,"uv_index":0.00},"hourly_units":{"time":"unixtime","temperature_2m":"°C","precipitation_probability":"%","precipitation":"mm","weather_code":"wmo code"},"hourly":{"time":[1791651600,1791655200,1791658800,1791662400,1791666000,1791669600,1791673200,1791676800,1791680400,1791684000,1791687600,1791691200,1791694800,1791698400,1791702000,1791705600,1791709200,1791712800,1791716400,1791720000,1791723600,1791727200,1791730800,1791734400],"temperature_2m":[12.2,11.5,10.6,10.0,9.9,9.5,9.1,9.7,9.3,9.6,9.6,9.3,9.6,9.5,9.7,10.5,11.1,11.8,12.5,13.2,13.8,13.8,13.4,13.7],"precipitation_probability":[1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],"precipitation":[0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00],"weather_code":[2,0,0,1,2,0,0,3,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,2]},"daily_units":{"time":"unixtime","temperature_2m_max":"°C","temperature_2m_min":"°C","sunrise":"unixtime","sunset":"unixtime","uv_index_max":"","precipitation_probability_max":"%"},"daily":{"time":[1791586800,1791673200],"temperature_2m_max":[14.2,13.8],"temperature_2m_min":[8.2,9.1],"sunrise":[1791613050,1791699551],"sunset":[1791652763,1791739032],"uv_index_max":[1.55,1.45],"precipitation_probability_max":[2,1]}})json";

void setUp() {}
void tearDown() {}

static std::string replaced(const std::string& from, const std::string& to) {
  std::string s(kFixture);
  const size_t at = s.find(from);
  TEST_ASSERT_TRUE_MESSAGE(at != std::string::npos, from.c_str());
  s.replace(at, from.size(), to);
  return s;
}

void test_fixture_parses() {
  WeatherModel m;
  meteo::ParseResult r = meteo::parse_forecast(kFixture, m);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_EQUAL_INT64(1791653400, m.current.time);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.9f, m.current.temp_c);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.8f, m.current.feels_c);
  TEST_ASSERT_EQUAL_UINT8(60, m.current.rh);  // integer in JSON, read as number
  TEST_ASSERT_EQUAL_UINT8(1, m.current.weather_code);
  TEST_ASSERT_EQUAL_UINT16(273, m.current.wind_dir_deg);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 35.3f, m.current.gust_kmh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 999.5f, m.current.pressure_hpa);
  TEST_ASSERT_EQUAL_UINT8(2, m.daily_count);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.2f, m.daily[0].max_c);
  TEST_ASSERT_EQUAL_INT64(1791739032, m.daily[1].sunset);
  TEST_ASSERT_EQUAL_UINT8(24, m.hourly_count);
  TEST_ASSERT_EQUAL_INT64(1791651600, m.hourly[0].time);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.7f, m.hourly[23].temp_c);
  TEST_ASSERT_EQUAL_UINT8(2, m.hourly[23].weather_code);
}

void test_failure_leaves_model_untouched() {
  WeatherModel m;
  TEST_ASSERT_TRUE(meteo::parse_forecast(kFixture, m).ok);
  m.valid = true;
  m.fetched_utc = 123;
  const std::string bad = replaced("\"temperature_2m\":11.9", "\"temperature_2m\":99.0");
  meteo::ParseResult r = meteo::parse_forecast(bad.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("range: current.temperature_2m", r.error);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.9f, m.current.temp_c);  // old value kept
  TEST_ASSERT_TRUE(m.valid);
  TEST_ASSERT_EQUAL_INT64(123, m.fetched_utc);
}

void test_rejects_truncated_json() {
  WeatherModel m;
  std::string cut(kFixture);
  cut.resize(cut.size() / 2);
  meteo::ParseResult r = meteo::parse_forecast(cut.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("json: IncompleteInput", r.error);
}

void test_rejects_wrong_type() {
  WeatherModel m;
  const std::string bad = replaced("\"surface_pressure\":999.5", "\"surface_pressure\":\"high\"");
  meteo::ParseResult r = meteo::parse_forecast(bad.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("missing: current.surface_pressure", r.error);
}

void test_rejects_probability_over_100() {
  WeatherModel m;
  const std::string bad = replaced("\"precipitation_probability\":[1,", "\"precipitation_probability\":[101,");
  meteo::ParseResult r = meteo::parse_forecast(bad.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("range: hourly.precipitation_probability", r.error);
}

void test_rejects_bad_timestamp() {
  WeatherModel m;
  const std::string bad = replaced("\"current\":{\"time\":1791653400", "\"current\":{\"time\":12");
  meteo::ParseResult r = meteo::parse_forecast(bad.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("range: current.time", r.error);
}

void test_rejects_mismatched_hourly_arrays() {
  WeatherModel m;
  const std::string bad = replaced("\"weather_code\":[2,0,0,", "\"weather_code\":[0,0,");
  meteo::ParseResult r = meteo::parse_forecast(bad.c_str(), m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("length: hourly arrays", r.error);
}

void test_null_probability_reads_as_zero() {
  WeatherModel m;
  const std::string s = replaced("\"precipitation_probability\":[1,", "\"precipitation_probability\":[null,");
  meteo::ParseResult r = meteo::parse_forecast(s.c_str(), m);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_EQUAL_UINT8(0, m.hourly[0].precip_prob);
}

void test_rejects_missing_section() {
  WeatherModel m;
  meteo::ParseResult r = meteo::parse_forecast("{\"error\":true,\"reason\":\"Invalid\"}", m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("missing: current.time", r.error);
}

void test_build_url() {
  char url[600];
  TEST_ASSERT_TRUE(meteo::build_url(url, sizeof(url), 51.713f, -0.455f));
  TEST_ASSERT_NOT_NULL(strstr(url, "latitude=51.713&longitude=-0.455"));
  TEST_ASSERT_NOT_NULL(strstr(url, "timeformat=unixtime"));
  TEST_ASSERT_NOT_NULL(strstr(url, "timezone=Europe%2FLondon"));
  char tiny[32];
  TEST_ASSERT_FALSE(meteo::build_url(tiny, sizeof(tiny), 51.713f, -0.455f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fixture_parses);
  RUN_TEST(test_failure_leaves_model_untouched);
  RUN_TEST(test_rejects_truncated_json);
  RUN_TEST(test_rejects_wrong_type);
  RUN_TEST(test_rejects_probability_over_100);
  RUN_TEST(test_rejects_bad_timestamp);
  RUN_TEST(test_rejects_mismatched_hourly_arrays);
  RUN_TEST(test_null_probability_reads_as_zero);
  RUN_TEST(test_rejects_missing_section);
  RUN_TEST(test_build_url);
  return UNITY_END();
}
