# Features

## Target users

One user: Shane, glancing at a wall or desk panel to decide whether to take a coat. Secondary: anyone who sees the object and wants to know what it is.

## Out of scope

- Multi-day forecasts (a single day is the design brief; see D-003)
- Fahrenheit or any unit switching
- Touch input (the panel has none)
- Cloud/LLM voice assistant features that ship with the Waveshare demo
- A web UI or phone app for configuration (Wi-Fi credentials are compiled in; see D-010)
- Colour or greyscale rendering — the panel is strictly 1-bit

## Features

### Display — panel layout

### F-001 Instrument-panel frame
**Priority:** Must
**Acceptance:**
- Double-line outer border with an inverted header bar reading `BOREAS MS-42 // <LOCATION>`, the home location name from `include/locations.h` (≤ 14 characters)
- Header carries date and a clock (HH:MM minimum; see F-016 for seconds)
- Three body columns: `EXT · OUTDOOR`, `INT · INDOOR`, `READOUTS`, each with an inverted section tab
- Bottom strip for the hourly trace and rain meter; single-line footer for status
- Renders at 400×300 landscape (`U8G2_R1`) with no element clipped
**Status:** In progress — frame, header, tabs and rules done and on the board (2026-10-10); regions to come
**Notes:** Layout grid specified in SPEC.md §Layout. Reference design in `design/`.

### F-002 Outdoor current conditions
**Priority:** Must
**Acceptance:**
- Current temperature in large numerals (≥ 48 px tall at panel resolution), °C
- Feels-like, today's high and low, stacked beside the condition icon. One line doesn't fit the 138 px column at a readable size: 149–167 px in the label font, measured with the `feels-oneline` preview scene, 2026-10-10
- Condition text (e.g. `LIGHT RAIN`) mapped from the Open-Meteo WMO weather code
- Condition icon drawn procedurally or from an XBM asset, ≥ 40 px
**Status:** Done 2026-10-10 — column drawn, previewed (typical and worst cases) and approved on the board

### F-003 Indoor conditions from the onboard SHTC3
**Priority:** Must
**Acceptance:**
- Indoor temperature to 0.1 °C and relative humidity to 1 %, refreshed at least once a minute
- 10-segment humidity level meter with the 30–60 % comfort band marked
- Δ in/out temperature line and dew point (computed, SPEC.md §Derived values)
- `COMFORT · NOMINAL` / `DRY` / `HUMID` status box driven by the comfort band
**Status:** Done 2026-10-10 — column drawn, previewed (typical, worst, sensor-failed) and on the board; out-of-band comfort is shown as an inverted box
**Notes:** The SHTC3 sits close to the ESP32 and the charge circuit; see AV-004 for self-heating.

### F-004 Readouts column
**Priority:** Must
**Acceptance:**
- Wind speed and compass direction, gust, outdoor humidity, pressure with trend arrow, UV index with band label, sunrise and sunset
- Dot-leader rows; values right-aligned
**Status:** Done 2026-10-10 — column drawn, previewed (typical, worst, no data) and on the board

### F-005 Twelve-hour temperature trace
**Priority:** Must
**Acceptance:**
- Polyline of hourly temperature for the next 12 h on a dotted grid, hour ticks beneath
- Current-hour marker and labelled peak value
**Status:** Not started

### F-006 Rain probability meter
**Priority:** Must
**Acceptance:**
- 12-segment VU-style meter of the maximum precipitation probability over the next 12 h (filled = probability reached)
- Advisory box beneath: `UMBRELLA ADVISED` when max probability ≥ 50 % or any hourly precipitation ≥ 0.5 mm, else a rotating neutral line (F-015)
**Status:** Not started

### F-007 Footer status line
**Priority:** Must
**Acceptance:**
- Last successful fetch time, next scheduled fetch, `RTC OK`
- Wi-Fi signal as a 4-cell bar from RSSI
- Battery as a 4-cell gauge plus voltage (from GPIO4 ADC, SPEC.md §Battery)
- Inverted `!! STALE — LINK DOWN` when the last fetch is older than 2 × the fetch interval
**Status:** Not started

### Data

