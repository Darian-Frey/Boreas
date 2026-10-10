// ui::draw_rain_meter: 12-cell rain probability meter and advisory box in
// the bottom strip (F-006).
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_rain_meter(u8g2_t* g, const PanelState& s);

}  // namespace ui
