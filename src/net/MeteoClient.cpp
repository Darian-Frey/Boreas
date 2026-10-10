#include "net/MeteoClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <cstdio>

#include "config.h"
#include "net/MeteoParse.h"

MeteoClient::Result MeteoClient::fetch(float latitude, float longitude, time_t now_utc, WeatherModel& model) {
  Result res;
  const uint32_t t0 = millis();

  char url[600];
  if (!meteo::build_url(url, sizeof(url), latitude, longitude)) {
    snprintf(res.error, sizeof(res.error), "url too long");
    return res;
  }

  // Client and TLS context are scoped to this call, so their heap is
  // released before the next redraw (AV-002).
  NetworkClientSecure tls;
  tls.useBuiltinCACertBundle();  // verify the server certificate (D-017)
  tls.setHandshakeTimeout(config::kTlsHandshakeTimeoutS);

  HTTPClient http;
  http.setConnectTimeout(config::kHttpConnectTimeoutMs);
  http.setTimeout(config::kHttpReadTimeoutMs);
  http.useHTTP10(true);  // no chunked encoding, so the body can be streamed to the parser
  http.setReuse(false);

  if (!http.begin(tls, url)) {
    snprintf(res.error, sizeof(res.error), "http begin failed");
    return res;
  }
  res.http_status = http.GET();
  if (res.http_status != HTTP_CODE_OK) {
    if (res.http_status < 0)
      snprintf(res.error, sizeof(res.error), "http: %s", HTTPClient::errorToString(res.http_status).c_str());
    else
      snprintf(res.error, sizeof(res.error), "http status %d", res.http_status);
    http.end();
    res.elapsed_ms = millis() - t0;
    return res;
  }

  const meteo::ParseResult parsed = meteo::parse_forecast(http.getStream(), model);
  http.end();
  res.elapsed_ms = millis() - t0;
  if (!parsed.ok) {
    snprintf(res.error, sizeof(res.error), "%s", parsed.error);
    return res;
  }
  model.valid = true;
  model.fetched_utc = now_utc;
  res.ok = true;
  return res;
}
