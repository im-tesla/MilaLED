#pragma once
#include "EffectBase.h"

class BreathingEffect : public EffectBase {
    uint8_t _phase = 0;
public:
    const char* id() const override { return "breathing"; }
    void reset() override { _phase = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint8_t bright = sin8(_phase);
        CRGB c = p.primary;
        c.nscale8(bright);
        fill_solid(leds, count, c);
        _phase += map(p.speed, 0, 255, 1, 5);
    }
};
