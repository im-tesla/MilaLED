#pragma once
#include "EffectBase.h"

class FireEffect : public EffectBase {
    static const uint8_t MAX_LEDS = 200;
    uint8_t _heat[MAX_LEDS] = {};
public:
    const char* id() const override { return "fire2012"; }
    void reset() override { memset(_heat, 0, sizeof(_heat)); }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint8_t cooling  = map(p.intensity, 0, 255, 20, 100);
        uint8_t sparking = map(p.speed,     0, 255, 50, 200);
        uint16_t n = (count < MAX_LEDS) ? count : MAX_LEDS;

        // cool
        for (uint16_t i = 0; i < n; i++)
            _heat[i] = qsub8(_heat[i], random8(0, ((cooling * 10) / n) + 2));
        // drift up
        for (uint16_t i = n - 1; i >= 2; i--)
            _heat[i] = (_heat[i-1] + _heat[i-2] + _heat[i-2]) / 3;
        // spark
        if (random8() < sparking) {
            uint8_t y = random8(7);
            _heat[y] = qadd8(_heat[y], random8(160, 255));
        }
        // render
        for (uint16_t i = 0; i < n; i++)
            leds[i] = ColorFromPalette(HeatColors_p, _heat[i]);
    }
};
