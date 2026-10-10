// ui::draw_indoor: the INT · INDOOR column (F-003): temperature, humidity,
// 10-segment RH meter with the comfort band, delta in/out and dew point,
// comfort status box.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_indoor(u8g2_t* g, const PanelState& s);

}  // namespace ui
