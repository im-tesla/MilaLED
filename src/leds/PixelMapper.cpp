#include "PixelMapper.h"

PixelMapper::PixelMapper(uint16_t segACount, bool segAHalf, uint16_t segBCount)
    : _segACount(segACount), _segAHalf(segAHalf), _segBCount(segBCount)
{
    _segAVirtual = segAHalf ? (segACount / 2) : segACount;
    _virtualCount = _segAVirtual + segBCount;
}

uint16_t PixelMapper::toPhysical(uint16_t virtualIndex) const {
    uint16_t maxPhysical = _segACount + _segBCount - 1;
    if (virtualIndex >= _virtualCount) return maxPhysical;

    if (virtualIndex < _segAVirtual) {
        return _segAHalf ? virtualIndex * 2 : virtualIndex;
    }
    // seg B
    return _segACount + (virtualIndex - _segAVirtual);
}
