#include "ui/RainMeter.h"

#include <cstdio>

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
