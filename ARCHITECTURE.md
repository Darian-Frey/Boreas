# Architecture

Descriptive only. Rationale lives in [DECISIONS.md](DECISIONS.md).

## System overview

```
                 ┌─────────────────────────────────────────────────────┐
  Open-Meteo ──► │ net/        WiFiMgr ── NtpSync ──┐                  │
  (HTTPS/JSON)   │             MeteoClient          │                  │
                 │                 │                ▼                  │
                 │                 ▼          PCF85063 RTC (I2C)       │
                 │ data/      WeatherModel ◄────── Clock               │
                 │                 │                                   │
  SHTC3 (I2C) ──►│ sensors/   IndoorModel      BatteryModel ◄── ADC GPIO4
                 │                 │                 │                 │
                 │                 ▼                 ▼                 │
                 │ app/       Scheduler ──► PanelState (one struct)    │
                 │                              │                      │
                 │ ui/        Layout ──► U8g2 full buffer (15 000 B)   │
                 │                              │                      │
                 │ display/   St7305Panel ──► SPI ──► ST7305 (HPM/LPM) │
                 └─────────────────────────────────────────────────────┘
                              KEY GPIO18 ──► app/Input ──► page select
```

Single-core cooperative loop on the Arduino framework (`loop()` with a millis-based scheduler). No RTOS tasks beyond what arduino-esp32 creates internally for Wi-Fi.

## Module responsibilities

**`src/net/`** — `WiFiMgr` owns association, reconnect backoff (`Backoff`, also used later for fetch retries) and RSSI reporting. It's polled from `loop()` with no Wi-Fi event callbacks, credentials are never written to NVS, and the driver's auto-reconnect is off so the policy lives in one place. `NtpSync` starts SNTP when Wi-Fi first comes up and sets the Europe/London TZ rule for local display; the system clock stays UTC. On each successful sync it logs RTC − NTP drift (AV-005) and writes the PCF85063 with the D-015 marker. The SNTP callback only sets an atomic flag consumed in `loop()`. `MeteoClient` performs one HTTPS GET, with TLS verified against the built-in CA bundle (D-017) and explicit connect, handshake and read timeouts, and streams the body into the parser. `MeteoParse.h` is header-only and free of Arduino calls: `build_url()` and `parse_forecast()` (ArduinoJson filter plus AV-009 range checks) fill a copy of the `WeatherModel` and commit it only if every check passes. The same parser runs on the host against fixtures (`test/test_meteo`). It never touches the display.

