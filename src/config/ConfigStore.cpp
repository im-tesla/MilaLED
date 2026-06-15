#include "ConfigStore.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char* CFG_PATH = "/config.json";

bool ConfigStore::begin() {
    return LittleFS.begin();
}

bool ConfigStore::load(Config& cfg) {
    File f = LittleFS.open(CFG_PATH, "r");
    if (!f) return false;
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, f)) { f.close(); return false; }
    f.close();

    cfg.power          = doc["power"]      | cfg.power;
    cfg.brightness     = doc["brightness"] | cfg.brightness;
    strlcpy(cfg.effect, doc["effect"] | cfg.effect, sizeof(cfg.effect));
    cfg.speed          = doc["speed"]      | cfg.speed;
    cfg.intensity      = doc["intensity"]  | cfg.intensity;
    cfg.colorPrimary   = doc["colorPrimary"]   | cfg.colorPrimary;
    cfg.colorSecondary = doc["colorSecondary"] | cfg.colorSecondary;
    strlcpy(cfg.palette, doc["palette"] | cfg.palette, sizeof(cfg.palette));
    cfg.segALeds = doc["segALeds"] | cfg.segALeds;
    cfg.segAHalf = doc.containsKey("segAHalf") ? doc["segAHalf"].as<bool>() : cfg.segAHalf;
    cfg.segBLeds = doc["segBLeds"] | cfg.segBLeds;
    cfg.segBHalf = doc.containsKey("segBHalf") ? doc["segBHalf"].as<bool>() : cfg.segBHalf;
    cfg.dataPin   = doc["dataPin"]   | cfg.dataPin;
    cfg.colorOrder= doc["colorOrder"]| cfg.colorOrder;
    cfg.chipset   = doc["chipset"]   | cfg.chipset;
    strlcpy(cfg.tvIp, doc["tvIp"] | cfg.tvIp, sizeof(cfg.tvIp));
    cfg.ambPollMs = doc["ambPollMs"] | cfg.ambPollMs;
    strlcpy(cfg.ambMapping, doc["ambMapping"] | cfg.ambMapping, sizeof(cfg.ambMapping));
    return true;
}

bool ConfigStore::save(const Config& cfg) {
    StaticJsonDocument<512> doc;
    doc["power"]          = cfg.power;
    doc["brightness"]     = cfg.brightness;
    doc["effect"]         = cfg.effect;
    doc["speed"]          = cfg.speed;
    doc["intensity"]      = cfg.intensity;
    doc["colorPrimary"]   = cfg.colorPrimary;
    doc["colorSecondary"] = cfg.colorSecondary;
    doc["palette"]        = cfg.palette;
    doc["segALeds"]       = cfg.segALeds;
    doc["segAHalf"]       = cfg.segAHalf;
    doc["segBLeds"]       = cfg.segBLeds;
    doc["segBHalf"]       = cfg.segBHalf;
    doc["dataPin"]        = cfg.dataPin;
    doc["colorOrder"]     = cfg.colorOrder;
    doc["chipset"]        = cfg.chipset;
    doc["tvIp"]           = cfg.tvIp;
    doc["ambPollMs"]      = cfg.ambPollMs;
    doc["ambMapping"]     = cfg.ambMapping;

    File f = LittleFS.open(CFG_PATH, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}
