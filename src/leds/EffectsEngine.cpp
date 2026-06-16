#include "EffectsEngine.h"
#include "effects/SolidEffect.cpp"
#include "effects/ColorTempEffect.cpp"
#include "effects/RainbowEffect.cpp"
#include "effects/CometEffect.cpp"
#include "effects/CylonEffect.cpp"
#include "effects/TheaterChaseEffect.cpp"
#include "effects/RunningLightsEffect.cpp"
#include "effects/FireEffect.cpp"
#include "effects/LavaEffect.cpp"
#include "effects/OceanEffect.cpp"
#include "effects/TwinkleEffect.cpp"
#include "effects/MeteorRainEffect.cpp"
#include "effects/SparkleEffect.cpp"
#include "effects/BreathingEffect.cpp"
#include "effects/StrobeEffect.cpp"
#include "effects/PerlinFlowEffect.cpp"
#include "effects/AmbilightEffect.cpp"
#include "effects/HyperionEffect.cpp"

static SolidEffect         _eSolid;
static ColorTempEffect     _eColorTemp;
static RainbowEffect       _eRainbow;
static CometEffect         _eComet;
static CylonEffect         _eCylon;
static TheaterChaseEffect  _eTheaterChase;
static RunningLightsEffect _eRunning;
static FireEffect          _eFire;
static LavaEffect          _eLava;
static OceanEffect         _eOcean;
static TwinkleEffect       _eTwinkle;
static MeteorRainEffect    _eMeteor;
static SparkleEffect       _eSparkle;
static BreathingEffect     _eBreathing;
static StrobeEffect        _eStrobe;
static PerlinFlowEffect    _ePerlinFlow;
static AmbilightEffect     _eAmbilight;
static HyperionEffect      _eHyperion;

static EffectBase* _effectList[] = {
    &_eSolid, &_eColorTemp, &_eRainbow, &_eComet, &_eCylon,
    &_eTheaterChase, &_eRunning, &_eFire, &_eLava, &_eOcean,
    &_eTwinkle, &_eMeteor, &_eSparkle, &_eBreathing, &_eStrobe,
    &_ePerlinFlow, &_eAmbilight, &_eHyperion
};
static const uint8_t EFFECT_COUNT = sizeof(_effectList) / sizeof(_effectList[0]);

// ---------------------------------------------------------------------------

