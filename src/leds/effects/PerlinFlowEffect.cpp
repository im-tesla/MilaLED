#pragma once
#include "EffectBase.h"

class PerlinFlowEffect : public EffectBase {
    uint16_t _t = 0;
public:
    const char* id() const override { return "perlinflow"; }
    void reset() override { _t = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        for (uint16_t i = 0; i < count; i++) {
            uint8_t n = inoise8(i * 20, _t);
            uint8_t hue = n + (p.primary.r >> 1);
            leds[i] = CHSV(hue, 240, n);
        }
        _t += map(p.speed, 0, 255, 1, 6);
    }
};
