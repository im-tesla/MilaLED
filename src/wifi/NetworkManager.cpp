#include "NetworkManager.h"
#include <WiFiManager.h>
#ifdef ESP32
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif

static WiFiManager _wm;
static bool _shouldSave = false;

// Called by WiFiManager BEFORE saving — validate credentials are non-empty
static void preSaveCallback() {
    // WiFiManager calls this right before writing to flash.
    // If SSID is empty, the user hit refresh or submitted an empty form.
    if (_wm.getWiFiSSID().length() < 2) {
        _shouldSave = false;
        return;
    }
    _shouldSave = true;
}

void NetworkManager::begin(const char* apName) {
    // Fallback AP for captive-portal setup when no saved network
    _wm.setConfigPortalTimeout(150);

    // Prevent accidental save from page refresh / empty form
    _wm.setSaveConfigCallback(preSaveCallback);

    // Connect timeout: give up after 10s and fall back to AP
    _wm.setConnectTimeout(10);

    // Don't reboot after saving — we handle that ourselves
    _wm.setBreakAfterConfig(false);

    bool connected = _wm.autoConnect(apName);

    if (_shouldSave) {
        // Valid credentials were saved — reboot to use them
        delay(500);
        ESP.restart();
    }

    if (connected) {
        MDNS.begin("milaled");
        MDNS.addService("wled", "_tcp", 80);
        MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
    }
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkManager::localIP() const {
    return WiFi.localIP().toString();
}

String NetworkManager::ssid() const {
    return WiFi.SSID();
}

void NetworkManager::loop() {
#ifdef ESP8266
    MDNS.update();
#endif
    // ESP32 mDNS runs automatically — no explicit update needed
}
