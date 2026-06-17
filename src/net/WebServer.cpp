#include "WebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#ifdef ESP32
#include <HTTPClient.h>
#else
#include <ESP8266HTTPClient.h>
#endif
#include <WiFiClient.h>
#include <WiFiManager.h>
#include "../version.h"

void MilaWebServer::begin(Config* cfg, ConfigStore* store, EffectsEngine* engine) {
    _cfg = cfg; _store = store; _engine = engine;

    // Serve gzipped React app.
    // streamFile() auto-adds Content-Encoding: gzip when filename ends in .gz — no manual header needed.
    _http.on("/", HTTP_GET, [this]() {
        File f = LittleFS.open("/index.html.gz", "r");
        if (!f) { _http.send(404, "text/plain", "Not found"); return; }
        _http.streamFile(f, "text/html");
        f.close();
    });

    // ── WLED-compatible JSON endpoints ──────────────────
    // All /json/* requests are dispatched by a single handler, matching
    // WLED's serveJson() pattern. This avoids route collision issues with
    // trailing slashes and POST/GET variants on the same URI.

    auto handleJson = [this]() {
        String uri = _http.uri();
        HTTPMethod method = _http.method();
        uint16_t n = _engine->virtualCount();

        // Build the state object used by /json/state and /json (si)
        auto getState = [&](JsonObject st) {
            st["on"] = true; st["bri"] = 255; st["transition"] = 7;
            st["ps"] = -1; st["pl"] = -1; st["lor"] = 0; st["mainseg"] = 0; st["ledmap"] = 0;
            JsonObject nl = st["nl"].to<JsonObject>();
            nl["on"] = false; nl["dur"] = 60; nl["mode"] = 1; nl["tbri"] = 0; nl["rem"] = -1;
            JsonObject udpn = st["udpn"].to<JsonObject>();
            udpn["send"] = false; udpn["recv"] = true; udpn["sgrp"] = 1; udpn["rgrp"] = 1;
            JsonArray segs = st["seg"].to<JsonArray>();
            JsonObject seg = segs.createNestedObject();
            seg["id"] = 0; seg["start"] = 0; seg["stop"] = n; seg["len"] = n;
            seg["grp"] = 1; seg["spc"] = 0; seg["of"] = 0; seg["on"] = true;
            seg["frz"] = false; seg["bri"] = 255; seg["cct"] = 127; seg["fx"] = 0;
            seg["sx"] = 128; seg["ix"] = 128; seg["sel"] = true; seg["rev"] = false; seg["mi"] = false;
        };

        auto getInfo = [&](JsonObject info) {
            info["ver"] = "0.14.1"; info["vid"] = 2405180; info["name"] = "MilaLED";
            info["arch"] = "esp32"; info["core"] = "3.1.2"; info["lwip"] = 2;
            info["freeheap"] = ESP.getFreeHeap(); info["uptime"] = millis()/1000;
            info["opt"] = 0; info["brand"] = "WLED"; info["product"] = "FOSS";
            info["mac"] = WiFi.macAddress(); info["ip"] = WiFi.localIP().toString();
            info["str"] = false; info["udpport"] = 21324; info["simplifiedui"] = false; info["live"] = false;
            info["liveseg"] = -1; info["lm"] = ""; info["lip"] = ""; info["ws"] = 0;
            info["fxcount"] = 18; info["palcount"] = 8; info["cpalcount"] = 0;
            info["clock"] = 160; info["flash"] = 4; info["ndc"] = 0;
            JsonObject leds = info["leds"].to<JsonObject>();
            leds["count"] = n; leds["pwr"] = 0; leds["fps"] = 0;
            leds["maxpwr"] = 65000; leds["maxseg"] = 1;
            leds["lc"] = 1; leds["rgbw"] = false; leds["wv"] = 0; leds["cct"] = 0;
            JsonArray sl = leds["seglc"].to<JsonArray>(); sl.add(1);
            JsonObject wf = info["wifi"].to<JsonObject>();
            wf["bssid"] = WiFi.BSSIDstr(); wf["rssi"] = WiFi.RSSI();
            wf["signal"] = WiFi.RSSI(); wf["channel"] = WiFi.channel();
            JsonObject fs = info["fs"].to<JsonObject>();
            fs["u"] = 0; fs["t"] = 1024; fs["pmt"] = 0;
            JsonArray maps = info["maps"].to<JsonArray>();
            maps.createNestedObject()["id"] = 0;
        };

        StaticJsonDocument<2048> doc;
        String out;

        // /json/info
        if (uri == "/json/info" || uri == "/json/info/") {
            getInfo(doc.to<JsonObject>());
        }
        // /json/state
        else if (uri == "/json/state" || uri == "/json/state/") {
            getState(doc.to<JsonObject>());
            if (method == HTTP_POST) doc["success"] = true;
        }
        // /json or /json/ (si: state+info — what Hyperion probes)
        else if (uri == "/json" || uri == "/json/") {
            if (method == HTTP_POST) { getState(doc.to<JsonObject>()); doc["success"] = true; }
            else {
                getState(doc["state"].to<JsonObject>());
                getInfo(doc["info"].to<JsonObject>());
                JsonArray fx = doc["effects"].to<JsonArray>();
                for (const auto* e : {"Solid","Blink","Breathe","Wipe","Rainbow","Scan","Fade","Theater",
                    "Running","Saw","Twinkle","Sparkle","Strobe","Fire","Fireworks","Aurora","Flow","Pacifica"})
                    fx.add(e);
            }
        }
        // /json/si (explicit state+info)
        else if (uri == "/json/si" || uri == "/json/si/") {
            getState(doc["state"].to<JsonObject>());
            getInfo(doc["info"].to<JsonObject>());
        }
        else { _http.send(404, "text/plain", "Not found"); return; }

        serializeJson(doc, out);
        _http.send(200, "application/json", out);
    };

    _http.on("/json/info", HTTP_GET, handleJson);
    _http.on("/json/state", HTTP_GET, handleJson);
    _http.on("/json/state", HTTP_POST, handleJson);
    _http.on("/json", HTTP_GET, handleJson);
    _http.on("/json/", HTTP_GET, handleJson);
    _http.on("/json", HTTP_POST, handleJson);
    _http.on("/json/", HTTP_POST, handleJson);
    _http.on("/json/si", HTTP_POST, handleJson);

    // Serve other assets; look for .gz variant first.
    _http.onNotFound([this, handleJson]() {
        // Catch any /json* path that registered routes missed (trailing slashes, etc)
        String uri = _http.uri();
        if (uri.startsWith("/json")) {
            handleJson();
            return;
        }
        String path = _http.uri();
        String gzPath = path + ".gz";
        if (LittleFS.exists(gzPath)) {
            File f = LittleFS.open(gzPath, "r");
            String contentType = "text/plain";
            if (path.endsWith(".js"))    contentType = "application/javascript";
            if (path.endsWith(".css"))   contentType = "text/css";
            if (path.endsWith(".json"))  contentType = "application/json";
            if (path.endsWith(".svg"))   contentType = "image/svg+xml";
            if (path.endsWith(".woff2")) contentType = "font/woff2";
            if (path.endsWith(".html"))  contentType = "text/html";
            // streamFile auto-adds Content-Encoding: gzip for .gz files (unless type is
            // application/octet-stream or application/x-gzip, so we never use those).
            _http.streamFile(f, contentType);
            f.close();
        } else if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            _http.streamFile(f, "text/plain");
            f.close();
        } else {
            // SPA fallback: serve index.html for any unknown path (client-side routing).
            File f = LittleFS.open("/index.html.gz", "r");
            if (f) {
                _http.streamFile(f, "text/html");
                f.close();
            } else {
                _http.send(404, "text/plain", "Not found");
            }
        }
    });

    // REST: presets
    _http.on("/api/presets", HTTP_GET, [this]() { handleRestPresets(); });
    _http.on("/api/presets", HTTP_POST, [this]() {
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, _http.arg("plain"))) {
            _http.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        const char* name = doc["name"] | "Unnamed";
        // sanitize name (no path separators)
        String safeName = String(name);
        safeName.replace("/", "_");
        safeName.replace("\\", "_");
        LittleFS.mkdir("/presets");
        String path = String("/presets/") + safeName + ".json";
        File f = LittleFS.open(path, "w");
        if (!f) { _http.send(500, "application/json", "{\"error\":\"write failed\"}"); return; }
        serializeJson(doc, f);
        f.close();
        _http.send(200, "application/json", "{\"ok\":true}");
    });
    _http.on("/api/presets", HTTP_DELETE, [this]() {
        StaticJsonDocument<64> doc;
        deserializeJson(doc, _http.arg("plain"));
        String name = doc["name"] | "";
        name.replace("/", "_");
        name.replace("\\", "_");
        LittleFS.remove(String("/presets/") + name + ".json");
        _http.send(200, "application/json", "{\"ok\":true}");
    });

    // REST: ambilight scan — starts async scan; progress comes via WebSocket
    _http.on("/api/ambilight/scan", HTTP_POST, [this]() {
        if (_scanActive) { _http.send(409, "application/json", "{\"error\":\"scan already running\"}"); return; }
        _scanActive = true;
        _scanCancel = false;
        _scanIp = 1;
        _scanBase = WiFi.localIP();
        _http.send(200, "application/json", "{\"ok\":true}");
    });

    // REST: cancel running ambilight scan
    _http.on("/api/ambilight/scan/cancel", HTTP_POST, [this]() {
        _scanCancel = true;
        _http.send(200, "application/json", "{\"ok\":true}");
    });

    // REST: erase WiFi credentials and restart into AP/captive-portal mode
    _http.on("/api/wifi/reset", HTTP_POST, [this]() {
        _http.send(200, "application/json", "{\"ok\":true}");
        delay(300);
        WiFiManager wm;
        wm.resetSettings();
        ESP.restart();
    });

    // REST: strip config — saves and restarts so FastLED reinitialises
    _http.on("/api/strip", HTTP_POST, [this]() {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, _http.arg("plain"))) {
            _http.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        // Segment array
        if (doc.containsKey("segments")) {
            JsonArray arr = doc["segments"];
            for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
                if (i < arr.size()) {
                    _cfg->segments[i].count = arr[i]["count"] | 0;
                    _cfg->segments[i].half  = arr[i].containsKey("half")
                        ? arr[i]["half"].as<bool>() : false;
                } else {
                    _cfg->segments[i] = SegmentCfg();
                }
            }
        }
        if (doc.containsKey("dataPin"))   _cfg->dataPin   = doc["dataPin"];
        if (doc.containsKey("colorOrder"))_cfg->colorOrder= doc["colorOrder"];
        if (doc.containsKey("chipset"))   _cfg->chipset   = doc["chipset"];
        _store->save(*_cfg);
        _http.send(200, "application/json", "{\"ok\":true}");
        _pendingRestart = true;
    });

    _http.begin();

    _ws.begin();
    _ws.onEvent([this](uint8_t n, WStype_t t, uint8_t* p, size_t l) {
        handleWsEvent(n, t, p, l);
    });
}

