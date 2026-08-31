#pragma once
#include <Arduino.h>

class NetworkManager {
public:
    void begin(const char* apName = "MilaLED");
    bool isConnected() const;
    String localIP() const;
    String ssid() const;
    String macAddress() const;
    void loop();
};
