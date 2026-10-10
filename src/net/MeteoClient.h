// One HTTPS GET to Open-Meteo, streamed through the filtered parser into a
// WeatherModel (F-008). TLS is verified against the core's built-in CA
// bundle (D-017). Every stage has an explicit timeout. On any failure the
// model is left exactly as it was.
#pragma once

#include <cstdint>
#include <ctime>

#include "data/Models.h"

class MeteoClient {
 public:
  struct Result {
    bool ok = false;
    int http_status = 0;    // HTTP code, or a negative HTTPClient error
    uint32_t elapsed_ms = 0;
    char error[48] = "";
  };

  // Fetches for the given coordinates. On success fills `model`, sets
  // model.valid and stamps model.fetched_utc = now_utc.
  Result fetch(float latitude, float longitude, time_t now_utc, WeatherModel& model);
};
