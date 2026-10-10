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

Write sequence (`St7305Panel::end_frame()`): `sendF("c", 0x38)` → `sendBuffer()` → delay 100 ms → `sendF("c", 0x39)`. Writing while in LPM is legal but the visible update may lag by up to 1 s. Verified on the board 2026-10-10: a 100 ms HPM hold gives an instant visible update. Holds of 300–1500 ms and staying in HPM were no different. Holds shorter than 100 ms are untested.

The panel **does not retain an image without power** (D-001 consequences; AV-006).

## Layout (400×300, `ui/grid.h`)

This table is authoritative and matches `src/ui/grid.h` exactly; `test/test_layout` checks its geometry. `design/panel-reference.png` is the rendered result (below). `design/panel-mockup.png` is a style reference only and disagrees in detail (BUG-001, fixed). All coordinates in panel pixels after rotation; `x`/`y` is the top-left, and regions are content areas *between* the 1 px rules.

Two 1 px border lines (outer at 3, inner at 5, 1 px gap) enclose a table of 1 px rules:

| Rule | Position |
|---|---|
| Outer border | rect (3, 3, 394, 294) |
| Inner border | rect (5, 5, 390, 290) |
| Column rules | x = 150 and x = 271, rows 26–198 |
| Body / strip rule | y = 199, x 6–393 |
| Strip / footer rule | y = 279, x 6–393 |
| Under the header | row 25 is left white, separating the header from the tabs |

| Region | x | y | w | h | Notes |
|---|---|---|---|---|---|
| Header bar | 5 | 5 | 390 | 20 | inverted, over the inner border's top edge; 6 px square badge, `BOREAS MS-42 // <LOCATION>` left; date + clock right |
| ├ EXT column | 6 | 26 | 144 | 173 | tab `EXT · OUTDOOR`, source `OPEN-METEO` |
| ├ INT column | 151 | 26 | 120 | 173 | tab `INT · INDOOR`, source `SHTC3` |
| └ READOUTS column | 272 | 26 | 122 | 173 | tab `READOUTS`; 7 dot-leader rows |
| Bottom strip | 6 | 200 | 388 | 79 | |
| ├ Trace | 6 | 200 | 250 | 79 | 12 h temperature trace |
| └ Rain meter | 256 | 200 | 138 | 79 | 12-cell meter + advisory box; no rule between trace and meter |
| Footer | 6 | 280 | 388 | 14 | status, Wi-Fi bars, battery gauge |

**Tabs**: inverted, 12 px high, at the top of each column, inset 1 px from the column edges, so a white column either side of each rule keeps neighbouring tabs from merging into one bar (BUG-001). Title in the label font left; the data source in the 5×7 font right.

Changes from the pre-code table (2026-10-10): the footer moved from y = 283 to y = 280 so its 14 px end at 293, inside the inner border (it previously overlapped the outer border at 296). The bottom strip shrank from 82 to 79 px to make room. Column widths are now given as content between the rules (EXT 144, READOUTS 122).

**Fonts** (`src/ui/Fonts.h`):

| Role | Font | Notes |
|---|---|---|
| Header badge, location, date | `u8g2_font_7x14B_tr` | monospaced: a 14-character location always fits, even with the seconds clock (checked with the `worst` preview fixture) |
| Header clock | `u8g2_font_9x15B_tr` | |
| Tabs, labels, values | `u8g2_font_6x12_tf` | Latin-1: has `°` and `·` |
| Tab source labels | `u8g2_font_5x7_tr` | `OPEN-METEO` doesn't fit beside `EXT · OUTDOOR` in 6×12 |
| Footer | `u8g2_font_profont11_tr` | |
| Outdoor temperature | `u8g2_font_logisoso50_tn` | digits only; ≈ 52 px, meets F-002 |
| Indoor values | `u8g2_font_logisoso24_tn` | digits only |

Glyph coverage: U8g2 `_tr` fonts are ASCII only (32–127), so `°` and `·` need `_tf` variants and `u8g2_DrawUTF8`. `logisoso*_tn` has digits only, so the hero `°` is drawn as a small circle. `Δ`, `▲` and `▼` are in none of these fonts; draw them as shapes (as `ui/TestCard` does) or 12×12 XBM icons. Remaining font choices are recorded here as each region lands.

Icons (D-018): drawn from primitives in `ui/Icons`, 40×40 box: sun, moon, sun-cloud, moon-cloud, cloud, fog, drizzle, rain, rain-ice, snow, storm, plus a small box for unknown codes. Review sheet: `--scene icons`. Small symbols (`Δ`, `▲`, `▼`, wind arrows) are drawn the same way.

### EXT · OUTDOOR column (`ui/Outdoor`, F-002)

Content area x 6–149, y 38–198 (below the tab); left/right padding 3 px.

