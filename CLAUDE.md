# CLAUDE.md

## Project

Boreas MS-42: a cassette-futurism weather station on the Waveshare ESP32-S3-RLCD-4.2 (ESP32-S3, 300×400 1-bit reflective LCD, SHTC3, PCF85063 RTC, 18650). Single-day Celsius forecast from Open-Meteo plus live indoor temperature/humidity, drawn as an instrument panel with U8g2. Read [DECISIONS.md](DECISIONS.md) before proposing structural changes; read [SPEC.md](SPEC.md) for every constant.

## Current state

- **Firmware started.** `src/display/St7305Panel`, the `src/sensors/` readers (`Shtc3`, `Battery`, `Rtc`) and `src/net/` (`WiFiMgr`, `NtpSync`, `Backoff`, `MeteoClient` + `MeteoParse.h`) run on the board; `src/data/` (`Models.h`, `Derived`, `PressureHistory`) is pure and compiled into the host tests; `pio test -e native` runs `test/test_sensors`, `test_net`, `test_meteo` and `test_derived`. `src/main.cpp` is a temporary scaffold (sensors, Wi-Fi, NTP, placeholder card redrawn every 60 s and on change) until `app/` and `ui/` exist. Building needs `include/secrets.h`; the build stops with a clear `#error` if it is missing. `test/bringup/` remains the peripheral regression test.
- `design/` — `panel-mockup.png` (4× scale; known to disagree with SPEC.md §Layout, BUG-001, so SPEC wins). Enclosure concept not yet added.
- `platformio.ini` — live; U8g2 pinned to a GitHub commit because the registry lacks ≥ 2.36.19.
- Hardware — board on the bench; Phase 0 bring-up complete (2026-10-10).
- SPEC.md carries **[verify]** tags on the battery divider ratio and the TF card CS pin. Strike each only after checking on the board or the Waveshare schematic.

## Active task

**Phase 1 — live panel (mains)** (ROADMAP.md), in progress: `src/display/` and the sensor readers done; Wi-Fi/NTP and `MeteoClient` (D-017 verified TLS) done; `data/Derived` and the D-016 preview harness done (`MemCanvas`, `tools/preview`, hash-identical with the board). `ui/grid.h`, `Fonts.h`, `Frame` and `data/PanelState.h` done; preview fixtures `typical`/`worst`/`unset` in `tools/preview/Fixtures.h` (public example data only). Outdoor column and icons done; the scaffold draws the live Main page. Next regions: indoor, readouts, trace, rain meter, footer; then `app/` (Scheduler, single `PanelState`) to replace the scaffold. Phase 0 completed 2026-10-10. `test/bringup/` stays as the peripheral regression test (`pio run -e bringup -t upload`). Open items to settle before or early in Phase 1: the BUG-002 RTC-validity rule (needs a D-007 amendment), IMP-002 exact library pins, and where the private forecast location lives (git-ignored header; never committed). Current measurements are deferred to Phase 3 (no meter yet). The location button is a deferred nice-to-have.

## Invariants

- `ui/` reads `PanelState` and writes pixels; it never calls network, sensor or time functions. It draws only through U8g2's C API (`u8g2_t*`) so it builds for the host preview.
- Exactly one `PanelState`, owned by `app/`.
- No heap allocation in the redraw path (AV-002). No `String` under `src/ui/`.
- Every GPIO is named in `include/pins.h`; no numeric pin literals elsewhere.
- The panel is never blanked after boot; failures keep the last good frame and set a flag.
- Every network operation has an explicit timeout.
- The real forecast location exists only in the git-ignored `include/locations.h`. Never put a real place name or coordinates in any tracked file, commit message or PR.
- `St7305Panel::end_frame()` is the only place in `src/` that sends `0x38`/`0x39` (`test/bringup` is exempt). D-006 calls it `endFrame()`, its original name.

## Build & test

```bash
pio run -t upload && pio device monitor          # flash + serial
pio run -e bringup -t upload                     # Phase 0 peripheral test
pio test -e native                               # host-side unit tests
pio run -e preview && .pio/build/preview/program --scene frame --fixture worst --scale 2 -o preview.png  # ui/ without hardware (D-016)
```

## Conventions

- British English in docs, comments and UI strings (`COLOUR` never appears on the panel, but `HUMID`, `NOMINAL` etc. follow the mockup's caps style).
- C++17, `-Wall -Wextra`, no exceptions, no RTTI. `snake_case` functions and variables, `PascalCase` types, `kConstant` for constexpr, `UPPER_SNAKE` only for build flags.
- One module per directory under `src/` with a `.h`/`.cpp` pair; headers include only what they use.
- Commit messages: multi-paragraph, imperative subject ≤ 72 chars, body explains why; reference IDs (`F-003`, `D-006`, `AV-002`, `BUG-`, `IMP-`).
- Docs are part of the commit: a change that invalidates SPEC.md/ARCHITECTURE.md updates them in the same commit.
- **Log, don't silently fix** (Maintenance Rule 8): bugs found while doing something else go in BUGS.md; refactor ideas go in IMPROVEMENTS.md with trade-offs. Ask before acting on either.

## Pitfalls

- See ATTACK_VECTORS.md for the canonical list. Most likely to bite first: AV-001 (wrong platform/core silently gives a blank panel), AV-006 (writing in LPM looks like a one-second lag), AV-004 (indoor temp reads high from board heat).
- `SPI.begin(11, -1, 12, 40)` must precede `u8g2.begin()`; U8g2's constructor pins alone are not enough on this core.
- The panel does not retain an image without power. A "blank screen" bug report may just be a flat cell.
- Waveshare's Arduino examples assume LVGL 8 *or* 9 installed globally — never both. Irrelevant to the PlatformIO build but a trap when running their demos.
- Cross-references between docs are maintained by hand; there is no `check_xrefs` tool yet, so treat them as possibly stale.

## Out of scope

Do not, without asking:
- Switch rendering library (D-005), framework/platform (D-002), or forecast provider (D-004).
- Add runtime Wi-Fi provisioning (D-010), a web UI, MQTT, or voice features (F-017 is Won't).
- Change the layout grid in `ui/grid.h` or the fonts without updating SPEC.md §Layout and the design export.
- Use `setInsecure()` anywhere, or switch to certificate pinning. TLS is verified with the built-in CA bundle (D-017).
- Create new document types or renumber any F-/D-/AV-/BUG-/IMP- IDs.
