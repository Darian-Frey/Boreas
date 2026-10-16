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
constexpr int16_t kCurveTop = 227;     // headroom above for the peak label, clear of the title
constexpr int16_t kCurveBottom = 244;  // room below for the low label, clear of the pointer
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

// Marks an extreme with a hollow square and its value, above (peak) or
// below (low) the point, on a cleared patch so the grid doesn't cross the
// digits. The label is kept inside the trace region.
void draw_extreme(u8g2_t* g, int16_t px, int16_t py, float value, bool above) {
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawBox(g, px - kPeakBox / 2, py - kPeakBox / 2 + 1, kPeakBox, kPeakBox);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawFrame(g, px - kPeakBox / 2 - 1, py - kPeakBox / 2, kPeakBox + 2, kPeakBox + 2);

  char buf[12];
  const long v = std::lround(value);
  snprintf(buf, sizeof(buf), "%ld\xC2\xB0", v == 0 ? 0L : v);
  const int16_t w = text_width(g, buf);
  int16_t lx = static_cast<int16_t>(px - w / 2);
  if (lx < kTrace.x + kPad) lx = kTrace.x + kPad;
  if (lx + w > kTrace.right() - kPad) lx = static_cast<int16_t>(kTrace.right() - kPad - w);
  // Frame spans py-2 .. py+4. Digits are 7 px tall and the degree sign
  // reaches 9 px above the baseline, so a low label sits 10 px below the frame.
  const int16_t baseline = above ? static_cast<int16_t>(py - kPeakBox / 2 - 3) : static_cast<int16_t>(py + 14);
  u8g2_SetDrawColor(g, 0);
  u8g2_DrawBox(g, lx - 1, baseline - 9, w + 2, 10);
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawUTF8(g, lx, baseline, buf);
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

  // Peak above and low below (F-005); a flat trace shows only the peak.
  draw_extreme(g, point_x(d.trace_peak), point_y(hi, lo, hi), hi, true);
  if (hi - lo >= 0.05f) draw_extreme(g, point_x(d.trace_low), point_y(lo, lo, hi), lo, false);

  // Pointer under the plot at the current time within the first hour.
  const float frac = static_cast<float>(s.now_utc - h[0].time) / 3600.0f;
  if (s.clock_valid && frac >= 0.0f && frac < 1.0f) {
    const int16_t nx = static_cast<int16_t>(std::lround(kPlotX0 + frac * kHourStep));
    u8g2_DrawTriangle(g, nx, kPointerY, nx - 3, kPointerY + 4, nx + 3, kPointerY + 4);
  }

  // Hour labels (local), centred under each point.
  char buf[4];
  for (int i = 0; i < n; ++i) {
    struct tm t;
    localtime_r(&h[i].time, &t);
    snprintf(buf, sizeof(buf), "%02d", t.tm_hour);
    u8g2_DrawStr(g, static_cast<int16_t>(point_x(i) - text_width(g, buf) / 2), kHourBaseline, buf);
  }
}

}  // namespace ui
