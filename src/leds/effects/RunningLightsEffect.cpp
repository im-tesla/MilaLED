#pragma once
#include "EffectBase.h"

class RunningLightsEffect : public EffectBase {
    uint16_t _pos = 0;
public:
    const char* id() const override { return "running"; }
    void reset() override { _pos = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        for (uint16_t i = 0; i < count; i++) {
            uint8_t level = sin8(((i * 256) / count + _pos) & 0xFF);
            leds[i] = p.primary;
            leds[i].nscale8(level);
        }
        _pos += map(p.speed, 0, 255, 1, 6);
    }
};
