// PanelState: everything the panel shows, in one struct (ARCHITECTURE.md).
// Exactly one instance, owned by app/. ui/ reads it through a const& and
// never fetches anything itself. Pure data, so host previews can build
// fixtures of it.
#pragma once

#include <cstdint>
#include <ctime>

#include "data/Derived.h"
#include "data/Models.h"

enum class Link : uint8_t { kDown, kConnecting, kUp };

struct PanelState {
  // Identity
  const char* location_name = "";  // header: BOREAS MS-42 // <name>

  // Clock. UTC; ui/ converts to local time with localtime_r (TZ set at boot).
  bool clock_valid = false;  // trusted RTC (D-015) or NTP; otherwise --:--
  time_t now_utc = 0;
  bool show_seconds = false;  // BOREAS_SECONDS_CLOCK build (D-008)
  bool rtc_ok = false;        // footer RTC OK / RTC UNSET (F-009)

  // Network
  Link link = Link::kDown;
  int8_t rssi = 0;  // dBm when link is up

  // Forecast fetch (F-007 footer)
  time_t last_fetch_utc = 0;  // 0 = never
  time_t next_fetch_utc = 0;

  // Models and the values derived from them
  WeatherModel weather;
  IndoorModel indoor;
  BatteryModel battery;
  DerivedModel derived;

  // Diagnostics
  const char* last_error = "";
};
