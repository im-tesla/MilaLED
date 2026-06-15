#include "NetworkManager.h"
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

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
    MDNS.update();
}
