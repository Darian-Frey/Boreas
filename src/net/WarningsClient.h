// One HTTPS GET of a Met Office regional warnings feed, parsed into a
// WarningModel for one area (F-019, D-021). Same transport rules as
// MeteoClient: CA-bundle TLS (D-017), explicit timeouts. The body is read
// into a static buffer (the regional feeds are 0.6-2 KB; a busy storm day
// stays well under kWarningsBufferBytes), so no heap is kept between calls.
// On any failure the model is left exactly as it was.
#pragma once

#include <cstdint>
#include <ctime>

#include "data/Models.h"

class WarningsClient {
 public:
  struct Result {
    bool ok = false;
    int http_status = 0;  // HTTP code, or a negative HTTPClient error
    uint32_t elapsed_ms = 0;
    uint32_t bytes = 0;
    char error[48] = "";
  };

  // On success replaces model's items, sets model.valid and stamps
  // model.fetched_utc = now_utc.
  Result fetch(const char* region, const char* area, time_t now_utc, WarningModel& model);
};