void MilaWebServer::loop() {
    _http.handleClient();
    _ws.loop();

    // Non-blocking ambilight scan: one IP per loop iteration
    if (_scanActive) {
        if (_scanCancel || _scanIp > 254) {
            // scan done or cancelled
            StaticJsonDocument<64> done;
            done["type"] = "scanProgress";
            done["pct"]  = 100;
            done["msg"]  = _scanCancel ? "cancelled" : "done";
            String out;
            serializeJson(done, out);
            _ws.broadcastTXT(out.c_str());
            _scanActive = false;
        } else {
            char ipBuf[16];
            snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", _scanBase[0], _scanBase[1], _scanBase[2], _scanIp);
            broadcastScanProgress((uint8_t)((_scanIp * 100) / 254), ipBuf);

            WiFiClient client;
            HTTPClient http;

            // Pre-connect TCP with short timeout so dead IPs don't stall the scan
            // (lwIP TCP SYN retransmit can take 2-3s; client.setTimeout caps it)
            client.setTimeout(80);
            if (client.connect(ipBuf, 1925)) {
                // reuse the already-connected socket
                String url = String("http://") + ipBuf + ":1925/ambilight/processed";
                http.begin(client, url);
                http.setTimeout(80);
                if (http.GET() == 200) {
                    StaticJsonDocument<64> probe;
                    if (!deserializeJson(probe, http.getStream()) && probe.containsKey("layer1")) {
                        StaticJsonDocument<64> resp;
                        resp["type"] = "ambilightFound";
                        resp["ip"]   = ipBuf;
                        String out;
                        serializeJson(resp, out);
                        _ws.broadcastTXT(out.c_str());
                    }
                }
                http.end();
            }
            client.stop();
            _scanIp++;
        }
    }

    if (_pendingRestart) {
        delay(500); // let HTTP response + flash write flush
        ESP.restart();
    }
}

