#pragma once
#include <FastLED.h>

struct EffectParams {
    uint8_t  speed;
    uint8_t  intensity;
    CRGB     primary;
    CRGB     secondary;
    CRGBPalette16 palette;
};

class EffectBase {
public:
    virtual ~EffectBase() = default;
    virtual const char* id() const = 0;
    virtual void reset() {}
    virtual void tick(CRGB* leds, uint16_t count, const EffectParams& p) = 0;
};
