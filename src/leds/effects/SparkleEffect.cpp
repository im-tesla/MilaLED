#pragma once
#include "EffectBase.h"

class SparkleEffect : public EffectBase {
public:
    const char* id() const override { return "sparkle"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fill_solid(leds, count, CRGB::Black);
        uint8_t n = map(p.intensity, 0, 255, 1, 20);
        for (uint8_t i = 0; i < n; i++) {
            leds[random16(count)] = p.primary;
        }
    }
};
