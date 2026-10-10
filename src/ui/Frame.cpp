#include "ui/Frame.h"

#include <cstdio>
#include <ctime>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

constexpr const char* kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
constexpr const char* kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                   "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
constexpr int16_t kBadgeSize = 6;
constexpr int16_t kClockGap = 8;  // between date and clock

void draw_header(u8g2_t* g, const PanelState& s) {
  fill(g, kHeader);
  u8g2_SetDrawColor(g, 0);

  // Left: square badge, then BOREAS MS-42 // <LOCATION>.
  const int16_t left = kHeader.x + kPad + 2;
  u8g2_DrawBox(g, left, kHeader.y + (kHeader.h - kBadgeSize) / 2, kBadgeSize, kBadgeSize);
  u8g2_SetFont(g, fonts::kHeader);
  char title[48];
  snprintf(title, sizeof(title), "BOREAS MS-42 // %s", s.location_name);
  u8g2_DrawUTF8(g, left + kBadgeSize + 5, centred_baseline(g, kHeader.y, kHeader.h), title);

  // Right: date then clock, local time.
  char date[16] = "--- -- ---";
  char clock[12] = "--:--";
  if (s.clock_valid) {
    struct tm t;
    localtime_r(&s.now_utc, &t);
    snprintf(date, sizeof(date), "%s %02d %s", kDays[t.tm_wday], t.tm_mday, kMonths[t.tm_mon]);
    if (s.show_seconds)
      snprintf(clock, sizeof(clock), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    else
      snprintf(clock, sizeof(clock), "%02d:%02d", t.tm_hour, t.tm_min);
  } else if (s.show_seconds) {
    snprintf(clock, sizeof(clock), "--:--:--");
  }
  const int16_t right = static_cast<int16_t>(kHeader.right() - kPad - 2);
  u8g2_SetFont(g, fonts::kClock);
  const int16_t clock_x = draw_right(g, right, centred_baseline(g, kHeader.y, kHeader.h), clock);
  u8g2_SetFont(g, fonts::kHeader);
  draw_right(g, static_cast<int16_t>(clock_x - kClockGap - 1), centred_baseline(g, kHeader.y, kHeader.h), date);

  u8g2_SetDrawColor(g, 1);
}

// Inverted tab across the top of a column: title left, optional source right.
void draw_tab(u8g2_t* g, const Rect& col, const char* title, const char* source) {
  const Rect tab{static_cast<int16_t>(col.x + kTabInset), col.y, static_cast<int16_t>(col.w - 2 * kTabInset), kTabHeight};
  fill(g, tab);
  u8g2_SetDrawColor(g, 0);
  u8g2_SetFont(g, fonts::kLabel);
  u8g2_DrawUTF8(g, tab.x + 2, centred_baseline(g, tab.y, tab.h), title);
  if (source && *source) {
    u8g2_SetFont(g, fonts::kSource);
    draw_right(g, static_cast<int16_t>(tab.right() - 2), centred_baseline(g, tab.y, tab.h), source);
  }
  u8g2_SetDrawColor(g, 1);
}

}  // namespace

void draw_frame(u8g2_t* g, const PanelState& s) {
  u8g2_SetFontMode(g, 1);
  u8g2_SetDrawColor(g, 1);

  u8g2_DrawFrame(g, kOuterBorder.x, kOuterBorder.y, kOuterBorder.w, kOuterBorder.h);
  u8g2_DrawFrame(g, kInnerBorder.x, kInnerBorder.y, kInnerBorder.w, kInnerBorder.h);
  draw_header(g, s);

  // Column rules span the body; horizontal rules span the inner width.
  u8g2_DrawVLine(g, kColRuleX1, kBodyTop, kBodyHeight);
  u8g2_DrawVLine(g, kColRuleX2, kBodyTop, kBodyHeight);
  u8g2_DrawHLine(g, kStrip.x, kBodyRuleY, kStrip.w);
  u8g2_DrawHLine(g, kStrip.x, kStripRuleY, kStrip.w);

  draw_tab(g, kExtCol, "EXT \xC2\xB7 OUTDOOR", "OPEN-METEO");
  draw_tab(g, kIntCol, "INT \xC2\xB7 INDOOR", "SHTC3");
  draw_tab(g, kReadCol, "READOUTS", nullptr);
}

}  // namespace ui
