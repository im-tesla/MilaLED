#include "NetworkManager.h"
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_coexist.h>
#include <NimBLEDevice.h>
#include <vector>
#include <algorithm>

static bool _prepared = false;

static const char* wifiReasonDesc(uint8_t reason) {
    switch (reason) {
        case 1:   return "UNSPECIFIED";
        case 2:   return "AUTH_EXPIRE";
        case 3:   return "AUTH_LEAVE";
        case 4:   return "ASSOC_EXPIRE";
        case 5:   return "ASSOC_TOOMANY";
        case 6:   return "NOT_AUTHED";
        case 7:   return "NOT_ASSOCED";
        case 8:   return "ASSOC_LEAVE";
        case 9:   return "ASSOC_NOT_AUTHED";
        case 13:  return "IE_INVALID";
        case 14:  return "MIC_FAILURE";
        case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
        case 16:  return "GROUP_KEY_UPDATE_TIMEOUT";
        case 17:  return "IE_IN_4WAY_DIFFERS";
        case 18:  return "GROUP_CIPHER_INVALID";
        case 19:  return "PAIRWISE_CIPHER_INVALID";
        case 20:  return "AKMP_INVALID";
        case 21:  return "UNSUPP_RSN_IE_VERSION";
        case 22:  return "INVALID_RSN_IE_CAP";
        case 23:  return "802_1X_AUTH_FAILED";
        case 24:  return "CIPHER_SUITE_REJECTED";
        case 200: return "BEACON_TIMEOUT";
        case 201: return "NO_AP_FOUND";
        case 202: return "AUTH_FAIL";
        case 203: return "ASSOC_FAIL";
        case 204: return "HANDSHAKE_TIMEOUT";
        case 205: return "CONNECTION_FAIL";
        default:  return "UNKNOWN";
    }
}

void NetworkManager::prepare() {
    if (_prepared) return;
    WiFi.mode(WIFI_STA);
    _prepared = true;
}

void NetworkManager::begin(const char*) {
    prepare();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    WiFi.setMinSecurity(WIFI_AUTH_OPEN);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    wifi_country_t country = {"PL", 1, 13, 20, WIFI_COUNTRY_POLICY_MANUAL};
    esp_wifi_set_country(&country);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_START) {
            Serial.println("[wifi]  STA started");
        } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            Serial.println("[wifi]  STA associated with AP");
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            IPAddress ip(info.got_ip.ip_info.ip.addr);
            Serial.printf("[wifi]  STA got IP: %s\n", ip.toString().c_str());
            _joinStatus = JOIN_SUCCESS;
            _joinError = "";
            WiFi.setAutoReconnect(true);
            esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
            NimBLEDevice::startAdvertising();
            MDNS.begin("milaled");
            MDNS.addService("wled", "_tcp", 80);
            MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            _lastDisconnectReason = reason;
            _disconnectCount++;
            Serial.printf("[wifi]  STA disconnected (count %d), reason: %d (%s)\n",
                _disconnectCount, reason, wifiReasonDesc(reason));

            if (_joinStatus == JOIN_CONNECTING) {
                if (reason == WIFI_REASON_AUTH_FAIL) {
                    if (_disconnectCount >= 3) {
                        _joinStatus = JOIN_FAILED;
                        _joinError = "Authentication failed (wrong password)";
                    }
                } else if (reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
                    if (_disconnectCount >= 4) {
                        _joinStatus = JOIN_FAILED;
                        _joinError = "Handshake timed out (check password / router)";
                    }
                } else if (reason == WIFI_REASON_NO_AP_FOUND) {
                    if (_disconnectCount >= 4) {
                        _joinStatus = JOIN_FAILED;
                        _joinError = "Network not found (ensure 2.4 GHz)";
                    }
                } else if (_disconnectCount >= 10) {
                    _joinStatus = JOIN_FAILED;
                    _joinError = String("Disconnected by router: ") + wifiReasonDesc(reason) + " (" + String(reason) + ")";
                }
            }
        }
    });

    // Check if we have saved Wi-Fi credentials from previous setup
    bool hasCredentials = (WiFi.SSID().length() > 0);
    if (hasCredentials) {
        Serial.printf("[wifi]  attempting connection to saved SSID '%s'...\n", WiFi.SSID().c_str());
        WiFi.begin();

        // Wait up to 6 seconds at boot for immediate connection
        uint32_t start = millis();
        while (millis() - start < 6000) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi]  connected! IP: %s\n", WiFi.localIP().toString().c_str());
                MDNS.begin("milaled");
                MDNS.addService("wled", "_tcp", 80);
                MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
                return;
            }
            delay(50);
        }
        Serial.println("[wifi]  saved network connecting in background");
    } else {
        Serial.println("[wifi]  no saved credentials; BLE ready for provisioning");
    }
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String NetworkManager::localIP() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
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
    if (_joinStatus == JOIN_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            _joinStatus = JOIN_SUCCESS;
            _joinError = "";
            WiFi.setAutoReconnect(true);
            esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
            NimBLEDevice::startAdvertising();
            Serial.printf("[wifi]  joined network successfully! IP: %s\n", WiFi.localIP().toString().c_str());
            MDNS.begin("milaled");
            MDNS.addService("wled", "_tcp", 80);
            MDNS.addServiceTxt("wled", "_tcp", "mac", WiFi.macAddress().c_str());
        } else if (WiFi.status() == WL_CONNECT_FAILED && _disconnectCount >= 3) {
            _joinStatus = JOIN_FAILED;
            if (_joinError.length() == 0) {
                _joinError = "Authentication failed (wrong password)";
            }
            esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
            NimBLEDevice::getAdvertising()->start();
            Serial.println("[wifi]  join failed: auth error");
        } else if (millis() - _joinStartTime > 25000) {
            _joinStatus = JOIN_FAILED;
            esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
            NimBLEDevice::getAdvertising()->start();
            if (_joinError.length() == 0) {
                if (_lastDisconnectReason > 0) {
                    _joinError = String("Connection failed: ") + wifiReasonDesc(_lastDisconnectReason) + " (" + String(_lastDisconnectReason) + ")";
                } else {
                    _joinError = "Connection timed out";
                }
            }
            Serial.printf("[wifi]  join failed: timeout (%s)\n", _joinError.c_str());
        }
    }
}

