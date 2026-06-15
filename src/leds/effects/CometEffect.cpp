#pragma once
#include "EffectBase.h"

class CometEffect : public EffectBase {
    uint16_t _pos = 0;
public:
    const char* id() const override { return "comet"; }
    void reset() override { _pos = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        fadeToBlackBy(leds, count, 80);
        uint8_t tail = map(p.intensity, 0, 255, 4, 20);
        for (uint8_t i = 0; i < tail; i++) {
            if (_pos >= i) {
                leds[_pos - i] += p.primary;
                leds[_pos - i].nscale8(255 - (i * (255 / tail)));
            }
        }
        if (++_pos >= count) _pos = 0;
    }
};
