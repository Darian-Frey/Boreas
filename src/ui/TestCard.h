// ui::draw_test_card: a fixed scene that exercises the fonts, glyph
// workarounds and primitives the panel layout will use (SPEC.md §Layout).
// Used to prove the D-016 host preview matches the board pixel for pixel.
#pragma once

#include <clib/u8g2.h>

namespace ui {

void draw_test_card(u8g2_t* g);

}  // namespace ui