| Element | Position | Font / size |
|---|---|---|
| Temperature, whole °C (never `-0`) | x 9, baseline 92 | `logisoso50_tn` (≈ 52 px digits, meets F-002's ≥ 48 px) |
| Degree sign + `C` | 6 px after the digits; double circle r 4/3 at baseline − 44; `C` at baseline − 30 | drawn circle; `9x15B` |
| Condition icon | x 9, y 100, 40×40 | `ui/Icons` |
| `FEELS` / `HI` / `LO` | labels x 55, values right-aligned at x 146, rows from y 104 every 13 px | `6x12_tf` |
| Dotted rule | y 150, every 3 px | — |
| Condition text | x 9, baseline 168 | `7x14B` (≤ 16 characters) |

Without a forecast the temperature and values show `--`, the icon is the fallback box, and the condition reads `NO DATA`. The worst case (`-25 °C`, `-28°`, `FREEZING RAIN`) fits; see the `worst` preview fixture.

### INT · INDOOR column (`ui/Indoor`, F-003)

Content area x 151–270, y 38–198; left/right padding 3 px (x 154–267).

| Element | Position | Font / size |
|---|---|---|
| `TEMP` label + value `21.4` + `°C` | baseline 64; label left, unit right-aligned at 267, value right-aligned 3 px before the unit | label/unit `6x12_tf`, value `logisoso24_tn` |
| Dotted rule | y 69 | — |
| `HUMID` label + value + `%` | baseline 96 | as above |
| Dotted rule | y 101 | — |
| `RH LEVEL` + `30·60` | baseline 112 | `6x12_tf` |
| RH meter | 10 cells, 10×10 px, 1 px gaps, from x 156, y 116; filled up to `rh_segments` | — |
| Comfort band bracket | under cells 4–6 (30–60 %), y 127–129 | — |
| Dotted rule | y 133 | — |
| `Δ IN/OUT` (drawn Δ) and `DEW`; values `+7.4°`, `9.9°` | labels baseline 145, values 158; second pair at x 214 | `6x12_tf` |
| Comfort box | x 154, y 170, 114×22; `COMFORT · <BAND>` centred | `6x12_tf`; outlined for `NOMINAL`, **inverted** for `DRY`/`HUMID`/`COLD`/`WARM` |

A failed sensor shows `--.-`, `--`, an empty meter and `COMFORT · ---`. The worst case (35.9 °C, 100 %, `+60.5°`) fits.

### READOUTS column (`ui/Readouts`, F-004)

Content area x 272–393, y 38–198; left/right padding 3 px (x 275–390). Seven rows, 23 px apart from y 38. Each row has its label left and value right-aligned on baseline row top + 15, with a dotted leader (every 3 px) at row top + 19. Labels and values in `7x14B_tr`.

| Row | Value | Notes |
|---|---|---|
| `WIND` | `SW 18` | 8-point compass + km/h, rounded |
| `GUST` | `31` | km/h |
| `HUM` | `84 %` | outdoor RH |
| `PRESS` | `1009` + trend | value right-aligned 11 px short of the edge; drawn ▲ / ▼ / ─ in the slot, blank while the trend is unknown (first ~3 h after boot) |
| `UV` | `2 LOW` | rounded index + band |
| `RISE` / `SET` | `07:10` / `18:26` | local time, from today's daily row (`DerivedModel::today`, so correct after midnight) |

Without a forecast every value shows `--` (times `--:--`). The worst case (`NW 118`, `140`, `100 %`, `1035▲`, `11 EXTREME`) fits.

### Temperature trace (`ui/Trace`, F-005)

Region x 6–255, y 200–278.

| Element | Position | Notes |
|---|---|---|
| Title `TEMP · NEXT 12 H` | x 9, baseline 211 | `6x12_tf` |
| Points | 13 (current hour + 12), x = 12 + 19·i (12 … 240) | from `DerivedModel::first_hour` |
| Grid | a dot at every point column, rows 218, 226 … 258 | — |
| Curve | 2 px polyline, scaled min→max into y 255…229 | flat data sits mid-band; top leaves room for the peak label under the title |
| Peak | hollow 7×7 square on the highest point; value above it (`17°`), clamped inside the region | `6x12_tf` |
| Now pointer | ▲ under the plot (y 260–264) at now's fraction of the first hour; only with a valid clock | — |
| Hour labels | local hour `HH` centred under each point, baseline 274 | `6x12_tf` |

Fewer than 2 points: `NO FORECAST` centred on a cleared patch over the grid.

### Rain meter and advisory (`ui/RainMeter`, F-006)

Region x 256–393, y 200–278; padding 3 px (x 259–390).

| Element | Position | Notes |
|---|---|---|
| Title `RAIN PROB.` | right-aligned at 390, baseline 211 | `6x12_tf` |
| Meter | 12 cells, 10×20 px, 1 px gaps, from x 259, y 217; filled to `rain_segments` | 12 h maximum probability |
| Scale | `0` left, `100` right (`5x7`); the reading (`60 %`) centred (`6x12`), baseline ~247 | `-- %` without data |
| Advisory box | x 259, y 252, 132×23 | `UMBRELLA ADVISED` inverted in `7x14B`; otherwise `NO UMBRELLA NEEDED` outlined in `6x12`; `NO FORECAST` without data |

The neutral line is fixed until F-015 adds the rotating one-liners. Its wording follows the umbrella rule: at, say, 40 % "no rain expected" would be wrong, but "no umbrella needed" is what the rule actually decides.

### Footer (`ui/Footer`, F-007)

Region x 6–393, y 280–293; padding 3 px (x 9–390); text in `6x12_tf`, vertically centred.

| Part | Content |
|---|---|
| Left | `UPD 13:00 · NEXT 13:30 · RTC OK`: last successful fetch and next scheduled fetch in local time (`--:--` when unknown or the clock is untrusted); `RTC UNSET` when the RTC isn't trusted (D-015) |
| Right, from the right edge | voltage (`3.9V`; `LOW 3.3V` below 3.4 V; `--V` without a reading) · battery gauge (outline 23×12 with a 2×4 nub, four 4×8 cells from `BatteryModel::cells`) · `BATT` · Wi-Fi bars (four 4×8 cells) · `WIFI`; 6 px between a label and its gauge, 10 px between the groups |
| Stale | forecast exists **and** is older than 2 × the fetch interval: the whole footer inverts and the left reads `!! STALE · LINK DOWN · UPD 13:00`, or `!! STALE · FETCH FAILING · UPD …` if Wi-Fi is up. The gauges stay, drawn in white. Before the first fetch the footer is normal, with `--:--`, not stale. The rest of the panel keeps the last good data. |

Wi-Fi bars from RSSI (`derived::wifi_bars`): 4 at ≥ −55 dBm, 3 at ≥ −67, 2 at ≥ −75, 1 at ≥ −85, else 0; 0 whenever the link is down. The board measured −55 to −60 dBm on the bench.

### Reference image

`design/panel-reference.png` (400×300, exact panel pixels) is the Main page rendered by the host preview from the `typical` fixture: the mockup's public example data, KINGS LANGLEY, TUE 06 OCT 13:30. It is the reference for the Phase 1 photograph. Current hash: `50c160a9`. Regenerate it in the same commit as any change under `src/ui/` that alters it (BUILD.md §4).

## Open-Meteo request

Endpoint: `https://api.open-meteo.com/v1/forecast`

Location: `locations::kHome` from `include/locations.h`, which is git-ignored so a real location is never committed (D-010). The committed template `locations.example.h` uses Kings Langley (`51.713`, `-0.455`) as a public example, and the query below shows those values. Timezone `Europe/London` for every location.

Query (one line in code):

```
latitude=51.713&longitude=-0.455&timezone=Europe%2FLondon
&timeformat=unixtime&forecast_days=2&forecast_hours=24&wind_speed_unit=kmh
&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,
         wind_speed_10m,wind_direction_10m,wind_gusts_10m,surface_pressure,uv_index
&hourly=temperature_2m,precipitation_probability,precipitation,weather_code
&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,
       precipitation_probability_max
```

`timeformat=unixtime` returns every time as UTC epoch seconds (daily dates are local midnight), so nothing parses ISO strings. `forecast_hours=24` starts the hourly arrays at the current hour, with exactly 24 rows. `forecast_days=2` keeps tomorrow's daily row for night mode (F-014). Optional `&models=ukmo_seamless` prefers the Met Office model (D-004). Built by `meteo::build_url()` (`src/net/MeteoParse.h`).

Measured 2026-10-10: body ≈ 2 KB (1 959 bytes); a verified HTTPS fetch takes 0.6–3.4 s (usually under 1 s) and peaks at about 55 KB of heap (TLS dominates). Free heap returns to within 0.6 KB of its pre-fetch value.

Transport (D-017): TLS verified against the core's built-in CA bundle; connect timeout 10 s, TLS handshake 10 s, read 10 s; HTTP/1.0 so the body streams straight into the parser. A fetch is attempted only with Wi-Fi up and a valid system clock.

### Validation (AV-009)

The ArduinoJson filter keeps only the requested fields. Every value is type-checked; a fetch fails as a whole, and the previous `WeatherModel` is kept, if any check fails:

| Field | Accepted |
|---|---|
| temperatures (current, apparent, daily max/min, hourly) | −60 … 60 °C |
| relative humidity, precipitation probabilities | 0 … 100 % (hourly/daily probabilities may be `null` → 0) |
| weather codes | 0 … 99 |
| wind speed, gusts | 0 … 400 km/h |
| wind direction | 0 … 360° |
| surface pressure | 850 … 1100 hPa |
| UV index | 0 … 20 |
| hourly precipitation | 0 … 500 mm (`null` → 0) |
| timestamps | 2024-01-01 … 2100-01-01 |
| hourly arrays | ≥ 12 rows, all the same length; first 24 kept |
| daily arrays | ≥ 1 row; first 2 kept |

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

- **Dew point** (Magnus, outdoor or indoor): `γ = ln(RH/100) + 17.62·T/(243.12+T)`; `Td = 243.12·γ/(17.62−γ)`. RH is clamped to ≥ 1 % (no `ln 0`).
- **Δ in/out** = `T_indoor − T_outdoor`, shown with sign, one decimal.
- **Comfort band**: RH 30–60 % and T 18–24 °C (both inclusive) → `NOMINAL`; RH < 30 → `DRY`; RH > 60 → `HUMID`; T outside band → `COLD`/`WARM` (RH wins if both).
- **Meters**: segments = round(percent × cells / 100), clamped 0…cells. Humidity meter 10 cells from indoor RH; rain meter 12 cells from the 12 h maximum probability.
- **Current hour**: the first hourly row whose hour hasn't ended (`time + 3600 > now`), so data up to 2 × the fetch interval old still starts at the right hour. The **12 h outlook** (advisory, rain meter) is the 12 rows from there. The **trace** is 13 points (now + 12 h), with min, max and the peak's offset precomputed.
- **Night**: the current time is before sunrise or at/after sunset of *today*, where today is the last daily row whose date (local midnight) is ≤ now. Night swaps sun → moon and sun-cloud → moon-cloud icons.
- **Wind**: 8-point compass (N, NE … NW), boundaries at 22.5° steps.
- **UV band** (WHO, shortened for the readouts column), from the rounded current UV index: 0–2 `LOW`, 3–5 `MOD`, 6–7 `HIGH`, 8–10 `V.HIGH`, ≥ 11 `EXTREME`.
- **Condition text**: from the WMO table above, ≤ 16 characters; unknown codes show `---` with no icon.
- **Advisory**: `UMBRELLA ADVISED` if max(precipitation_probability[next 12 h]) ≥ 50 % or any precipitation ≥ 0.5 mm; else a line from `strings.h` (F-015).
- **Pressure trend** (`data/PressureHistory`): each successful fetch stores (`current.time`, surface pressure) in a 16-slot ring. A repeat of the same `current.time` replaces the last sample; older samples are ignored. The latest sample is compared with the stored sample whose age is closest to 3 h, accepted only within 2.5–3.5 h, so it works at 15, 30 or 60 min fetch intervals. ▲ if +1.0 hPa or more, ▼ if −1.0 or less, else ─. Unknown, shown blank, until a sample in that window exists, i.e. about 3 h after boot. RAM only.
- **Stale**: never fetched, or `now − last_success > 2 × fetch_interval` (exactly 2× is not yet stale).

## Scheduling (`config.h`)

| Tick | Default | Notes |
|---|---|---|
| Redraw | 60 s, on the minute | plus immediately after any data change |
| Sensor read | 60 s | SHTC3 one-shot: wake, normal-mode measurement (`0x7866`), sleep. The sensor sleeps between reads; the low-precision "low power" measurement mode isn't used |
| Fetch | 30 min; 15 min while advisory active; 60 min in night mode | aligned to the clock, offset by 15 s |
| NTP | 6 h | SNTP (`pool.ntp.org`, `time.google.com`) started once Wi-Fi is first up; lwIP re-syncs every 6 h. Each sync writes the RTC and its NTP marker (D-015) and logs RTC − NTP drift. First sync flagged overdue after 60 s of Wi-Fi with no sync |
| Wi-Fi reconnect | 15 s per association attempt; on failure or link loss wait 5 s, then 10, 20 … capped at 5 min; reset on success | measured 2026-10-10: boot to connected 5 s; after a reset the router refused the first association and the retry connected at 21.8 s |
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

`V_batt = ADC_mV × 3 / 1000` **[verify divider against a multimeter; readings so far plausible: 3.97 V on battery, 4.12–4.14 V charging]**, 16-sample average, ADC attenuation 11 dB. Gauge thresholds (single Li-ion cell): 4 cells ≥ 4.0 V, 3 ≥ 3.8, 2 ≥ 3.6, 1 ≥ 3.4, 0 below; `LOW BATT` in footer below 3.4 V; graceful halt (final redraw then deep sleep) at 3.2 V. No USB/mains indicator: GPIO4 measures the cell, not VBUS, and voltage can't separate "charging" from "full on battery" (BUG-003). On USB the gauge simply reads full.

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
