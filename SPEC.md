# Spec

Authoritative technical reference. Constants here are the single source of truth; other documents link rather than restate. Values marked **[verify]** are taken from Waveshare's documentation or third-party ports and must be confirmed against the board in Phase 0.

## Hardware

**Board:** Waveshare ESP32-S3-RLCD-4.2 (`ESP32-S3-RLCD-4.2`, with-battery SKU).

| Item | Part | Notes |
|---|---|---|
| MCU module | ESP32-S3-WROOM-1-N16R8 | Dual-core LX7 @ 240 MHz, 16 MB flash, 8 MB octal PSRAM, Wi-Fi 2.4 GHz, BLE 5 |
| Panel | 4.2" RLCD, Sitronix ST7305, 300×400, 1-bit, reflective, no backlight | SPI, on-chip GRAM |
| Temp/humidity | Sensirion SHTC3 | I2C 0x70 |
| RTC | NXP PCF85063 | I2C 0x51; PH1.0 header for a rechargeable backup cell |
| Audio | ES8311 codec, ES7210 ADC, dual mics, 8 Ω 2 W speaker | Unused in this version |
| Battery | 18650 holder, onboard charger; CHG and WRN LEDs | Voltage on ADC via 3:1 divider |
| Storage | TF slot (FAT32) | Unused in this version |
| Buttons | BOOT (GPIO0), KEY (GPIO18), PWR (hardware, long-press off / click on) | Active low |
| USB | USB-C, native USB-Serial/JTAG | Flash and logs |

### Pin map (`include/pins.h`)

| Function | GPIO | Source |
|---|---|---|
| LCD SCLK | 11 | Waveshare schematic (`LCD_SCL`) |
| LCD MOSI | 12 | Waveshare schematic (`LCD_SDA`) |
| LCD DC | 5 | Waveshare schematic (`LCD_RS`) |
| LCD TE | 6 | Waveshare schematic (`LCD_TE`); tearing-effect output, unused |
| LCD CS | 40 | Waveshare schematic |
| LCD RST | 41 | Waveshare schematic |
| LCD backlight | — | none; panel is reflective |
| I2C SDA | 13 | Waveshare schematic; shared bus (RTC, SHTC3); also header P1-14 |
| I2C SCL | 14 | Waveshare schematic; shared bus; also header P1-16 |
| RTC interrupt | 15 | Waveshare schematic (`RTC_INT`); unused |
| I2S MCLK / BCLK / LRCLK | 16 / 9 / 45 | Waveshare schematic; unused |
| I2S DOUT (speaker) / DIN (mic) | 8 / 10 | Waveshare schematic (`I2S_DSDIN` / `I2S_ASDOUT`); unused |
| Speaker amp enable | 46 | Waveshare schematic (`PA_CTRL`); keep LOW |
| KEY button | 18 | Waveshare schematic; active low, internal pull-up; also header P1-15 |
| BOOT button | 0 | Waveshare schematic; active low; also header P1-5 |
| Location button (add-on) | 17 | Proposed: external momentary switch to GND on header P1-13; active low, internal pull-up. Location cycling is not yet a feature |
| Battery ADC | 4 | Waveshare schematic (`BAT_ADC`); 3:1 divider **[verify ratio]** |
| TF card MOSI / SCK / MISO | 21 / 38 / 39 | Waveshare schematic pin table; CS not shown there **[verify]**; unused |
| USB D− / D+ | 19 / 20 | native USB; also header P1-6 / P1-8 |
| UART0 TX / RX | 43 / 44 | header P1-10 / P1-12; unused (logs go over USB CDC) |

I2C scan on the board (2026-10-10): `0x18`, `0x40`, `0x51`, `0x70`. `0x51` is the PCF85063 and `0x70` the SHTC3; `0x18` and `0x40` are the standard addresses of the ES8311 codec and ES7210 mic ADC. Nothing answers at the QMI8658C's `0x6A`/`0x6B`, so the schematic's QMI8658C column is template residue: the board has no IMU.

### Expansion header P1 (2×8, 2.54 mm, back of board)

| Pin | Signal | Pin | Signal |
|---|---|---|---|
| 1 | 3V3 | 2 | VBUS (USB 5 V) |
| 3 | GND | 4 | GND |
| 5 | GPIO0 (BOOT) | 6 | GPIO19 (USB D−) |
| 7 | GPIO1 | 8 | GPIO20 (USB D+) |
| 9 | GPIO2 | 10 | GPIO43 (U0TXD) |
| 11 | GPIO3 | 12 | GPIO44 (U0RXD) |
| 13 | GPIO17 | 14 | GPIO13 (I2C SDA) |
| 15 | GPIO18 (KEY) | 16 | GPIO14 (I2C SCL) |

The free header GPIOs are 1, 2, 3 and 17. GPIO3 is a strapping pin, so keep it unused. GPIO17 is taken for the location button. That keeps GPIO1 and GPIO2 free for analogue use later: they're on ADC1, which still works while Wi-Fi is on, whereas GPIO17 is on ADC2, which doesn't. GPIO7 and GPIO42 are unused but not brought out.

