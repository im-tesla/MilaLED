#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // Debug: flash GREEN on first tick so we can confirm this version loaded
        static uint8_t dbg = 0;
        if (dbg < 10) { fill_solid(leds, count, CRGB(0,255,0)); dbg++; return; }

        // intensity  0 → pure red   (255,0,0)
        // intensity ~63 → orange    (255,63,0)
        // intensity ~127 → warm-wh
        // intensity ~191 → cool-wh
        // intensity 255 → pure white(255,255,255)
        fill_solid(leds, count, CRGB(255, p.intensity, p.intensity));
    }
};
