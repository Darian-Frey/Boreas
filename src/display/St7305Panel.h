// St7305Panel — the only owner of the ST7305 RLCD (ARCHITECTURE.md §display).
//
// Wraps the U8g2 full-buffer driver, the SPI prelude and the HPM/LPM power
// modes. ui/ draws into canvas(); app/ calls end_frame() to show it.
// end_frame() is the only place in the firmware that sends 0x38/0x39
// (D-006, AV-006, SPEC.md §Panel modes).
#pragma once

#include <U8g2lib.h>

#include <cstdint>

class St7305Panel {
 public:
  // Initialises SPI and the controller, then shows a cleared frame and
  // leaves the panel in LPM. Call once at boot, before any drawing.
  void begin();

  // The 400x300 landscape (U8G2_R1) 1-bit buffer to draw into. Drawing
  // only touches RAM; nothing reaches the panel until end_frame().
  U8G2& canvas();

  // The same canvas through U8g2's C API. ui/ draws through this so the
  // identical code builds for the host preview (D-016).
  u8g2_t* raw();

  // Self-heal (D-019, BUG-004): re-runs the controller's full start-up
  // (hardware reset pulse + init sequence) without touching SPI set-up.
  // Clears panel RAM and the canvas, and leaves the panel in HPM: draw and
  // end_frame() straight afterwards. Returns how long it took, in ms.
  uint32_t reinit();

  // Sends the buffer: HPM (0x38) -> sendBuffer() -> hold -> LPM (0x39).
  // Blocks for the hold (kHpmHoldMs), so the panel is always in LPM on
  // return; safe to sleep straight after.
  void end_frame();

  // Duration of the last sendBuffer() in microseconds (Diag page).
  uint32_t last_send_us() const { return last_send_us_; }

 private:
  uint32_t last_send_us_ = 0;
};