void EffectsEngine::begin(const Config& cfg) {
    _mapper = new PixelMapper(cfg.segALeds, cfg.segAHalf, cfg.segBLeds, cfg.segBHalf);
    _physCount = _mapper->physicalCount();
    _virtCount = _mapper->virtualCount();
    _leds = new CRGB[_physCount];
    _vbuf = new CRGB[_virtCount];

    // FastLED pin, chipset, and color order are compile-time template constants.
    // Macros generate the cross-product of (chipset × color order × pin).
    // Pins 2-5, 12-14 common to all; 15-33 ESP32-only.
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define LED_PIN_CASES(CHIP, ORDER) \
    switch (cfg.dataPin) { \
        case  4: FastLED.addLeds<CHIP,  4, ORDER>(_leds, _physCount); break; \
        case  5: FastLED.addLeds<CHIP,  5, ORDER>(_leds, _physCount); break; \
        case 21: FastLED.addLeds<CHIP, 21, ORDER>(_leds, _physCount); break; \
        default: FastLED.addLeds<CHIP,  2, ORDER>(_leds, _physCount); break; \
    }
#elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define LED_PIN_CASES(CHIP, ORDER) \
    switch (cfg.dataPin) { \
        case  2: FastLED.addLeds<CHIP,  2, ORDER>(_leds, _physCount); break; \
        case  4: FastLED.addLeds<CHIP,  4, ORDER>(_leds, _physCount); break; \
        case  5: FastLED.addLeds<CHIP,  5, ORDER>(_leds, _physCount); break; \
        case 12: FastLED.addLeds<CHIP, 12, ORDER>(_leds, _physCount); break; \
        case 13: FastLED.addLeds<CHIP, 13, ORDER>(_leds, _physCount); break; \
        case 14: FastLED.addLeds<CHIP, 14, ORDER>(_leds, _physCount); break; \
        case 15: FastLED.addLeds<CHIP, 15, ORDER>(_leds, _physCount); break; \
        case 16: FastLED.addLeds<CHIP, 16, ORDER>(_leds, _physCount); break; \
        case 21: FastLED.addLeds<CHIP, 21, ORDER>(_leds, _physCount); break; \
        case 22: FastLED.addLeds<CHIP, 22, ORDER>(_leds, _physCount); break; \
        case 27: FastLED.addLeds<CHIP, 27, ORDER>(_leds, _physCount); break; \
        case 32: FastLED.addLeds<CHIP, 32, ORDER>(_leds, _physCount); break; \
        default: FastLED.addLeds<CHIP,  2, ORDER>(_leds, _physCount); break; \
    }
#else
#define LED_PIN_CASES(CHIP, ORDER) \
    switch (cfg.dataPin) { \
        case  2: FastLED.addLeds<CHIP,  2, ORDER>(_leds, _physCount); break; \
        case  4: FastLED.addLeds<CHIP,  4, ORDER>(_leds, _physCount); break; \
        case  5: FastLED.addLeds<CHIP,  5, ORDER>(_leds, _physCount); break; \
        case 12: FastLED.addLeds<CHIP, 12, ORDER>(_leds, _physCount); break; \
        case 13: FastLED.addLeds<CHIP, 13, ORDER>(_leds, _physCount); break; \
        case 14: FastLED.addLeds<CHIP, 14, ORDER>(_leds, _physCount); break; \
        case 15: FastLED.addLeds<CHIP, 15, ORDER>(_leds, _physCount); break; \
        case 16: FastLED.addLeds<CHIP, 16, ORDER>(_leds, _physCount); break; \
        default: FastLED.addLeds<CHIP,  2, ORDER>(_leds, _physCount); break; \
    }
#endif

#define ORDER_CASES(CHIP) \
    switch (cfg.colorOrder) { \
        case 0: LED_PIN_CASES(CHIP, RGB) break; \
        case 1: LED_PIN_CASES(CHIP, RBG) break; \
        case 2: LED_PIN_CASES(CHIP, GRB) break; \
        case 3: LED_PIN_CASES(CHIP, GBR) break; \
        case 4: LED_PIN_CASES(CHIP, BRG) break; \
        case 5: LED_PIN_CASES(CHIP, BGR) break; \
        default:LED_PIN_CASES(CHIP, GRB) break; \
    }

    switch (cfg.chipset) {
        case 0: ORDER_CASES(WS2811)  break;
        case 1: ORDER_CASES(WS2812B) break;
        case 2: ORDER_CASES(WS2815)  break;
        case 3: ORDER_CASES(WS2813)  break;
        case 4: ORDER_CASES(SK6812)  break;
        default:ORDER_CASES(WS2815)  break;
    }
#undef LED_PIN_CASES
#undef ORDER_CASES
    FastLED.setBrightness(cfg.brightness);

    // ── Status broadcast on extra GPIOs ──────────────────
    // Register auxiliary controllers that point at the same _leds buffer
    // so every output pin drives the full strip identically.
#define AUX_LED(CHIP, PIN, ORDER) \
    FastLED.addLeds<CHIP, PIN, ORDER>(_leds, _physCount); \
    _auxPinCount++;

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define AUX_PINS(CHIP, ORDER) { _auxPinCount = 0; \
    AUX_LED(CHIP,  2, ORDER) AUX_LED(CHIP,  4, ORDER) \
    AUX_LED(CHIP,  5, ORDER) AUX_LED(CHIP, 12, ORDER) \
    AUX_LED(CHIP, 21, ORDER) AUX_LED(CHIP, 32, ORDER) \
}
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define AUX_PINS(CHIP, ORDER) { _auxPinCount = 0; \
    AUX_LED(CHIP, 21, ORDER) \
    AUX_LED(CHIP,  2, ORDER) \
    AUX_LED(CHIP,  4, ORDER) \
    AUX_LED(CHIP,  5, ORDER) \
}
#else  // ESP8266
#define AUX_PINS(CHIP, ORDER) { _auxPinCount = 0; \
    AUX_LED(CHIP,  2, ORDER) AUX_LED(CHIP,  4, ORDER) \
    AUX_LED(CHIP,  5, ORDER) \
}
#endif

#define AUX_ORDER_ALL(CHIP) \
    switch (cfg.colorOrder) { \
        case 0: AUX_PINS(CHIP, RGB) break; \
        case 1: AUX_PINS(CHIP, RBG) break; \
        case 2: AUX_PINS(CHIP, GRB) break; \
        case 3: AUX_PINS(CHIP, GBR) break; \
        case 4: AUX_PINS(CHIP, BRG) break; \
        case 5: AUX_PINS(CHIP, BGR) break; \
        default:AUX_PINS(CHIP, GRB) break; \
    }

    switch (cfg.chipset) {
        case 0: AUX_ORDER_ALL(WS2811)  break;
        case 1: AUX_ORDER_ALL(WS2812B) break;
        case 2: AUX_ORDER_ALL(WS2815)  break;
        case 3: AUX_ORDER_ALL(WS2813)  break;
        case 4: AUX_ORDER_ALL(SK6812)  break;
        default:AUX_ORDER_ALL(WS2815)  break;
    }
