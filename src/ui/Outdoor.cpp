#include "ui/Outdoor.h"

#include <cmath>
#include <cstdio>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Column content below the tab: x 6..149, y 38..198.
constexpr int16_t kLeft = kExtCol.x + kPad;                                  // 9
constexpr int16_t kRight = static_cast<int16_t>(kExtCol.right() - kPad);     // 146
constexpr int16_t kHeroBaseline = 92;  // logisoso50: 50 px digits, tops at ~42
constexpr int16_t kIconY = 100;
constexpr int16_t kStatsX = kLeft + kIconSize + 6;
constexpr int16_t kStatsTop = 104;
constexpr int16_t kStatsStep = 13;
constexpr int16_t kRuleY = 150;
constexpr int16_t kConditionBaseline = 168;

// Whole degrees, never "-0".
int whole(float c) {
  const long r = std::lround(c);
  return r == 0 ? 0 : static_cast<int>(r);
}

}  // namespace

void draw_outdoor(u8g2_t* g, const PanelState& s) {
  const DerivedModel& d = s.derived;
  const CurrentConditions& c = s.weather.current;
  char buf[24];

  // Hero temperature, then a drawn degree sign (the font has digits only) and C.
  u8g2_SetFont(g, fonts::kHero);
  if (d.weather_valid)
    snprintf(buf, sizeof(buf), "%d", whole(c.temp_c));
  else
    snprintf(buf, sizeof(buf), "--");
  u8g2_DrawStr(g, kLeft, kHeroBaseline, buf);
  const int16_t after = static_cast<int16_t>(kLeft + u8g2_GetStrWidth(g, buf) + 6);
  u8g2_DrawCircle(g, after + 4, kHeroBaseline - 44, 4, U8G2_DRAW_ALL);
  u8g2_DrawCircle(g, after + 4, kHeroBaseline - 44, 3, U8G2_DRAW_ALL);
  u8g2_SetFont(g, fonts::kClock);
  u8g2_DrawStr(g, after + 11, kHeroBaseline - 30, "C");

  // Icon left, feels / high / low stacked right.
  draw_icon(g, d.weather_valid ? d.icon : Icon::kNone, kLeft, kIconY);
  u8g2_SetFont(g, fonts::kLabel);
  const int16_t ascent = u8g2_GetAscent(g);
  const DailyForecast& today = s.weather.daily[0];
  const struct {
    const char* label;
    float value;
  } rows[] = {{"FEELS", c.feels_c}, {"HI", today.max_c}, {"LO", today.min_c}};
  for (int i = 0; i < 3; ++i) {
    const int16_t baseline = static_cast<int16_t>(kStatsTop + ascent + i * kStatsStep);
    u8g2_DrawStr(g, kStatsX, baseline, rows[i].label);
    if (d.weather_valid)
      snprintf(buf, sizeof(buf), "%d\xC2\xB0", whole(rows[i].value));
    else
      snprintf(buf, sizeof(buf), "--");
    draw_right(g, kRight, baseline, buf);
  }

  // Dotted rule, then the condition text.
  for (int16_t x = kLeft; x <= kRight; x += 3) u8g2_DrawPixel(g, x, kRuleY);
  u8g2_SetFont(g, fonts::kHeader);
  u8g2_DrawStr(g, kLeft, kConditionBaseline, d.weather_valid ? d.condition : "NO DATA");
}

}  // namespace ui
