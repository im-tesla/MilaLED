#pragma once
#ifdef ESP32
#include <WebServer.h>
using WebServerClass = WebServer;
class BleServer;
#else
#include <ESP8266WebServer.h>
using WebServerClass = ESP8266WebServer;
#endif
#include <WebSocketsServer.h>
#include <FS.h>
#include "../config/ConfigStore.h"
#include "../leds/EffectsEngine.h"

class MilaWebServer {
public:
    void begin(Config* cfg, ConfigStore* store, EffectsEngine* engine);
    void loop();
    void broadcastState();
    void broadcastScanProgress(uint8_t pct, const char* msg);
#ifdef ESP32
    void setBleServer(BleServer* ble) { _ble = ble; }
#endif

private:
    WebServerClass   _http{80};
    WebSocketsServer _ws{81};
    Config*          _cfg    = nullptr;
    ConfigStore*     _store  = nullptr;
    EffectsEngine*   _engine = nullptr;
#ifdef ESP32
    BleServer*       _ble = nullptr;
#endif
    bool             _pendingRestart = false;
    bool             _scanActive  = false;
    bool             _scanCancel  = false;
    uint16_t         _scanIp      = 0;
    IPAddress        _scanBase;

    void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
    void handleWsMessage(const char* json);
    void handleRestPresets();
    String buildStateJson();
    void streamRobust(File& f, const String& contentType, bool gzip);
};
