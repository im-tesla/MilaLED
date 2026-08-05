#pragma once
#ifdef ESP32
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../config/ConfigStore.h"
#include "../leds/EffectsEngine.h"

class MilaWebServer;

class BleServer {
public:
    void begin(Config* cfg, ConfigStore* store, EffectsEngine* engine);
    void notifyState();
    void setWebServer(MilaWebServer* web) { _web = web; }

    // Called by the command characteristic's write callback with one raw
    // chunk: [seq][more][...JSON bytes].
    void onCommandChunk(const uint8_t* data, size_t len);

private:
    Config*         _cfg    = nullptr;
    ConfigStore*    _store  = nullptr;
    EffectsEngine*  _engine = nullptr;
    MilaWebServer*  _web    = nullptr;
    NimBLECharacteristic* _stateChar = nullptr;
    String _rxBuffer;

    void handleCommand(const char* json);
    String buildCoreStateJson();
};
#endif
