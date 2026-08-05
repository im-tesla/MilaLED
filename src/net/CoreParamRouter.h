#pragma once
#include <ArduinoJson.h>
#include "../config/ConfigStore.h"

struct ParamApplyResult {
    bool anyChanged      = false;
    bool discreteChanged = false;
};

// Applies the "core" LED control params shared between the WebSocket and
// BLE command channels onto `cfg`. Does not save to flash or broadcast —
// callers decide what to do with the result (immediate LED apply on
// anyChanged, save+broadcast on discreteChanged).
ParamApplyResult applyCoreParams(Config& cfg, JsonDocument& doc);
