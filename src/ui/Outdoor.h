// ui::draw_outdoor: the EXT · OUTDOOR column (F-002): hero temperature,
// condition icon, feels-like / high / low, condition text.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_outdoor(u8g2_t* g, const PanelState& s);

}  // namespace ui
