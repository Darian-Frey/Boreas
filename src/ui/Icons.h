// Condition icons, drawn from primitives in a 40x40 box (F-002). No
// bitmap assets: the shapes follow the panel's 1 px line style and need no
// conversion step.
#pragma once

#include <clib/u8g2.h>

#include <cstdint>

#include "data/Derived.h"

namespace ui {

constexpr int16_t kIconSize = 40;

// (x, y) is the top-left of the 40x40 box. Draws only inside the box.
void draw_icon(u8g2_t* g, Icon icon, int16_t x, int16_t y);

// Symbols missing from the fonts (SPEC.md §Layout), each in a 7x7 box whose
// top-left is (x, y); sized to sit beside 6x12 capitals.
void draw_delta(u8g2_t* g, int16_t x, int16_t y);
void draw_up_arrow(u8g2_t* g, int16_t x, int16_t y);
void draw_down_arrow(u8g2_t* g, int16_t x, int16_t y);

}  // namespace ui
