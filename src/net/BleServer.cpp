#include "BleServer.h"
#ifdef ESP32
#include <ArduinoJson.h>
#include <cstring>
#include "CoreParamRouter.h"
#include "WebServer.h"
#include "../version.h"

namespace {
    const char* SERVICE_UUID    = "7a2eec00-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const char* CMD_CHAR_UUID   = "7a2eec01-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const char* STATE_CHAR_UUID = "7a2eec02-4b0f-4bde-9f3f-1a7c6d9b2e10";
    const size_t CHUNK_PAYLOAD  = 100; // keep in sync with CHUNK_PAYLOAD in useBluetoothTransport.ts

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

    class StateCallbacks : public NimBLECharacteristicCallbacks {
    public:
        explicit StateCallbacks(BleServer* owner) : _owner(owner) {}
        void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
            if (subValue > 0) _owner->requestInitialNotify(); // push current state as soon as a client subscribes
        }
    private:
        BleServer* _owner;
    };
}

void BleServer::begin(Config* cfg, ConfigStore* store, EffectsEngine* engine) {
    _cfg = cfg; _store = store; _engine = engine;

    NimBLEDevice::init("MilaLED");
    NimBLEDevice::setMTU(247);

    _server = NimBLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks());

    NimBLEService* service = _server->createService(SERVICE_UUID);

    NimBLECharacteristic* cmdChar = service->createCharacteristic(
        CMD_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    cmdChar->setCallbacks(new CommandCallbacks(this));

    _stateChar = service->createCharacteristic(STATE_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
    _stateChar->setCallbacks(new StateCallbacks(this));

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
        _rxLen = 0;
        _rxDesynced = false;
    }

    if (_rxDesynced) return; // still waiting for a fresh seq==0 after an earlier overflow drop

    size_t payloadLen = len - 2;
    if (_rxLen + payloadLen > BLE_MAX_COMMAND_SIZE) {
        _rxLen = 0;
        _rxDesynced = true; // ignore stray continuation chunks from this aborted train
        return;
    }
    memcpy(_rxBuffer + _rxLen, data + 2, payloadLen);
    _rxLen += payloadLen;

    if (!more) {
        _rxBuffer[_rxLen] = '\0';

        // Hand the completed JSON off to the main loop instead of applying it
        // here — this callback runs on the NimBLE host task, and Config/
        // EffectsEngine must only be touched from the single-threaded loop().
        // Both buffers are fixed-size, so this memcpy never allocates and is
        // always safe inside the critical section — a still-undrained
        // previous command is simply overwritten (latest wins).
        portENTER_CRITICAL(&_mux);
        memcpy(_pendingCommand, _rxBuffer, _rxLen + 1);
        _pendingLen = _rxLen;
        _cmdPending = true;
        portEXIT_CRITICAL(&_mux);

        _rxLen = 0;
    }
}

void BleServer::requestInitialNotify() {
    portENTER_CRITICAL(&_mux);
    _notifyPending = true;
    portEXIT_CRITICAL(&_mux);
}

void BleServer::loop() {
    char cmd[BLE_MAX_COMMAND_SIZE + 1];
    bool hasCmd   = false;
    bool doNotify = false;

    portENTER_CRITICAL(&_mux);
    if (_cmdPending) {
        memcpy(cmd, _pendingCommand, _pendingLen + 1);
        _cmdPending = false;
        hasCmd = true;
    }
    if (_notifyPending) {
        _notifyPending = false;
        doNotify = true;
    }
    portEXIT_CRITICAL(&_mux);

    if (hasCmd)   handleCommand(cmd);
    if (doNotify) notifyState();
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

    // Clamp chunk size to the actual negotiated MTU so notifications never
    // silently truncate — requesting MTU 247 in begin() doesn't guarantee
    // the central grants it. Falls back to the BLE spec's minimum ATT MTU
    // (23) if no peer info is available yet.
    uint16_t mtu = 23;
    if (_server && _server->getConnectedCount() > 0) {
        mtu = _server->getPeerInfo(0).getMTU();
    }
    size_t chunkPayload = (mtu > 5) ? (mtu - 5) : 1; // ATT overhead (3) + our seq/more header (2)
    if (chunkPayload > CHUNK_PAYLOAD) chunkPayload = CHUNK_PAYLOAD;

    String json = buildCoreStateJson();

    size_t len    = json.length();
    size_t offset = 0;
    uint8_t seq   = 0;
    uint8_t frame[2 + CHUNK_PAYLOAD]; // sized for the largest possible chunk (CHUNK_PAYLOAD), even though a low-MTU connection will use less

    do {
        size_t chunkLen = len - offset;
        if (chunkLen > chunkPayload) chunkLen = chunkPayload;
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
