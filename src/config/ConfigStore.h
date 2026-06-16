#pragma once
#include <Arduino.h>

#define MAX_SEGMENTS 4

struct SegmentCfg {
    uint16_t count;
    bool     half;
    uint8_t  effect = 0;   // effect index (0=solid..17=hyperion)
    uint8_t  speed  = 128;
    uint8_t  intensity = 128;
    SegmentCfg() : count(0), half(false), effect(0), speed(128), intensity(128) {}
    SegmentCfg(uint16_t c, bool h) : count(c), half(h), effect(0), speed(128), intensity(128) {}
};

struct Config {
    bool     power        = true;
    uint8_t  brightness   = 180;
    char     effect[32]   = "rainbow";
    uint8_t  speed        = 128;
    uint8_t  intensity    = 128;
    uint32_t colorPrimary = 0xFF4500;
    uint32_t colorSecondary = 0x000080;
    char     palette[32]  = "RainbowColors";

    SegmentCfg segments[MAX_SEGMENTS] = {
        { 120, false },  // seg[0]: 120 LEDs, full density
        { 0, false },    // seg[1-3]: inactive
        { 0, false },
        { 0, false },
    };

    uint8_t  dataPin      = 2;
    uint8_t  colorOrder   = 2;   // 0=RGB, 1=RBG, 2=GRB, 3=GBR, 4=BRG, 5=BGR
    uint8_t  chipset      = 2;   // 0=WS2811, 1=WS2812B, 2=WS2815, 3=WS2813, 4=SK6812

    char     tvIp[16]     = "";
    uint16_t ambPollMs    = 100;
    char     ambMapping[16] = "right";
};

class ConfigStore {
public:
    bool begin();
    bool load(Config& cfg);
    bool save(const Config& cfg);
};
