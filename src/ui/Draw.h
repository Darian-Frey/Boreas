// Small drawing helpers shared by ui/ regions. Pure pixel maths over
// U8g2's C API.
#pragma once

#include <clib/u8g2.h>

#include "ui/grid.h"

namespace ui {

// Baseline that vertically centres the current font's capitals in a band.
inline int16_t centred_baseline(u8g2_t* g, int16_t top, int16_t height) {
  const int16_t ascent = u8g2_GetAscent(g);
  return static_cast<int16_t>(top + (height + ascent) / 2);
}

inline int16_t text_width(u8g2_t* g, const char* utf8) {
  return static_cast<int16_t>(u8g2_GetUTF8Width(g, utf8));
}

// Draws UTF-8 text with its right edge on `right` (inclusive); returns the x it started at.
inline int16_t draw_right(u8g2_t* g, int16_t right, int16_t baseline, const char* utf8) {
  const int16_t x = static_cast<int16_t>(right + 1 - text_width(g, utf8));
  u8g2_DrawUTF8(g, x, baseline, utf8);
  return x;
}

inline void fill(u8g2_t* g, const grid::Rect& r) { u8g2_DrawBox(g, r.x, r.y, r.w, r.h); }

}  // namespace ui