Confirm every **[verify]** against the Waveshare schematic PDF (Resources page) during Phase 0 and strike the tag.

## Panel

- Native geometry 300 (W) × 400 (H). Boreas uses `U8G2_R1` for 400×300 landscape. Origin top-left after rotation.
- Framebuffer: 1 bpp, 300 × 400 / 8 = **15 000 bytes**, U8g2 full-buffer (`_F_`) constructor.
- SPI: `SPI.begin(11, -1, 12, 40)` before `u8g2.begin()`. Clock **24 MHz** (GPIO-matrix routing caps practical SPI at ~40 MHz; ST7305 minimum write period 30 ns).
- U8g2 constructor: `U8G2_ST7305_300X400_F_4W_HW_SPI(rotation, cs, dc, reset)`, confirmed present in U8g2 2.37.1 (commit `b3a40de`) and working on the panel (2026-10-10): `U8G2_R1` gives correct landscape orientation, all four corner pixels visible, 1 px lines and the checkerboard render cleanly.
- The driver's init sequence sets frame rates HPM 32 Hz / LPM 1 Hz (`0xB2 0x12`), enables auto power-down, and leaves the panel in **HPM** with display on. Firmware must send `0x39` after the first frame.

### Panel modes

| Mode | Command | Self-refresh | Use |
|---|---|---|---|
| High Power Mode (HPM) | `0x38` | 32 Hz | During buffer writes; always-on for the seconds-clock build |
| Low Power Mode (LPM) | `0x39` | 1 Hz | Held between redraws |
| Display off | `0x28` (U8g2 `setPowerSave(1)`) | — | Not LPM; avoid except for shutdown |

Write sequence (`St7305Panel::endFrame()`): `sendF("c", 0x38)` → `sendBuffer()` → delay 100 ms → `sendF("c", 0x39)`. Writing while in LPM is legal but the visible update may lag by up to 1 s. Verified on the board 2026-10-10: a 100 ms HPM hold gives an instant visible update. Holds of 300–1500 ms and staying in HPM were no different. Holds shorter than 100 ms are untested.

The panel **does not retain an image without power** (D-001 consequences; AV-006).

## Layout (400×300, `ui/grid.h`)

This table is authoritative. Where `design/panel-mockup.png` disagrees, this table wins (BUG-001). All coordinates in panel pixels after rotation. Border lines are 1 px unless stated; the mockup's doubled lines are 2 px with a 2 px gap.

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Outer frame | 3 | 3 | 394 | 294 | double line |
| Header bar | 5 | 5 | 390 | 20 | inverted; badge left, date + clock right |
| Body | 5 | 26 | 390 | 173 | three columns |
| ├ EXT column | 5 | 26 | 145 | 173 | tab 12 px high, inverted |
| ├ INT column | 151 | 26 | 120 | 173 | tab 12 px |
| └ READOUTS column | 272 | 26 | 123 | 173 | tab 12 px; 7 dot-leader rows |
| Bottom strip | 5 | 200 | 390 | 82 | trace left 250 px, rain meter right |
| Footer | 5 | 283 | 390 | 14 | status, Wi-Fi bars, battery gauge |

Big outdoor temperature: U8g2 `u8g2_font_logisoso42_tn` **[provisional]**. Indoor hero values: `u8g2_font_logisoso24_tn`. Labels: `u8g2_font_6x12_tr` or `u8g2_font_profont11_tr`. Header badge: `u8g2_font_7x14B_tr`. Final choices recorded here after the Phase 1 font pass.

Icons: XBM, 40×40 (condition), 12×12 (wind arrow, trend arrows), generated by `tools/icons.py` from `assets/icons/src/`.

## Open-Meteo request

Endpoint: `https://api.open-meteo.com/v1/forecast`

Location: `locations::kHome` from `include/locations.h`, which is git-ignored so a real location is never committed (D-010). The committed template `locations.example.h` uses Kings Langley (`51.713`, `-0.455`) as a public example, and the query below shows those values. Timezone `Europe/London` for every location.

Query (one line in code):

```
latitude=51.713&longitude=-0.455&timezone=Europe%2FLondon
&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,
         wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,uv_index
&hourly=temperature_2m,precipitation_probability,precipitation,weather_code
&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,
       precipitation_probability_max
&forecast_days=2&wind_speed_unit=kmh
```

`forecast_days=2` so the 12-hour window crosses midnight. Optional `&models=ukmo_seamless` to prefer the Met Office model (D-004). ArduinoJson filter keeps only the fields above; expected body ≈ 6–8 KB, filtered document ≤ 4 KB.

### WMO weather code → condition text / icon

