# Bluetooth Control (ESP32) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an always-on Bluetooth LE control channel on ESP32 boards (power/effect/palette/brightness/speed/intensity/colors, plus read-only segment/version info), alongside the existing WiFi/WebSocket channel, paired with a GitHub-Pages-hosted copy of the same React UI that connects over Web Bluetooth instead of WiFi.

**Architecture:** Firmware gets a new ESP32-only `BleServer` (NimBLE) exposing a custom GATT service with a chunked write characteristic (commands) and a chunked notify characteristic (state), reusing the WebSocket handler's param-apply rules via an extracted `applyCoreParams` function, and cross-notifying the existing `MilaWebServer` so both channels stay in sync. The frontend gets a second transport hook (`useBluetoothTransport`) with the same shape as `useWebSocket`, selected at build time via `VITE_TRANSPORT=ble`, plus a connect dialog and capability-based hiding of BLE-unsupported tabs (Presets, Strip Config, Ambilight, WiFi reset). A GitHub Actions workflow deploys the BLE build to GitHub Pages.

**Tech Stack:** NimBLE-Arduino 2.5.1 (firmware BLE stack), ArduinoJson v6 (existing), Web Bluetooth API + `@types/web-bluetooth` (frontend), Vite build modes, GitHub Actions (`actions/deploy-pages`).

## Global Constraints

- ESP8266 is never touched — no BLE hardware exists on that platform. All new firmware code lives behind `#ifdef ESP32`.
- NimBLE-Arduino version: `h2zero/NimBLE-Arduino@^2.5.1` — verified to compile cleanly against both `esp32dev` (RAM 10.8%/35,500B, Flash 43.2%) and `esp32-c3-supermini` (RAM 7.2%/23,460B, Flash 14.6%) using the exact API calls this plan uses.
- BLE advertised device name: `"MilaLED"`.
- GATT UUIDs (fixed, must match exactly between firmware and frontend):
  - Service: `7a2eec00-4b0f-4bde-9f3f-1a7c6d9b2e10`
  - Command characteristic (write/write-without-response): `7a2eec01-4b0f-4bde-9f3f-1a7c6d9b2e10`
  - State characteristic (notify): `7a2eec02-4b0f-4bde-9f3f-1a7c6d9b2e10`
- BLE chunk framing: every write/notify payload is `[seq: uint8][more: uint8][...JSON bytes]`. `seq==0` (re)starts a reassembly buffer; `more==0` marks the final chunk. Chunk payload size is a fixed `100` bytes on both sides (`CHUNK_PAYLOAD` in `BleServer.cpp` and `useBluetoothTransport.ts` — keep these two constants numerically identical if either changes).
- v1 BLE-writable fields: `power`, `brightness`, `effect`, `speed`, `intensity`, `colorPrimary`, `colorSecondary`, `palette`. v1 BLE state (read-only) additionally includes: `virtualLeds`, `segments`, `version`.
- Out of scope for v1 (per design spec): presets CRUD, strip reconfiguration (pin/chipset/segments+reboot), Ambilight TV scan, WiFi reset. These stay WiFi/WS-only; the BLE frontend build hides their tabs/sections entirely.
- No new automated test framework is introduced for the frontend (none exists today — no vitest/jest in `web/package.json`). Firmware verification is compile-only (`pio run -e <env>`) since there's no hardware-in-the-loop test harness in this repo (only `PixelMapper` has a native unit test, and it's pure logic — BLE/GATT code isn't). Final functional verification is manual, on real hardware (Task 12).
- Added during Task 3 review: `BleServer`'s command reassembly buffer is capped at `MAX_RX_COMMAND_SIZE = 512` bytes (a client that never sends a terminating chunk gets dropped, not an unbounded heap grow), and `onCommandChunk` (NimBLE host task) never mutates `Config`/`EffectsEngine` directly — it hands the completed JSON to a `portMUX_TYPE`-guarded pending-command slot that `BleServer::loop()` (called from the main Arduino loop, single-threaded, added in Task 4) drains and applies. This keeps all `Config`/`EffectsEngine` access on the same task as the rest of the codebase's WS/Hyperion/Ambilight handling.
- Added during Task 3 re-review: an overflow drop sets `_rxDesynced = true` so stray continuation chunks from the aborted train are ignored until the next `seq==0` (rather than being silently reinterpreted as a new command's start), and both sides of the cross-task hand-off use `std::move` on the `String` payload so no heap-allocating copy happens while `_mux` is held.
- Added during Task 6 review: `useBluetoothTransport`'s `connect()` now guards `!navigator.bluetooth` before calling `requestDevice()` (unsupported browsers would otherwise throw synchronously, uncaught by the promise chain's `.catch()`), and `sendRaw`'s `writeChunks().catch(...)` now calls `setError(...)` instead of silently swallowing post-connect write failures. Flagged but deliberately not fixed here: whether two separate `sendRaw` invocations (e.g. two UI controls both hitting "immediate" send in the same tick) could overlap their GATT writes on the same characteristic — carry this to the final whole-branch review once Task 7/8's actual call pattern exists to check against.
- Added during Task 3 third review round: `_pendingCommand` is only assigned when `_cmdPending` is currently false (i.e. the mailbox is empty/already-drained) — a backlogged second command is dropped rather than overwriting an undrained one. This guarantees `_pendingCommand` never holds a live heap buffer at assignment time, so Arduino `String`'s move-assignment always takes its true zero-cost pointer-steal path and never falls into its capacity-reuse path (which would otherwise do a bounded `memmove` + `free()` while `_mux` is held, in that narrow backlog case).

---

### Task 1: NimBLE dependency + `bleEnabled` config field

**Files:**
- Modify: `platformio.ini:51-55` (esp32dev `lib_deps` block)
- Modify: `src/config/ConfigStore.h:36` (add field after `ambMapping`)
- Modify: `src/config/ConfigStore.cpp` (load/save)

**Interfaces:**
- Produces: `Config::bleEnabled` (bool, default `true`), persisted in `config.json`. Later tasks (4) read this to decide whether to start BLE.

- [ ] **Step 1: Add the NimBLE dependency**

In `platformio.ini`, the `[env:esp32dev]` block currently reads:

```ini
lib_deps  =
  fastled/FastLED
  links2004/WebSockets
  bblanchon/ArduinoJson
  tzapu/WiFiManager
```

Change it to:

```ini
lib_deps  =
  fastled/FastLED
  links2004/WebSockets
  bblanchon/ArduinoJson
  tzapu/WiFiManager
  h2zero/NimBLE-Arduino@^2.5.1
```

Do **not** touch `esp12e`/`nodemcuv2`/`d1_mini`'s `lib_deps` — those stay ESP8266-only. `nodemcu-32s`, `esp32-s3-devkitc-1`, `esp32-c6-devkitc-1`, and `esp32-c3-supermini` all use `lib_deps = ${env:esp32dev.lib_deps}`, so they pick up NimBLE automatically.

- [ ] **Step 2: Add the config field**

In `src/config/ConfigStore.h`, after line 36 (`char ambMapping[16] = "right";`), add:

```cpp
    bool     bleEnabled   = true;
```

- [ ] **Step 3: Load and save the field**

In `src/config/ConfigStore.cpp`, in `ConfigStore::load`, after the `ambMapping` line (line 58), add:

```cpp
    cfg.bleEnabled = doc["bleEnabled"] | cfg.bleEnabled;
```

In `ConfigStore::save`, after the `doc["ambMapping"] = cfg.ambMapping;` line (line 85), add:

```cpp
    doc["bleEnabled"]     = cfg.bleEnabled;
```

- [ ] **Step 4: Verify both platforms still compile**

