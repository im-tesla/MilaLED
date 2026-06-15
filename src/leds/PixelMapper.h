#pragma once
#include <stdint.h>

class PixelMapper {
public:
    PixelMapper(uint16_t segACount, bool segAHalf, uint16_t segBCount, bool segBHalf);

    uint16_t toPhysical(uint16_t virtualIndex) const;
    uint16_t virtualCount() const { return _virtualCount; }
    uint16_t physicalCount() const { return _segACount + _segBCount; }
    uint16_t segACount() const { return _segACount; }
    bool     segAHalf()  const { return _segAHalf; }
    bool     segBHalf()  const { return _segBHalf; }

private:
    uint16_t _segACount;
    bool     _segAHalf;
    uint16_t _segBCount;
    bool     _segBHalf;
    uint16_t _segAVirtual;
    uint16_t _segBVirtual;
    uint16_t _virtualCount;
};
