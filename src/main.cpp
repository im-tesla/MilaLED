#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "version.h"
#include "config/ConfigStore.h"
#include "leds/EffectsEngine.h"
#include "wifi/NetworkManager.h"
#include "net/WebServer.h"
#ifdef ESP32
#include "net/BleServer.h"
#endif

static Config         cfg;
static ConfigStore    cfgStore;
static EffectsEngine  engine;
static NetworkManager network;
static MilaWebServer  webServer;
#ifdef ESP32
static BleServer      bleServer;
#endif

static uint32_t lastSave = 0;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("MilaLED " MILALED_VERSION);
    Serial.println("──────────────────");

    Serial.println("[init]  mounting LittleFS...");
    cfgStore.begin();    // mounts LittleFS

    Serial.println("[init]  loading config...");
    cfgStore.load(cfg);  // loads saved config or uses defaults

    uint8_t activeSegs = 0;
    for (uint8_t i = 0; i < MAX_SEGMENTS; i++)
        if (cfg.segments[i].count > 0) activeSegs++;
    Serial.printf("[init]  activeSegs:%u  virt:%u  phys:%u  pin:%u\n",
        activeSegs, engine.virtualCount(), engine.physCount(), cfg.dataPin);

    Serial.println("[init]  starting FastLED...");
    engine.begin(cfg);   // allocates LED arrays, sets up FastLED
    engine.setStatus(EffectsEngine::STATUS_BOOTING);  // blue pulse

    // Run a few ticks so the boot indicator actually shows on the strip
    for (uint8_t i = 0; i < 5; i++) {
        engine.tick();
        delay(20);
    }

    Serial.println("[wifi]  connecting (or opening config portal)...");
    network.begin("MilaLED");  // AP+STA WiFi, blocks until connected or timeout

    LittleFS.mkdir("/presets"); // ensure preset directory exists

    Serial.println("[http]  starting web server...");
    webServer.begin(&cfg, &cfgStore, &engine);

#ifdef ESP32
    if (cfg.bleEnabled) {
        Serial.println("[ble]   starting BLE server...");
        bleServer.begin(&cfg, &cfgStore, &engine);
        webServer.setBleServer(&bleServer);
        bleServer.setWebServer(&webServer);
    }
#endif

    Serial.println("[ota]   starting ArduinoOTA...");
    ArduinoOTA.setHostname("milaled");
    ArduinoOTA.begin();

    // Show status on strip: green if connected, yellow blink if AP mode
    if (network.isConnected()) {
        Serial.print("[wifi]  connected! "); Serial.println(network.localIP().c_str());
        engine.setStatus(EffectsEngine::STATUS_OK);
    } else {
        Serial.println("[wifi]  AP mode — connect to 'MilaLED' hotspot");
        engine.setStatus(EffectsEngine::STATUS_AP_MODE);
    }

    Serial.println("──────────────────");
    Serial.println("ready");
}

void loop() {
    hyperionLoop();           // UDP receive (ports 19446+4048)
    engine.flushHyperion();   // UDP→LEDs at zero latency
    engine.ambilightPoll();   // HTTP poll TV (non-blocking, skips tick gate)
    network.loop();           // MDNS.update()
    webServer.loop();         // HTTP + WebSocket handlers
#ifdef ESP32
    bleServer.loop(); // drains any command reassembled on the NimBLE task and applies it here
#endif
    engine.tick();            // LED frame update (20ms throttled)
    ArduinoOTA.handle();      // OTA update check

    // Persist continuous params (brightness/speed/etc.) every 30s without broadcasting.
    // Discrete params (effect/power/palette) are saved immediately in handleWsMessage.
    if (millis() - lastSave > 30000) {
        lastSave = millis();
        cfgStore.save(cfg);
    }
}
