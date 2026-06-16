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

    // Load segment array — migrate old 2-segment format if present
    if (doc.containsKey("segments")) {
        JsonArray arr = doc["segments"];
        for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
            if (i < arr.size()) {
                cfg.segments[i].count = arr[i]["count"] | 0;
                cfg.segments[i].half  = arr[i].containsKey("half")
                    ? arr[i]["half"].as<bool>() : false;
            } else {
                cfg.segments[i].count = 0;
                cfg.segments[i].half  = false;
            }
        }
    } else if (doc.containsKey("segALeds")) {
        // Migration: old 2-segment format → new segments array
        cfg.segments[0].count = doc["segALeds"] | 120;
        cfg.segments[0].half  = doc.containsKey("segAHalf")
            ? doc["segAHalf"].as<bool>() : true;
        cfg.segments[1].count = doc["segBLeds"] | 58;
        cfg.segments[1].half  = doc.containsKey("segBHalf")
            ? doc["segBHalf"].as<bool>() : false;
        cfg.segments[2] = {0, false};
        cfg.segments[3] = {0, false};
    } else {
        // Fresh install — keep defaults
    }

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

    JsonArray arr = doc["segments"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
        JsonObject seg = arr.createNestedObject();
        seg["count"] = cfg.segments[i].count;
        seg["half"]  = cfg.segments[i].half;
    }

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