void NetworkManager::resetSettings() {
    Serial.println("[wifi]  erasing saved WiFi credentials...");
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_STA);
    delay(100);
}

void NetworkManager::startScan() {
    int16_t status = WiFi.scanComplete();
    if (status == -1) {
        return;
    }
    if (status >= 0) {
        WiFi.scanDelete();
    }
    Serial.println("[wifi]  starting async WiFi scan...");
    WiFi.scanNetworks(true, false, false, 100);
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

    _channelCache.clear();
    std::vector<ScannedItem> list;
    list.reserve(n);

    for (int16_t i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        s.trim();
        if (s.length() == 0) continue;

        int32_t r = WiFi.RSSI(i);
        bool sec = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        _channelCache.push_back({s, (uint8_t)WiFi.channel(i)});

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

    std::sort(list.begin(), list.end(), [](const ScannedItem& a, const ScannedItem& b) {
        return a.rssi > b.rssi;
    });

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
    String cleanSsid = ssid;
    cleanSsid.trim();
    String cleanPass = password;
    cleanPass.trim();

    Serial.printf("[wifi]  startJoin: '%s' (pw len %d)\n", cleanSsid.c_str(), cleanPass.length());
    _targetSsid = cleanSsid;
    _joinStatus = JOIN_CONNECTING;
    _joinStartTime = millis();
    _disconnectCount = 0;
    _lastDisconnectReason = 0;
    _joinError = "";

    uint8_t targetChannel = 0;
    for (const auto& entry : _channelCache) {
        if (entry.first == cleanSsid) {
            targetChannel = entry.second;
            break;
        }
    }
    // If not in cache, check scan results
    if (targetChannel == 0) {
        int16_t n = WiFi.scanComplete();
        if (n > 0) {
            for (int16_t i = 0; i < n; i++) {
                if (WiFi.SSID(i) == cleanSsid) {
                    targetChannel = WiFi.channel(i);
                    break;
                }
            }
        }
    }
    if (targetChannel > 0) {
        Serial.printf("[wifi]  target '%s' is on channel %d\n", cleanSsid.c_str(), targetChannel);
    }

    esp_coex_preference_set(ESP_COEX_PREFER_WIFI);
    NimBLEDevice::getAdvertising()->stop();
    WiFi.setMinSecurity(WIFI_AUTH_OPEN);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);

    WiFi.persistent(true);
    WiFi.setAutoReconnect(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);

    WiFi.begin(cleanSsid.c_str(), cleanPass.c_str());
}
