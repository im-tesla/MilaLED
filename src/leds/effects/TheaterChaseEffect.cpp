#pragma once
#include "EffectBase.h"

class TheaterChaseEffect : public EffectBase {
    uint8_t _offset = 0;
public:
    const char* id() const override { return "theater"; }
    void reset() override { _offset = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint8_t spacing = map(p.intensity, 0, 255, 2, 6);
        fill_solid(leds, count, CRGB::Black);
        for (uint16_t i = _offset; i < count; i += spacing) {
            leds[i] = p.primary;
        }
        if (++_offset >= spacing) _offset = 0;
    }
};