**`src/data/`** — Plain structs: `WeatherModel` (current, 24 hourly rows, today's daily row, fetch timestamp, stale flag), `IndoorModel`, `BatteryModel`, `PanelState` (`data/PanelState.h`: the models plus `DerivedModel`, location name, UTC clock and validity, RTC and link status, fetch times, last error). `Derived` (pure, host-tested in `test/test_derived`) computes dew point, Δ in/out, comfort band, meter segments, WMO code → condition text/icon (with night variants), compass point, UV band, the current-hour index, the 12 h outlook (max probability, max precipitation, umbrella flag), trace min/max/peak and the stale flag. `PressureHistory` holds time-stamped pressure samples for the 3 h trend.

**`src/sensors/`** — `Shtc3` (I2C 0x70; ID check at boot; one-shot wake/measure/sleep with CRC check; applies the AV-004 offset to `temp_c`, keeps the raw value), `Battery` (ADC on GPIO4, 16-sample average, divider-corrected; `from_volts()` maps to gauge cells and LOW/HALT flags), `Rtc` (PCF85063 over raw I2C, not SensorLib; holds UTC; `read()` applies the D-015 validity rule; `set_from_ntp()` is the only writer of the NTP marker). `CivilTime.h` converts calendar fields to and from `time_t` (no `timegm()` on ESP32 newlib) and is unit-tested on the host. The sensor readers fill `IndoorModel` and `BatteryModel` in `src/data/Models.h`.

**`src/app/`** — `Scheduler` fires the ticks (redraw 60 s, fetch 30/15 min, NTP 6 h, sensor 60 s) and assembles `PanelState`. `Input` debounces KEY and drives page selection with the 60 s auto-return. `Boot` runs the self-test sequence and splash.

**`src/ui/`** — `Layout` draws a `PanelState` into the U8g2 buffer using a fixed grid (SPEC.md §Layout). All `ui/` code draws through U8g2's **C API** (`u8g2_t*`, from `St7305Panel::raw()` on the board or `MemCanvas::u8g2()` on the host), never the Arduino `U8G2` class, so it builds unchanged for the host preview (D-016). `grid.h` holds every region rectangle (SPEC.md §Layout), `Fonts.h` every font role, and `Draw.h` small helpers (centred baselines, right-aligned text). `Frame` draws the fixed chrome: borders, header, rules, tabs. `Outdoor`, `Indoor` and `Readouts` draw the three body columns, `Trace` and `RainMeter` the bottom strip, `Footer` the status line, and `Icons` the condition icons and small symbols. `TestCard` is a fixed scene for the D-016 hash check. Sub-drawers: `Frame`, `OutdoorCol`, `IndoorCol`, `ReadoutsCol`, `Trace`, `RainMeter`, `Footer`, `Icons`, plus the `Detail` and `Diag` pages. Fonts are U8g2 built-ins selected in `ui/Fonts.h`. Icons are drawn from primitives in `ui/Icons` (D-018).

**`src/display/`** — `MemCanvas` is a 400×300 canvas in RAM built on U8g2's portable C core, with a pixel hash; it builds for the board and the host. `St7305Panel` wraps the `U8G2_ST7305_...` constructor, the `SPI.begin(11,-1,12,40)` prelude, rotation, and the HPM/LPM commands (`0x38`/`0x39`). It exposes `begin()` (init, then a cleared frame, ending in LPM), `canvas()` / `raw()` (the buffer as `U8G2&` or `u8g2_t*`; `ui/` uses `raw()`) and `end_frame()`. `end_frame()` sends the buffer in HPM, holds 100 ms, then drops to LPM. It blocks for the hold, so the panel is always in LPM when it returns.

**`include/`** — `pins.h` (every GPIO in one place), `config.h` (thresholds, calibration offset and RTC marker now; intervals, night hours and timezone as later modules need them), `locations.h` (git-ignored; forecast location name and lat/lon, from `locations.example.h`), `secrets.h` (git-ignored; Wi-Fi credentials, from `secrets.example.h`), `version.h`.

**`tools/`** — Host-side helpers. **Preview** (D-016, `tools/preview/`, `pio run -e preview`): a host-native build of `src/ui/`, `src/data/`, `display/MemCanvas` and U8g2's C core (the pinned package's `clib/` minus hardware drivers). It renders named scenes to PNG with a dependency-free writer (`Png.h`). Pixel identity with the board is checked by hash.

## Data flow per cycle

1. `Scheduler` tick → `Shtc3.read()` → `IndoorModel`; `Battery.read()` → `BatteryModel`; `Rtc.now()` → clock.
2. On fetch tick → `MeteoClient.fetch()`; on success replace `WeatherModel` and clear stale; on failure keep the old model, set stale if age > 2 × interval, schedule retry.
3. `Derived.update(PanelState)` computes everything the UI needs so `ui/` does no arithmetic beyond pixel maths.
4. `Layout.draw(PanelState)` into the buffer; `St7305Panel::end_frame()` sends it in HPM then drops to LPM.

## Key invariants

- **The panel is never blank after boot.** Every failure path leaves the last good `PanelState` on screen with a flag, never a cleared buffer.
- **`ui/` is pure.** It reads `PanelState` and writes pixels. No network, sensor or time calls inside `ui/`.
- **One `PanelState`.** There is exactly one, owned by `app/`; modules receive `const&` to draw or `&` to fill, never copies that could diverge.
- **All GPIOs live in `pins.h`.** No numeric pin literals elsewhere.
- **No heap allocation in the redraw path.** The U8g2 buffer is static; `Layout` uses stack and static buffers only (AV-002).
- **Network calls are bounded.** Every HTTP/NTP operation has an explicit timeout; `loop()` latency never exceeds the HTTP timeout + 100 ms.

## Cross-cutting concerns

- **Logging:** `Serial` at 115200 via `log.h` macros with level and module tag; `LOG_LEVEL` set in `platformio.ini`. Log init calls `Serial.setTxTimeoutMs(0)` so an unread USB port drops output instead of stalling `loop()` (AV-011). Diagnostics page shows the last error string.
- **Errors:** functions return `bool` or a small `Result` enum; no exceptions (disabled in the build). Every failure sets a module-level `lastError` surfaced on the Diag page.
- **Time:** all timestamps are `time_t` UTC internally; local conversion only in `ui/` via `localtime_r`.
- **Concurrency:** none user-visible. Wi-Fi callbacks only set flags consumed in `loop()`.
- **Persistence:** none in Phase 1. Phase 3 may use `RTC_DATA_ATTR` for pressure history across deep sleep.