| Codes | Text | Icon |
|---|---|---|
| 0 | CLEAR | sun |
| 1, 2 | MAINLY CLEAR / PARTLY CLOUDY | sun-cloud |
| 3 | OVERCAST | cloud |
| 45, 48 | FOG | fog |
| 51–57 | DRIZZLE | drizzle |
| 61, 63, 65, 80–82 | LIGHT / MODERATE / HEAVY RAIN | rain |
| 66, 67 | FREEZING RAIN | rain-ice |
| 71–77, 85, 86 | SNOW | snow |
| 95–99 | THUNDERSTORM | storm |

Night variants (sun → moon) chosen when current time is outside sunrise–sunset.

## Derived values (`data/Derived`)

- **Dew point** (Magnus, outdoor or indoor): `γ = ln(RH/100) + 17.62·T/(243.12+T)`; `Td = 243.12·γ/(17.62−γ)`.
- **Δ in/out** = `T_indoor − T_outdoor`, shown with sign, one decimal.
- **Comfort band**: RH 30–60 % and T 18–24 °C → `NOMINAL`; RH < 30 → `DRY`; RH > 60 → `HUMID`; T outside band → `COLD`/`WARM` (RH wins if both).
- **Advisory**: `UMBRELLA ADVISED` if max(precipitation_probability[next 12 h]) ≥ 50 % or any precipitation ≥ 0.5 mm; else a line from `strings.h` (F-015).
- **Pressure trend**: compare current surface pressure with the value fetched 3 h earlier (kept in a 6-slot ring); ▲ if +1.0 hPa or more, ▼ if −1.0 or less, else ─.
- **Stale**: `now − last_success > 2 × fetch_interval`.

## Scheduling (`config.h`)

| Tick | Default | Notes |
|---|---|---|
| Redraw | 60 s, on the minute | plus immediately after any data change |
| Sensor read | 60 s | SHTC3 low-power one-shot |
| Fetch | 30 min; 15 min while advisory active; 60 min in night mode | aligned to the clock, offset by 15 s |
| NTP | 6 h | writes RTC on success |
| Page auto-return | 60 s | F-013 |
| Night mode | 23:00–06:00 | F-014 |

### Retry

On fetch failure: retry after 1 min, 2 min, 4 min, 8 min, then every 15 min until success. Any success resets the sequence. The stale flag is independent of retries.

## RTC (PCF85063, I2C 0x51)

- Holds **UTC**. Local time is applied only in `ui/` (ARCHITECTURE.md §Time).
- Time registers `0x04`–`0x0A` (BCD): seconds (bit 7 = oscillator-stop flag), minutes, hours, day, weekday, month, year (2000-based). Writing the seconds register clears the OS flag.
- Register `0x03` is a free RAM byte used as the NTP marker: `kRtcNtpMarker = 0xB5`. Verified writable and readable on the board, 2026-10-10.
- **Valid** = OS flag clear **and** register `0x03 == kRtcNtpMarker` (D-015). `NtpSync` writes the time, then the marker. Nothing else writes the marker.
- No backup cell is fitted yet, so the RTC loses time whenever the board loses power. That was observed in Phase 0 (D-007, BUILD.md §First power-on step 3).

## Battery

`V_batt = ADC_mV × 3 / 1000` **[verify divider against a multimeter; readings so far plausible: 3.97 V on battery, 4.12–4.14 V charging]**, 16-sample average, ADC attenuation 11 dB. Gauge thresholds (single Li-ion cell): 4 cells ≥ 4.0 V, 3 ≥ 3.8, 2 ≥ 3.6, 1 ≥ 3.4, 0 below; `LOW BATT` in footer below 3.4 V; graceful halt (final redraw then deep sleep) at 3.2 V. USB present if `V_batt` reads > 4.15 V while CHG is lit **[verify; no dedicated VBUS sense pin known]**.

## Panel timing (measured 2026-10-10)

| Item | Value |
|---|---|
| `sendBuffer()` at 24 MHz, full 15 000-byte frame | 21 ms |
| `loop()` worst-case gap with a redraw (serial TX timeout 0) | 50 ms |
| HPM hold before `0x39` that gives an instant visible update | 100 ms (shorter untested) |

## Power (to be measured in Phase 0/3)

| State | Target / measured |
|---|---|
| Idle, Wi-Fi associated, panel LPM | — mA |
| Idle, Wi-Fi off, panel LPM | — mA |
| Redraw (HPM write) | — mA for — ms |
| Fetch (TLS + parse) | — mA for — s |
| Deep sleep | — µA |

Projected runtime from a 3 000 mAh cell: fill in after measurement.

## Enclosure (Phase 4)

Panel active area, board outline, mounting hole positions, USB-C and button offsets: **from the Waveshare 2D drawing, Phase 4**. Gel: see D-012.

## Build flags

| Flag | Default | Effect |
|---|---|---|
| `BOREAS_SECONDS_CLOCK` | 0 | HH:MM:SS, HPM always, 1 s redraw (D-008) |
| `BOREAS_LOG_LEVEL` | 3 (info) | 0 none … 5 verbose |
| `BOREAS_FAKE_DATA` | 0 | Skip network; render a canned `WeatherModel` for layout work |
