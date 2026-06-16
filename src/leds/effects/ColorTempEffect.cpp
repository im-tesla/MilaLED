#pragma once
#include "EffectBase.h"

class ColorTempEffect : public EffectBase {
public:
    const char* id() const override { return "colortemp"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        // intensity  0 → deep orange-red (255,20,0)
        // intensity  64 → warm amber   (255,85,0)
        // intensity 128 → warm white   (255,150,30)
        // intensity 192 → cool white   (255,210,120)
        // intensity 255 → pure white   (255,255,255)
        static bool once = false;
        if (!once) { Serial.printf("[colortemp] v2 range: intensity=%u\n", p.intensity); once = true; }
        uint8_t r = 255;
        uint8_t g = map(p.intensity, 0, 255,  20, 255);
        uint8_t b = map(p.intensity, 0, 255,   0, 255);
        fill_solid(leds, count, CRGB(r, g, b));
    }
};
