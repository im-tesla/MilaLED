#include "NetworkManager.h"
#include <WiFiManager.h>
#ifdef ESP32
#include <ESPmDNS.h>
#else
#include <ESP8266mDNS.h>
#endif

static WiFiManager _wm;

void NetworkManager::begin(const char* apName) {
    _wm.setConfigPortalTimeout(180);
    _wm.setConnectTimeout(15);
    _wm.autoConnect(apName);
    if (WiFi.status() == WL_CONNECTED) {
        MDNS.begin("milaled");
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
