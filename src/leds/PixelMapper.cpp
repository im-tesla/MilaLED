#include "PixelMapper.h"

PixelMapper::PixelMapper(uint16_t segACount, bool segAHalf, uint16_t segBCount, bool segBHalf)
    : _segACount(segACount), _segAHalf(segAHalf), _segBCount(segBCount), _segBHalf(segBHalf)
{
    _segAVirtual  = segAHalf ? (segACount / 2) : segACount;
    _segBVirtual  = segBHalf ? (segBCount / 2) : segBCount;
    _virtualCount = _segAVirtual + _segBVirtual;
}

uint16_t PixelMapper::toPhysical(uint16_t virtualIndex) const {
    uint16_t maxPhysical = _segACount + _segBCount - 1;
    if (virtualIndex >= _virtualCount) return maxPhysical;

    if (virtualIndex < _segAVirtual) {
        return _segAHalf ? virtualIndex * 2 : virtualIndex;
    }
    // seg B — apply half-density skip when enabled
    uint16_t segBVirtualIdx = virtualIndex - _segAVirtual;
    return _segACount + (_segBHalf ? segBVirtualIdx * 2 : segBVirtualIdx);
}
