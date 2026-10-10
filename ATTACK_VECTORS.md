# Attack Vectors

Project-specific failure modes the project must be resilient against.
Grouped by category. Each vector lists detection method and severity.
Severity: Critical (must hold) | Major (regression on release blocks) | Minor (track only).

## Toolchain and build

### AV-001 Core/library version drift breaks the ST7305 driver
**Severity:** Major
**Description.** The U8g2 ST7305 300×400 constructor exists only from U8g2 2.36.19 and needs arduino-esp32 ≥ 3.3.0 (pioarduino). An unpinned upgrade, or a build on the official `espressif32` platform (2.x core), fails to compile or produces a blank panel with no error.
**Detection.** `platformio.ini` pins exact versions; `pio run` in CI (GitHub Actions) on every push; BUILD.md §Toolchain lists the pinned set.
**Related decisions.** D-002, D-005.
**History.** Identified 2026-10-10 from Waveshare's RLCD guide version requirements.

## Memory and stability

### AV-002 Heap fragmentation and allocation in the redraw path
**Severity:** Critical
**Description.** The S3 has ample RAM, but a 72-hour soak can still die from fragmentation if `String`, ArduinoJson dynamic documents or `std::vector` growth sit inside the per-minute loop. Symptom: reboot after hours/days, or a fetch that starts failing with no network cause.
**Detection.** Diagnostics page shows `ESP.getFreeHeap()` and `getMaxAllocHeap()`; soak log (Phase 1 deliverable) must show no downward trend; `-Wall -Werror` plus a grep in CI for `String ` and `new ` under `src/ui/`.
**Related decisions.** D-005.
**Related features.** F-008 (JSON filter), F-011.

### AV-011 Serial logging stalls the loop when no host is reading
**Severity:** Major
**Description.** With USB plugged into a computer but no serial monitor open, the ESP32-S3's USB CDC port is "connected" but nothing drains it. Once its TX buffer fills, each `Serial` write blocks for the core's default 100 ms TX timeout. A few log lines per event stall `loop()` for most of a second, so button presses are missed and redraws lag. It looks like a panel or debounce bug, and it disappears as soon as a monitor is opened.
**Detection.** `Serial.setTxTimeoutMs(0)` immediately after `Serial.begin()` (in `log.h` init for the firmware), so log output is dropped rather than waited on. The bring-up test reports `LOOP max gap` every 10 s: it measured 751 ms with the port unread before the fix and 50 ms after.
**Related features.** F-013 (KEY), F-011.
**History.** Found 2026-10-10 in Phase 0: the KEY count missed most fast presses on the bench with no monitor open, and updates appeared about 1 s late. The lag looked like AV-006 (writing in LPM), but it was this stall delaying the press detection. With the fix, every HPM hold from 100 ms up gave instant updates.

## Network and data

### AV-003 Fetch hammering on failure
**Severity:** Major
**Description.** A broken DNS or an Open-Meteo outage must not turn into a tight retry loop that drains the battery or trips Open-Meteo's fair-use limits.
**Detection.** Retry sequence in SPEC.md §Retry; unit test on the scheduler's backoff (host-native, `test/test_scheduler`); serial log counts fetch attempts per hour.
**Related decisions.** D-004, D-011.

### AV-004 SHTC3 self-heating and board-heat bias
**Severity:** Major
**Description.** The SHTC3 sits on the same PCB as the ESP32, the charger and the panel driver. Indoor temperature may read 1–3 °C high, worse while charging or during Wi-Fi bursts, and humidity correspondingly low.
**Detection.** Phase 0: compare against a reference thermometer at rest, while charging and after a fetch; record the offsets in SPEC.md and apply a calibration constant if stable. Diagnostics page shows raw and corrected values.
**Related features.** F-003.
**History.** Anticipated 2026-10-10 from board layout. First comparison 2026-10-10: the SHTC3 read 26.0–26.4 °C on USB (charging, panel redrawing every 10 s) against a room thermometer at 25.4 °C, about +0.6 to +1.0 °C. One sample only; not yet a calibration constant. Repeat on battery, at rest, and after a Wi-Fi fetch in Phase 1.

### AV-005 RTC drift or reset
**Severity:** Minor
**Description.** Without a backup cell the PCF85063 resets on every battery swap; with one, it may still drift. Either shows as a wrong clock after a power cycle with no Wi-Fi.
**Detection.** `RTC OK` requires the oscillator-stop flag clear; NTP sync logs the RTC−NTP delta each time so drift is visible in the serial log and on the Diag page.
**Related decisions.** D-007.

### AV-009 Spoofed or malformed forecast body
**Severity:** Minor
**Description.** With `setInsecure()` (D-011) a hostile network can serve any body; and even an honest server can change its schema. A naïve parser could crash on unexpected types or render nonsense.
**Detection.** Every field read through ArduinoJson is type-checked with a default; out-of-range values (|T| > 60 °C, probability outside 0–100, bad timestamps) mark the fetch failed rather than rendered. Phase 2 fault injection feeds malformed JSON from a local server.
**Related decisions.** D-011.

## Display and panel

### AV-006 Panel power-mode mishandling
**Severity:** Critical
**Description.** Three ways to get it wrong: (a) writing in LPM and assuming the image is visible immediately; (b) leaving the panel in HPM and wasting power; (c) putting the ESP32 into deep sleep with the panel in a state that clears or ghosts. The panel holds no image without power at all.
**Detection.** Phase 0 bench test: write a test card, scope/observe refresh behaviour per mode, measure current in each; Phase 3 sleep tests confirm the image survives the sleep/wake cycle before deep sleep is adopted. `St7305Panel` has a single `end_frame()` that encodes the only legal sequence and blocks until the panel is back in LPM.
**Related decisions.** D-001, D-006.
**Related features.** F-011.

### AV-007 Layout clipping or overlap at real panel scale
**Severity:** Major
**Description.** The design mockup was drawn at 2× in a browser with web fonts; U8g2 bitmap fonts have different metrics. Numbers like `-10.5` or `100 %` may overflow their cells; dot leaders may collide with values.
**Detection.** `tools/preview.py` renders worst-case strings; `test/test_layout` asserts each region's bounding box against `grid.h`; a photographic check against the mockup is a Phase 1 acceptance item.
**Related decisions.** D-009.

### AV-008 Ghosting or contrast loss under the gel
**Severity:** Major
**Description.** A reflective panel loses contrast when light must pass through a coloured gel twice; in evening lamp light the panel may be unreadable from across a room.
**Detection.** Phase 4 swatch test at three light levels; result recorded in D-012. If unreadable, the reversal condition in D-012 applies.
**Related decisions.** D-001, D-012.

## Power

### AV-010 Brown-out during Wi-Fi bursts on a low cell
**Severity:** Major
**Description.** Wi-Fi TX draws short >300 mA peaks; a cell near 3.3 V with a tired protection circuit can dip below the S3's brown-out threshold, causing a reset loop that never reaches the "low battery" code path.
**Detection.** Footer `LOW BATT` and graceful halt thresholds (SPEC.md §Battery) sit above the brown-out region; Phase 3 test with a deliberately discharged cell; `esp_reset_reason()` logged at boot and shown on the Diag page.
**Related features.** F-007.
