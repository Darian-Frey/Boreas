#include "ui/Readouts.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Column content below the tab: x 272..393, y 38..198.
constexpr int16_t kLeft = kReadCol.x + kPad;                               // 275
constexpr int16_t kRight = static_cast<int16_t>(kReadCol.right() - kPad);  // 390
constexpr int16_t kFirstRowTop = 38;
constexpr int16_t kRowStep = 23;
constexpr int16_t kBaselineOffset = 15;  // from row top
constexpr int16_t kLeaderOffset = 19;    // dotted leader under each row
constexpr int16_t kArrowSlot = 11;       // room right of the pressure value for the trend arrow

void hhmm(char* buf, size_t size, time_t utc) {
  struct tm t;
  localtime_r(&utc, &t);
  snprintf(buf, size, "%02d:%02d", t.tm_hour, t.tm_min);
}

}  // namespace

void draw_readouts(u8g2_t* g, const PanelState& s) {
  const DerivedModel& d = s.derived;
  const CurrentConditions& c = s.weather.current;
  const DailyForecast& day = s.weather.daily[d.today];
  const bool ok = d.weather_valid;

  char wind[16] = "--", gust[16] = "--", hum[16] = "--", press[16] = "--", uv[16] = "--", rise[8] = "--:--",
       set[8] = "--:--";
  if (ok) {
    snprintf(wind, sizeof(wind), "%s %ld", d.wind_compass, std::lround(c.wind_kmh));
    snprintf(gust, sizeof(gust), "%ld", std::lround(c.gust_kmh));
    snprintf(hum, sizeof(hum), "%u %%", c.rh);
    snprintf(press, sizeof(press), "%ld", std::lround(c.pressure_hpa));
    snprintf(uv, sizeof(uv), "%ld %s", std::lround(c.uv), d.uv_band);
    hhmm(rise, sizeof(rise), day.sunrise);
    hhmm(set, sizeof(set), day.sunset);
  }
  const char* const labels[] = {"WIND", "GUST", "HUM", "PRESS", "UV", "RISE", "SET"};
  const char* const values[] = {wind, gust, hum, press, uv, rise, set};
  constexpr int kPressRow = 3;

  u8g2_SetFont(g, fonts::kReadout);
  for (int i = 0; i < 7; ++i) {
    const int16_t top = static_cast<int16_t>(kFirstRowTop + i * kRowStep);
    const int16_t baseline = static_cast<int16_t>(top + kBaselineOffset);
    u8g2_DrawStr(g, kLeft, baseline, labels[i]);
    const int16_t value_right = static_cast<int16_t>(i == kPressRow && ok ? kRight - kArrowSlot : kRight);
    draw_right(g, value_right, baseline, values[i]);
    for (int16_t x = kLeft; x <= kRight; x += 3) u8g2_DrawPixel(g, x, top + kLeaderOffset);

    if (i == kPressRow && ok) {
      // Trend arrow in a 7x7 box beside the value; blank until known.
      const int16_t ax = static_cast<int16_t>(kRight - 7);
      const int16_t ay = static_cast<int16_t>(baseline - 8);
      switch (d.pressure_trend) {
        case PressureHistory::Trend::kRising: draw_up_arrow(g, ax, ay); break;
        case PressureHistory::Trend::kFalling: draw_down_arrow(g, ax, ay); break;
        case PressureHistory::Trend::kSteady: u8g2_DrawBox(g, ax - 1, ay + 3, 9, 2); break;
        case PressureHistory::Trend::kUnknown: break;
      }
    }
  }
}

}  // namespace ui