void MilaWebServer::broadcastState() {
    String json = buildStateJson();
    _ws.broadcastTXT(json.c_str());
}

void MilaWebServer::broadcastScanProgress(uint8_t pct, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["type"] = "scanProgress";
    doc["pct"]  = pct;
    doc["msg"]  = msg;
    String out;
    serializeJson(doc, out);
    _ws.broadcastTXT(out.c_str());
}

void MilaWebServer::handleWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (type == WStype_TEXT) {
        // ensure null-terminated
        char buf[512];
        size_t copyLen = (len < 511) ? len : 511;
        memcpy(buf, payload, copyLen);
        buf[copyLen] = '\0';
        handleWsMessage(buf);
    } else if (type == WStype_CONNECTED) {
        String json = buildStateJson();
        _ws.sendTXT(num, json.c_str());
    }
}

void MilaWebServer::handleWsMessage(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json)) return;

    bool anyChanged      = false;
    bool discreteChanged = false; // needs save + broadcast

    // Continuous params: update config + LEDs immediately, but do NOT save or echo
    // (echoing would fight the slider and cause teleport-back jitter)
    if (doc.containsKey("brightness")) { _cfg->brightness = doc["brightness"]; anyChanged = true; }
    if (doc.containsKey("speed"))      { _cfg->speed      = doc["speed"];      anyChanged = true; }
    if (doc.containsKey("intensity"))  { _cfg->intensity  = doc["intensity"];  anyChanged = true; }
    if (doc.containsKey("colorPrimary")) {
        const char* hex = doc["colorPrimary"];
        if (hex && hex[0] == '#') _cfg->colorPrimary = strtoul(hex + 1, nullptr, 16);
        anyChanged = true;
    }
    if (doc.containsKey("colorSecondary")) {
        const char* hex = doc["colorSecondary"];
        if (hex && hex[0] == '#') _cfg->colorSecondary = strtoul(hex + 1, nullptr, 16);
        anyChanged = true;
    }
    if (doc.containsKey("ambPollMs")) { _cfg->ambPollMs = doc["ambPollMs"]; anyChanged = true; }

    // Discrete params: save to flash + broadcast so other clients see the change
    if (doc.containsKey("power"))      { _cfg->power = doc["power"];                                              anyChanged = discreteChanged = true; }
    if (doc.containsKey("effect"))     { strlcpy(_cfg->effect,     doc["effect"]     | "", sizeof(_cfg->effect));  anyChanged = discreteChanged = true; }
    if (doc.containsKey("palette"))    { strlcpy(_cfg->palette,    doc["palette"]    | "", sizeof(_cfg->palette)); anyChanged = discreteChanged = true; }
    if (doc.containsKey("tvIp"))       { strlcpy(_cfg->tvIp,       doc["tvIp"]       | "", sizeof(_cfg->tvIp));    anyChanged = discreteChanged = true; }
    if (doc.containsKey("ambMapping")) { strlcpy(_cfg->ambMapping, doc["ambMapping"] | "", sizeof(_cfg->ambMapping)); anyChanged = discreteChanged = true; }

    if (anyChanged)      _engine->applyConfig(*_cfg);
    if (discreteChanged) { _store->save(*_cfg); broadcastState(); }
}

