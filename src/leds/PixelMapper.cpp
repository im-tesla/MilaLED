#include "PixelMapper.h"

PixelMapper::PixelMapper(const SegmentCfg* seg, uint8_t count) {
    _physCount = 0;
    _virtCount = 0;
    _segCount  = 0;

    for (uint8_t i = 0; i < count && i < MAX_SEGMENTS; i++) {
        if (seg[i].count == 0) continue;

        uint8_t s          = _segCount;
        _physCounts[s]     = seg[i].count;
        _half[s]           = seg[i].half;
        _virtCounts[s]     = seg[i].half ? (seg[i].count / 2) : seg[i].count;
        _virtOffsets[s]    = _virtCount;
        _physStart[s]      = _physCount;

        _physCount  += _physCounts[s];
        _virtCount  += _virtCounts[s];
        _segCount++;
    }
}

uint16_t PixelMapper::toPhysical(uint16_t vi) const {
    if (vi >= _virtCount) return _physCount - 1;

    for (uint8_t s = 0; s < _segCount; s++) {
        if (vi < _virtOffsets[s] + _virtCounts[s]) {
            uint16_t local = vi - _virtOffsets[s];
            return _physStart[s] + (_half[s] ? local * 2 : local);
        }
    }
    return 0;  // unreachable
}

uint16_t PixelMapper::segStart(uint8_t i) const { return _physStart[i]; }
uint16_t PixelMapper::segEnd(uint8_t i)   const { return _physStart[i] + _physCounts[i]; }
bool     PixelMapper::segHalf(uint8_t i)  const { return _half[i]; }
