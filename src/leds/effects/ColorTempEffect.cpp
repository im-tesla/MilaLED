#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // Simulated blackbody color temperature curve:
        // intensity  0 → deep amber  (255,20,0)   ~1800K candle
        // intensity  64 → warm amber (255,79,16)  ~2400K
        // intensity 128 → warm white (255,138,64) ~3200K halogen
        // intensity 192 → neutral    (255,197,144)~4500K
        // intensity 255 → pure white (255,255,255)~6500K
        // Blue follows a quadratic ramp to avoid pink mid-tones.
        uint8_t r = 255;
        uint8_t g = map(p.intensity, 0, 255, 20, 255);
        uint8_t b = (uint16_t)p.intensity * p.intensity / 255;
        fill_solid(leds, count, CRGB(r, g, b));
    }
};