void MilaWebServer::handleRestPresets() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    File dir = LittleFS.open("/presets", "r");
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                StaticJsonDocument<256> p;
                if (!deserializeJson(p, f)) {
                    arr.add(p);
                }
            }
            f.close();
            f = dir.openNextFile();
        }
    }
    if (dir) dir.close();
    String out;
    serializeJson(doc, out);
    _http.send(200, "application/json", out);
}

String MilaWebServer::buildStateJson() {
    StaticJsonDocument<1024> doc;
    doc["type"]           = "state";
    doc["power"]          = _cfg->power;
    doc["brightness"]     = _cfg->brightness;
    doc["effect"]         = _cfg->effect;
    doc["speed"]          = _cfg->speed;
    doc["intensity"]      = _cfg->intensity;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%06lX", _cfg->colorPrimary);
    doc["colorPrimary"]   = hex;
    snprintf(hex, sizeof(hex), "#%06lX", _cfg->colorSecondary);
    doc["colorSecondary"] = hex;
    doc["palette"]        = _cfg->palette;
    doc["virtualLeds"]    = _engine->virtualCount();
    doc["ip"]             = WiFi.localIP().toString();
    doc["ssid"]           = WiFi.SSID();

    JsonArray segs = doc["segments"].to<JsonArray>();
    uint16_t physOff = 0;
    for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
        JsonObject seg = segs.createNestedObject();
        seg["count"]     = _cfg->segments[i].count;
        seg["half"]      = _cfg->segments[i].half;
        seg["start"]     = physOff;
        seg["virtCount"] = _cfg->segments[i].half
            ? (_cfg->segments[i].count / 2) : _cfg->segments[i].count;
        physOff += _cfg->segments[i].count;
    }
    doc["dataPin"]        = _cfg->dataPin;
    doc["colorOrder"]     = _cfg->colorOrder;
    doc["chipset"]        = _cfg->chipset;
    doc["version"]        = MILALED_VERSION;
    doc["tvIp"]           = _cfg->tvIp;
    const char* statusMap[] = {"idle", "polling", "error"};
    doc["ambStatus"]      = statusMap[_engine->ambStatus() % 3];
    doc["ambPollMs"]      = _cfg->ambPollMs;
    doc["ambMapping"]     = _cfg->ambMapping;
    String out;
    serializeJson(doc, out);
    return out;
}
