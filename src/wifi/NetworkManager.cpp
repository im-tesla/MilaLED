#include "NetworkManager.h"
#include <ArduinoJson.h>
#ifdef ESP32
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_system.h>
#else
#include <ESP8266mDNS.h>
#include <user_interface.h>
#endif
#include <vector>
#include <algorithm>

// Original IEEE Espressif OUI MAC address
static uint8_t _customMac[6] = {0xDC, 0x06, 0x75, 0x66, 0xAC, 0x13};
static bool _prepared = false;

void NetworkManager::prepare() {
    if (_prepared) return;
#ifdef ESP8266
    WiFi.mode(WIFI_STA);
    wifi_set_macaddr(STATION_IF, _customMac);
    WiFi.mode(WIFI_AP_STA);
#elif defined(ESP32)
    WiFi.mode(WIFI_STA);
    esp_wifi_set_mac(WIFI_IF_STA, _customMac);
    WiFi.mode(WIFI_AP_STA);
#endif
    _prepared = true;
}

void NetworkManager::begin(const char* apName) {
    _apName = apName;
    prepare();

#ifdef ESP32
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_START) {
            Serial.println("[wifi]  STA started");
        } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            Serial.println("[wifi]  STA associated with AP");
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("[wifi]  STA got IP: %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
            _joinStatus = JOIN_SUCCESS;
            _isAp = false;
            _dnsServer.stop();
            MDNS.begin("milaled");
            MDNS.addService("wled", "_tcp", 80);
            MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            Serial.printf("[wifi]  STA disconnected, reason: %d\n", reason);
            if (_joinStatus == JOIN_CONNECTING) {
                if (reason == WIFI_REASON_AUTH_FAIL || 
                    reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || 
                    reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
                    _joinStatus = JOIN_FAILED;
                    _joinError = "Authentication failed (wrong password)";
                } else if (reason == WIFI_REASON_NO_AP_FOUND) {
                    _joinStatus = JOIN_FAILED;
                    _joinError = "Network not found (check 2.4 GHz)";
                } else if (reason == WIFI_REASON_ASSOC_FAIL) {
                    _joinStatus = JOIN_FAILED;
                    _joinError = "Association rejected by router";
                }
            }
        }
    });
#endif

    // Check if we have saved Wi-Fi credentials from previous setup
    bool hasCredentials = (WiFi.SSID().length() > 0);
    bool connected = false;

    if (hasCredentials) {
        Serial.printf("[wifi]  attempting connection to '%s'...\n", WiFi.SSID().c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin();

        // Wait up to 10 seconds for connection
        uint32_t start = millis();
        while (millis() - start < 10000) {
            if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                break;
            }
            delay(50);
        }
    }

    if (connected) {
        _isAp = false;
        Serial.print("[wifi]  connected! IP: ");
        Serial.println(WiFi.localIP().toString());
        MDNS.begin("milaled");
        MDNS.addService("wled", "_tcp", 80);
        MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
    } else {
        // Fallback to AP mode with Captive Portal DNS server
        _isAp = true;
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apName);
        delay(100);

        _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        _dnsServer.start(53, "*", WiFi.softAPIP());

        Serial.printf("[wifi]  AP mode '%s' started at IP: %s\n",
            apName, WiFi.softAPIP().toString().c_str());

        MDNS.begin("milaled");
        MDNS.addService("wled", "_tcp", 80);
        MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
    }
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::isAp() const {
    return _isAp;
}

String NetworkManager::localIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

String NetworkManager::apIP() const {
    return WiFi.softAPIP().toString();
}

String NetworkManager::ssid() const {
    if (isConnected()) {
        return WiFi.SSID();
    }
    return "";
}

String NetworkManager::macAddress() const {
    return WiFi.macAddress();
}

