#pragma once
#include <FastLED.h>
#include "effects/EffectBase.h"
#include "PixelMapper.h"
#include "../config/ConfigStore.h"

// forward-declare all effect types
class SolidEffect;
class ColorTempEffect;
class RainbowEffect;
class CometEffect;
class CylonEffect;
class TheaterChaseEffect;
class RunningLightsEffect;
class FireEffect;
class LavaEffect;
class OceanEffect;
class TwinkleEffect;
class MeteorRainEffect;
class SparkleEffect;
class BreathingEffect;
class StrobeEffect;
class PerlinFlowEffect;
class AmbilightEffect;

class EffectsEngine {
public:
    void begin(const Config& cfg);
    void tick();
    void applyConfig(const Config& cfg);
    uint16_t virtualCount() const { return _mapper ? _mapper->virtualCount() : 0; }
    uint8_t  ambStatus() const;

private:
    CRGB*        _leds      = nullptr;
    PixelMapper* _mapper    = nullptr;
    CRGB*        _vbuf      = nullptr;
    uint16_t     _physCount = 0;
    uint16_t     _virtCount = 0;

    EffectBase*  _active    = nullptr;
    EffectParams _params;
    uint32_t     _lastTick  = 0;
    uint16_t     _frameMs   = 20;

    void flushVirtualToPhysical();
    EffectBase* findEffect(const char* id) const;
};