#undef AUX_LED
#undef AUX_PINS
#undef AUX_ORDER_ALL
    // ── End broadcast setup ──────────────────────────────

    applyConfig(cfg);
}

void EffectsEngine::applyConfig(const Config& cfg) {
    FastLED.setBrightness(cfg.power ? cfg.brightness : 0);
    _params.speed     = cfg.speed;
    _params.intensity = cfg.intensity;
    _params.primary   = CRGB(cfg.colorPrimary >> 16,
                              (cfg.colorPrimary >> 8) & 0xFF,
                               cfg.colorPrimary & 0xFF);
    _params.secondary = CRGB(cfg.colorSecondary >> 16,
                              (cfg.colorSecondary >> 8) & 0xFF,
                               cfg.colorSecondary & 0xFF);

    // wire AmbilightEffect config
    _eAmbilight.configure(cfg.tvIp, cfg.ambPollMs, cfg.ambMapping);

    EffectBase* e = findEffect(cfg.effect);
    if (e && e != _active) { e->reset(); _active = e; }
    else if (!_active) { _eSolid.reset(); _active = &_eSolid; }
}

void EffectsEngine::tick() {
    uint32_t now = millis();
    if (now - _lastTick < _frameMs) return;
    _lastTick = now;

    if (!_leds || _physCount == 0) return;

    // ── System status indicator ──────────────────────────
    if (_status != STATUS_NONE) {
        uint32_t elapsed = now - _statusStart;
        CRGB statusColor = CRGB::Black;

        switch (_status) {
            case STATUS_BOOTING: {
                // Blue pulse — breathing between dim and bright
                uint8_t b = beatsin8(25, 30, 180, 0, 0);
                statusColor = CRGB(0, 0, b);
                break;
            }
            case STATUS_AP_MODE: {
                // Yellow blink every 600ms
                statusColor = ((elapsed % 600) < 300) ? CRGB(255, 160, 0) : CRGB::Black;
                break;
            }
            case STATUS_OK: {
                // Green — solid for 2s then switch to normal effect
                if (elapsed < 2000) {
                    statusColor = CRGB(0, 255, 0);
                } else {
                    _status = STATUS_NONE;
                }
                break;
            }
            default:
                _status = STATUS_NONE;
                break;
        }

        // Write status color — all controllers share _leds, so one fill_solid hits every GPIO
        fill_solid(_leds, _physCount, statusColor);
        FastLED.show();
        if (_status != STATUS_NONE) return;
    }
    // ── End status indicator ─────────────────────────────

    if (!_active) return;

    _active->tick(_vbuf, _virtCount, _params);

    // Apply intensity as a global brightness scale on the virtual buffer.
    // Individual effects may also interpret intensity for behaviour (tail
    // length, sparkle count, etc.), but this ensures EVERY effect responds
    // to the intensity slider as a dimmer.
    if (_params.intensity < 255) {
        uint8_t scale = map(_params.intensity, 0, 255, 16, 255);  // floor at 6%
        for (uint16_t i = 0; i < _virtCount; i++) {
            _vbuf[i].nscale8(scale);
        }
    }

    flushVirtualToPhysical();
    FastLED.show();
}

void EffectsEngine::flushVirtualToPhysical() {
    for (uint16_t v = 0; v < _virtCount; v++) {
        _leds[_mapper->toPhysical(v)] = _vbuf[v];
    }
    // Set skipped physical LEDs to black for half-density segments
    if (_mapper->segAHalf()) {
        for (uint16_t p = 1; p < _mapper->segACount(); p += 2) {
            _leds[p] = CRGB::Black;
        }
    }
    if (_mapper->segBHalf()) {
        uint16_t segBStart = _mapper->segACount();
        for (uint16_t p = segBStart + 1; p < _physCount; p += 2) {
            _leds[p] = CRGB::Black;
        }
    }
}

EffectBase* EffectsEngine::findEffect(const char* id) const {
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (strcmp(_effectList[i]->id(), id) == 0) return _effectList[i];
    }
    return nullptr;
}

uint8_t EffectsEngine::ambStatus() const {
    return _eAmbilight.getStatus();
}