Run:
```bash
pio run -e esp32dev
pio run -e esp12e
```
Expected: both `[SUCCESS]`. The esp12e build must not reference NimBLE at all (it won't — the dependency was only added to the ESP32 block).

- [ ] **Step 5: Commit**

```bash
git add platformio.ini src/config/ConfigStore.h src/config/ConfigStore.cpp
git commit -m "feat: add NimBLE dependency and bleEnabled config flag"
```

---

### Task 2: Extract shared `applyCoreParams` param router

**Files:**
- Create: `src/net/CoreParamRouter.h`
- Create: `src/net/CoreParamRouter.cpp`
- Modify: `src/net/WebServer.cpp:335-368` (`handleWsMessage`)

**Interfaces:**
- Produces: `struct ParamApplyResult { bool anyChanged; bool discreteChanged; }` and `ParamApplyResult applyCoreParams(Config& cfg, JsonDocument& doc)`. Consumed by `WebServer.cpp` (this task) and `BleServer.cpp` (Task 3).
- Consumes: nothing new — mirrors the existing continuous/discrete rules currently inline in `handleWsMessage`, minus the WS-only fields (`ambPollMs`, `tvIp`, `ambMapping`), which stay in `WebServer.cpp`.

- [ ] **Step 1: Create `CoreParamRouter.h`**

```cpp
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
```

- [ ] **Step 2: Create `CoreParamRouter.cpp`**

```cpp
#include "CoreParamRouter.h"

ParamApplyResult applyCoreParams(Config& cfg, JsonDocument& doc) {
    ParamApplyResult r;

    // Continuous params: apply immediately; caller must NOT save/broadcast
    // right away (that fights sliders and causes teleport-back jitter).
    if (doc.containsKey("brightness")) { cfg.brightness = doc["brightness"]; r.anyChanged = true; }
    if (doc.containsKey("speed"))      { cfg.speed      = doc["speed"];      r.anyChanged = true; }
    if (doc.containsKey("intensity"))  { cfg.intensity  = doc["intensity"];  r.anyChanged = true; }
    if (doc.containsKey("colorPrimary")) {
        const char* hex = doc["colorPrimary"];
        if (hex && hex[0] == '#') cfg.colorPrimary = strtoul(hex + 1, nullptr, 16);
        r.anyChanged = true;
    }
    if (doc.containsKey("colorSecondary")) {
        const char* hex = doc["colorSecondary"];
        if (hex && hex[0] == '#') cfg.colorSecondary = strtoul(hex + 1, nullptr, 16);
        r.anyChanged = true;
    }

    // Discrete params: caller should save to flash + broadcast/notify.
    if (doc.containsKey("power")) { cfg.power = doc["power"]; r.anyChanged = r.discreteChanged = true; }
    if (doc.containsKey("effect")) {
        strlcpy(cfg.effect, doc["effect"] | "", sizeof(cfg.effect));
        r.anyChanged = r.discreteChanged = true;
    }
    if (doc.containsKey("palette")) {
        strlcpy(cfg.palette, doc["palette"] | "", sizeof(cfg.palette));
        r.anyChanged = r.discreteChanged = true;
    }

    return r;
}
```

- [ ] **Step 3: Refactor `WebServer::handleWsMessage` to use it**

In `src/net/WebServer.cpp`, add `#include "CoreParamRouter.h"` near the top (with the other includes), then replace the body of `handleWsMessage` (currently lines 335-368):

```cpp
void MilaWebServer::handleWsMessage(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json)) return;

    bool anyChanged      = false;
    bool discreteChanged = false; // needs save + broadcast

    // Continuous params: update config + LEDs immediately, but do NOT save or echo
    // (echoing would fight the slider and cause teleport-back jitter)
    if (doc.containsKey("brightness")) { _cfg->brightness = doc["brightness"]; anyChanged = true; }
    if (doc.containsKey("speed"))      { _cfg->speed      = doc["speed"];      anyChanged = true; }
    if (doc.containsKey("intensity"))  { _cfg->intensity  = doc["intensity"];  anyChanged = true; }
    if (doc.containsKey("colorPrimary")) {
        const char* hex = doc["colorPrimary"];
        if (hex && hex[0] == '#') _cfg->colorPrimary = strtoul(hex + 1, nullptr, 16);
        anyChanged = true;
    }
    if (doc.containsKey("colorSecondary")) {
        const char* hex = doc["colorSecondary"];
        if (hex && hex[0] == '#') _cfg->colorSecondary = strtoul(hex + 1, nullptr, 16);
        anyChanged = true;
    }
    if (doc.containsKey("ambPollMs")) { _cfg->ambPollMs = doc["ambPollMs"]; anyChanged = true; }

    // Discrete params: save to flash + broadcast so other clients see the change
    if (doc.containsKey("power"))      { _cfg->power = doc["power"];                                              anyChanged = discreteChanged = true; }
    if (doc.containsKey("effect"))     { strlcpy(_cfg->effect,     doc["effect"]     | "", sizeof(_cfg->effect));  anyChanged = discreteChanged = true; }
    if (doc.containsKey("palette"))    { strlcpy(_cfg->palette,    doc["palette"]    | "", sizeof(_cfg->palette)); anyChanged = discreteChanged = true; }
    if (doc.containsKey("tvIp"))       { strlcpy(_cfg->tvIp,       doc["tvIp"]       | "", sizeof(_cfg->tvIp));    anyChanged = discreteChanged = true; }
    if (doc.containsKey("ambMapping")) { strlcpy(_cfg->ambMapping, doc["ambMapping"] | "", sizeof(_cfg->ambMapping)); anyChanged = discreteChanged = true; }

    if (anyChanged)      _engine->applyConfig(*_cfg);
    if (discreteChanged) { _store->save(*_cfg); broadcastState(); }
}
```

with:

```cpp
void MilaWebServer::handleWsMessage(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json)) return;

    ParamApplyResult r = applyCoreParams(*_cfg, doc);
    bool anyChanged      = r.anyChanged;
    bool discreteChanged = r.discreteChanged;

    // WS-only continuous param
    if (doc.containsKey("ambPollMs")) { _cfg->ambPollMs = doc["ambPollMs"]; anyChanged = true; }

    // WS-only discrete params
    if (doc.containsKey("tvIp"))       { strlcpy(_cfg->tvIp,       doc["tvIp"]       | "", sizeof(_cfg->tvIp));    anyChanged = discreteChanged = true; }
    if (doc.containsKey("ambMapping")) { strlcpy(_cfg->ambMapping, doc["ambMapping"] | "", sizeof(_cfg->ambMapping)); anyChanged = discreteChanged = true; }

    if (anyChanged)      _engine->applyConfig(*_cfg);
    if (discreteChanged) { _store->save(*_cfg); broadcastState(); }
}
```

This is behavior-preserving: every `if (doc.containsKey(...))` check and its effect on `cfg`/`anyChanged`/`discreteChanged` is identical to before, just split between the shared function and the WS-only tail.

- [ ] **Step 4: Verify compilation on both platforms**

Run:
```bash
pio run -e esp32dev
pio run -e esp12e
```
Expected: both `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add src/net/CoreParamRouter.h src/net/CoreParamRouter.cpp src/net/WebServer.cpp
git commit -m "refactor: extract shared applyCoreParams from handleWsMessage"
```

---

### Task 3: `BleServer` — GATT service, chunked protocol, command handling

**Files:**
- Create: `src/net/BleServer.h`
- Create: `src/net/BleServer.cpp`

**Interfaces:**
- Consumes: `ParamApplyResult applyCoreParams(Config&, JsonDocument&)` from Task 2; `Config`/`ConfigStore`/`EffectsEngine` from existing code; `MilaWebServer` (forward-declared here, full type used only in the `.cpp`, calling its already-public `broadcastState()`).
- Produces: `class BleServer` with `begin(Config*, ConfigStore*, EffectsEngine*)`, `notifyState()`, `setWebServer(MilaWebServer*)`, `onCommandChunk(const uint8_t*, size_t)`. Consumed by Task 4 (`main.cpp`, `WebServer.cpp`).

- [ ] **Step 1: Create `BleServer.h`**

```cpp
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

    String _rxBuffer;          // NimBLE task only — no cross-task access
    bool   _rxDesynced = false; // NimBLE task only — true after an overflow drop, until the next seq==0

    // Shared between the NimBLE task (writer) and loop() (reader/clearer);
    // all access must happen under _mux.
    String       _pendingCommand;
    bool         _cmdPending = false;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    void handleCommand(const char* json);
    String buildCoreStateJson();
};
#endif
```

- [ ] **Step 2: Create `BleServer.cpp`**

```cpp
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
        portENTER_CRITICAL(&_mux);
        if (!_cmdPending) {
            // Only assign when the mailbox is empty: _pendingCommand then holds
            // no live heap buffer, so std::move always takes Arduino String's
            // zero-cost pointer-steal path, never its memmove+free reuse path.
            // If a previous command is still undrained, drop this new one
            // rather than risk that heap work while the critical section is held.
            _pendingCommand = std::move(_rxBuffer);
            _cmdPending = true;
        }
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
```

Note: `_web` is `nullptr` until Task 4 wires it via `setWebServer`, so `if (_web)` safely no-ops until then — this task compiles and works standalone.

- [ ] **Step 3: Verify compilation**

Run:
```bash
pio run -e esp32dev
pio run -e esp32-c3-supermini
```
Expected: both `[SUCCESS]`. (`BleServer` isn't instantiated yet, but PlatformIO compiles all `.cpp` files under `src/`, so this validates the NimBLE API usage for real.)

- [ ] **Step 4: Commit**

```bash
git add src/net/BleServer.h src/net/BleServer.cpp
git commit -m "feat: add BleServer GATT service with chunked command/state protocol"
```

---

### Task 4: Wire `BleServer` into `main.cpp`, cross-sync with `MilaWebServer`

**Files:**
- Modify: `src/net/WebServer.h` (forward-declare `BleServer`, add setter/pointer, guard with `#ifdef ESP32`)
- Modify: `src/net/WebServer.cpp:306-309` (`broadcastState`)
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `BleServer` from Task 3, `MilaWebServer::broadcastState()` (already public).
- Produces: cross-wiring so a change from either channel (WS or BLE) is reflected on both.

- [ ] **Step 1: Extend `WebServer.h`**

Change the top of `src/net/WebServer.h` from:

```cpp
#pragma once
#ifdef ESP32
#include <WebServer.h>
using WebServerClass = WebServer;
#else
#include <ESP8266WebServer.h>
using WebServerClass = ESP8266WebServer;
#endif
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
    WebServerClass   _http{80};
    WebSocketsServer _ws{81};
    Config*          _cfg    = nullptr;
    ConfigStore*     _store  = nullptr;
    EffectsEngine*   _engine = nullptr;
    bool             _pendingRestart = false;
```

to:

```cpp
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
```

(The rest of the file — `_scanActive` through the end — is unchanged.)

- [ ] **Step 2: Cross-notify BLE from `broadcastState`**

In `src/net/WebServer.cpp`, add near the top (with the other includes):

```cpp
#ifdef ESP32
#include "BleServer.h"
#endif
```

Then change `broadcastState` (lines 306-309) from:

```cpp
void MilaWebServer::broadcastState() {
    String json = buildStateJson();
    _ws.broadcastTXT(json.c_str());
}
```

to:

```cpp
void MilaWebServer::broadcastState() {
    String json = buildStateJson();
    _ws.broadcastTXT(json.c_str());
#ifdef ESP32
    if (_ble) _ble->notifyState();
#endif
}
```

- [ ] **Step 3: Instantiate and wire `BleServer` in `main.cpp`**

At the top of `src/main.cpp`, after `#include "net/WebServer.h"`, add:

```cpp
#ifdef ESP32
#include "net/BleServer.h"
#endif
```

After `static MilaWebServer  webServer;`, add:

```cpp
#ifdef ESP32
static BleServer      bleServer;
#endif
```

In `setup()`, right after the existing `webServer.begin(&cfg, &cfgStore, &engine);` line, add:

```cpp
#ifdef ESP32
    if (cfg.bleEnabled) {
        Serial.println("[ble]   starting BLE server...");
        bleServer.begin(&cfg, &cfgStore, &engine);
        webServer.setBleServer(&bleServer);
        bleServer.setWebServer(&webServer);
    }
#endif
```

In `loop()`, right after the existing `webServer.loop();` line, add:

```cpp
#ifdef ESP32
    bleServer.loop(); // drains any command reassembled on the NimBLE task and applies it here
#endif
```

This call is safe even when `cfg.bleEnabled` is false (BLE never started) — `BleServer::loop()` just checks its internal `_cmdPending` flag, which stays false if `begin()` was never called. NimBLE-Arduino's own GATT/advertising handling still runs on its own FreeRTOS task and needs no polling — this call only drains the command hand-off added in Task 3 to keep `Config`/`EffectsEngine` mutation on the single-threaded main loop.

- [ ] **Step 4: Verify compilation on both platforms**

Run:
```bash
pio run -e esp32dev
pio run -e esp12e
```
Expected: both `[SUCCESS]`.

- [ ] **Step 5: Commit**

```bash
git add src/net/WebServer.h src/net/WebServer.cpp src/main.cpp
git commit -m "feat: wire BleServer into boot sequence, cross-sync with WebServer"
```

---

### Task 5: Extract `useThrottledSender` from `useWebSocket`

**Files:**
- Create: `web/src/hooks/useThrottledSender.ts`
- Modify: `web/src/hooks/useWebSocket.ts`

**Interfaces:**
- Produces: `useThrottledSender(sendRaw: (json: string) => void): (data: object) => void`. Consumed by `useWebSocket` (this task) and `useBluetoothTransport` (Task 6).
- Produces (updated): `useWebSocket(url, onMessage): { status: WsStatus; send: (data: object) => void; connect: () => void }` — adds a no-op `connect` so the shape matches `useBluetoothTransport`'s later.

- [ ] **Step 1: Create `useThrottledSender.ts`**

```ts
import { useRef, useCallback } from 'react'

const THROTTLE_MS = 50 // max one message per 50ms per key, to avoid flooding the device

/**
 * Per-key throttled sender: at most one message per key per THROTTLE_MS.
 * Keys sent too recently are batched and flushed together after the window.
 */
export function useThrottledSender(sendRaw: (json: string) => void) {
  const lastSentRef = useRef<Record<string, number>>({})
  const pendingRef  = useRef<Record<string, unknown>>({})
  const timerRef    = useRef<ReturnType<typeof setTimeout> | null>(null)

  const flush = useCallback(() => {
    timerRef.current = null
    const pending = pendingRef.current
    if (Object.keys(pending).length === 0) return
    pendingRef.current = {}
    sendRaw(JSON.stringify(pending))
  }, [sendRaw])

  const send = useCallback((data: object) => {
    const now = Date.now()
    const entries = Object.entries(data as Record<string, unknown>)

    const immediate: Record<string, unknown> = {}
    const deferred:  Record<string, unknown> = {}

    for (const [k, v] of entries) {
      const last = lastSentRef.current[k] ?? 0
      if (now - last >= THROTTLE_MS) {
        immediate[k] = v
        lastSentRef.current[k] = now
      } else {
        deferred[k] = v
      }
    }

    if (Object.keys(immediate).length > 0) {
      sendRaw(JSON.stringify(immediate))
    }

    if (Object.keys(deferred).length > 0) {
      Object.assign(pendingRef.current, deferred)
      if (!timerRef.current) {
        timerRef.current = setTimeout(flush, THROTTLE_MS)
      }
    }
  }, [flush])

  return send
}
```

- [ ] **Step 2: Refactor `useWebSocket.ts` to use it**

Replace the full contents of `web/src/hooks/useWebSocket.ts` with:

```ts
import { useEffect, useRef, useCallback } from 'react'
import { useThrottledSender } from './useThrottledSender'

export type WsStatus = 'connecting' | 'open' | 'closed'

export function useWebSocket(
  url: string,
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void; connect: () => void } {
  const wsRef        = useRef<WebSocket | null>(null)
  const statusRef    = useRef<WsStatus>('connecting')
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage

  useEffect(() => {
    const ws = new WebSocket(url)
    wsRef.current = ws
    ws.onopen    = () => { statusRef.current = 'open' }
    ws.onclose   = () => { statusRef.current = 'closed'; setTimeout(() => {
      // simple reconnect
      wsRef.current = new WebSocket(url)
    }, 2000) }
    ws.onmessage = (e) => {
      try { onMessageRef.current(JSON.parse(e.data)) } catch {}
    }
    return () => ws.close()
  }, [url])

  const sendRaw = useCallback((json: string) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(json)
    }
  }, [])

  const send = useThrottledSender(sendRaw)

  // WS auto-connects on mount; exposed as a no-op only so useLedState has a
  // uniform { status, send, connect } shape across both transports.
  const connect = useCallback(() => {}, [])

  return { status: statusRef.current, send, connect }
}
```

This is behavior-preserving: `sendRaw` reproduces the exact `readyState === OPEN` guard that both the "immediate" and "deferred/flush" paths used before, and `useThrottledSender` reproduces the per-key throttle/batch logic unchanged.

- [ ] **Step 3: Verify it builds**

Run (from `web/`):
```bash
npx tsc -b --noEmit
```
Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add web/src/hooks/useThrottledSender.ts web/src/hooks/useWebSocket.ts
git commit -m "refactor: extract useThrottledSender from useWebSocket"
```

---

### Task 6: `useBluetoothTransport` hook

**Files:**
- Create: `web/src/hooks/useBluetoothTransport.ts`
- Modify: `web/package.json` (add `@types/web-bluetooth` devDependency)
- Modify: `web/tsconfig.app.json` (add `"web-bluetooth"` to the `types` array)

**Interfaces:**
- Consumes: `useThrottledSender` (Task 5), `WsStatus` type (`useWebSocket.ts`).
- Produces: `useBluetoothTransport(onMessage: (data: unknown) => void): { status: WsStatus; send: (data: object) => void; connect: () => void; error: string | null }`. Consumed by `useLedState` (Task 7).

- [ ] **Step 1: Add Web Bluetooth types**

In `web/package.json`, add to `devDependencies` (alphabetical, matching existing style):

```json
    "@types/web-bluetooth": "^0.0.21",
```

Run (from `web/`):
```bash
npm install
```
Expected: `package-lock.json` updates, no errors.

`web/tsconfig.app.json` already sets `"types": ["vite/client"]` — once `types` is explicitly listed, TypeScript stops auto-including every `@types/*` package and only loads the ones named there. Change that line to:

```json
    "types": ["vite/client", "web-bluetooth"],
```

Without this, `tsc -b --noEmit` in Step 4 below fails with `Cannot find name 'BluetoothRemoteGATTCharacteristic'` / `Property 'bluetooth' does not exist on type 'Navigator'`, even though `@types/web-bluetooth` is installed.

- [ ] **Step 2: Create `useBluetoothTransport.ts`**

```ts
import { useCallback, useRef, useState } from 'react'
import { useThrottledSender } from './useThrottledSender'
import type { WsStatus } from './useWebSocket'

const SERVICE_UUID    = '7a2eec00-4b0f-4bde-9f3f-1a7c6d9b2e10'
const CMD_CHAR_UUID   = '7a2eec01-4b0f-4bde-9f3f-1a7c6d9b2e10'
const STATE_CHAR_UUID = '7a2eec02-4b0f-4bde-9f3f-1a7c6d9b2e10'
const CHUNK_PAYLOAD   = 100 // keep in sync with CHUNK_PAYLOAD in BleServer.cpp

export function useBluetoothTransport(
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void; connect: () => void; error: string | null } {
  const [status, setStatus] = useState<WsStatus>('closed')
  const [error, setError]   = useState<string | null>(null)
  const cmdCharRef   = useRef<BluetoothRemoteGATTCharacteristic | null>(null)
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage
  const rxBufferRef  = useRef<Uint8Array[]>([])

  const handleNotification = useCallback((event: Event) => {
    const target = event.target as BluetoothRemoteGATTCharacteristic
    const value = target.value
    if (!value) return
    const bytes = new Uint8Array(value.buffer)
    if (bytes.length < 2) return
    const seq  = bytes[0]
    const more = bytes[1]
    const payload = bytes.slice(2)

    if (seq === 0) rxBufferRef.current = []
    rxBufferRef.current.push(payload)

    if (!more) {
      const total = rxBufferRef.current.reduce((n, b) => n + b.length, 0)
      const joined = new Uint8Array(total)
      let offset = 0
      for (const chunk of rxBufferRef.current) { joined.set(chunk, offset); offset += chunk.length }
      rxBufferRef.current = []
      try {
        const json = new TextDecoder().decode(joined)
        onMessageRef.current(JSON.parse(json))
      } catch {
        // dropped/corrupt frame train — ignore, the next state push will recover
      }
    }
  }, [])

  const sendRaw = useCallback((json: string) => {
    const cmdChar = cmdCharRef.current
    if (!cmdChar) return
    const bytes = new TextEncoder().encode(json)

    const writeChunks = async () => {
      let offset = 0
      let seq = 0
      do {
        const chunkLen = Math.min(CHUNK_PAYLOAD, bytes.length - offset)
        const more = offset + chunkLen < bytes.length
        const frame = new Uint8Array(2 + chunkLen)
        frame[0] = seq
        frame[1] = more ? 1 : 0
        frame.set(bytes.subarray(offset, offset + chunkLen), 2)
        await cmdChar.writeValueWithoutResponse(frame)
        offset += chunkLen
        seq++
      } while (offset < bytes.length)
    }
    writeChunks().catch((err: Error) => {
      setError(err.message || 'Bluetooth write failed')
    })
  }, [])

  const send = useThrottledSender(sendRaw)

  const connect = useCallback(() => {
    setError(null)

    if (!navigator.bluetooth) {
      setError('Web Bluetooth is not supported in this browser')
      return
    }

    setStatus('connecting')

    navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    })
      .then(device => {
        device.addEventListener('gattserverdisconnected', () => {
          setStatus('closed')
          cmdCharRef.current = null
        })
        return device.gatt!.connect()
      })
      .then(server => server.getPrimaryService(SERVICE_UUID))
      .then(service => Promise.all([
        service.getCharacteristic(CMD_CHAR_UUID),
        service.getCharacteristic(STATE_CHAR_UUID),
      ]))
      .then(([cmdChar, stateChar]) => {
        cmdCharRef.current = cmdChar
        stateChar.addEventListener('characteristicvaluechanged', handleNotification)
        return stateChar.startNotifications()
      })
      .then(() => setStatus('open'))
      .catch((err: Error) => {
        setStatus('closed')
        setError(err.message || 'Bluetooth connection failed')
      })
  }, [handleNotification])

  return { status, send, connect, error }
}
```

- [ ] **Step 3: Verify it builds**

Run (from `web/`):
```bash
npx tsc -b --noEmit
```
Expected: no errors (in particular, no missing types for `navigator.bluetooth`/`BluetoothRemoteGATTCharacteristic` — confirms `@types/web-bluetooth` is wired up).

- [ ] **Step 4: Commit**

```bash
git add web/src/hooks/useBluetoothTransport.ts web/package.json web/package-lock.json
git commit -m "feat: add useBluetoothTransport hook (Web Bluetooth GATT client)"
```

---

### Task 7: Transport capabilities + wire `useLedState`

**Files:**
- Create: `web/src/lib/capabilities.ts`
- Modify: `web/src/hooks/useLedState.ts:1-3,61,78,85-86`

**Interfaces:**
- Produces: `capabilities: { presets, stripConfig, ambilight, wifiReset }` and `TRANSPORT: 'wifi' | 'ble'`, both exported from `web/src/lib/capabilities.ts`. Consumed by Task 8 (`App.tsx`) and Task 9 (`TabBar.tsx`, `SettingsTab.tsx`).
- Produces (updated): `useLedState(wsUrl)` return value gains `connect: () => void` and `error: string | null`.

- [ ] **Step 1: Create `capabilities.ts`**

```ts
export interface TransportCapabilities {
  presets: boolean
  stripConfig: boolean
  ambilight: boolean
  wifiReset: boolean
}

const isBle = import.meta.env.VITE_TRANSPORT === 'ble'

export const capabilities: TransportCapabilities = isBle
  ? { presets: false, stripConfig: false, ambilight: false, wifiReset: false }
  : { presets: true, stripConfig: true, ambilight: true, wifiReset: true }

export const TRANSPORT: 'wifi' | 'ble' = isBle ? 'ble' : 'wifi'
```

- [ ] **Step 2: Wire transport selection into `useLedState.ts`**

At the top of `web/src/hooks/useLedState.ts`, change:

```ts
import { useState, useCallback } from 'react'
import { useWebSocket } from './useWebSocket'
```

to:

```ts
import { useState, useCallback } from 'react'
import { useWebSocket } from './useWebSocket'
import { useBluetoothTransport } from './useBluetoothTransport'
import { TRANSPORT } from '@/lib/capabilities'
```

Then, inside `useLedState`, replace:

```ts
  const { send, status } = useWebSocket(wsUrl, onMessage)
```

with:

```ts
  /* eslint-disable react-hooks/rules-of-hooks -- TRANSPORT is a build-time
     constant (Vite inlines import.meta.env and dead-code-eliminates the
     unused branch), so exactly one of these two hooks ever actually runs
     for a given bundle. A block disable/enable pair is used instead of
     eslint-disable-next-line because the ternary's hook calls span multiple
     lines, which a single-line directive does not cover. */
  const { send, status, connect, error } = TRANSPORT === 'ble'
    ? useBluetoothTransport(onMessage)
    : { ...useWebSocket(wsUrl, onMessage), error: null as string | null }
  /* eslint-enable react-hooks/rules-of-hooks */
```

And finally, change the return statement from:

```ts
  return { state, update, status, scanProgress, foundTvs, send }
```

to:

```ts
  return { state, update, status, scanProgress, foundTvs, send, connect, error }
```

- [ ] **Step 3: Verify it builds**

Run (from `web/`):
```bash
npx tsc -b --noEmit
npx eslint src/hooks/useLedState.ts src/lib/capabilities.ts
```
Expected: no errors from either command — the `eslint` run specifically confirms the block-disable comment actually suppresses `react-hooks/rules-of-hooks` on both branches of the ternary (a single-line `eslint-disable-next-line` would not, since the hook calls span multiple lines).

- [ ] **Step 4: Commit**

```bash
git add web/src/lib/capabilities.ts web/src/hooks/useLedState.ts
git commit -m "feat: select WS/BLE transport via VITE_TRANSPORT build flag"
```

---

### Task 8: BLE connect dialog + `App.tsx` gating

**Files:**
- Create: `web/src/components/shared/BleConnectDialog.tsx`
- Modify: `web/src/App.tsx`
- Modify: `web/src/i18n/en.json`, `web/src/i18n/pl.json` (add `ble` keys)

**Interfaces:**
- Consumes: `TRANSPORT` from `@/lib/capabilities` (Task 7), `WsStatus` type, `useLedState`'s new `connect`/`error` fields.
- Produces: `BleConnectDialog` component, gating logic in `App.tsx` (only reached when `TRANSPORT === 'ble'`).

- [ ] **Step 1: Add i18n strings**

In `web/src/i18n/en.json`, add a new top-level key (after `"ambilight"`):

```json
  "ambilight": { "status": "Status", "polling": "Polling", "idle": "Idle", "error": "Error" },
  "ble": {
    "title": "Connect to MilaLED",
    "description": "Pair with your strip over Bluetooth to control it without joining its WiFi network.",
    "connect": "Connect via Bluetooth",
    "connecting": "Connecting…",
    "unsupported": "This browser doesn't support Web Bluetooth. Use Chrome on Android, or the Bluefy app on iOS."
  }
```

In `web/src/i18n/pl.json`, add the matching key (after `"ambilight"`):

```json
  "ambilight": { "status": "Status", "polling": "Pobieranie", "idle": "Bezczynny", "error": "Błąd" },
  "ble": {
    "title": "Połącz z MilaLED",
    "description": "Sparuj się z taśmą przez Bluetooth, aby sterować nią bez łączenia się z jej siecią WiFi.",
    "connect": "Połącz przez Bluetooth",
    "connecting": "Łączenie…",
    "unsupported": "Ta przeglądarka nie obsługuje Web Bluetooth. Użyj Chrome na Androidzie lub aplikacji Bluefy na iOS."
  }
```

- [ ] **Step 2: Create `BleConnectDialog.tsx`**

```tsx
import { useTranslation } from 'react-i18next'
import type { WsStatus } from '@/hooks/useWebSocket'

interface Props {
  status: WsStatus
  error: string | null
  onConnect: () => void
}

export function BleConnectDialog({ status, error, onConnect }: Props) {
  const { t } = useTranslation()
  const supported = typeof navigator !== 'undefined' && 'bluetooth' in navigator

  return (
    <div className="min-h-[100dvh] flex flex-col items-center justify-center gap-4 bg-background p-6 text-center">
      <h1 className="text-lg font-semibold text-zinc-100">{t('ble.title')}</h1>
      {!supported ? (
        <p className="text-sm text-zinc-400 max-w-xs">{t('ble.unsupported')}</p>
      ) : (
        <>
          <p className="text-sm text-zinc-400 max-w-xs">{t('ble.description')}</p>
          <button
            onClick={onConnect}
            disabled={status === 'connecting'}
            className="px-6 py-3 rounded-xl bg-amber-400 hover:bg-amber-300 text-zinc-950 font-semibold disabled:opacity-50 transition-colors"
          >
            {status === 'connecting' ? t('ble.connecting') : t('ble.connect')}
          </button>
          {error && <p className="text-sm text-red-400 max-w-xs">{error}</p>}
        </>
      )}
    </div>
  )
}
```

- [ ] **Step 3: Gate `App.tsx` on connection status**

In `web/src/App.tsx`, add imports:

```tsx
import { BleConnectDialog } from '@/components/shared/BleConnectDialog'
import { TRANSPORT } from '@/lib/capabilities'
```

Change the destructuring from:

```tsx
  const { state, update, status, scanProgress, foundTvs } = useLedState(WS_URL)
```

to:

```tsx
  const { state, update, status, scanProgress, foundTvs, connect, error } = useLedState(WS_URL)
```

Immediately after the `useEffect` that toggles the dark-mode class (before the `return (` of the main UI), add:

```tsx
  if (TRANSPORT === 'ble' && status !== 'open') {
    return <BleConnectDialog status={status} error={error} onConnect={connect} />
  }
```

- [ ] **Step 4: Verify it builds**

Run (from `web/`):
```bash
npx tsc -b --noEmit
```
Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add web/src/components/shared/BleConnectDialog.tsx web/src/App.tsx web/src/i18n/en.json web/src/i18n/pl.json
git commit -m "feat: add BLE connect dialog, gate App on connection status"
```

---

### Task 9: Hide out-of-scope tabs/sections in the BLE build

**Files:**
- Modify: `web/src/components/layout/TabBar.tsx`
- Modify: `web/src/components/tabs/SettingsTab.tsx`

**Interfaces:**
- Consumes: `capabilities` from `@/lib/capabilities` (Task 7).

- [ ] **Step 1: Hide the Presets tab when unsupported**

Replace the full contents of `web/src/components/layout/TabBar.tsx` with:

```tsx
import { useTranslation } from 'react-i18next'
import { Lightning, PaintBrush, FloppyDisk, GearSix } from '@phosphor-icons/react'
import { capabilities } from '@/lib/capabilities'

const TABS = [
  { id: 'effects',  Icon: Lightning,  labelKey: 'tabs.effects' },
  { id: 'color',    Icon: PaintBrush, labelKey: 'tabs.color' },
  { id: 'presets',  Icon: FloppyDisk, labelKey: 'tabs.presets' },
  { id: 'settings', Icon: GearSix,    labelKey: 'tabs.settings' },
] as const

export type TabId = typeof TABS[number]['id']

interface Props {
  active: TabId
  onSelect: (id: TabId) => void
}

export function TabBar({ active, onSelect }: Props) {
  const { t } = useTranslation()
  const visibleTabs = TABS.filter(tab => tab.id !== 'presets' || capabilities.presets)

  return (
    <nav className="fixed bottom-0 left-0 right-0 bg-zinc-950/95 backdrop-blur border-t border-zinc-800 pb-4 z-10">
      <div className={visibleTabs.length === 4 ? 'grid grid-cols-4' : 'grid grid-cols-3'}>
        {visibleTabs.map(({ id, Icon, labelKey }) => {
          const isActive = active === id
          return (
            <button
              key={id}
              onClick={() => onSelect(id)}
              className={`flex flex-col items-center gap-1 py-3 transition-colors active:scale-95 ${
                isActive ? 'text-amber-400' : 'text-zinc-500 hover:text-zinc-300'
              }`}
            >
              <Icon size={22} weight={isActive ? 'fill' : 'regular'} />
              <span className="text-[10px] font-medium">{t(labelKey)}</span>
            </button>
          )
        })}
      </div>
    </nav>
  )
}
```

(Both `grid-cols-4` and `grid-cols-3` are written as literal strings so Tailwind's static class scanner picks up both — a template-interpolated class name like `` `grid-cols-${n}` `` would not be detected.)

- [ ] **Step 2: Gate Strip/Network/Ambilight sections in `SettingsTab.tsx`**

In `web/src/components/tabs/SettingsTab.tsx`, add an import:

```tsx
import { capabilities } from '@/lib/capabilities'
```

Wrap the **Strip** section — currently:

```tsx
      {/* Strip */}
      <section className="space-y-2">
        ...
      </section>
```

(lines 132-299 in the current file, from the `{/* Strip */}` comment through its closing `</section>` right before the `{/* Network */}` comment) — in a capability check:

```tsx
      {capabilities.stripConfig && (
      <section className="space-y-2">
        {/* Strip */}
        ...
      </section>
      )}
```

Concretely: insert `{capabilities.stripConfig && (` on the line immediately before `<section className="space-y-2">` that precedes the `{/* Strip */}` comment (i.e. before current line 133), and insert `)}` on its own line immediately after that section's closing `</section>` (current line 299).

Wrap the **Network** section (current lines 301-348, from `{/* Network */}` through its `</section>`) the same way, using `capabilities.wifiReset`:

```tsx
      {capabilities.wifiReset && (
      <section className="space-y-2">
        {/* Network */}
        ...
      </section>
      )}
```

Wrap the **Ambilight** section (current lines 350-441, from `{/* Ambilight */}` through its `</section>`) the same way, using `capabilities.ambilight`:

```tsx
      {capabilities.ambilight && (
      <section className="space-y-2">
        {/* Ambilight */}
        ...
      </section>
      )}
```

Leave the **Language** section (current lines 443-463) and **Firmware version** section (current lines 465-474) untouched — both are relevant regardless of transport.

- [ ] **Step 3: Verify it builds**

Run (from `web/`):
```bash
npx tsc -b --noEmit
```
Expected: no errors (in particular, no unbalanced-JSX errors from the added wrapping parens/braces).

- [ ] **Step 4: Commit**

```bash
git add web/src/components/layout/TabBar.tsx web/src/components/tabs/SettingsTab.tsx
git commit -m "feat: hide presets/strip-config/ambilight/wifi-reset in BLE build"
```

---

### Task 10: `build:ble` script + Vite mode

**Files:**
- Create: `web/.env.ble`
- Modify: `web/package.json` (scripts)

**Interfaces:**
- Produces: `npm run build:ble` in `web/`, outputting to `web/dist-ble/` with `VITE_TRANSPORT=ble` baked in. Consumed by Task 11 (GitHub Actions workflow).

- [ ] **Step 1: Create the mode-specific env file**

```
VITE_TRANSPORT=ble
```

Save as `web/.env.ble`. Vite automatically loads `.env.<mode>` files when built with `--mode <mode>`.

- [ ] **Step 2: Add the build script**

In `web/package.json`, change the `scripts` block from:

```json
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "lint": "eslint .",
    "preview": "vite preview"
  },
```

to:

```json
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "build:ble": "tsc -b && vite build --mode ble --outDir dist-ble",
    "lint": "eslint .",
    "preview": "vite preview"
  },
```

`scripts/build_web.py` (the firmware-embedding pipeline) is untouched — it calls `npm run build`, which still produces the unchanged WiFi bundle in `web/dist/`.

- [ ] **Step 3: Verify the BLE bundle builds cleanly**

Run (from `web/`):
```bash
npm run build:ble
```
Expected: succeeds, produces `web/dist-ble/index.html` and assets. Manually open `web/dist-ble/index.html` in a text editor (or grep) to confirm it references the same JS bundle structure as `web/dist/` — i.e. it's a real Vite build output, not an error page.

- [ ] **Step 4: Commit**

```bash
git add web/.env.ble web/package.json
git commit -m "feat: add build:ble script producing a Web-Bluetooth-only bundle"
```

---

### Task 11: GitHub Actions — deploy BLE build to GitHub Pages

**Files:**
- Create: `.github/workflows/deploy-ble-pages.yml`

**Interfaces:**
- Consumes: `npm run build:ble` (Task 10), outputting `web/dist-ble`.

- [ ] **Step 1: Create the workflow**

```yaml
name: Deploy BLE control page to GitHub Pages

on:
  push:
    branches: [master]
    paths:
      - 'web/**'
      - '.github/workflows/deploy-ble-pages.yml'
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: true

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
          cache-dependency-path: web/package-lock.json
      - name: Install dependencies
        run: npm ci
        working-directory: web
      - name: Build BLE bundle
        run: npm run build:ble
        working-directory: web
      - uses: actions/configure-pages@v5
      - uses: actions/upload-pages-artifact@v3
        with:
          path: web/dist-ble

  deploy:
    needs: build
    runs-on: ubuntu-latest
    permissions:
      pages: write
      id-token: write
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

- [ ] **Step 2: One-time repo setting (manual, not part of this commit)**

In the GitHub repo's Settings → Pages, set **Source** to "GitHub Actions" (instead of a branch). This can't be done from a commit — note it for whoever merges this, and confirm it's done before relying on the deployed URL.

- [ ] **Step 3: Verify the workflow YAML is well-formed**

Run:
```bash
python -c "import yaml,sys; yaml.safe_load(open('.github/workflows/deploy-ble-pages.yml'))" && echo OK
```
Expected: `OK`. (This only validates YAML syntax — actual execution can only be confirmed by pushing and watching the Actions tab, which is a `git push` and thus outside this plan's scope; flag it to the user before pushing.)

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/deploy-ble-pages.yml
git commit -m "ci: deploy BLE web UI build to GitHub Pages"
```

---

### Task 12: Manual end-to-end hardware verification

This task has no code changes — it's a checklist to run against real hardware once Tasks 1-11 are merged, since BLE/GATT behavior can't be verified in CI or this sandbox. Not a substitute for Tasks 1-11's own compile/build verification — this is the final functional check.

- [ ] **Step 1: Flash and boot**

```bash
pio run -e esp32dev --target uploadfs
pio run -e esp32dev --target upload
pio device monitor
```
Expected serial output includes `[ble]   starting BLE server...` right after the WiFi connection lines.

- [ ] **Step 2: Confirm the device is discoverable**

Using nRF Connect (Android/iOS) or a similar BLE scanner, scan for "MilaLED". Confirm the custom service `7a2eec00-...` appears with the two characteristics (`7a2eec01-...` write, `7a2eec02-...` notify).

- [ ] **Step 3: Deploy and test the GitHub Pages BLE UI**

After Task 11's workflow has run once (requires the repo's Pages source set to "GitHub Actions" per Task 11 Step 2), open the deployed URL on:
- **Android Chrome:** tap "Connect via Bluetooth", pick "MilaLED" from the picker, confirm the UI unlocks and mirrors the strip's current state (power/effect/brightness/etc.).
- **iOS Bluefy:** same flow — this is the harder case since Bluefy is a third-party WebKit wrapper; note any quirks.

- [ ] **Step 4: Verify core controls work over BLE**

Toggle power, change effect, drag the brightness/speed/intensity sliders, change primary/secondary color, change palette. Confirm the physical strip responds within roughly the same latency as the WiFi UI.

- [ ] **Step 5: Verify cross-channel sync**

With the WiFi-served page open in one browser tab and the GitHub Pages BLE page connected in another, change the effect from the WiFi tab and confirm the BLE tab's UI updates to match (and vice versa) — this exercises the Task 4 cross-notify wiring.

- [ ] **Step 6: Verify hidden tabs**

Confirm the BLE-connected UI shows only Effects/Color/Settings (no Presets tab), and that Settings shows only Language + Firmware version (no Strip/Network/Ambilight sections).

- [ ] **Step 7: Verify disconnect/reconnect**

Move the phone out of range (or toggle its Bluetooth off), confirm the UI falls back to the connect dialog, then reconnect and confirm control resumes.

- [ ] **Step 8: Regression-check the WiFi build**

Open the device's own WiFi-served page as before and confirm nothing changed — all four tabs present, all existing functionality (presets, strip reconfig+reboot, Ambilight scan, WiFi reset) working as it did before this feature was added.

- [ ] **Step 9: Verify negotiated MTU on both target platforms**

Added per the final whole-branch review (see "Post-Implementation Fixes" below): `notifyState()` now clamps its chunk size to the actual negotiated ATT MTU rather than assuming 247. Confirm on both Android Chrome and Bluefy/iOS that a multi-segment state update (configure 2+ active segments on the WiFi build first, so `buildCoreStateJson()`'s payload is large enough to require multiple chunks) round-trips correctly regardless of whatever MTU each platform actually negotiates.

- [ ] **Step 10: Measure WiFi/Hyperion impact of always-on BLE advertising**

ESP32's BLE and WiFi radios share one antenna and a coexistence scheduler. With a Hyperion/HyperHDR source streaming UDP frames, compare frame smoothness/jitter with `bleEnabled` on vs. off (toggle added in "Post-Implementation Fixes" below, in the WiFi build's Strip section). If BLE advertising visibly degrades Hyperion streaming, note it — the toggle is the documented mitigation.

No commit for this task — if any step surfaces a bug, fix it in the relevant earlier task's files and re-run that task's own verification step before returning here.

---

## Post-Implementation Fixes (Final Whole-Branch Review)

After Tasks 1-11 were each individually implemented and reviewed, a final whole-branch review (the broad review a per-task gate can't do) found 2 Critical and 4 Important issues that only became visible with the complete picture. This section documents the fixes applied in response, using the same task-brief-driven, implement-then-review process as Tasks 1-11.

**FR-1 (Critical): GitHub Pages deploy would serve a blank page.** `web/vite.config.ts` never set a `base`, so the BLE build's asset paths resolve against `/` instead of the actual GitHub Pages subpath (`https://im-tesla.github.io/MilaLED/`), 404ing every JS/CSS asset. `web/index.html`'s inline `<style>` font `url('/fonts/...')` has the same problem — it's a `public/`-directory asset referenced by a hardcoded absolute path, which Vite does not automatically rewrite for a non-root `base` (that's expected, documented behavior — `public/` asset references must be written to account for `base` manually, unlike bundled JS/CSS imports which Vite rewrites automatically).

**FR-2 (Critical): BLE clients never receive the strip's actual state.** `BleServer`'s state characteristic had no `onSubscribe` handler and `main.cpp`/`useBluetoothTransport.ts` never requested a state push after connecting, so a freshly-connected BLE client's UI showed hardcoded frontend defaults (`useLedState.ts`'s `DEFAULT`) until something happened to change a *discrete* param on either channel. Fixed by adding an `onSubscribe` callback that requests an initial `notifyState()` push, safely handed off through the same mailbox mechanism as commands (never called directly from the NimBLE task).

**FR-3 (Important): Overlapping BLE GATT writes.** `useThrottledSender` throttles per-key, so two different controls (e.g. brightness slider + speed slider) can each independently fire an "immediate" send in the same tick. `useBluetoothTransport`'s `sendRaw` started a fresh `writeChunks()` for each call with no serialization — two in-flight multi-chunk writes on the same characteristic risk `NetworkError: GATT operation already in progress` (Chrome) or interleaved chunk trains corrupting each other's reassembly on the firmware side. Fixed with a promise-chain write queue.

**FR-4 (Important): `bleEnabled` was unreachable from the UI/API.** The config flag existed (Task 1) and was read at boot (Task 4), but nothing ever wrote it — the only way to disable the BLE radio was hand-editing `config.json` on the filesystem image. Fixed by adding it to the existing reboot-gated `/api/strip` flow (the same mechanism already used for `dataPin`/`colorOrder`/`chipset`) and adding a toggle to the WiFi build's Settings → Strip section.

**FR-5 (Important): The command mailbox's `std::move`-based hand-off could still allocate under a backlog.** Tasks 3's hardening rounds closed the unconditional heap-copy case, but a residual narrow case remained (documented in Task 3's own history). The final review's recommendation — replace the `String`-based mailbox with fixed-size `char` buffers — eliminates the possibility structurally rather than continuing to guard around it: a bounded `memcpy` inside a critical section can never allocate, never fragment the heap, and never hit any of Arduino `String::move()`'s capacity-reuse edge cases. This supersedes the `std::move`/`_cmdPending`-guard mechanics described in Task 3's Global Constraints notes above; those notes are left as a historical record of the iteration, but `BleServer.h`/`.cpp`'s actual final code (below) uses fixed buffers throughout.

**FR-6 (Important): `notifyState()` assumed a negotiated MTU it never verified.** Requesting MTU 247 in `begin()` doesn't guarantee the central grants it; at the BLE-spec-minimum MTU of 23, a fixed 102-byte chunk silently truncates in the NimBLE stack with no error, and the client's `JSON.parse` failure is swallowed. Fixed by querying the actual negotiated MTU per-connection (`NimBLEServer::getPeerInfo(0).getMTU()`) and clamping the chunk size to what it actually allows, falling back to the spec-minimum 23 if no connection info is available yet.

### FR-1: Vite `base` path + `%BASE_URL%` font reference

**Files:**
- Modify: `web/vite.config.ts`
- Modify: `web/index.html`

Replace the full contents of `web/vite.config.ts`:

```ts
import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig(({ mode }) => ({
  plugins: [react()],
  // The WiFi build is served from the device's own root ("/"). The BLE
  // build is deployed to a GitHub Pages project page subpath, and relative
  // asset paths work there regardless of the actual repo/org name (this
  // app has no client-side routing, so relative paths are safe).
  base: mode === 'ble' ? './' : '/',
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
}))
```

In `web/index.html`, change:

```html
      src: url('/fonts/Geist-Variable.woff2') format('woff2');
```

to:

```html
      src: url('%BASE_URL%fonts/Geist-Variable.woff2') format('woff2');
```

`%BASE_URL%` is a literal placeholder Vite text-replaces in `.html` files with the configured `base` value at build time — this is the documented mechanism for referencing `public/`-directory assets from raw HTML/CSS in a way that survives a non-root `base` (unlike `<script src>`/`<link href>` on local module files, which Vite already rewrites automatically as part of bundling — the `<script type="module" src="/src/main.tsx">` tag needs no change).

**Verify:**
```bash
npm run build
npm run build:ble
```
Then inspect `web/dist-ble/index.html` — the emitted `<script>`/`<link>` tags should reference `./assets/...` (relative), and the font `url(...)` inside the emitted `<style>` block should read `./fonts/Geist-Variable.woff2` (i.e. `%BASE_URL%` resolved to `./`). Also inspect `web/dist/index.html` (the unaffected WiFi build) and confirm its asset paths are still root-absolute (`/assets/...`, `/fonts/...`) — `base: '/'` for the default mode must produce byte-identical output to before this fix, since `scripts/build_web.py` and the ESP's own web server assume root-absolute paths.

### FR-2 + FR-5 + FR-6: `BleServer` — initial-state push, fixed-buffer mailbox, MTU-aware chunking

**Files:**
- Modify: `src/net/BleServer.h` (full replacement)
- Modify: `src/net/BleServer.cpp` (full replacement)

These three fixes are combined into one pass because they touch overlapping code (`begin()`, `loop()`, the mailbox, `notifyState()`). Replace the full contents of both files.

`src/net/BleServer.h`:

```cpp
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
```

`src/net/BleServer.cpp`:

```cpp
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
```

**Verify:**
```bash
pio run -e esp32dev
pio run -e esp32-c3-supermini
```
Both must succeed. Hand-trace: (1) the `onSubscribe`/`requestInitialNotify`/`loop()` path — confirm a fresh subscription results in exactly one `notifyState()` call from `loop()`, never from the NimBLE task directly; (2) the fixed-buffer mailbox — confirm `onCommandChunk`'s `memcpy` into `_pendingCommand` and `loop()`'s `memcpy` out of it are both bounded by `_rxLen`/`_pendingLen + 1` and never read/write past `BLE_MAX_COMMAND_SIZE + 1`; (3) `notifyState()`'s MTU clamp — confirm `chunkPayload` is never 0 and never exceeds `CHUNK_PAYLOAD`, and that `frame[2 + CHUNK_PAYLOAD]` is large enough for the largest possible `chunkLen`.

### FR-4: `bleEnabled` reachable from the UI (firmware half)

**Files:**
- Modify: `src/net/WebServer.cpp` (`/api/strip` POST handler, `buildStateJson()`)

In the `/api/strip` POST handler, after the existing `if (doc.containsKey("chipset")) _cfg->chipset = doc["chipset"];` line, add:

```cpp
        if (doc.containsKey("bleEnabled")) _cfg->bleEnabled = doc["bleEnabled"];
```

In `buildStateJson()`, after `doc["chipset"] = _cfg->chipset;`, add:

```cpp
    doc["bleEnabled"]     = _cfg->bleEnabled;
```

This reuses the existing reboot-gated strip-config flow (`_pendingRestart = true` already fires for this handler) — changing `bleEnabled` reboots the device, which is required anyway since `bleServer.begin()` only runs once at boot in `setup()`.

**Verify:**
```bash
pio run -e esp32dev
pio run -e esp12e
```
Both must succeed (the ESP8266 build gets the new `bleEnabled` field in its state JSON too, since `Config::bleEnabled` isn't ESP32-guarded — harmless there, it's simply never acted on).

### FR-3: BLE write queue serialization (frontend)

**Files:**
- Modify: `web/src/hooks/useBluetoothTransport.ts`

Add a `useRef` import if not already present (it already is), and add a write-queue ref alongside the existing refs:

```ts
  const writeQueueRef = useRef<Promise<void>>(Promise.resolve())
```

Change `sendRaw`'s body from:

```ts
    writeChunks().catch((err: Error) => {
      setError(err.message || 'Bluetooth write failed')
    })
```

to:

```ts
    // Chain onto the previous write instead of firing concurrently — two
    // in-flight writeValueWithoutResponse() calls on the same characteristic
    // risk "GATT operation already in progress" (Chrome) or interleaved
    // chunk trains corrupting each other's reassembly on the firmware side.
    writeQueueRef.current = writeQueueRef.current
      .then(writeChunks)
      .catch((err: Error) => {
        setError(err.message || 'Bluetooth write failed')
      })
```

The `.catch()` staying part of the chained assignment is what matters: it means a failed write doesn't permanently break the queue for subsequent sends (the chain continues from a resolved promise either way).

**Verify:**
```bash
npx tsc -b --noEmit
```
Hand-trace: two `sendRaw` calls issued back-to-back (e.g. simulating a slider drag on one control plus a color change on another, both landing in the same tick) — confirm the second call's `writeChunks` only begins after the first's promise (success or caught failure) settles, never concurrently.

### FR-4: `bleEnabled` toggle (frontend half)

**Files:**
- Modify: `web/src/hooks/useLedState.ts` (`LedState` interface, `DEFAULT`)
- Modify: `web/src/components/tabs/SettingsTab.tsx`
- Modify: `web/src/i18n/en.json`, `web/src/i18n/pl.json`

In `useLedState.ts`, add `bleEnabled: boolean` to the `LedState` interface (after `chipset: number`) and to `DEFAULT` (after `chipset: 2,`, value `true`):

```ts
  chipset: number
  bleEnabled: boolean
```
```ts
  chipset: 2,
  bleEnabled: true,
```

In `SettingsTab.tsx`, add the `Switch` import:

```tsx
import { Switch } from '@/components/ui/switch'
```

Add local state (alongside the existing `chipset` state):

```tsx
  const [bleEnabled, setBleEnabled] = useState(state.bleEnabled)
```

Add it to the existing resync `useEffect`'s body and dependency array:

```tsx
  useEffect(() => {
    if (state.segments?.length) setSegments([...state.segments])
    setDataPin(state.dataPin)
    setColorOrder(state.colorOrder)
    setChipset(state.chipset)
    setBleEnabled(state.bleEnabled)
  }, [state.segments, state.dataPin, state.colorOrder, state.chipset, state.bleEnabled])
```

Add it to `saveStrip`'s POST body:

```tsx
      body: JSON.stringify({
        segments: segments.filter(s => s.count >= 0).slice(0, MAX_SEGMENTS),
        dataPin,
        colorOrder,
        chipset,
        bleEnabled,
      }),
```

Add a toggle row inside the Strip section's inner box (`<div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-3">`), placed right before the `saveStrip` `Button`:

```tsx
          <div className="flex items-center justify-between text-sm">
            <span className="text-zinc-400">{t('settings.bleEnabled')}</span>
            <Switch
              checked={bleEnabled}
              onCheckedChange={setBleEnabled}
              className="data-[state=checked]:bg-amber-400"
            />
          </div>
```

In `web/src/i18n/en.json`, add to the `settings` object (after `"chipset": "LED chipset",`):

```json
    "bleEnabled": "Bluetooth control",
```

In `web/src/i18n/pl.json`, add to the `settings` object (after `"chipset": "Układ LED",`):

```json
    "bleEnabled": "Sterowanie Bluetooth",
```

Note: this toggle is only reachable in the WiFi build (the whole Strip section is gated by `capabilities.stripConfig`, which is `false` for the BLE build) — appropriate, since you can't disable your own BLE connection from within that same BLE connection anyway.

**Verify:**
```bash
npx tsc -b --noEmit
npm run build
npm run build:ble
```
All three must succeed.
