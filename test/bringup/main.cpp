// Boreas MS-42 — Phase 0 bring-up test (ROADMAP.md Phase 0).
//
// Exercises every peripheral the project needs, with no Boreas modules:
//   1. I2C scan on the shared bus (expect 0x51 RTC, 0x70 SHTC3)
//   2. PCF85063 RTC time and oscillator-stop flag
//   3. SHTC3 temperature / humidity
//   4. Battery voltage on the ADC
//   5. BOOT / KEY / location button press and release
//   6. U8g2 ST7305 test card, written in HPM then dropped to LPM
//   7. TF card over SDMMC 1-bit: mount (never format), write/read/delete a test file
//
// The RTC and SHTC3 are driven with raw I2C rather than SensorLib, so a
// failure here points at the hardware, not a library API.
//
// This is test code: it sends 0x38/0x39 directly. In the firmware only
// St7305Panel::endFrame() may do that.

#include <Arduino.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "pins.h"

namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr uint32_t kSerialWaitMs = 3000;
constexpr uint32_t kSpiClockHz = 24000000;  // SPEC.md §Panel
constexpr uint32_t kReportIntervalMs = 10000;
constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kBootHpmHoldMs = 2000;    // BUILD.md: test card drops to LPM after 2 s

// How long the panel stays in HPM after each write before 0x39. BOOT cycles
// through these to find the shortest hold with no visible lag (SPEC.md
// §Panel modes [verify minimum], AV-006). 0 = never drop to LPM (control).
constexpr uint32_t kHpmHoldOptionsMs[] = {100, 300, 600, 1000, 1500, 0};
constexpr size_t kHpmHoldOptionCount = sizeof(kHpmHoldOptionsMs) / sizeof(kHpmHoldOptionsMs[0]);

constexpr uint8_t kRtcAddr = 0x51;
constexpr uint8_t kShtc3Addr = 0x70;
constexpr int kBatterySamples = 16;
constexpr float kBatteryDivider = 3.0f;      // SPEC.md §Battery [verify ratio]

constexpr uint8_t kCmdHpm = 0x38;
constexpr uint8_t kCmdLpm = 0x39;

U8G2_ST7305_300X400_F_4W_HW_SPI u8g2(U8G2_R1, pins::kLcdCs, pins::kLcdDc, pins::kLcdRst);

struct RtcReading {
  bool ok;
  bool osc_stopped;
  int year, month, day, hour, minute, second;
};

struct ShtReading {
  bool ok;
  float temp_c;
  float rh;
};

struct Button {
  const char* name;
  int8_t pin;
  bool stable_down;
  bool last_raw_down;
  uint32_t changed_ms;
  uint32_t presses;
};

Button buttons[] = {
    {"BOOT", pins::kBootButton, false, false, 0, 0},
    {"KEY", pins::kKeyButton, false, false, 0, 0},
    {"LOC", pins::kLocationButton, false, false, 0, 0},
};

char i2c_found[64] = "";
RtcReading rtc_last{};
ShtReading sht_last{};
float batt_last = 0.0f;
uint32_t last_report_ms = 0;
size_t hpm_hold_index = 0;
bool lpm_pending = false;
uint32_t lpm_due_ms = 0;
uint32_t last_send_us = 0;
uint32_t loop_last_ms = 0;
uint32_t loop_max_gap_ms = 0;  // longest gap between loop() passes this report period

// ---------------------------------------------------------------- I2C

void scan_i2c() {
  char* out = i2c_found;
  size_t left = sizeof(i2c_found);
  out[0] = '\0';
  int count = 0;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      int n = snprintf(out, left, "%s0x%02X", count ? " " : "", addr);
      if (n > 0 && static_cast<size_t>(n) < left) {
        out += n;
        left -= n;
      }
      ++count;
    }
  }
  Serial.printf("I2C: %s (%d device%s)\n", count ? i2c_found : "none", count,
                count == 1 ? "" : "s");
  bool has_rtc = strstr(i2c_found, "0x51") != nullptr;
  bool has_sht = strstr(i2c_found, "0x70") != nullptr;
  if (!has_rtc) Serial.println("I2C: RTC (0x51) MISSING");
  if (!has_sht) Serial.println("I2C: SHTC3 (0x70) MISSING");
}

