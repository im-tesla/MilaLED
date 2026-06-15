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

static EffectBase* _effectList[] = {
    &_eSolid, &_eColorTemp, &_eRainbow, &_eComet, &_eCylon,
    &_eTheaterChase, &_eRunning, &_eFire, &_eLava, &_eOcean,
    &_eTwinkle, &_eMeteor, &_eSparkle, &_eBreathing, &_eStrobe,
    &_ePerlinFlow, &_eAmbilight
};
static const uint8_t EFFECT_COUNT = sizeof(_effectList) / sizeof(_effectList[0]);

// ---------------------------------------------------------------------------

void EffectsEngine::begin(const Config& cfg) {
    _mapper = new PixelMapper(cfg.segALeds, cfg.segAHalf, cfg.segBLeds, cfg.segBHalf);
    _physCount = _mapper->physicalCount();
    _virtCount = _mapper->virtualCount();
    _leds = new CRGB[_physCount];
    _vbuf = new CRGB[_virtCount];

    // FastLED pin must be a compile-time template constant.
    // Switch covers all GPIO pins exposed in the UI pin selector.
    switch (cfg.dataPin) {
        case  4: FastLED.addLeds<WS2815,  4, GRB>(_leds, _physCount); break;
        case  5: FastLED.addLeds<WS2815,  5, GRB>(_leds, _physCount); break;
        case 12: FastLED.addLeds<WS2815, 12, GRB>(_leds, _physCount); break;
        case 13: FastLED.addLeds<WS2815, 13, GRB>(_leds, _physCount); break;
        case 14: FastLED.addLeds<WS2815, 14, GRB>(_leds, _physCount); break;
        default: FastLED.addLeds<WS2815,  2, GRB>(_leds, _physCount); break;
    }
    FastLED.setBrightness(cfg.brightness);

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
    if (!_active) return;
    uint32_t now = millis();
    if (now - _lastTick < _frameMs) return;
    _lastTick = now;

    _active->tick(_vbuf, _virtCount, _params);
    flushVirtualToPhysical();
    FastLED.show();
}

void EffectsEngine::flushVirtualToPhysical() {
    for (uint16_t v = 0; v < _virtCount; v++) {
        _leds[_mapper->toPhysical(v)] = _vbuf[v];
    }
    // Fill skipped physical LEDs by copying even→odd neighbour for half-density segments
    if (_mapper->segAHalf()) {
        for (uint16_t p = 1; p < _mapper->segACount(); p += 2) {
            _leds[p] = _leds[p - 1];
        }
    }
    if (_mapper->segBHalf()) {
        uint16_t segBStart = _mapper->segACount();
        for (uint16_t p = segBStart + 1; p < _physCount; p += 2) {
            _leds[p] = _leds[p - 1];
        }
    }
}

EffectBase* EffectsEngine::findEffect(const char* id) const {
    for (uint8_t i = 0; i < EFFECT_COUNT; i++) {
        if (strcmp(_effectList[i]->id(), id) == 0) return _effectList[i];
    }
    return nullptr;
}
