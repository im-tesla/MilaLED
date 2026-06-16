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
        // LED writing is done in EffectsEngine::flushHyperion() at zero latency.
        // This tick() exists only to satisfy the EffectBase interface.
    }
};

#endif // HYPERION_STANDALONE_SKIP
