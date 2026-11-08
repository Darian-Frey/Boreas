#include "net/WarningsClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <cstdio>

#include "config.h"
#include "net/WarningsParse.h"

namespace {
char g_body[config::kWarningsBufferBytes];
}  // namespace

WarningsClient::Result WarningsClient::fetch(const char* region, const char* area, time_t now_utc,
                                             WarningModel& model) {
  Result res;
  const uint32_t t0 = millis();

  char url[120];
  if (!warnings::build_url(url, sizeof(url), region)) {
    snprintf(res.error, sizeof(res.error), "url too long");
    return res;
  }

  NetworkClientSecure tls;
  tls.useBuiltinCACertBundle();  // D-017
  tls.setHandshakeTimeout(config::kTlsHandshakeTimeoutS);

  HTTPClient http;
  http.setConnectTimeout(config::kHttpConnectTimeoutMs);
  http.setTimeout(config::kHttpReadTimeoutMs);
  http.useHTTP10(true);  // no chunked encoding: the body is the bytes until close
  http.setReuse(false);

  if (!http.begin(tls, url)) {
    snprintf(res.error, sizeof(res.error), "warn: http begin failed");
    return res;
  }
  res.http_status = http.GET();
  if (res.http_status != HTTP_CODE_OK) {
    if (res.http_status < 0)
      snprintf(res.error, sizeof(res.error), "warn: %s", HTTPClient::errorToString(res.http_status).c_str());
    else
      snprintf(res.error, sizeof(res.error), "warn: http status %d", res.http_status);
    http.end();
    res.elapsed_ms = millis() - t0;
    return res;
  }

  // Read until the server closes, the buffer fills or the read deadline passes.
  NetworkClient* stream = http.getStreamPtr();
  size_t n = 0;
  bool full = false;
  uint32_t last_data = millis();
  while (stream && (stream->connected() || stream->available())) {
    const int avail = stream->available();
    if (avail > 0) {
      if (n + 1 >= sizeof(g_body)) {
        full = true;
        break;
      }
      const size_t room = sizeof(g_body) - 1 - n;
      n += stream->read(reinterpret_cast<uint8_t*>(g_body + n), static_cast<size_t>(avail) < room ? avail : room);
      last_data = millis();
    } else if (millis() - last_data >= config::kHttpReadTimeoutMs) {
      break;
    } else {
      delay(5);
    }
  }
  g_body[n] = '\0';
  http.end();
  res.bytes = n;
  res.elapsed_ms = millis() - t0;
  if (full) {
    snprintf(res.error, sizeof(res.error), "warn: feed over %u bytes", static_cast<unsigned>(sizeof(g_body) - 1));
    return res;
  }

  const warnings::ParseResult parsed = warnings::parse(g_body, area, now_utc, model);
  if (!parsed.ok) {
    snprintf(res.error, sizeof(res.error), "warn: %s", parsed.error);
    return res;
  }
  model.fetched_utc = now_utc;
  res.ok = true;
  return res;
}
