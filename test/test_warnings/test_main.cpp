// Host-native tests for src/net/WarningsParse.h and derived::select_warning
// (F-019, D-021). kFeed is the real Met Office East of England feed fetched
// 2026-11-08 (Crown copyright); its area list includes the public example
// area in locations.example.h. Other cases edit it.

#include <unity.h>

#include <string>

#include "data/Derived.h"
#include "net/WarningsParse.h"

static const char* kFeed = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<rss xmlns:dc="http://purl.org/dc/elements/1.1/" version="2.0">
    <channel>
        <title>Met Office warnings for East of England</title>
        <link>https://www.metoffice.gov.uk/weather/warnings-and-advice/uk-warnings</link>
        <description>Weather warnings of severe and extreme weather from the Met Office</description>
        <language>en-gb</language>
        <copyright>(c) Crown copyright</copyright>
        <pubDate>Sun, 08 Nov 2026 10:00:18 GMT</pubDate>
        <dc:creator>webteam@metoffice.gov.uk</dc:creator>
        <item>
            <title>Yellow warning of fog affecting East of England</title>
            <link>https://www.metoffice.gov.uk/weather/warnings-and-advice/uk-warnings#?date=2026-11-08&amp;id=dd63f8cc-72a9-4311-96de-0bec87a9abae&amp;referrer=rss</link>
            <description>Yellow warning of fog affecting East of England: Bedford, Cambridgeshire, Central Bedfordshire, Essex, Hertfordshire, Luton, Norfolk, Peterborough, Southend-on-Sea, Suffolk, Thurrock valid from 1800 Sun 08 Nov to 1100 Mon 09 Nov</description>
            <guid isPermaLink="false">https://www.metoffice.gov.uk/weather/warnings-and-advice/uk-warnings#?date=2026-11-08&amp;id=dd63f8cc-72a9-4311-96de-0bec87a9abae&amp;referrer=rss&amp;region=East of England</guid>
            <enclosure length="15446" type="image/png" url="https://data.consumer-digital.api.metoffice.gov.uk/v1/warnings/rss/image/yellow-fog.png"/>
        </item>
    </channel>
</rss>)xml";

static const time_t kNow = 1794132000;      // 2026-11-08 10:00 UTC (feed pubDate)
static const time_t kFogStart = 1794160800;  // 1800 Sun 08 Nov GMT
static const time_t kFogEnd = 1794222000;    // 1100 Mon 09 Nov GMT

void setUp() {}
void tearDown() {}

static std::string edited(const std::string& from, const std::string& to) {
  std::string s(kFeed);
  size_t at = 0;
  int n = 0;
  while ((at = s.find(from, at)) != std::string::npos) {
    s.replace(at, from.size(), to);
    at += to.size();
    ++n;
  }
  TEST_ASSERT_TRUE_MESSAGE(n > 0, from.c_str());
  return s;
}

void test_url() {
  char buf[120];
  TEST_ASSERT_TRUE(warnings::build_url(buf, sizeof(buf), "ee"));
  TEST_ASSERT_EQUAL_STRING("https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/ee", buf);
  char tiny[20];
  TEST_ASSERT_FALSE(warnings::build_url(tiny, sizeof(tiny), "ee"));
}

void test_feed_parses_for_listed_area() {
  WarningModel m;
  const warnings::ParseResult r = warnings::parse(kFeed, "Hertfordshire", kNow, m);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  TEST_ASSERT_TRUE(m.valid);
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
  TEST_ASSERT_EQUAL(WarningLevel::kYellow, m.items[0].level);
  TEST_ASSERT_EQUAL_STRING("FOG", m.items[0].hazard);
  TEST_ASSERT_EQUAL_INT64(kFogStart, m.items[0].start);
  TEST_ASSERT_EQUAL_INT64(kFogEnd, m.items[0].end);
}

void test_area_match_is_whole_entry_any_case() {
  WarningModel m;
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "hertfordshire", kNow, m).ok);
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "Thurrock", kNow, m).ok);  // last entry, before "valid from"
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "Bedford", kNow, m).ok);   // first entry
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
  // Not a substring of "Central Bedfordshire" or the region name.
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "Bedfordshire", kNow, m).ok);
  TEST_ASSERT_EQUAL_UINT8(0, m.count);
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "East of England", kNow, m).ok);
  TEST_ASSERT_EQUAL_UINT8(0, m.count);
}

void test_unlisted_area_gives_valid_empty_model() {
  WarningModel m;
  m.count = 3;
  TEST_ASSERT_TRUE(warnings::parse(kFeed, "Kent", kNow, m).ok);
  TEST_ASSERT_TRUE(m.valid);
  TEST_ASSERT_EQUAL_UINT8(0, m.count);
}

void test_feed_without_items_clears_warnings() {
  const std::string s = std::string(kFeed).substr(0, std::string(kFeed).find("<item>")) + "</channel></rss>";
  WarningModel m;
  m.count = 2;
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Hertfordshire", kNow, m).ok);
  TEST_ASSERT_EQUAL_UINT8(0, m.count);
}

void test_truncated_feed_leaves_model_untouched() {
  const std::string s = std::string(kFeed).substr(0, 600);
  WarningModel m;
  m.valid = true;
  m.count = 1;
  m.items[0].level = WarningLevel::kAmber;
  const warnings::ParseResult r = warnings::parse(s.c_str(), "Hertfordshire", kNow, m);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
  TEST_ASSERT_EQUAL(WarningLevel::kAmber, m.items[0].level);
}

