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

    // Called by the command characteristic's write callback (runs on the
    // NimBLE host task) with one raw chunk: [seq][more][...JSON bytes].
    void onCommandChunk(const uint8_t* data, size_t len);

    // Call every main-loop iteration: drains a fully-reassembled command
    // (handed off from the NimBLE task under a critical section) and applies
    // it from the single-threaded main-loop context, so Config/EffectsEngine
    // are never mutated from two tasks at once.
    void loop();

private:
    Config*         _cfg    = nullptr;
    ConfigStore*    _store  = nullptr;
    EffectsEngine*  _engine = nullptr;
    MilaWebServer*  _web    = nullptr;
    NimBLECharacteristic* _stateChar = nullptr;

    String _rxBuffer; // NimBLE task only — no cross-task access

    // Shared between the NimBLE task (writer) and loop() (reader/clearer);
    // all access must happen under _mux.
    String       _pendingCommand;
    bool         _cmdPending = false;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    void handleCommand(const char* json);
    String buildCoreStateJson();
};
#endif
