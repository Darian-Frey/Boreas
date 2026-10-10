#include "ui/Trace.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include "ui/Draw.h"
#include "ui/Fonts.h"
#include "ui/grid.h"

namespace ui {

namespace {

using namespace grid;

// Trace region: x 6..255, y 200..278.
constexpr int16_t kTitleBaseline = 211;
constexpr int16_t kPlotX0 = 12;    // first point (current hour)
constexpr int16_t kHourStep = 19;  // px per hour; 12 steps end at x 240
constexpr int16_t kPlotTop = 218;
constexpr int16_t kPlotBottom = 258;
constexpr int16_t kCurveTop = 226;  // headroom above the curve for the peak label
constexpr int16_t kCurveBottom = 255;
constexpr int16_t kGridStep = 8;
constexpr int16_t kPointerY = 260;
constexpr int16_t kHourBaseline = 274;
constexpr int16_t kPeakBox = 5;

int16_t point_x(int i) { return static_cast<int16_t>(kPlotX0 + i * kHourStep); }

int16_t point_y(float t, float lo, float hi) {
  if (hi - lo < 0.05f) return (kCurveTop + kCurveBottom) / 2;
  const float f = (t - lo) / (hi - lo);
  return static_cast<int16_t>(std::lround(kCurveBottom - f * (kCurveBottom - kCurveTop)));
}

}  // namespace

void draw_trace(u8g2_t* g, const PanelState& s) {
  const DerivedModel& d = s.derived;
  u8g2_SetFont(g, fonts::kLabel);
  u8g2_DrawUTF8(g, kTrace.x + kPad, kTitleBaseline, "TEMP \xC2\xB7 NEXT 12 H");

  const int n = d.trace_count;
  const int16_t last_x = point_x(12);

  // Dotted grid: a dot at every hour column on each grid row.
  for (int16_t y = kPlotTop; y <= kPlotBottom; y = static_cast<int16_t>(y + kGridStep))
    for (int i = 0; i <= 12; ++i) u8g2_DrawPixel(g, point_x(i), y);

  if (n < 2) {
    // Message on a cleared patch so the grid dots don't run through it.
    const char* msg = "NO FORECAST";
    const int16_t w = text_width(g, msg);
    const int16_t x = static_cast<int16_t>((kPlotX0 + last_x - w) / 2);
    const int16_t baseline = centred_baseline(g, kPlotTop, kPlotBottom - kPlotTop);
    u8g2_SetDrawColor(g, 0);
    u8g2_DrawBox(g, x - 8, baseline - u8g2_GetAscent(g) - 4, w + 16, u8g2_GetAscent(g) + 8);
    u8g2_SetDrawColor(g, 1);
    u8g2_DrawStr(g, x, baseline, msg);
    return;
  }

  const HourlyForecast* h = &s.weather.hourly[d.first_hour];
  const float lo = d.trace_min_c, hi = d.trace_max_c;

  // Curve, 2 px thick.
  for (int i = 0; i + 1 < n; ++i) {
    const int16_t x0 = point_x(i), x1 = point_x(i + 1);
    const int16_t y0 = point_y(h[i].temp_c, lo, hi), y1 = point_y(h[i + 1].temp_c, lo, hi);
    u8g2_DrawLine(g, x0, y0, x1, y1);
    u8g2_DrawLine(g, x0, y0 + 1, x1, y1 + 1);
  }

  // Peak: hollow square and its value above, kept inside the region.
  const int16_t px = point_x(d.trace_peak), py = point_y(hi, lo, hi);
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawBox(g, px - kPeakBox / 2, py - kPeakBox / 2 + 1, kPeakBox, kPeakBox);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawFrame(g, px - kPeakBox / 2 - 1, py - kPeakBox / 2, kPeakBox + 2, kPeakBox + 2);
  char buf[12];
  snprintf(buf, sizeof(buf), "%ld\xC2\xB0", std::lround(hi) == 0 ? 0L : std::lround(hi));
  const int16_t w = text_width(g, buf);
  int16_t lx = static_cast<int16_t>(px - w / 2);
  if (lx < kTrace.x + kPad) lx = kTrace.x + kPad;
  if (lx + w > kTrace.right() - kPad) lx = static_cast<int16_t>(kTrace.right() - kPad - w);
  u8g2_DrawUTF8(g, lx, static_cast<int16_t>(py - kPeakBox / 2 - 3), buf);

  // Pointer under the plot at the current time within the first hour.
  const float frac = static_cast<float>(s.now_utc - h[0].time) / 3600.0f;
  if (s.clock_valid && frac >= 0.0f && frac < 1.0f) {
    const int16_t nx = static_cast<int16_t>(std::lround(kPlotX0 + frac * kHourStep));
    u8g2_DrawTriangle(g, nx, kPointerY, nx - 3, kPointerY + 4, nx + 3, kPointerY + 4);
  }

  // Hour labels (local), centred under each point.
  for (int i = 0; i < n; ++i) {
    struct tm t;
    localtime_r(&h[i].time, &t);
    snprintf(buf, sizeof(buf), "%02d", t.tm_hour);
    u8g2_DrawStr(g, static_cast<int16_t>(point_x(i) - text_width(g, buf) / 2), kHourBaseline, buf);
  }
}

}  // namespace ui