// ---------------------------------------------------------------- RTC

uint8_t bcd_to_bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t bin_to_bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

int month_from_build(const char* m) {
  static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* p = strstr(kMonths, m);
  return p ? static_cast<int>((p - kMonths) / 3) + 1 : 1;
}

// PCF85063 time registers start at 0x04: sec (bit 7 = OS flag), min,
// hour, day, weekday, month, year (BCD).
RtcReading read_rtc() {
  RtcReading r{};
  Wire.beginTransmission(kRtcAddr);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) return r;
  if (Wire.requestFrom(kRtcAddr, static_cast<uint8_t>(7)) != 7) return r;
  uint8_t raw[7];
  for (uint8_t& b : raw) b = Wire.read();
  r.ok = true;
  r.osc_stopped = raw[0] & 0x80;
  r.second = bcd_to_bin(raw[0] & 0x7F);
  r.minute = bcd_to_bin(raw[1] & 0x7F);
  r.hour = bcd_to_bin(raw[2] & 0x3F);
  r.day = bcd_to_bin(raw[3] & 0x3F);
  r.month = bcd_to_bin(raw[5] & 0x1F);
  r.year = 2000 + bcd_to_bin(raw[6]);
  return r;
}

// Bring-up convenience only: if the oscillator-stop flag is set, the time
// is invalid, so load the build timestamp and clear the flag (writing the
// seconds register clears OS). The firmware sets the RTC from NTP (D-007).
void set_rtc_from_build() {
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], '\0'};
  int day = atoi(__DATE__ + 4);
  int year = atoi(__DATE__ + 7);
  int hh = atoi(__TIME__);
  int mm = atoi(__TIME__ + 3);
  int ss = atoi(__TIME__ + 6);
  Wire.beginTransmission(kRtcAddr);
  Wire.write(0x04);
  Wire.write(bin_to_bcd(ss));
  Wire.write(bin_to_bcd(mm));
  Wire.write(bin_to_bcd(hh));
  Wire.write(bin_to_bcd(day));
  Wire.write(0);  // weekday unused
  Wire.write(bin_to_bcd(month_from_build(mon)));
  Wire.write(bin_to_bcd(year % 100));
  Wire.endTransmission();
}

