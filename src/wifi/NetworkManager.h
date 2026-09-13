#pragma once
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <vector>
#include <utility>

class NetworkManager {
public:
    enum JoinStatus {
        JOIN_IDLE,
        JOIN_CONNECTING,
        JOIN_SUCCESS,
        JOIN_FAILED
    };

    void prepare();
    void begin(const char* apName = nullptr);
    bool isConnected() const;
    bool isAp() const { return false; }
    String localIP() const;
    String ssid() const;
    String macAddress() const;
    void loop();
    void resetSettings();

    // Scanning
    void startScan();
    int16_t scanStatus();
    String getScanResultsJson();
    void cleanScan();

    // Joining
    void startJoin(const String& ssid, const String& password);
    JoinStatus joinStatus() const { return _joinStatus; }
    String joinError() const { return _joinError; }
    void clearJoinStatus() { _joinStatus = JOIN_IDLE; _joinError = ""; }

private:
    JoinStatus   _joinStatus           = JOIN_IDLE;
    String       _joinError            = "";
    uint32_t     _joinStartTime        = 0;
    uint8_t      _disconnectCount      = 0;
    uint8_t      _lastDisconnectReason = 0;
    String       _targetSsid           = "";
    std::vector<std::pair<String, uint8_t>> _channelCache;
};
