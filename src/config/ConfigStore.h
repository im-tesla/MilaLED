#pragma once
#include <Arduino.h>

struct Config {
    bool     power        = true;
    uint8_t  brightness   = 180;
    char     effect[32]   = "rainbow";
    uint8_t  speed        = 128;
    uint8_t  intensity    = 128;
    uint32_t colorPrimary = 0xFF4500;
    uint32_t colorSecondary = 0x000080;
    char     palette[32]  = "RainbowColors";

    uint16_t segALeds     = 120;
    bool     segAHalf     = true;
    uint16_t segBLeds     = 58;
    bool     segBHalf     = false;
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
