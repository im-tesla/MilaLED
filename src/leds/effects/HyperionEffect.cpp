#pragma once
#include "EffectBase.h"
#ifdef ESP32
#include <WiFi.h>
#endif
#include <WiFiUdp.h>

class HyperionEffect : public EffectBase {
public:
    const char* id() const override { return "hyperion"; }

    void reset() override {
        _lastPacket = 0;
        _ledCount = 0;
    }

    void tick(CRGB* leds, uint16_t count, const EffectParams&) override {
        if (!_started) {
            _sock.begin(21324);
            _started = true;
        }

        // Read all queued UDP packets (non-blocking — parsePacket returns 0 if nothing)
        uint16_t packetSize = _sock.parsePacket();
        if (packetSize >= 4) {
            uint8_t buf[1536];  // enough for 500 LEDs × 3 + header
            uint16_t len = (packetSize < (uint16_t)sizeof(buf)) ? packetSize : (uint16_t)sizeof(buf);
            _sock.read(buf, len);

            switch (buf[0]) {
                // WLED WARLS: [1] [R] [G] [B]...
                case 1:
                // Hyperion sends 4 (WARLS with timeout)
                case 4: {
                    uint16_t n = (len - 1) / 3;
                    uint16_t max = count < n ? count : n;
                    for (uint16_t i = 0; i < max; i++) {
                        uint16_t off = 1 + i * 3;
                        leds[i].r = buf[off];
                        leds[i].g = buf[off + 1];
                        leds[i].b = buf[off + 2];
                    }
                    _lastPacket = millis();
                    _timeout    = 2500;
                    break;
                }
                // WLED DRGB: [2] [timeout_hi] [timeout_lo] [R] [G] [B]...
                case 2: {
                    _timeout = ((uint16_t)buf[1] << 8) | buf[2];
                    uint16_t n = (len - 3) / 3;
                    uint16_t max = count < n ? count : n;
                    for (uint16_t i = 0; i < max; i++) {
                        uint16_t off = 3 + i * 3;
                        leds[i].r = buf[off];
                        leds[i].g = buf[off + 1];
                        leds[i].b = buf[off + 2];
                    }
                    _lastPacket = millis();
                    break;
                }
            }
        }

        // Fade to black if no packet for _timeout ms
        if (_lastPacket > 0 && _timeout > 0) {
            if (millis() - _lastPacket > _timeout) {
                fill_solid(leds, count, CRGB::Black);
                _timeout = 0;
            }
        }
    }

private:
    WiFiUDP  _sock;
    bool     _started    = false;
    uint32_t _lastPacket = 0;
    uint16_t _timeout    = 2500;
    uint16_t _ledCount   = 0;
};