### F-008 Open-Meteo forecast fetch
**Priority:** Must
**Acceptance:**
- One HTTPS GET per cycle to the Open-Meteo forecast endpoint for the home location in `include/locations.h`, requesting current, hourly (next 24 h) and daily (today and tomorrow) fields listed in SPEC.md §Open-Meteo request
- Parsed with an ArduinoJson filter so the JSON parse peaks below 40 KB of heap. The peak for the whole fetch, TLS included, is measured and recorded in SPEC.md (about 55 KB, 2026-10-10). Free heap returns to its pre-fetch level after each fetch (AV-002)
- Fetch interval 30 min; 15 min when `UMBRELLA ADVISED` is active
- Failure leaves the previous data on screen and sets the stale flag; retries with backoff (SPEC.md §Retry)
**Status:** In progress — fetch, verified TLS, validation and retry working on the board (2026-10-10); the stale flag and the advisory-driven 15 min interval come with `app/Scheduler`

### F-009 Time from NTP, held by the RTC
**Priority:** Must
**Acceptance:**
- NTP sync on boot and every 6 h; Europe/London zone with BST handled by the TZ rule
- PCF85063 RTC written after each sync and read at boot, so the clock is correct within 2 s after a power cycle with no Wi-Fi
- `RTC OK` in the footer only when the RTC time was set by NTP and has not lost power since (oscillator-stop flag clear and NTP marker present, D-015); otherwise `RTC UNSET` and the clock shows `--:--` until NTP succeeds
**Status:** Not started

### F-010 Wi-Fi station with reconnect
**Priority:** Must
**Acceptance:**
- Joins the configured 2.4 GHz network on boot; reconnects automatically after loss with capped backoff
- Loss of Wi-Fi never blanks or freezes the panel; indoor sensor and clock keep updating
**Status:** Not started

### Behaviour

### F-011 Refresh cadence and panel power mode
**Priority:** Must
**Acceptance:**
- Panel redrawn once a minute (clock tick) and on any data change
- After each redraw the ST7305 is put into Low Power Mode (1 Hz self-refresh); switched to High Power Mode only for the write (SPEC.md §Panel modes)
**Status:** Not started

### F-012 Boot splash and self-test
**Priority:** Should
**Acceptance:**
- On boot: badge, firmware version, then a checklist of `RTC`, `SHTC3`, `WIFI`, `NTP`, `METEO` ticking off as each comes up; any failure shown inline, boot continues
**Status:** Not started

### F-013 KEY button cycles pages
**Priority:** Should
**Acceptance:**
- Short press on KEY (GPIO18): cycle Main → Detail (full 24 h hourly table) → Diagnostics (IP, RSSI, heap, battery, uptime, last error) → Main
- Returns to Main automatically after 60 s
**Status:** Not started

### F-014 Night mode
**Priority:** Could
**Acceptance:**
- Between configurable hours (default 23:00–06:00) the panel shows a reduced layout: clock, indoor conditions, tomorrow's outlook; fetch interval drops to 60 min
**Status:** Not started

### F-015 Footer one-liners
**Priority:** Could
**Acceptance:**
- When no advisory is active, the advisory box rotates through a short list of neutral status lines on each redraw (e.g. `CLEAR SKIES`, `ALL SYSTEMS: 42`)
- Lines live in a single table in `src/strings.h`
**Status:** Not started

### F-016 Seconds in the header clock
**Priority:** Could
**Acceptance:**
- Header clock shows HH:MM:SS, updated every second, with the panel held in HPM
**Status:** Not started
**Notes:** Conflicts with F-011's LPM strategy and with battery life; see D-008. Likely mains-only.

### F-017 Spoken advisory on KEY long-press
**Priority:** Won't (this version)
**Notes:** The board has an ES8311 codec and speaker; a pre-rendered "umbrella advised" clip is cheap to play. Deferred until Phases 1–3 are stable.

### Hardware

### F-018 Enclosure
**Priority:** Should
**Acceptance:**
- 3D-printed case in putty/beige with a recessed charcoal bezel, corner screws, cut-outs for USB-C, KEY, PWR and the speaker grille
- Amber (or chosen) lighting-gel overlay held flat over the panel
- 18650 accessible without tools
**Status:** Not started
**Notes:** Concept render in `design/`. Gel choice recorded in D-012.

## Candidate features (uncommitted)

- Pressure history sparkline from the last 24 h of fetched values, stored in RTC memory across deep sleep
- Deep-sleep battery mode: wake every 15 min, fetch, redraw, sleep (needs the LPM-on-sleep behaviour of the ST7305 verified — see AV-006)
- Moon phase glyph in the header on clear nights
- Home Assistant MQTT publish of indoor readings
- A second, smaller Boreas MS-25 build on the original Pi Zero + Inky pHAT, reusing the layout engine via a host-side renderer
