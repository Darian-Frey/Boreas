# Build

From a sealed Waveshare box to a flashed Boreas. Sections 1–3 are the one-time blank-board path; 4 onward is day-to-day.

## 1. Supported platforms

- **Host:** Linux x86-64 (developed on Linux Mint 21/22). macOS should work unchanged; Windows needs the Espressif USB driver and is untested.
- **Target:** Waveshare ESP32-S3-RLCD-4.2 only. Other ESP32-S3 boards will build but drive the wrong pins.

## 2. First power-on (brand-new board)

1. **Unbox.** You should have: the board with panel fitted, two acrylic stand pieces, the 18650 cell (with-battery SKU), an 8 Ω speaker on an MX1.25 lead, a screwdriver. The panel has a protective film — leave it on until the enclosure is done.
2. **Fit the cell.** Observe polarity marks on the holder. If the **WRN** LED lights, the cell is reversed — remove it immediately. Don't press on the screen while seating the cell or the USB cable.
3. **Fit an RTC backup cell** (optional now, needed for D-007): PH1.0 header, rechargeable only (MS621FE or equivalent). Never a non-rechargeable coin cell — the board charges it.
4. **Power on.** Single click of **PWR**. The factory firmware runs Waveshare's demo (expect a splash and possibly the voice-assistant UI). Note what it does in BUGS.md/CHANGELOG if anything is odd; it's a useful "the panel works" baseline. Long-press PWR to switch off.
5. **Charge.** USB-C in; **CHG** lit while charging, off when full. Leave it to charge fully once before relying on the battery gauge.

## 3. Host setup

### 3.1 USB serial

The S3 uses its native USB-Serial/JTAG; no driver needed on Linux.

```bash
sudo usermod -aG dialout $USER     # then log out and back in
# Ubuntu/Mint ship brltty, which grabs some serial devices. If the board
# appears then vanishes from dmesg, remove it:
sudo apt remove brltty
```

Plug in; `ls /dev/ttyACM*` should show the board (usually `/dev/ttyACM0`). `dmesg | tail` shows "Espressif USB JTAG/serial debug unit".

### 3.2 PlatformIO Core

```bash
python3 -m pip install --user platformio     # or: pipx install platformio
pio --version                                # ≥ 6.1
```

The **pioarduino** platform (arduino-esp32 3.x) is pulled automatically by `platformio.ini`; nothing else to install. If you prefer the VS Code / Antigravity PlatformIO extension, it uses the same core.

### 3.3 `platformio.ini` (reference copy)

```ini
[env:boreas]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600

board_build.flash_mode = qio
board_build.arduino.memory_type = qio_opi      ; 16 MB flash + 8 MB octal PSRAM (N16R8)
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv

build_flags =
  -DBOARD_HAS_PSRAM
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DARDUINO_USB_MODE=1
  -DBOREAS_LOG_LEVEL=3
  -Wall -Wextra

lib_deps =
  olikraus/U8g2 @ ^2.36.19
  bblanchon/ArduinoJson @ ^7.3.0
  lewisxhe/SensorLib @ 0.3.1

[env:bringup]
extends = env:boreas
build_src_filter = +<../test/bringup/>
```

Pin exact versions (not `^`) once Phase 0 confirms a working set — AV-001.

### 3.4 Arduino IDE fallback (for Waveshare's stock demos)

Only needed to run the unmodified Waveshare examples. Arduino IDE 2.x; Boards Manager → *esp32 by Espressif* **≥ 3.3.0**. Board: **ESP32S3 Dev Module**. Tools menu:

| Setting | Value |
|---|---|
| USB CDC On Boot | Enabled |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| USB Mode | Hardware CDC and JTAG |
| Upload Mode | UART0 / Hardware CDC |

Libraries from the demo zip's `Arduino/libraries` (LVGL 8.3.11 **or** 9.3.0 — not both; SensorLib 0.3.1; U8g2). Demos: `01_WIFI_AP`, `02_WIFI_STA`, `03_ADC_Test`, `04_I2C_PCF85063`, `05_I2C_SHTC3`, `06_SD_Card`, `07_Audio_Test`, `08_LVGL_V8_Test`, `09_LVGL_V9_Test`, `10_U8G2_Test`. `05`, `04`, `03` and `10` are the ones worth running once as a smoke test; their logic is ported into `test/bringup/`.

## 4. Build, flash, monitor

```bash
cp include/secrets.example.h include/secrets.h   # edit SSID/password
pio run                                           # compile
pio run -t upload                                 # flash
pio device monitor                                # 115200 baud, Ctrl-C to exit
```

**First flash / board won't enter download mode:** hold **BOOT**, click **PWR** (or re-plug USB) while holding, release BOOT, then upload. Factory firmware and any Boreas build with CDC-on-boot enabled normally auto-enter download mode without this.

**Bring-up test:** `pio run -e bringup -t upload && pio device monitor` — expect `RTC OK`, `SHTC3 OK T=.. RH=..`, `BATT x.xx V`, an `I2C:` line listing the addresses found, `KEY DOWN`/`KEY UP` (and `BOOT …`, `LOC …`) as you press each button, and a test card on the panel that drops into LPM after 2 s.

**Fake-data layout work:** `pio run -t upload -e boreas --build-flag=-DBOREAS_FAKE_DATA=1` skips Wi-Fi and renders a canned model — fastest loop for UI iteration, or use `tools/preview.py` without flashing at all.

## 5. Tests

```bash
pio test -e native        # host-native: scheduler backoff, derived values, WMO mapping, layout boxes
pio test -e boreas        # on-device: I2C devices present, panel constructor, ADC sanity
```

## 6. Cross-compilation

None; PlatformIO fetches the Xtensa toolchain.

## 7. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| No `/dev/ttyACM0` | not in `dialout`; brltty; bad cable (power-only) | §3.1; try another cable |
| `A fatal error occurred: Failed to connect` | not in download mode | hold BOOT while powering on |
| Compiles but panel stays blank | wrong U8g2 constructor/rotation, `SPI.begin` pins not called before `u8g2.begin()`, or official `espressif32` platform (2.x core) picked up | check `platformio.ini` platform URL; AV-001 |
| Panel updates a second late | written while in LPM | `endFrame()` must switch to HPM first (SPEC.md §Panel modes) |
| Random reboots after hours | heap creep | Diag page heap figures; AV-002 |
| Wrong time after power cycle | no RTC cell, or RTC not written after NTP | D-007 |
| `Brownout detector was triggered` | weak cell or USB supply | charge; AV-010 |
| Serial shows nothing until reset | CDC-on-boot off | `-DARDUINO_USB_CDC_ON_BOOT=1` |
| Indoor temp reads high | board heat | AV-004 calibration |
