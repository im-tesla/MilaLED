#pragma once
#include "EffectBase.h"

class SolidEffect : public EffectBase {
public:
    const char* id() const override { return "solid"; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fill_solid(leds, count, p.primary);
    }
};
