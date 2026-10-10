// ui::draw_readouts: the READOUTS column (F-004): seven rows of label and
// right-aligned value over dotted leaders: wind, gust, humidity, pressure
// with trend arrow, UV with band, sunrise, sunset.
#pragma once

#include <clib/u8g2.h>

#include "data/PanelState.h"

namespace ui {

void draw_readouts(u8g2_t* g, const PanelState& s);

}  // namespace ui
