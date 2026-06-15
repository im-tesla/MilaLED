#pragma once
#include "EffectBase.h"

class TwinkleEffect : public EffectBase {
public:
    const char* id() const override { return "twinkle"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fadeToBlackBy(leds, count, 20);
        uint8_t chance = map(p.speed, 0, 255, 1, 8);
        for (uint8_t i = 0; i < chance; i++) {
            uint16_t idx = random16(count);
            leds[idx] = p.primary;
        }
    }
};