void NetworkManager::loop() {
    if (_isAp) {
        _dnsServer.processNextRequest();
    }
#ifdef ESP8266
    MDNS.update();
#endif

    if (_joinStatus == JOIN_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            _joinStatus = JOIN_SUCCESS;
            _isAp = false;
            _dnsServer.stop();
            Serial.printf("[wifi]  joined network successfully! IP: %s\n", WiFi.localIP().toString().c_str());
            MDNS.begin("milaled");
            MDNS.addService("wled", "_tcp", 80);
            MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
        } else if (WiFi.status() == WL_CONNECT_FAILED) {
            _joinStatus = JOIN_FAILED;
            _joinError = "Authentication failed (wrong password)";
            Serial.println("[wifi]  join failed: auth error");
        } else if (millis() - _joinStartTime > 20000) {
            _joinStatus = JOIN_FAILED;
            if (_joinError.length() == 0) {
                _joinError = "Connection timed out";
            }
            Serial.println("[wifi]  join failed: timeout");
        }
    }

    if (_joinStatus == JOIN_FAILED && !isConnected()) {
        if (!_isAp) {
            _isAp = true;
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(_apName.c_str());
            _dnsServer.start(53, "*", WiFi.softAPIP());
            Serial.println("[wifi]  restored fallback AP mode");
        }
    }
}

void NetworkManager::resetSettings() {
    Serial.println("[wifi]  erasing saved WiFi credentials...");
#ifdef ESP32
    WiFi.disconnect(true, true);
#else
    WiFi.disconnect(true);
#endif
    delay(100);
}

void NetworkManager::startScan() {
    int16_t status = WiFi.scanComplete();
    if (status == -1) {
        // scan already running
        return;
    }
    if (status >= 0) {
        WiFi.scanDelete();
    }
    Serial.println("[wifi]  starting async WiFi scan...");
#ifdef ESP32
    // Fast active scan: 100ms per channel (down from default 300ms)
    WiFi.scanNetworks(true, false, false, 100);
#else
    WiFi.scanNetworks(true);
#endif
}

int16_t NetworkManager::scanStatus() {
    return WiFi.scanComplete();
}

void NetworkManager::cleanScan() {
    WiFi.scanDelete();
}

struct ScannedItem {
    String  ssid;
    int32_t rssi;
    bool    secure;
};

String NetworkManager::getScanResultsJson() {
    int16_t n = WiFi.scanComplete();
    if (n <= 0) {
        return "[]";
    }

    std::vector<ScannedItem> list;
    list.reserve(n);

    for (int16_t i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        s.trim();
        if (s.length() == 0) continue; // ignore hidden / empty SSIDs

        int32_t r = WiFi.RSSI(i);
#ifdef ESP32
        bool sec = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
#else
        bool sec = (WiFi.encryptionType(i) != ENC_TYPE_NONE);
#endif

        // Deduplicate: keep highest RSSI
        bool exists = false;
        for (auto& item : list) {
            if (item.ssid == s) {
                exists = true;
                if (r > item.rssi) {
                    item.rssi = r;
                    item.secure = sec;
                }
                break;
            }
        }
        if (!exists) {
            list.push_back({s, r, sec});
        }
    }

    // Sort by signal strength descending
    std::sort(list.begin(), list.end(), [](const ScannedItem& a, const ScannedItem& b) {
        return a.rssi > b.rssi;
    });

    // Limit to top 12 networks for fast transmission over BLE
    if (list.size() > 12) {
        list.resize(12);
    }

    StaticJsonDocument<1536> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& item : list) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"]   = item.ssid;
        obj["rssi"]   = item.rssi;
        obj["secure"] = item.secure;
    }

    String out;
    serializeJson(doc, out);
    return out;
}

void NetworkManager::startJoin(const String& ssid, const String& password) {
    Serial.printf("[wifi]  joining '%s'...\n", ssid.c_str());
    _targetSsid = ssid;
    _joinStatus = JOIN_CONNECTING;
    _joinStartTime = millis();
    _joinError = "";

    // Find channel from recent scan results if available
    int32_t targetChannel = 0;
    int16_t n = WiFi.scanComplete();
    if (n > 0) {
        for (int16_t i = 0; i < n; i++) {
            if (WiFi.SSID(i) == ssid) {
                targetChannel = WiFi.channel(i);
                break;
            }
        }
    }
    if (targetChannel > 0) {
        Serial.printf("[wifi]  target '%s' is on channel %d\n", ssid.c_str(), targetChannel);
    }

    // Stop AP mode so the single 2.4 GHz radio is not locked to channel 1
    if (_isAp) {
        _dnsServer.stop();
        WiFi.softAPdisconnect(true);
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    delay(50);

    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
#ifdef ESP32
    if (targetChannel > 0) {
        WiFi.begin(ssid.c_str(), password.c_str(), targetChannel);
    } else {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
#else
    WiFi.begin(ssid.c_str(), password.c_str());
#endif
}
