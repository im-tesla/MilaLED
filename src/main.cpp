#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>
#include "version.h"
#include "config/ConfigStore.h"
#include "leds/EffectsEngine.h"
#include "wifi/NetworkManager.h"
#include "net/WebServer.h"
#include "net/BleServer.h"
#include <esp_wifi.h>

static Config         cfg;
static ConfigStore    cfgStore;
static EffectsEngine  engine;
static NetworkManager network;
static MilaWebServer  webServer;
static BleServer      bleServer;

static uint32_t lastSave = 0;

static void handleSerialCommands() {
    static String inputLine = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            inputLine.trim();
            if (inputLine.length() > 0) {
                Serial.printf("[cmd] received: '%s'\n", inputLine.c_str());
                if (inputLine == "status") {
                    Serial.printf("[status] Wi-Fi: %s, SSID: '%s', IP: %s, RSSI: %d, MAC: %s\n",
                        network.isConnected() ? "CONNECTED" : "DISCONNECTED",
                        network.ssid().c_str(), network.localIP().c_str(),
                        WiFi.RSSI(), network.macAddress().c_str());
                } else if (inputLine == "scan") {
                    Serial.println("[cmd] scanning networks...");
                    int n = WiFi.scanNetworks(false, false, false, 250);
                    Serial.printf("[cmd] scan complete: found %d networks\n", n);
                    uint16_t apCount = n;
                    std::vector<wifi_ap_record_t> records(apCount);
                    if (esp_wifi_scan_get_ap_records(&apCount, records.data()) == ESP_OK) {
                        for (uint16_t i = 0; i < apCount; i++) {
                            const char* ciphers[] = {"NONE", "WEP40", "WEP104", "TKIP", "CCMP", "TKIP_CCMP", "AES_CMAC", "UNKNOWN"};
                            const char* pairCipher = (records[i].pairwise_cipher <= 6) ? ciphers[records[i].pairwise_cipher] : "OTHER";
                            const char* grpCipher = (records[i].group_cipher <= 6) ? ciphers[records[i].group_cipher] : "OTHER";
                            Serial.printf("  [%02d] SSID: '%-20s' CH: %2d  RSSI: %3d  AUTH: %d  PAIR: %-9s  GRP: %-9s  PHY: %s%s%s\n",
                                i, records[i].ssid, records[i].primary, records[i].rssi,
                                (int)records[i].authmode, pairCipher, grpCipher,
                                records[i].phy_11b ? "b" : "",
                                records[i].phy_11g ? "g" : "",
                                records[i].phy_11n ? "n" : "");
                        }
                    }
                    WiFi.scanDelete();
                } else if (inputLine.startsWith("join ")) {
                    int spaceIdx = inputLine.indexOf(' ', 5);
                    String s, p;
                    if (spaceIdx > 0) {
                        s = inputLine.substring(5, spaceIdx);
                        p = inputLine.substring(spaceIdx + 1);
                    } else {
                        s = inputLine.substring(5);
                        p = "";
                    }
                    s.trim();
                    p.trim();
                    Serial.printf("[cmd] joining '%s'...\n", s.c_str());
                    network.startJoin(s, p);
                } else if (inputLine == "forget") {
                    network.resetSettings();
                    Serial.println("[cmd] settings erased");
                } else if (inputLine == "restart") {
                    ESP.restart();
                }
                inputLine = "";
            }
        } else {
            inputLine += c;
        }
    }
}

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

    LittleFS.mkdir("/presets"); // ensure preset directory exists

    if (cfg.bleEnabled) {
        Serial.println("[ble]   starting BLE server...");
        bleServer.begin(&cfg, &cfgStore, &engine);
        webServer.setBleServer(&bleServer);
        bleServer.setWebServer(&webServer);
    }

    // Initialize the Wi-Fi/LwIP stack in STA mode
    network.prepare();

    Serial.println("[http]  starting web server...");
    webServer.begin(&cfg, &cfgStore, &engine, &network);

    Serial.println("[wifi]  starting network manager...");
    network.begin();

    Serial.println("[ota]   starting ArduinoOTA...");
    ArduinoOTA.setHostname("milaled");
    ArduinoOTA.begin();

    // Show status on strip: green if connected, none if in BLE mode
    if (network.isConnected()) {
        Serial.print("[wifi]  connected! "); Serial.println(network.localIP().c_str());
        engine.setStatus(EffectsEngine::STATUS_OK);
    } else {
        Serial.println("[wifi]  disconnected — BLE mode");
        engine.setStatus(EffectsEngine::STATUS_NONE);
    }

    Serial.println("──────────────────");
    Serial.println("ready");
}

void loop() {
    handleSerialCommands();
    hyperionLoop();           // UDP receive (ports 19446+4048)
    engine.flushHyperion();   // UDP→LEDs at zero latency
    engine.ambilightPoll();   // HTTP poll TV (non-blocking, skips tick gate)
    network.loop();           // MDNS.update() / join watcher
    webServer.loop();         // HTTP + WebSocket handlers
    bleServer.loop();         // drains BLE command queue
    // Pause FastLED output during active Wi-Fi handshake so RMT interrupts
    // on ESP32-C3 do not disrupt 802.11 auth / 4-way EAPOL timing.
    if (network.joinStatus() != NetworkManager::JOIN_CONNECTING) {
        engine.tick();        // LED frame update (20ms throttled)
    }
    ArduinoOTA.handle();      // OTA update check

    // Persist continuous params (brightness/speed/etc.) every 30s without broadcasting.
    // Discrete params (effect/power/palette) are saved immediately in handleWsMessage.
    if (millis() - lastSave > 30000) {
        lastSave = millis();
        cfgStore.save(cfg);
    }
}
