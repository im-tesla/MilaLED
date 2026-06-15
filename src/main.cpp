#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "config/ConfigStore.h"
#include "leds/EffectsEngine.h"
#include "wifi/NetworkManager.h"
#include "net/WebServer.h"

static Config         cfg;
static ConfigStore    cfgStore;
static EffectsEngine  engine;
static NetworkManager network;
static MilaWebServer  webServer;

static uint32_t lastSave = 0;

void setup() {
    Serial.begin(115200);

    cfgStore.begin();    // mounts LittleFS
    cfgStore.load(cfg);  // loads saved config or uses defaults

    engine.begin(cfg);   // allocates LED arrays, sets up FastLED
    engine.setStatus(EffectsEngine::STATUS_BOOTING);  // blue pulse

    // Run a few ticks so the boot indicator actually shows on the strip
    for (uint8_t i = 0; i < 5; i++) {
        engine.tick();
        delay(20);
    }

    network.begin("MilaLED");  // AP+STA WiFi, blocks until connected or timeout

    LittleFS.mkdir("/presets"); // ensure preset directory exists

    webServer.begin(&cfg, &cfgStore, &engine);

    ArduinoOTA.setHostname("milaled");
    ArduinoOTA.begin();

    // Show status on strip: green if connected, yellow blink if AP mode
    if (network.isConnected()) {
        engine.setStatus(EffectsEngine::STATUS_OK);
    } else {
        engine.setStatus(EffectsEngine::STATUS_AP_MODE);
    }

    Serial.println("MilaLED ready");
}

void loop() {
    network.loop();          // MDNS.update()
    webServer.loop();        // HTTP + WebSocket handlers
    engine.tick();           // LED frame update (20ms throttled)
    ArduinoOTA.handle();     // OTA update check

    // Persist continuous params (brightness/speed/etc.) every 30s without broadcasting.
    // Discrete params (effect/power/palette) are saved immediately in handleWsMessage.
    if (millis() - lastSave > 30000) {
        lastSave = millis();
        cfgStore.save(cfg);
    }
}
