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
    enum Status {
        STATUS_BOOTING  = 0,  // blue pulse
        STATUS_AP_MODE  = 1,  // yellow blink (config portal open)
        STATUS_OK       = 2,  // green solid, then fade to effect
        STATUS_NONE     = 255 // normal effect mode
    };

    void begin(const Config& cfg);
    void tick();
    void applyConfig(const Config& cfg);
    void setStatus(Status s) { _status = s; _statusStart = millis(); }
    Status getStatus() const { return _status; }
    uint16_t virtualCount() const { return _mapper ? _mapper->virtualCount() : 0; }
    uint8_t  ambStatus() const;

private:
    CRGB*        _leds      = nullptr;
    PixelMapper* _mapper    = nullptr;
    CRGB*        _vbuf      = nullptr;
    uint16_t     _physCount = 0;
    uint16_t     _virtCount = 0;

    // Auxiliary controllers mirror the main LED buffer to extra GPIOs.
    // They point directly at _leds so there's no arbitrary LED count limit.
    // C3 gets 1 aux (GPIO21), ESP32 gets 4, ESP8266 gets 3.
    uint8_t  _auxPinCount = 0;

    EffectBase*  _active    = nullptr;
    EffectParams _params;
    uint32_t     _lastTick  = 0;
    uint16_t     _frameMs   = 20;
    Status       _status       = STATUS_BOOTING;
    uint32_t     _statusStart  = 0;

    void flushVirtualToPhysical();
    EffectBase* findEffect(const char* id) const;
};
