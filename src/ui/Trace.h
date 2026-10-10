// ui::draw_trace: 12 h temperature trace in the bottom strip (F-005):
// dotted grid, hourly polyline, current-time pointer, labelled peak, hour
// labels.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_trace(u8g2_t* g, const PanelState& s);

}  // namespace ui
