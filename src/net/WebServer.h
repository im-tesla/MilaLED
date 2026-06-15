#pragma once
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include "../config/ConfigStore.h"
#include "../leds/EffectsEngine.h"

class MilaWebServer {
public:
    void begin(Config* cfg, ConfigStore* store, EffectsEngine* engine);
    void loop();
    void broadcastState();
    void broadcastScanProgress(uint8_t pct, const char* msg);

private:
    ESP8266WebServer _http{80};
    WebSocketsServer _ws{81};
    Config*          _cfg    = nullptr;
    ConfigStore*     _store  = nullptr;
    EffectsEngine*   _engine = nullptr;
    bool             _pendingRestart = false;

    void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
    void handleWsMessage(const char* json);
    void handleRestPresets();
    void handleAmbilightScan();
    String buildStateJson();
};
