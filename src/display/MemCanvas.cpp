#include "display/MemCanvas.h"

namespace {

// A display that only exists in memory: no interface, no commands. The
// full frame buffer is read directly, so tile transfers are ignored.
const u8x8_display_info_t kDisplayInfo = {
    /* chip_enable_level = */ 0,
    /* chip_disable_level = */ 1,
    /* post_chip_enable_wait_ns = */ 0,
    /* pre_chip_disable_wait_ns = */ 0,
    /* reset_pulse_width_ms = */ 0,
    /* post_reset_wait_ms = */ 0,
    /* sda_setup_time_ns = */ 0,
    /* sck_pulse_width_ns = */ 0,
    /* sck_clock_hz = */ 0,
    /* spi_mode = */ 0,
    /* i2c_bus_clock_100kHz = */ 0,
    /* data_setup_time_ns = */ 0,
    /* write_pulse_width_ns = */ 0,
    /* tile_width = */ MemCanvas::kWidth / 8,
    /* tile_height = */ (MemCanvas::kHeight + 7) / 8,
    /* default_x_offset = */ 0,
    /* flipmode_x_offset = */ 0,
    /* pixel_width = */ MemCanvas::kWidth,
    /* pixel_height = */ MemCanvas::kHeight,
};

uint8_t mem_display_cb(u8x8_t* u8x8, uint8_t msg, uint8_t, void*) {
  switch (msg) {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &kDisplayInfo);
      break;
    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      break;
    default:
      break;
  }
  return 1;
}

}  // namespace

MemCanvas::MemCanvas() : u8g2_(), buf_() {
  u8g2_SetupDisplay(&u8g2_, mem_display_cb, u8x8_cad_empty, u8x8_dummy_cb, u8x8_dummy_cb);
  u8g2_SetupBuffer(&u8g2_, buf_, kTileRows, u8g2_ll_hvline_vertical_top_lsb, &u8g2_cb_r0);
  u8g2_InitDisplay(&u8g2_);
  u8g2_ClearBuffer(&u8g2_);
}

// vertical_top_lsb: each byte is an 8-pixel column, bit 0 at the top.
bool MemCanvas::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return false;
  return (buf_[(y / 8) * kWidth + x] >> (y % 8)) & 1;
}

uint32_t MemCanvas::hash() const {
  uint32_t h = 2166136261u;
  for (int y = 0; y < kHeight; ++y) {
    for (int x0 = 0; x0 < kWidth; x0 += 8) {
      uint8_t byte = 0;
      for (int b = 0; b < 8; ++b)
        if (pixel(x0 + b, y)) byte |= static_cast<uint8_t>(0x80 >> b);
      h = (h ^ byte) * 16777619u;
    }
  }
  return h;
}
