// App: owns every module and the single PanelState (ARCHITECTURE.md
// §app). loop() polls the network, fetches and redraws according to
// app/Schedule. main.cpp only calls begin() and loop().
#pragma once

#include <Wire.h>

#include <cstdint>
#include <ctime>

#include "app/Schedule.h"
#include "data/PanelState.h"
#include "data/PressureHistory.h"
#include "display/St7305Panel.h"
#include "net/MeteoClient.h"
#include "net/NtpSync.h"
#include "net/WiFiMgr.h"
#include "sensors/Battery.h"
#include "sensors/Rtc.h"
#include "sensors/Shtc3.h"
#include "storage/SdLog.h"

class App {
 public:
  App() : shtc3_(Wire), rtc_(Wire), ntp_(rtc_) {}

  void begin();
  void loop();

 private:
  void read_sensors();
  bool service_fetch(time_t now);
  void update_state(time_t now);
  void redraw();
  int64_t current_minute() const;
  void log_health(const char* event, const char* note);
  void service_serial();

  // Modules
  St7305Panel panel_;
  Shtc3 shtc3_;
  Rtc rtc_;
  Battery battery_;
  WiFiMgr wifi_;
  NtpSync ntp_;
  MeteoClient meteo_;
  SdLog sd_;

  // The one PanelState (CLAUDE.md invariant). Its models are the live
  // data: the fetch writes state_.weather, the sensors write indoor/battery.
  PanelState state_;

  schedule::FetchScheduler fetch_schedule_;
  PressureHistory pressure_;
  MeteoClient::Result last_fetch_;
  RtcReading rtc_reading_;
  bool clock_valid_ = false;  // system clock set from a trusted RTC (D-015) or NTP
  bool advisory_ = false;     // UMBRELLA ADVISED in the latest data: 15 min fetches
  uint32_t fetch_count_ = 0;
  uint32_t fetch_ok_ = 0;
  uint32_t fetch_fail_ = 0;
  uint32_t last_health_ms_ = 0;
  uint32_t last_panel_init_ms_ = 0;
  uint32_t panel_reinits_ = 0;
  uint32_t redraw_count_ = 0;
  int64_t last_minute_ = -1;
  char cmd_[16] = "";  // serial command being typed
  uint8_t cmd_len_ = 0;
};
