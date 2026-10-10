// Font roles (SPEC.md §Layout). Every font in ui/ is chosen here.
// _tr fonts are ASCII only; text with a degree sign or middle dot must use
// a _tf (Latin-1) font and u8g2_DrawUTF8.
#pragma once

#include <clib/u8g2.h>

namespace ui {
namespace fonts {

inline const uint8_t* const kHeader = u8g2_font_7x14B_tr;    // badge, location, date
inline const uint8_t* const kClock = u8g2_font_9x15B_tr;     // header clock
inline const uint8_t* const kLabel = u8g2_font_6x12_tf;      // tabs, labels, values (has ° ·)
inline const uint8_t* const kSource = u8g2_font_5x7_tr;      // small tab source labels
inline const uint8_t* const kSmall = u8g2_font_profont11_tr; // footer
inline const uint8_t* const kHero = u8g2_font_logisoso50_tn;   // outdoor temperature, >= 48 px (F-002), digits only
inline const uint8_t* const kIndoor = u8g2_font_logisoso24_tn; // indoor values (digits only)

}  // namespace fonts
}  // namespace ui
