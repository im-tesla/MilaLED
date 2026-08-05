#pragma once
#ifdef ESP32
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../config/ConfigStore.h"
#include "../leds/EffectsEngine.h"

class MilaWebServer;

// Reassembly cap: comfortably above the 256-byte JSON parse budget in
// handleCommand(), so a client that never sends a terminating chunk can't
// grow the buffer without bound. Fixed-size (not String) so the cross-task
// mailbox hand-off in onCommandChunk()/loop() never allocates on the heap.
static const size_t BLE_MAX_COMMAND_SIZE = 512;

class BleServer {
public:
    void begin(Config* cfg, ConfigStore* store, EffectsEngine* engine);
    void loop();
    void notifyState();
    void setWebServer(MilaWebServer* web) { _web = web; }

    // Called by the command characteristic's write callback (runs on the
    // NimBLE host task) with one raw chunk: [seq][more][...JSON bytes].
    void onCommandChunk(const uint8_t* data, size_t len);

    // Called by the state characteristic's subscribe callback (NimBLE host
    // task) when a client enables notifications, so it gets the strip's
    // current state immediately instead of stale UI defaults.
    void requestInitialNotify();

private:
    Config*         _cfg    = nullptr;
    ConfigStore*    _store  = nullptr;
    EffectsEngine*  _engine = nullptr;
    MilaWebServer*  _web    = nullptr;
    NimBLEServer*         _server    = nullptr;
    NimBLECharacteristic* _stateChar = nullptr;

    // NimBLE task only — no cross-task access.
    char   _rxBuffer[BLE_MAX_COMMAND_SIZE + 1];
    size_t _rxLen = 0;
    bool   _rxDesynced = false;

    // Shared mailbox between the NimBLE task (writer) and loop() (reader/
    // clearer); all access must happen under _mux. Fixed-size buffers mean
    // every access is a bounded memcpy — no heap allocation is possible, so
    // there's no risk from holding _mux during the copy, regardless of
    // backlog (a still-undrained previous command is simply overwritten).
    char         _pendingCommand[BLE_MAX_COMMAND_SIZE + 1];
    size_t       _pendingLen = 0;
    bool         _cmdPending = false;
    bool         _notifyPending = false;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    void handleCommand(const char* json);
    String buildCoreStateJson();
};
#endif
