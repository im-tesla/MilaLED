#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        static uint8_t dbg = 0;
        if (dbg < 5) { fill_solid(leds, count, CRGB(0,255,0)); dbg++; return; }

        // 6-point lookup table: blackbody temperature → 8-bit sRGB
        // ~1000K   intensity   0: (255, 51,  0)  deep orange
        // ~2200K   intensity  51: (255,137, 18)  warm amber
        // ~3200K   intensity 128: (255,180,101)  halogen warm white
        // ~4200K   intensity 192: (255,209,163)  neutral white
        // ~5400K   intensity 224: (255,232,206)  cool white
        // ~6500K   intensity 255: (255,249,253)  daylight
        static const uint8_t K_G[6] = { 51,137,180,209,232,249};
        static const uint8_t K_B[6] = {  0, 18,101,163,206,253};
        static const uint8_t K_X[6] = {  0, 51,128,192,224,255};

        uint8_t seg = 0;
        while (seg < 5 && p.intensity > K_X[seg+1]) seg++;

        uint16_t span = K_X[seg+1] - K_X[seg];
        uint16_t pos  = p.intensity - K_X[seg];

        uint8_t g = K_G[seg] + (pos * (K_G[seg+1] - K_G[seg])) / span;
        uint8_t b = K_B[seg] + (pos * (K_B[seg+1] - K_B[seg])) / span;

        fill_solid(leds, count, CRGB(255, g, b));
    }
};
