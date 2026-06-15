#pragma once
#include "EffectBase.h"

class MeteorRainEffect : public EffectBase {
    uint16_t _pos = 0;
public:
    const char* id() const override { return "meteor"; }
    void reset() override { _pos = 0; }
    void tick(CRGB* leds, uint16_t count, const EffectParams& p) override {
        uint8_t tail = map(p.intensity, 0, 255, 4, 16);
        fadeToBlackBy(leds, count, 40);
        if (_pos < count + tail) {
            for (uint8_t i = 0; i < tail; i++) {
                if (_pos >= i && (_pos - i) < count) {
                    uint8_t bright = 255 - (i * (255 / tail));
                    leds[_pos - i] += p.primary;
                    leds[_pos - i].nscale8(bright);
                }
            }
        }
        if (++_pos >= count + tail) _pos = 0;
    }
};
