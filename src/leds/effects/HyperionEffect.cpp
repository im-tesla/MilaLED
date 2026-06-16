#pragma once
#include "EffectBase.h"

// Only define the class when included by EffectsEngine.cpp (which has
// _hBuf/_hLen/_hLast in scope). When compiled standalone by PlatformIO,
// this file produces an empty .o — the real definition is in EffectsEngine.cpp.o.
#ifdef EFFECTS_ENGINE

// ── Effect (reads from global buffer populated by hyperionLoop) ──

class HyperionEffect : public EffectBase {
public:
    const char* id() const override { return "hyperion"; }
    void reset() override {}

    void tick(CRGB* leds, uint16_t count, const EffectParams&) override {
        // _hBuf, _hLen, _hLast defined at top of EffectsEngine.cpp (same TU)

        uint32_t now = millis();
        if (_hLen < 4) return;

        if (now - _hLast > 2500) {
            fill_solid(leds, count, CRGB::Black);
            _hLen = 0;
            return;
        }

        const uint8_t* p = _hBuf;
        uint16_t len = _hLen;
        uint8_t type = p[0];

        // DRGB: [2] [timeout] [R] [G] [B]... (Hyperion WLED controller)
        if (type == 2 && len >= 3) {
            uint16_t n = (len - 2) / 3;
            uint16_t max = count < n ? count : n;
            for (uint16_t i = 0; i < max; i++) {
                uint16_t off = 2 + i * 3;
                leds[i].r = p[off];
                leds[i].g = p[off+1];
                leds[i].b = p[off+2];
            }
        }
        // WARLS: [1] [timeout] [R] [G] [B]...
        else if (type == 1 && len >= 2) {
            uint16_t n = (len - 1) / 3;
            uint16_t max = count < n ? count : n;
            for (uint16_t i = 0; i < max; i++) {
                uint16_t off = 1 + i * 3;
                leds[i].r = p[off];
                leds[i].g = p[off+1];
                leds[i].b = p[off+2];
            }
        }
        // Raw RGB with no header
        else if (len >= 3) {
            uint16_t n = len / 3;
            uint16_t max = count < n ? count : n;
            for (uint16_t i = 0; i < max; i++) {
                uint16_t off = i * 3;
                leds[i].r = p[off];
                leds[i].g = p[off+1];
                leds[i].b = p[off+2];
            }
        }
    }
};

#endif // HYPERION_STANDALONE_SKIP