// PCF85063 register 0x03 is a free RAM byte. The firmware uses it as a
// "set by NTP" marker (D-015), so confirm here that it holds a value.
// The original value is restored.
uint8_t rtc_read_reg(uint8_t reg) {
  Wire.beginTransmission(kRtcAddr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(kRtcAddr, static_cast<uint8_t>(1));
  return Wire.available() ? Wire.read() : 0;
}

void rtc_write_reg(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(kRtcAddr);
  Wire.write(reg);
  Wire.write(v);
  Wire.endTransmission();
}

void test_rtc_ram_byte() {
  uint8_t original = rtc_read_reg(0x03);
  bool ok = true;
  for (uint8_t pattern : {0xA5, 0x5A}) {
    rtc_write_reg(0x03, pattern);
    ok = ok && rtc_read_reg(0x03) == pattern;
  }
  rtc_write_reg(0x03, original);
  Serial.printf("RTC RAM byte %s (was 0x%02X, restored)\n", ok ? "OK" : "FAIL", original);
}

void report_rtc(const RtcReading& r) {
  if (!r.ok) {
    Serial.println("RTC FAIL no response");
    return;
  }
  Serial.printf("RTC %s %04d-%02d-%02d %02d:%02d:%02d\n",
                r.osc_stopped ? "FAIL osc-stop flag set" : "OK", r.year, r.month,
                r.day, r.hour, r.minute, r.second);
}

// ---------------------------------------------------------------- SHTC3

bool sht_command(uint16_t cmd) {
  Wire.beginTransmission(kShtc3Addr);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  return Wire.endTransmission() == 0;
}

uint8_t sht_crc(uint8_t a, uint8_t b) {
  uint8_t crc = 0xFF;
  for (uint8_t byte : {a, b}) {
    crc ^= byte;
    for (int i = 0; i < 8; ++i) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
  }
  return crc;
}

ShtReading read_sht() {
  ShtReading r{};
  if (!sht_command(0x3517)) return r;  // wake up
  delayMicroseconds(300);
  if (!sht_command(0x7866)) return r;  // normal mode, T first, no clock stretch
  delay(15);
  if (Wire.requestFrom(kShtc3Addr, static_cast<uint8_t>(6)) != 6) return r;
  uint8_t d[6];
  for (uint8_t& b : d) b = Wire.read();
  sht_command(0xB098);  // sleep
  if (sht_crc(d[0], d[1]) != d[2] || sht_crc(d[3], d[4]) != d[5]) return r;
  uint16_t t_raw = (d[0] << 8) | d[1];
  uint16_t h_raw = (d[3] << 8) | d[4];
  r.ok = true;
  r.temp_c = -45.0f + 175.0f * t_raw / 65536.0f;
  r.rh = 100.0f * h_raw / 65536.0f;
  return r;
}

void report_sht(const ShtReading& r) {
  if (r.ok)
    Serial.printf("SHTC3 OK T=%.2f RH=%.1f\n", r.temp_c, r.rh);
  else
    Serial.println("SHTC3 FAIL no response or CRC error");
}

// ---------------------------------------------------------------- TF card

// Mounts the card in SDMMC 1-bit mode and round-trips a test file. Never
// formats, and touches no file but its own.
void test_sd_card() {
  if (!SD_MMC.setPins(pins::kSdClk, pins::kSdCmd, pins::kSdD0)) {
    Serial.println("SD FAIL setPins");
    return;
  }
  if (!SD_MMC.begin("/sdcard", /*mode1bit=*/true, /*format_if_mount_failed=*/false)) {
    Serial.println("SD FAIL mount (no card, wrong pins, or not FAT)");
    return;
  }
  static const char* const kTypes[] = {"NONE", "MMC", "SD", "SDHC", "UNKNOWN"};
  const uint8_t type = SD_MMC.cardType();
  Serial.printf("SD OK type %s, card %llu MB, fs %llu MB, used %llu MB\n", kTypes[type < 5 ? type : 4],
                SD_MMC.cardSize() / (1024ULL * 1024), SD_MMC.totalBytes() / (1024ULL * 1024),
                SD_MMC.usedBytes() / (1024ULL * 1024));

  const char* path = "/boreas_sdtest.txt";
  const char* payload = "boreas bring-up sd test";
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("SD FAIL open for write");
    SD_MMC.end();
    return;
  }
  f.print(payload);
  f.close();
  f = SD_MMC.open(path, FILE_READ);
  String back = f ? f.readString() : String();
  if (f) f.close();
  const bool same = back == payload;
  const bool removed = SD_MMC.remove(path);
  Serial.printf("SD %s write/read %s, test file %s\n", same && removed ? "OK" : "FAIL", same ? "match" : "MISMATCH",
                removed ? "removed" : "NOT removed");
  SD_MMC.end();
}

// ---------------------------------------------------------------- Battery

float read_battery_v() {
  uint32_t sum_mv = 0;
  for (int i = 0; i < kBatterySamples; ++i) sum_mv += analogReadMilliVolts(pins::kBatteryAdc);
  float pin_mv = static_cast<float>(sum_mv) / kBatterySamples;
  return pin_mv * kBatteryDivider / 1000.0f;
}

// ---------------------------------------------------------------- Buttons

// Returns the button that was pressed this poll, or nullptr. Releases are
// logged but don't trigger a redraw.
Button* poll_buttons() {
  Button* pressed = nullptr;
  uint32_t now = millis();
  for (Button& b : buttons) {
    bool raw_down = digitalRead(b.pin) == LOW;
    if (raw_down != b.last_raw_down) {
      b.last_raw_down = raw_down;
      b.changed_ms = now;
    }
    if (raw_down != b.stable_down && now - b.changed_ms >= kDebounceMs) {
      b.stable_down = raw_down;
      if (raw_down) ++b.presses;
      Serial.printf("%8lu ms  %s %s (%lu presses)\n", static_cast<unsigned long>(now), b.name,
                    raw_down ? "DOWN" : "UP", static_cast<unsigned long>(b.presses));
      if (raw_down) pressed = &b;
    }
  }
  return pressed;
}

