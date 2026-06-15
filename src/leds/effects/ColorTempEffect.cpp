#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // intensity 0→warm white (low blue), 255→cool white (full blue)
        uint8_t r = 255;
        uint8_t g = map(p.intensity, 0, 255, 200, 255);
        uint8_t b = map(p.intensity, 0, 255, 80, 255);
        fill_solid(leds, count, CRGB(r, g, b));
    }
};
