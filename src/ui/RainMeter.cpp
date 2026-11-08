#include "ui/RainMeter.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Rain region: x 256..393, y 200..278.
constexpr int16_t kLeft = kRain.x + kPad;                               // 259
constexpr int16_t kRight = static_cast<int16_t>(kRain.right() - kPad);  // 390
constexpr int16_t kTitleBaseline = 211;
constexpr int16_t kCells = 12;
constexpr int16_t kCellW = 10;
constexpr int16_t kCellGap = 1;
constexpr int16_t kMeterY = 217;
constexpr int16_t kMeterH = 20;
constexpr int16_t kScaleBaseline = 247;
constexpr Rect kAdvisory{kLeft, 252, static_cast<int16_t>(kRight - kLeft + 1), 23};
constexpr int16_t kWarnLine1 = 11;  // baselines inside the advisory box (F-019)
constexpr int16_t kWarnLine2 = 20;

const char* level_word(WarningLevel l) {
  switch (l) {
    case WarningLevel::kRed: return "RED";
    case WarningLevel::kAmber: return "AMBER";
    case WarningLevel::kYellow: return "YELLOW";
    default: return "";
  }
}

// Shortens text to fit `width` pixels. Hazard lists are cut back to whole
// hazards with a trailing '+' ("YELLOW THUNDERSTORMS+": more follow);
// anything else loses characters from the end.
void fit(u8g2_t* g, char* text, int16_t width) {
  if (text_width(g, text) <= width) return;
  for (char* plus = std::strrchr(text, '+'); plus; plus = std::strrchr(text, '+')) {
    plus[1] = '\0';
    if (text_width(g, text) <= width) return;
    plus[0] = '\0';
  }
  size_t n = std::strlen(text);
  while (n > 0 && text_width(g, text) > width) text[--n] = '\0';
}

// F-019: a Met Office warning replaces the umbrella line. Two lines: level
// and hazard ("AMBER WIND"), then the window ("UNTIL TUE 06:00" while in
// force, "FROM SUN 18:00" before). Inverted for amber and red, outlined for
// yellow.
void draw_warning(u8g2_t* g, const Warning& w, bool active) {
  char line1[40];
  int n = snprintf(line1, sizeof(line1), "%s ", level_word(w.level));
  for (const char* p = w.hazard; *p && n + 1 < static_cast<int>(sizeof(line1)); ++p) {
    if (p[0] == ',' && p[1] == ' ') {  // "RAIN, WIND" -> "RAIN+WIND"
      line1[n++] = '+';
      ++p;
    } else {
      line1[n++] = *p;
    }
  }
  line1[n] = '\0';

  char line2[24];
  const time_t when = active ? w.end : w.start;
  if (when == 0) {
    snprintf(line2, sizeof(line2), active ? "IN FORCE" : "SOON");
  } else {
    static const char* const kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    struct tm t;
    localtime_r(&when, &t);
    snprintf(line2, sizeof(line2), "%s %s %02d:%02d", active ? "UNTIL" : "FROM", kDays[t.tm_wday % 7], t.tm_hour,
             t.tm_min);
  }

  const bool loud = w.level != WarningLevel::kYellow;
  if (loud) {
    fill(g, kAdvisory);
    u8g2_SetDrawColor(g, 0);
  } else {
    u8g2_DrawFrame(g, kAdvisory.x, kAdvisory.y, kAdvisory.w, kAdvisory.h);
  }
  const int16_t inner = static_cast<int16_t>(kAdvisory.w - 6);
  u8g2_SetFont(g, fonts::kLabel);
  fit(g, line1, inner);
  u8g2_DrawStr(g, static_cast<int16_t>(kAdvisory.x + (kAdvisory.w - text_width(g, line1)) / 2),
               kAdvisory.y + kWarnLine1, line1);
  u8g2_SetFont(g, fonts::kSource);
  fit(g, line2, inner);
  u8g2_DrawStr(g, static_cast<int16_t>(kAdvisory.x + (kAdvisory.w - text_width(g, line2)) / 2),
               kAdvisory.y + kWarnLine2, line2);
  u8g2_SetDrawColor(g, 1);
}

}  // namespace

void draw_rain_meter(u8g2_t* g, const PanelState& s) {
  const DerivedModel& d = s.derived;
  const bool ok = d.weather_valid && d.trace_count > 0;

  u8g2_SetFont(g, fonts::kLabel);
  draw_right(g, kRight, kTitleBaseline, "RAIN PROB.");

  for (int16_t i = 0; i < kCells; ++i) {
    const int16_t x = static_cast<int16_t>(kLeft + i * (kCellW + kCellGap));
    if (ok && i < d.rain_segments)
      u8g2_DrawBox(g, x, kMeterY, kCellW, kMeterH);
    else
      u8g2_DrawFrame(g, x, kMeterY, kCellW, kMeterH);
  }

  // Scale: 0 and 100 at the ends, the reading in the middle.
  u8g2_SetFont(g, fonts::kSource);
  u8g2_DrawStr(g, kLeft, kScaleBaseline, "0");
  draw_right(g, kRight, kScaleBaseline, "100");
  char buf[12];
  if (ok)
    snprintf(buf, sizeof(buf), "%u %%", d.max_prob_12h);
  else
    snprintf(buf, sizeof(buf), "-- %%");
  u8g2_SetFont(g, fonts::kLabel);
  const int16_t mid = static_cast<int16_t>((kLeft + kRight - text_width(g, buf)) / 2);
  u8g2_DrawStr(g, mid, kScaleBaseline + 1, buf);

  if (s.derived.warning >= 0 && s.derived.warning < s.warnings.count) {
    draw_warning(g, s.warnings.items[s.derived.warning], s.derived.warning_active);
    return;
  }

  // Advisory: inverted when an umbrella is advised, outlined otherwise.
  // F-015 will rotate the neutral line through a table of one-liners.
  const char* text = !ok ? "NO FORECAST" : d.umbrella ? "UMBRELLA ADVISED" : "NO UMBRELLA NEEDED";
  if (ok && d.umbrella) {
    fill(g, kAdvisory);
    u8g2_SetDrawColor(g, 0);
    u8g2_SetFont(g, fonts::kHeader);
  } else {
    u8g2_DrawFrame(g, kAdvisory.x, kAdvisory.y, kAdvisory.w, kAdvisory.h);
    u8g2_SetFont(g, fonts::kLabel);
  }
  u8g2_DrawStr(g, static_cast<int16_t>(kAdvisory.x + (kAdvisory.w - text_width(g, text)) / 2),
               centred_baseline(g, kAdvisory.y, kAdvisory.h), text);
  u8g2_SetDrawColor(g, 1);
}

}  // namespace ui
