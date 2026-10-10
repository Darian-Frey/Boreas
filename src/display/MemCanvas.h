// MemCanvas: a 400x300 1-bit U8g2 canvas in RAM, with no display attached.
//
// Uses only U8g2's portable C core, so it builds for the board and the
// host alike. The host preview (D-016) draws ui/ into it and writes a PNG.
// The board can draw the same scene into one and compare hash(), which
// proves both builds produce identical pixels.
#pragma once

#include <clib/u8g2.h>

#include <cstdint>

class MemCanvas {
 public:
  static constexpr int kWidth = 400;
  static constexpr int kHeight = 300;

  MemCanvas();

  // Logical 400x300 landscape canvas: the same coordinates ui/ uses on the
  // panel (U8G2_R1 there, R0 here, so no rotation is involved).
  u8g2_t* u8g2() { return &u8g2_; }

  bool pixel(int x, int y) const;

  // FNV-1a over all pixels in row-major order, one bit each, packed 8 per
  // byte from x = 0. Identical images give identical hashes on any platform.
  uint32_t hash() const;

 private:
  static constexpr int kTileRows = (kHeight + 7) / 8;  // 38: U8g2 works in 8-pixel tiles
  u8g2_t u8g2_;
  uint8_t buf_[kWidth * kTileRows];
};
