#pragma once
#include "EffectBase.h"

class CylonEffect : public EffectBase {
    uint16_t _pos = 0;
    int8_t   _dir = 1;
public:
    const char* id() const override { return "cylon"; }
    void reset() override { _pos = 0; _dir = 1; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fadeToBlackBy(leds, count, 60);
        uint8_t step = map(p.speed, 0, 255, 1, 4);
        uint8_t eye  = map(p.intensity, 0, 255, 2, 8);
        for (uint8_t i = 0; i < eye; i++) {
            if (_pos + i < count) leds[_pos + i] = p.primary;
        }
        _pos += _dir * step;
        if ((int16_t)_pos >= (int16_t)(count - eye) || _pos == 0) _dir = -_dir;
    }
};
