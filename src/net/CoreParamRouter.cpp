#include "CoreParamRouter.h"

ParamApplyResult applyCoreParams(Config& cfg, JsonDocument& doc) {
    ParamApplyResult r;

    // Continuous params: apply immediately; caller must NOT save/broadcast
    // right away (that fights sliders and causes teleport-back jitter).
    if (doc.containsKey("brightness")) { cfg.brightness = doc["brightness"]; r.anyChanged = true; }
    if (doc.containsKey("speed"))      { cfg.speed      = doc["speed"];      r.anyChanged = true; }
    if (doc.containsKey("intensity"))  { cfg.intensity  = doc["intensity"];  r.anyChanged = true; }
    if (doc.containsKey("colorPrimary")) {
        const char* hex = doc["colorPrimary"];
        if (hex && hex[0] == '#') cfg.colorPrimary = strtoul(hex + 1, nullptr, 16);
        r.anyChanged = true;
    }
    if (doc.containsKey("colorSecondary")) {
        const char* hex = doc["colorSecondary"];
        if (hex && hex[0] == '#') cfg.colorSecondary = strtoul(hex + 1, nullptr, 16);
        r.anyChanged = true;
    }

    // Discrete params: caller should save to flash + broadcast/notify.
    if (doc.containsKey("power")) { cfg.power = doc["power"]; r.anyChanged = r.discreteChanged = true; }
    if (doc.containsKey("effect")) {
        strlcpy(cfg.effect, doc["effect"] | "", sizeof(cfg.effect));
        r.anyChanged = r.discreteChanged = true;
    }
    if (doc.containsKey("palette")) {
        strlcpy(cfg.palette, doc["palette"] | "", sizeof(cfg.palette));
        r.anyChanged = r.discreteChanged = true;
    }

    return r;
}
