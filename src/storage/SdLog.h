// SdLog: append-only CSV health log on the TF card (SPEC.md §Health log).
//
// Mounts the card in SDMMC 1-bit mode and never formats it. Each append
// opens, writes and closes the file, so a power cut loses at most the line
// being written. If the card is missing or a write fails, logging pauses
// and the next append tries to remount. The panel never depends on it.
#pragma once

#include <cstddef>

class SdLog {
 public:
  // Mounts the card; returns true if it is usable. Safe to call again.
  bool begin();

  // Appends one line (no trailing newline needed) to `path`. If the file is
  // new, `header` is written first. Returns false if the line was not written.
  bool append(const char* path, const char* header, const char* line);

  bool mounted() const { return mounted_; }

 private:
  bool mounted_ = false;
};
