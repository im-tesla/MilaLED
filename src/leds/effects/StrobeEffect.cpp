#pragma once
#include "EffectBase.h"

class StrobeEffect : public EffectBase {
    uint8_t _tick = 0;
public:
    const char* id() const override { return "strobe"; }
    void reset() override { _tick = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint8_t period = map(p.speed, 0, 255, 2, 20);
        bool on = (_tick % period) < 2;
        fill_solid(leds, count, on ? p.primary : CRGB::Black);
        _tick++;
    }
};
