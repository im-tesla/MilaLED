#include "BleServer.h"
#ifdef ESP32
#include <ArduinoJson.h>
#include <cstring>
#include <utility>
#include "CoreParamRouter.h"
#include "WebServer.h"
#include "../version.h"

namespace {
    const char* SERVICE_UUID    = "7a2eec00-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const char* CMD_CHAR_UUID   = "7a2eec01-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const char* STATE_CHAR_UUID = "7a2eec02-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const size_t CHUNK_PAYLOAD  = 100; // keep in sync with CHUNK_PAYLOAD in useBluetoothTransport.ts

    // Reassembly cap: comfortably above the 256-byte StaticJsonDocument budget
    // handleCommand() parses into, so a client that never sends a terminating
    // chunk can't grow _rxBuffer without bound.
    const size_t MAX_RX_COMMAND_SIZE = 512;

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
            NimBLEDevice::startAdvertising(); // stay discoverable after a client disconnects
        }
    };

    class CommandCallbacks : public NimBLECharacteristicCallbacks {
    public:
        explicit CommandCallbacks(BleServer* owner) : _owner(owner) {}
        void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
            std::string val = characteristic->getValue();
            _owner->onCommandChunk(reinterpret_cast<const uint8_t*>(val.data()), val.length());
        }
    private:
        BleServer* _owner;
    };
}

void BleServer::begin(Config* cfg, ConfigStore* store, EffectsEngine* engine) {
    _cfg = cfg; _store = store; _engine = engine;

    NimBLEDevice::init("MilaLED");
    NimBLEDevice::setMTU(247);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* service = server->createService(SERVICE_UUID);

    NimBLECharacteristic* cmdChar = service->createCharacteristic(
        CMD_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    cmdChar->setCallbacks(new CommandCallbacks(this));

    _stateChar = service->createCharacteristic(STATE_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);

    service->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->start();
}

void BleServer::onCommandChunk(const uint8_t* data, size_t len) {
    if (len < 2) return; // malformed: missing seq/more header

    uint8_t seq  = data[0];
    uint8_t more = data[1];

    if (seq == 0) {
        _rxBuffer = "";
        _rxDesynced = false;
    }

    if (_rxDesynced) return; // still waiting for a fresh seq==0 after an earlier overflow drop

    if (_rxBuffer.length() + (len - 2) > MAX_RX_COMMAND_SIZE) {
        _rxBuffer = "";
        _rxDesynced = true; // ignore stray continuation chunks from this aborted train
        return;
    }
    _rxBuffer.concat(reinterpret_cast<const char*>(data + 2), len - 2);

    if (!more) {
        // Hand the completed JSON off to the main loop instead of applying it
        // here — this callback runs on the NimBLE host task, and Config/
        // EffectsEngine must only be touched from the single-threaded loop().
        // std::move avoids an allocating copy while the critical section is held.
        portENTER_CRITICAL(&_mux);
        _pendingCommand = std::move(_rxBuffer);
        _cmdPending = true;
        portEXIT_CRITICAL(&_mux);
        _rxBuffer = "";
    }
}

void BleServer::loop() {
    String cmd;
    bool hasCmd = false;

    portENTER_CRITICAL(&_mux);
    if (_cmdPending) {
        cmd = std::move(_pendingCommand);
        _cmdPending = false;
        hasCmd = true;
    }
    portEXIT_CRITICAL(&_mux);

    if (hasCmd) handleCommand(cmd.c_str());
}

void BleServer::handleCommand(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json)) return;

    ParamApplyResult r = applyCoreParams(*_cfg, doc);

    if (r.anyChanged) _engine->applyConfig(*_cfg);
    if (r.discreteChanged) {
        _store->save(*_cfg);
        notifyState();
        if (_web) _web->broadcastState(); // keep the WiFi/WS clients in sync too
    }
}

void BleServer::notifyState() {
    if (!_stateChar) return;
    String json = buildCoreStateJson();

    size_t len    = json.length();
    size_t offset = 0;
    uint8_t seq   = 0;
    uint8_t frame[2 + CHUNK_PAYLOAD];

    do {
        size_t chunkLen = len - offset;
        if (chunkLen > CHUNK_PAYLOAD) chunkLen = CHUNK_PAYLOAD;
        bool more = (offset + chunkLen) < len;

        frame[0] = seq;
        frame[1] = more ? 1 : 0;
        memcpy(frame + 2, json.c_str() + offset, chunkLen);

        _stateChar->setValue(frame, 2 + chunkLen);
        _stateChar->notify();

        offset += chunkLen;
        seq++;
    } while (offset < len);
}

String BleServer::buildCoreStateJson() {
    StaticJsonDocument<512> doc;
    doc["type"]           = "state";
    doc["power"]          = _cfg->power;
    doc["brightness"]     = _cfg->brightness;
    doc["effect"]         = _cfg->effect;
    doc["speed"]          = _cfg->speed;
    doc["intensity"]      = _cfg->intensity;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%06lX", _cfg->colorPrimary);
    doc["colorPrimary"]   = hex;
    snprintf(hex, sizeof(hex), "#%06lX", _cfg->colorSecondary);
    doc["colorSecondary"] = hex;
    doc["palette"]        = _cfg->palette;
    doc["virtualLeds"]    = _engine->virtualCount();
    doc["version"]        = MILALED_VERSION;

    JsonArray segs = doc["segments"].to<JsonArray>();
    uint16_t physOff = 0;
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
        if (_cfg->segments[i].count == 0 && activeCount > 0) continue;
        JsonObject seg = segs.createNestedObject();
        seg["count"]     = _cfg->segments[i].count;
        seg["half"]      = _cfg->segments[i].half;
        seg["start"]     = physOff;
        seg["virtCount"] = _cfg->segments[i].half
            ? (_cfg->segments[i].count / 2) : _cfg->segments[i].count;
        physOff += _cfg->segments[i].count;
        activeCount++;
    }

    String out;
    serializeJson(doc, out);
    return out;
}
#endif
