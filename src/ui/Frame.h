// ui::draw_frame: the fixed chrome of the Main page (F-001): double border,
// inverted header (badge, location, date, clock), column rules and tabs,
// bottom-strip and footer rules. Regions draw inside it.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_frame(u8g2_t* g, const PanelState& s);

}  // namespace ui
