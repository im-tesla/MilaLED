#pragma once
#include "EffectBase.h"

class LavaEffect : public EffectBase {
    uint16_t _t = 0;
public:
    const char* id() const override { return "lava"; }
    void reset() override { _t = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        for (uint16_t i = 0; i < count; i++) {
            uint8_t noise = inoise8(i * 30, _t);
            leds[i] = ColorFromPalette(LavaColors_p, noise);
        }
        _t += map(p.speed, 0, 255, 1, 8);
    }
};
