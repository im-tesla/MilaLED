#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // Flash green for 5 frames to prove firmware landed
        static uint8_t dbg = 0;
        if (dbg < 5) { fill_solid(leds, count, CRGB(0,255,0)); dbg++; return; }

        // Blackbody-inspired color temperature:
        // intensity  0 → candle  (255,70,10)
        // intensity  64 → warm   (255,120,30)
        // intensity 128 → warm-w (255,165,70)
        // intensity 192 → neutr  (255,215,155)
        // intensity 255 → white  (255,255,255)
        uint8_t g =  70 + (uint16_t)p.intensity * 185 / 255;
        uint8_t b =  10 + (uint16_t)p.intensity * p.intensity / 266;
        fill_solid(leds, count, CRGB(255, g, b));
    }
};