void test_levels_and_multi_hazard() {
  WarningModel m;
  std::string s = edited("Yellow warning of fog", "Amber warning of rain, wind");
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Hertfordshire", kNow, m).ok);
  TEST_ASSERT_EQUAL(WarningLevel::kAmber, m.items[0].level);
  TEST_ASSERT_EQUAL_STRING("RAIN, WIND", m.items[0].hazard);
  s = edited("Yellow warning", "Red warning");
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Hertfordshire", kNow, m).ok);
  TEST_ASSERT_EQUAL(WarningLevel::kRed, m.items[0].level);
}

void test_entities_decoded() {
  WarningModel m;
  const std::string s = edited("Hertfordshire", "Brighton &amp; Hove");
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Brighton & Hove", kNow, m).ok);
  TEST_ASSERT_EQUAL_UINT8(1, m.count);
}

void test_bst_window() {
  // 0600 Wed 07 Oct is BST: 05:00 UTC.
  WarningModel m;
  const std::string s = edited("from 1800 Sun 08 Nov to 1100 Mon 09 Nov", "from 1200 Tue 06 Oct to 0600 Wed 07 Oct");
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Hertfordshire", 1791288000, m).ok);
  TEST_ASSERT_EQUAL_INT64(1791284400, m.items[0].start);  // 11:00 UTC
  TEST_ASSERT_EQUAL_INT64(1791349200, m.items[0].end);
}

void test_year_rollover() {
  WarningModel m;
  const std::string s = edited("from 1800 Sun 08 Nov to 1100 Mon 09 Nov", "from 1200 Wed 30 Dec to 0600 Fri 01 Jan");
  TEST_ASSERT_TRUE(warnings::parse(s.c_str(), "Hertfordshire", 1798545600, m).ok);  // 2026-12-30 06:00 UTC
  TEST_ASSERT_EQUAL_INT64(1798632000, m.items[0].start);  // 2026-12-30 12:00 UTC
  TEST_ASSERT_EQUAL_INT64(1798783200, m.items[0].end);  // 2027-01-01 06:00 UTC
}

void test_dst_rule() {
  using warnings::detail::is_bst;
  TEST_ASSERT_FALSE(is_bst(1774744200));  // 2026-03-29 00:30 UTC
  TEST_ASSERT_TRUE(is_bst(1774751400));   // 2026-03-29 02:30 UTC
}

static Warning make(WarningLevel l, time_t start, time_t end) {
  Warning w;
  w.level = l;
  w.start = start;
  w.end = end;
  return w;
}

void test_select_by_level_then_in_force() {
  WarningModel m;
  m.valid = true;
  m.count = 3;
  m.items[0] = make(WarningLevel::kYellow, kNow - 3600, kNow + 3600);
  m.items[1] = make(WarningLevel::kAmber, kNow + 3 * 3600, kNow + 9 * 3600);  // to come
  m.items[2] = make(WarningLevel::kAmber, kNow - 600, kNow + 600);            // in force
  bool active = false;
  TEST_ASSERT_EQUAL_INT8(2, derived::select_warning(m, kNow, &active));
  TEST_ASSERT_TRUE(active);
  m.items[2].end = kNow;  // over
  TEST_ASSERT_EQUAL_INT8(1, derived::select_warning(m, kNow, &active));
  TEST_ASSERT_FALSE(active);
}

void test_select_ignores_far_future_and_invalid() {
  WarningModel m;
  m.valid = true;
  m.count = 1;
  m.items[0] = make(WarningLevel::kRed, kNow + derived::kWarningLookaheadS + 60, kNow + 30 * 3600);
  bool active = true;
  TEST_ASSERT_EQUAL_INT8(-1, derived::select_warning(m, kNow, &active));
  TEST_ASSERT_FALSE(active);
  m.items[0].start = kNow + derived::kWarningLookaheadS;
  TEST_ASSERT_EQUAL_INT8(0, derived::select_warning(m, kNow, &active));
  m.valid = false;
  TEST_ASSERT_EQUAL_INT8(-1, derived::select_warning(m, kNow, &active));
}

void test_select_earliest_of_equal() {
  WarningModel m;
  m.valid = true;
  m.count = 2;
  m.items[0] = make(WarningLevel::kYellow, kNow + 5 * 3600, kNow + 9 * 3600);
  m.items[1] = make(WarningLevel::kYellow, kNow + 2 * 3600, kNow + 9 * 3600);
  TEST_ASSERT_EQUAL_INT8(1, derived::select_warning(m, kNow, nullptr));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_url);
  RUN_TEST(test_feed_parses_for_listed_area);
  RUN_TEST(test_area_match_is_whole_entry_any_case);
  RUN_TEST(test_unlisted_area_gives_valid_empty_model);
  RUN_TEST(test_feed_without_items_clears_warnings);
  RUN_TEST(test_truncated_feed_leaves_model_untouched);
  RUN_TEST(test_levels_and_multi_hazard);
  RUN_TEST(test_entities_decoded);
  RUN_TEST(test_bst_window);
  RUN_TEST(test_year_rollover);
  RUN_TEST(test_dst_rule);
  RUN_TEST(test_select_by_level_then_in_force);
  RUN_TEST(test_select_ignores_far_future_and_invalid);
  RUN_TEST(test_select_earliest_of_equal);
  return UNITY_END();
}
