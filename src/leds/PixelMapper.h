#pragma once
#include <stdint.h>
#include "../config/ConfigStore.h"

class PixelMapper {
public:
    PixelMapper(const SegmentCfg* segs, uint8_t count);

    uint16_t toPhysical(uint16_t virtualIndex) const;
    uint16_t virtualCount() const { return _virtCount; }
    uint16_t physicalCount() const { return _physCount; }

    // Per-segment half-density flags for flushVirtualToPhysical
    uint8_t  activeSegments() const { return _segCount; }
    uint16_t segStart(uint8_t i) const;  // physical start
    uint16_t segEnd(uint8_t i) const;    // physical end
    bool     segHalf(uint8_t i) const;   // half density flag

private:
    uint8_t  _segCount;
    uint16_t _virtOffsets[MAX_SEGMENTS];  // virtual start offset per seg
    uint16_t _virtCounts[MAX_SEGMENTS];   // virtual count per seg
    uint16_t _physStart[MAX_SEGMENTS];    // physical start per seg
    uint16_t _physCounts[MAX_SEGMENTS];   // physical count per seg
    bool     _half[MAX_SEGMENTS];
    uint16_t _virtCount;
    uint16_t _physCount;
};
