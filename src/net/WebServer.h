#pragma once
#include <WebServer.h>
using WebServerClass = WebServer;
class BleServer;
#include <WebSocketsServer.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "../config/ConfigStore.h"
#include "../leds/EffectsEngine.h"

class NetworkManager;

class MilaWebServer {
public:
    void begin(Config* cfg, ConfigStore* store, EffectsEngine* engine, NetworkManager* network = nullptr);
    void loop();
    void broadcastState();
    void broadcastScanProgress(uint8_t pct, const char* msg);
    void setNetworkManager(NetworkManager* network) { _network = network; }
    String buildStateJson();
    void setBleServer(BleServer* ble) { _ble = ble; }
    bool handleBleCommand(const char* json, String& response);

private:
    WebServerClass   _http{80};
    WebSocketsServer _ws{81};
    Config*          _cfg     = nullptr;
    ConfigStore*     _store   = nullptr;
    EffectsEngine*   _engine  = nullptr;
    NetworkManager*  _network = nullptr;
    BleServer*       _ble     = nullptr;
    bool             _pendingRestart   = false;
    bool             _pendingWifiReset = false;
    bool             _scanActive       = false;
    bool             _scanCancel       = false;
    uint16_t         _scanIp           = 0;
    IPAddress        _scanBase;
    bool             _bleScanPending   = false;
    bool             _bleJoinPending   = false;

    void handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
    void handleWsMessage(const char* json);
    void handleRestPresets();
    String buildPresetsJson();
    void applyStripConfig(JsonDocument& doc);
    void streamRobust(File& f, const String& contentType, bool gzip);
};