// ---------------------------------------------------------------- Panel

void draw_test_card() {
  char line[96];
  u8g2.clearBuffer();

  // Double outer frame, as SPEC.md §Layout.
  u8g2.drawFrame(3, 3, 394, 294);
  u8g2.drawFrame(5, 5, 390, 290);

  // Inverted header bar.
  u8g2.drawBox(5, 5, 390, 20);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(10, 20, "BOREAS MS-42 // BRING-UP");
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x12_tr);
  int y = 42;
  auto put = [&](const char* s) {
    u8g2.drawStr(12, y, s);
    y += 14;
  };

  snprintf(line, sizeof(line), "I2C   %s", i2c_found[0] ? i2c_found : "none");
  put(line);
  if (rtc_last.ok)
    snprintf(line, sizeof(line), "RTC   %s %04d-%02d-%02d %02d:%02d",
             rtc_last.osc_stopped ? "FAIL" : "OK", rtc_last.year, rtc_last.month,
             rtc_last.day, rtc_last.hour, rtc_last.minute);
  else
    snprintf(line, sizeof(line), "RTC   FAIL");
  put(line);
  if (sht_last.ok)
    snprintf(line, sizeof(line), "SHTC3 OK %.1f C  %.0f %%RH", sht_last.temp_c, sht_last.rh);
  else
    snprintf(line, sizeof(line), "SHTC3 FAIL");
  put(line);
  snprintf(line, sizeof(line), "BATT  %.2f V", batt_last);
  put(line);
  snprintf(line, sizeof(line), "BTN   BOOT %lu  KEY %lu  LOC %lu",
           static_cast<unsigned long>(buttons[0].presses),
           static_cast<unsigned long>(buttons[1].presses),
           static_cast<unsigned long>(buttons[2].presses));
  put(line);

  // Big numerals: checks the hero font and rotation.
  u8g2.setFont(u8g2_font_logisoso42_tn);
  u8g2.drawStr(12, 170, "-10.5");

  // Checkerboard and 1 px lines: checks pixel addressing and contrast.
  for (int cy = 0; cy < 8; ++cy)
    for (int cx = 0; cx < 8; ++cx)
      if ((cx + cy) & 1) u8g2.drawBox(250 + cx * 16, 40 + cy * 16, 16, 16);
  // Three groups of four 1 px lines at 1, 2 and 3 px gaps.
  for (int gap = 1; gap <= 3; ++gap) {
    int y0 = 180 + (gap - 1) * 24;
    for (int i = 0; i < 4; ++i) u8g2.drawHLine(250, y0 + i * (gap + 1), 100);
    snprintf(line, sizeof(line), "%dpx", gap);
    u8g2.drawStr(356, y0 + 9, line);
  }

  // Corner markers: each must be visible, proving no edge is clipped.
  u8g2.drawBox(0, 0, 3, 3);
  u8g2.drawBox(397, 0, 3, 3);
  u8g2.drawBox(0, 297, 3, 3);
  u8g2.drawBox(397, 297, 3, 3);

  u8g2.setFont(u8g2_font_6x12_tr);
  uint32_t hold = kHpmHoldOptionsMs[hpm_hold_index];
  if (hold)
    snprintf(line, sizeof(line), "HPM HOLD %lu MS   SEND %lu MS", static_cast<unsigned long>(hold),
             static_cast<unsigned long>(last_send_us / 1000));
  else
    snprintf(line, sizeof(line), "HPM HOLD: STAY IN HPM   SEND %lu MS",
             static_cast<unsigned long>(last_send_us / 1000));
  u8g2.drawStr(12, 271, line);
  u8g2.drawStr(12, 285, "KEY: redraw   BOOT: next HPM hold");
}

