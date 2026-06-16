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
        uint32_t now = millis();
        if (_hLen < 3) return;

        if (now - _hLast > 2500) {
            fill_solid(leds, count, CRGB::Black);
            _hLen = 0;
            return;
        }

        const uint8_t* p = _hBuf;
        uint16_t len = _hLen;

        // Key insight: raw RGB has no header → packet length is evenly
        // divisible by 3. DRGB/WARLS have 1-2 byte headers → never % 3 == 0.
        // This avoids misinterpreting the first R value as a protocol byte.
        uint16_t off = 0;
        if (len % 3 == 0) {
            // Raw RGB (Hyperion identify + stream default)
            off = 0;
        } else if (p[0] == 2) {
            // DRGB: [2][timeout][R][G][B]...
            off = 2;
        } else if (p[0] == 1) {
            // WARLS: [1][timeout][R][G][B]...
            off = 1;
        } else {
            // Unknown format — try raw
            off = 0;
        }

        uint16_t n = (len - off) / 3;
        uint16_t max = count < n ? count : n;
        for (uint16_t i = 0; i < max; i++) {
            uint16_t pos = off + i * 3;
            leds[i].r = p[pos];
            leds[i].g = p[pos+1];
            leds[i].b = p[pos+2];
        }
    }
};

#endif // HYPERION_STANDALONE_SKIP
