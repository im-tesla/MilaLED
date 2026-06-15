#pragma once
#include "EffectBase.h"

class RainbowEffect : public EffectBase {
    uint8_t _hue = 0;
public:
    const char* id() const override { return "rainbow"; }
    void reset() override { _hue = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fill_rainbow(leds, count, _hue, 255 / count);
        _hue += map(p.speed, 0, 255, 1, 8);
    }
};
