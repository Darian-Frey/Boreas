#include "storage/SdLog.h"

#include <SD_MMC.h>

#include <cstring>

#include "pins.h"

bool SdLog::begin() {
  if (mounted_) return true;
  if (!SD_MMC.setPins(pins::kSdClk, pins::kSdCmd, pins::kSdD0)) return false;
  // 1-bit mode: only CMD, CLK and D0 are wired. Never format: the card may
  // hold the user's files.
  mounted_ = SD_MMC.begin("/sdcard", /*mode1bit=*/true, /*format_if_mount_failed=*/false);
  return mounted_;
}

bool SdLog::append(const char* path, const char* header, const char* line) {
  if (!mounted_ && !begin()) return false;

  // Create the parent directory on first use.
  const char* slash = std::strrchr(path, '/');
  if (slash && slash != path) {
    char dir[64];
    const size_t n = static_cast<size_t>(slash - path);
    if (n < sizeof(dir)) {
      std::memcpy(dir, path, n);
      dir[n] = '\0';
      if (!SD_MMC.exists(dir)) SD_MMC.mkdir(dir);
    }
  }

  const bool is_new = !SD_MMC.exists(path);
  File f = SD_MMC.open(path, FILE_APPEND);
  if (!f) {
    // Card pulled or failed: drop the mount and retry on the next append.
    SD_MMC.end();
    mounted_ = false;
    return false;
  }
  bool ok = true;
  if (is_new && header) ok = f.println(header) > 0;
  ok = ok && f.println(line) > 0;
  f.close();
  if (!ok) {
    SD_MMC.end();
    mounted_ = false;
  }
  return ok;
}

int32_t SdLog::dump(const char* path, Print& out) {
  if (!mounted_ && !begin()) return -1;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return -1;
  uint8_t buf[512];
  int32_t total = 0;
  while (f.available()) {
    const size_t n = f.read(buf, sizeof(buf));
    if (n == 0) break;
    out.write(buf, n);
    total += static_cast<int32_t>(n);
  }
  f.close();
  return total;
}

int32_t SdLog::size(const char* path) {
  if (!mounted_ && !begin()) return -1;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return -1;
  const int32_t n = static_cast<int32_t>(f.size());
  f.close();
  return n;
}
