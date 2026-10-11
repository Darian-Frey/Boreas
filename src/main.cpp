// Boreas MS-42 firmware entry point. Everything lives in app/App.

#include <Arduino.h>

#include "app/App.h"

namespace {

App app;

}  // namespace

void setup() { app.begin(); }

void loop() { app.loop(); }