// HPM for the write, then LPM after hpm_hold_ms (D-006). Non-blocking: the
// switch back to LPM happens in loop(), so buttons are still polled while
// the panel is in HPM. hpm_hold_ms == 0 leaves the panel in HPM.
void send_frame(uint32_t hpm_hold_ms) {
  uint32_t t0 = micros();
  u8g2.sendF("c", kCmdHpm);
  u8g2.sendBuffer();
  last_send_us = micros() - t0;
  Serial.printf("%8lu ms  FRAME sent in %lu us, HPM hold %lu ms\n",
                static_cast<unsigned long>(millis()), static_cast<unsigned long>(last_send_us),
                static_cast<unsigned long>(hpm_hold_ms));
  lpm_pending = hpm_hold_ms != 0;
  lpm_due_ms = millis() + hpm_hold_ms;
}

void service_lpm() {
  if (lpm_pending && static_cast<int32_t>(millis() - lpm_due_ms) >= 0) {
    u8g2.sendF("c", kCmdLpm);
    lpm_pending = false;
    Serial.printf("%8lu ms  LPM\n", static_cast<unsigned long>(millis()));
  }
}

void refresh_readings() {
  rtc_last = read_rtc();
  sht_last = read_sht();
  batt_last = read_battery_v();
  report_rtc(rtc_last);
  report_sht(sht_last);
  Serial.printf("BATT %.2f V\n", batt_last);
  Serial.printf("LOOP max gap %lu ms\n", static_cast<unsigned long>(loop_max_gap_ms));
  loop_max_gap_ms = 0;
}

}  // namespace

void setup() {
  Serial.begin(kSerialBaud);
  // Never let logging stall the loop: with no host reading the USB CDC port,
  // each write otherwise waits up to 100 ms for buffer space (AV-011).
  Serial.setTxTimeoutMs(0);
  uint32_t start = millis();
  while (!Serial && millis() - start < kSerialWaitMs) delay(10);
  Serial.println("\n=== Boreas MS-42 bring-up ===");
  // Expect 16 MB flash and 8 MB PSRAM (N16R8). PSRAM 0 means the
  // memory_type / BOARD_HAS_PSRAM settings in platformio.ini are wrong.
  Serial.printf("CHIP %s rev %d, flash %lu MB, PSRAM %lu KB, reset reason %d\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)),
                static_cast<unsigned long>(ESP.getPsramSize() / 1024),
                static_cast<int>(esp_reset_reason()));

  pinMode(pins::kSpeakerAmpEnable, OUTPUT);
  digitalWrite(pins::kSpeakerAmpEnable, LOW);

  for (Button& b : buttons) pinMode(b.pin, INPUT_PULLUP);
  analogSetAttenuation(ADC_11db);  // global: per-pin form errors before the pin is analogue

  Wire.begin(pins::kI2cSda, pins::kI2cScl);
  scan_i2c();

  test_rtc_ram_byte();
  RtcReading r = read_rtc();
  if (r.ok && r.osc_stopped) {
    Serial.println("RTC osc-stop flag set; loading build time " __DATE__ " " __TIME__);
    set_rtc_from_build();
  }
  refresh_readings();
  test_sd_card();

  SPI.begin(pins::kLcdSclk, -1, pins::kLcdMosi, pins::kLcdCs);
  u8g2.setBusClock(kSpiClockHz);
  u8g2.begin();
  Serial.println("PANEL init done (driver leaves HPM)");

  draw_test_card();
  send_frame(kBootHpmHoldMs);
  Serial.println("PANEL test card sent; LPM follows after the hold");
  last_report_ms = millis();
}

void loop() {
  uint32_t now = millis();
  if (loop_last_ms && now - loop_last_ms > loop_max_gap_ms) loop_max_gap_ms = now - loop_last_ms;
  loop_last_ms = now;

  Button* pressed = poll_buttons();
  bool redraw = pressed != nullptr;
  if (pressed == &buttons[0]) {
    hpm_hold_index = (hpm_hold_index + 1) % kHpmHoldOptionCount;
    Serial.printf("HPM hold now %lu ms (0 = stay in HPM)\n",
                  static_cast<unsigned long>(kHpmHoldOptionsMs[hpm_hold_index]));
  }
  if (millis() - last_report_ms >= kReportIntervalMs) {
    last_report_ms = millis();
    refresh_readings();
    redraw = true;
  }
  if (redraw) {
    draw_test_card();
    send_frame(kHpmHoldOptionsMs[hpm_hold_index]);
  }
  service_lpm();
  delay(2);
}
