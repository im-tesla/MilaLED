#pragma once
#include "EffectBase.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

struct AmbZone { uint8_t r, g, b; };

class AmbilightEffect : public EffectBase {
public:
    const char* id() const override { return "ambilight"; }

    void configure(const char* tvIp, uint16_t pollMs, const char* mapping) {
        strncpy(_tvIp, tvIp, sizeof(_tvIp) - 1);
        _tvIp[sizeof(_tvIp) - 1] = '\0';
        _pollMs = pollMs;
        strncpy(_mapping, mapping, sizeof(_mapping) - 1);
        _mapping[sizeof(_mapping) - 1] = '\0';
    }

    void reset() override { _lastPoll = 0; }

    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint32_t now = millis();
        if (now - _lastPoll >= _pollMs) {
            _lastPoll = now;
            fetchAndParse();
        }
        applyZones(leds, count);
    }

private:
    char     _tvIp[16]    = "";
    uint16_t _pollMs      = 100;
    char     _mapping[16] = "right";
    uint32_t _lastPoll    = 0;

    static const uint8_t MAX_ZONES = 10;
    AmbZone  _zones[MAX_ZONES] = {};
    uint8_t  _zoneCount = 0;

    void fetchAndParse() {
        if (_tvIp[0] == '\0') return;
        WiFiClient client;
        HTTPClient http;
        String url = String("http://") + _tvIp + "/ambilight/processed";
        http.begin(client, url);
        http.setTimeout(500);
        int code = http.GET();
        if (code != 200) { http.end(); return; }

        // Use stream parsing to save RAM
        StaticJsonDocument<2048> doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        http.end();
        if (err) return;

        if (strcmp(_mapping, "average") == 0) {
            uint32_t r = 0, g = 0, b = 0;
            uint8_t n = 0;
            const char* sides[] = {"left", "right", "top"};
            for (const char* side : sides) {
                JsonObject obj = doc["layer1"][side];
                for (JsonPair kv : obj) {
                    r += (uint8_t)(int)kv.value()["r"];
                    g += (uint8_t)(int)kv.value()["g"];
                    b += (uint8_t)(int)kv.value()["b"];
                    n++;
                }
            }
            _zoneCount = 0;
            if (n > 0) {
                _zones[0] = { (uint8_t)(r / n), (uint8_t)(g / n), (uint8_t)(b / n) };
                _zoneCount = 1;
            }
            return;
        }

        JsonObject sideObj = doc["layer1"][_mapping];
        _zoneCount = 0;
        for (JsonPair kv : sideObj) {
            if (_zoneCount >= MAX_ZONES) break;
            _zones[_zoneCount++] = {
                (uint8_t)(int)kv.value()["r"],
                (uint8_t)(int)kv.value()["g"],
                (uint8_t)(int)kv.value()["b"]
            };
        }
    }

    void applyZones(CRGB* leds, uint16_t count) {
        if (_zoneCount == 0) { fill_solid(leds, count, CRGB::Black); return; }
        if (_zoneCount == 1) { fill_solid(leds, count, CRGB(_zones[0].r, _zones[0].g, _zones[0].b)); return; }

        for (uint16_t i = 0; i < count; i++) {
            float t = (float)i / (float)(count - 1) * (float)(_zoneCount - 1);
            uint8_t z0 = (uint8_t)t;
            uint8_t z1 = (z0 + 1 < _zoneCount) ? z0 + 1 : z0;
            float frac = t - (float)z0;
            leds[i].r = (uint8_t)(_zones[z0].r + frac * (float)(_zones[z1].r - _zones[z0].r));
            leds[i].g = (uint8_t)(_zones[z0].g + frac * (float)(_zones[z1].g - _zones[z0].g));
            leds[i].b = (uint8_t)(_zones[z0].b + frac * (float)(_zones[z1].b - _zones[z0].b));
        }
    }
};
