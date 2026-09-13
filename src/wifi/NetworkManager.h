#pragma once
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <DNSServer.h>

class NetworkManager {
public:
    enum JoinStatus {
        JOIN_IDLE,
        JOIN_CONNECTING,
        JOIN_SUCCESS,
        JOIN_FAILED
    };

    void prepare();
    void begin(const char* apName = "MilaLED");
    bool isConnected() const;
    bool isAp() const;
    String localIP() const;
    String apIP() const;
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
    DNSServer    _dnsServer;
    bool         _isAp          = false;
    JoinStatus   _joinStatus    = JOIN_IDLE;
    String       _joinError     = "";
    uint32_t     _joinStartTime   = 0;
    uint8_t      _disconnectCount = 0;
    String       _targetSsid      = "";
    String       _apName          = "MilaLED";
};
