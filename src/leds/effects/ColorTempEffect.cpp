#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // intensity  0 → candle    (255,40,0 deep orange)
        // intensity 128 → warm white (255,180,60)
        // intensity 255 → pure white (255,255,255)
        uint8_t r = 255;
        uint8_t g = map(p.intensity, 0, 255,  40, 255);
        uint8_t b = map(p.intensity, 0, 255,   0, 255);
        fill_solid(leds, count, CRGB(r, g, b));
    }
};
