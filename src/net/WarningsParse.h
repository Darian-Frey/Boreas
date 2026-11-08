// Met Office regional warnings RSS → WarningModel (F-019, D-021).
//
// Header-only and free of Arduino calls: the same code parses the feed on
// the board and fixtures on the host (test/test_warnings). The feed is small
// RSS: each <item> has a <title> such as
//   "Yellow warning of fog affecting East of England"
// and a <description> listing the areas and the validity window:
//   "...affecting East of England: Bedford, Hertfordshire, ... valid from
//    1800 Sun 08 Nov to 1100 Mon 09 Nov"
// Only warnings whose area list contains `area` (whole entry, any case) are
// kept. Times are UK local; they're converted to UTC with the UK DST rule.
#pragma once

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "data/Models.h"
#include "sensors/CivilTime.h"

namespace warnings {

inline bool build_url(char* buf, size_t size, const char* region) {
  const int n = snprintf(buf, size, "https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/%s", region);
  return n > 0 && static_cast<size_t>(n) < size;
}

struct ParseResult {
  bool ok = false;
  char error[40] = "";
};

namespace detail {

// Last Sunday of a month, as a day of the month.
inline int last_sunday(int year, int month) {
  const int days_in = month == 3 || month == 10 ? 31 : 30;  // only used for March and October
  const time_t last = civil::to_utc({year, month, days_in, 12, 0, 0});
  return days_in - civil::weekday(last);  // weekday 0 = Sunday
}

// UK: BST (UTC+1) from 01:00 UTC on the last Sunday of March to 01:00 UTC
// on the last Sunday of October.
inline bool is_bst(time_t utc) {
  const int y = civil::from_utc(utc).year;
  const time_t start = civil::to_utc({y, 3, last_sunday(y, 3), 1, 0, 0});
  const time_t end = civil::to_utc({y, 10, last_sunday(y, 10), 1, 0, 0});
  return utc >= start && utc < end;
}

inline time_t uk_local_to_utc(int year, int month, int day, int hour, int minute) {
  const time_t as_if_utc = civil::to_utc({year, month, day, hour, minute, 0});
  return is_bst(as_if_utc - 3600) ? as_if_utc - 3600 : as_if_utc;
}

inline int month_number(const char* m) {
  static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; ++i)
    if (std::strncmp(m, kMonths[i], 3) == 0) return i + 1;
  return 0;
}

// "1800 Sun 08 Nov" → UTC. The feed has no year: take the one that puts the
// time nearest to now.
inline time_t parse_when(const char* p, time_t now_utc) {
  int hhmm = 0, day = 0;
  char wday[4] = "", mon[4] = "";
  if (std::sscanf(p, "%4d %3s %2d %3s", &hhmm, wday, &day, mon) != 4) return 0;
  const int month = month_number(mon);
  if (month == 0 || day < 1 || day > 31 || hhmm / 100 > 23 || hhmm % 100 > 59) return 0;
  const int year = civil::from_utc(now_utc).year;
  time_t best = 0;
  for (int y = year - 1; y <= year + 1; ++y) {
    const time_t t = uk_local_to_utc(y, month, day, hhmm / 100, hhmm % 100);
    const time_t d = t > now_utc ? t - now_utc : now_utc - t;
    const time_t bd = best > now_utc ? best - now_utc : now_utc - best;
    if (best == 0 || d < bd) best = t;
  }
  return best;
}

// Copies the text of the first <tag>…</tag> in [begin, end) into out,
// decoding the few XML entities the feed uses. Returns false if absent.
inline bool tag_text(const char* begin, const char* end, const char* tag, char* out, size_t size) {
  char open[24], close[24];
  snprintf(open, sizeof(open), "<%s>", tag);
  snprintf(close, sizeof(close), "</%s>", tag);
  const char* a = std::strstr(begin, open);
  if (!a || a >= end) return false;
  a += std::strlen(open);
  const char* b = std::strstr(a, close);
  if (!b || b > end) return false;
  size_t n = 0;
  for (const char* p = a; p < b && n + 1 < size; ++p) {
    if (*p == '&') {
      static const struct {
        const char* ent;
        char ch;
      } kEnt[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}, {"&#39;", '\''}};
      bool done = false;
      for (const auto& e : kEnt) {
        const size_t len = std::strlen(e.ent);
        if (static_cast<size_t>(b - p) >= len && std::strncmp(p, e.ent, len) == 0) {
          out[n++] = e.ch;
          p += len - 1;
          done = true;
          break;
        }
      }
      if (done) continue;
    }
    out[n++] = *p;
  }
  out[n] = '\0';
  return true;
}

inline bool equal_nocase(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}

// Is `area` one of the comma-separated entries between ": " and " valid from"?
inline bool lists_area(const char* desc, const char* area) {
  const char* list = std::strstr(desc, ": ");
  if (!list) return false;
  list += 2;
  const char* stop = std::strstr(list, " valid from");
  if (!stop) stop = list + std::strlen(list);
  const size_t want = std::strlen(area);
  const char* p = list;
  while (p < stop) {
    while (p < stop && (*p == ' ' || *p == ',')) ++p;
    const char* q = p;
    while (q < stop && *q != ',') ++q;
    const char* e = q;
    while (e > p && e[-1] == ' ') --e;
    if (static_cast<size_t>(e - p) == want && equal_nocase(p, area, want)) return true;
    p = q;
  }
  return false;
}

}  // namespace detail

// Parses one feed. On success replaces out's items with the matching
// warnings (up to kMaxWarnings) and sets out.valid; on failure leaves out
// untouched. The caller stamps fetched_utc.
inline ParseResult parse(const char* xml, const char* area, time_t now_utc, WarningModel& out) {
  ParseResult r;
  if (!std::strstr(xml, "<channel>") || !std::strstr(xml, "</channel>")) {
    snprintf(r.error, sizeof(r.error), "not a complete RSS feed");
    return r;
  }
  using namespace detail;
  WarningModel m = out;
  m.count = 0;
  for (const char* item = std::strstr(xml, "<item>"); item; item = std::strstr(item + 6, "<item>")) {
    const char* item_end = std::strstr(item, "</item>");
    if (!item_end) break;
    char title[160], desc[1024];
    if (!tag_text(item, item_end, "title", title, sizeof(title)) ||
        !tag_text(item, item_end, "description", desc, sizeof(desc)))
      continue;
    if (!lists_area(desc, area)) continue;

    Warning w;
    if (std::strncmp(title, "Red", 3) == 0) w.level = WarningLevel::kRed;
    else if (std::strncmp(title, "Amber", 5) == 0) w.level = WarningLevel::kAmber;
    else if (std::strncmp(title, "Yellow", 6) == 0) w.level = WarningLevel::kYellow;
    else continue;

    const char* h = std::strstr(title, "warning of ");
    const char* he = std::strstr(title, " affecting");
    if (h && he && he > h) {
      h += 11;
      size_t n = 0;
      for (const char* p = h; p < he && n + 1 < sizeof(w.hazard); ++p)
        w.hazard[n++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
      w.hazard[n] = '\0';
    }

    const char* from = std::strstr(desc, "valid from ");
    const char* to = from ? std::strstr(from, " to ") : nullptr;
    if (from) w.start = parse_when(from + 11, now_utc);
    if (to) w.end = parse_when(to + 4, now_utc);

    if (m.count < kMaxWarnings) m.items[m.count++] = w;
  }
  m.valid = true;
  out = m;
  r.ok = true;
  return r;
}

}  // namespace warnings
