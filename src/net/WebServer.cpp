#include "WebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "../wifi/NetworkManager.h"
#include "../version.h"
#include "CoreParamRouter.h"
#include "BleServer.h"

void MilaWebServer::begin(Config* cfg, ConfigStore* store, EffectsEngine* engine, NetworkManager* network) {
    _cfg = cfg; _store = store; _engine = engine; _network = network;

    // Serve gzipped React app.
    _http.on("/", HTTP_GET, [this]() {
        File f = LittleFS.open("/index.html.gz", "r");
        if (!f) { _http.send(404, "text/plain", "Not found"); return; }
        streamRobust(f, "text/html", true);
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
            streamRobust(f, contentType, true);
            f.close();
        } else if (LittleFS.exists(path)) {
            File f = LittleFS.open(path, "r");
            streamRobust(f, "text/plain", false);
            f.close();
        } else {
            // SPA fallback: serve index.html for any unknown path (client-side routing).
            File f = LittleFS.open("/index.html.gz", "r");
            if (f) {
                streamRobust(f, "text/html", true);
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

    // REST: Wi-Fi scan
    _http.on("/api/wifi/scan", HTTP_GET, [this]() {
        if (!_network) { _http.send(500, "application/json", "{\"error\":\"no network\"}"); return; }
        int16_t sc = _network->scanStatus();
        if (sc == -2) {
            _network->startScan();
            _http.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        } else if (sc == -1) {
            _http.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
        } else {
            String results = _network->getScanResultsJson();
            _network->cleanScan();
            String resp = String("{\"scanning\":false,\"networks\":") + results + "}";
            _http.send(200, "application/json", resp);
        }
    });

    _http.on("/api/wifi/scan", HTTP_POST, [this]() {
        if (!_network) { _http.send(500, "application/json", "{\"error\":\"no network\"}"); return; }
        _network->cleanScan();
        _network->startScan();
        _http.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
    });

    // REST: Wi-Fi join
    _http.on("/api/wifi/join", HTTP_POST, [this]() {
        if (!_network) { _http.send(500, "application/json", "{\"error\":\"no network\"}"); return; }
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, _http.arg("plain"))) {
            _http.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        const char* ssid = doc["ssid"] | "";
        const char* pass = doc["password"] | "";
        if (strlen(ssid) == 0) {
            _http.send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        _network->startJoin(ssid, pass);
        _http.send(200, "application/json", "{\"ok\":true,\"status\":\"connecting\"}");
    });

    // REST: Wi-Fi status
    _http.on("/api/wifi/status", HTTP_GET, [this]() {
        if (!_network) { _http.send(500, "application/json", "{\"error\":\"no network\"}"); return; }
        StaticJsonDocument<256> doc;
        NetworkManager::JoinStatus js = _network->joinStatus();
        if (js == NetworkManager::JOIN_CONNECTING) {
            doc["status"] = "connecting";
        } else if (js == NetworkManager::JOIN_SUCCESS || _network->isConnected()) {
            doc["status"] = "connected";
        } else if (js == NetworkManager::JOIN_FAILED) {
            doc["status"] = "failed";
            doc["error"] = _network->joinError();
        } else {
            doc["status"] = _network->isConnected() ? "connected" : "idle";
        }
        doc["connected"] = _network->isConnected();
        doc["isAp"] = _network->isAp();
        doc["ssid"] = _network->ssid();
        doc["ip"] = _network->localIP();
        String out;
        serializeJson(doc, out);
        _http.send(200, "application/json", out);
    });

    // REST: erase WiFi credentials and restart into AP mode
    _http.on("/api/wifi/reset", HTTP_POST, [this]() {
        _http.send(200, "application/json", "{\"ok\":true}");
        _pendingWifiReset = true;
    });

    // REST: strip config — saves and restarts so FastLED reinitialises
    _http.on("/api/strip", HTTP_POST, [this]() {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, _http.arg("plain"))) {
            _http.send(400, "application/json", "{\"error\":\"bad json\"}");
            return;
        }
        applyStripConfig(doc);
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
                        if (_ble) _ble->notifyJson(out);
                    }
                }
                http.end();
            }
            client.stop();
            _scanIp++;
        }
    }

    if (_pendingWifiReset) {
        delay(300);
        if (_network) _network->resetSettings();
        ESP.restart();
    }

    if (_bleScanPending && _network) {
        int16_t sc = _network->scanStatus();
        if (sc >= 0) {
            _bleScanPending = false;
            String list = _network->getScanResultsJson();
            _network->cleanScan();
            String notify = String("{\"type\":\"wifiScan\",\"networks\":") + list + "}";
            if (_ble) _ble->notifyJson(notify);
        }
    }
    if (_bleJoinPending && _network) {
        NetworkManager::JoinStatus js = _network->joinStatus();
        if (js == NetworkManager::JOIN_SUCCESS) {
            _bleJoinPending = false;
            StaticJsonDocument<256> res;
            res["type"]   = "wifiJoinResult";
            res["status"] = "connected";
            res["ip"]     = _network->localIP();
            res["ssid"]   = _network->ssid();
            String out;
            serializeJson(res, out);
            if (_ble) _ble->notifyJson(out);
            broadcastState();
        } else if (js == NetworkManager::JOIN_FAILED) {
            _bleJoinPending = false;
            StaticJsonDocument<256> res;
            res["type"]   = "wifiJoinResult";
            res["status"] = "failed";
            res["error"]  = _network->joinError();
            String out;
            serializeJson(res, out);
            if (_ble) _ble->notifyJson(out);
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
    if (_ble) _ble->notifyState();
}

void MilaWebServer::broadcastScanProgress(uint8_t pct, const char* msg) {
    StaticJsonDocument<128> doc;
    doc["type"] = "scanProgress";
    doc["pct"]  = pct;
    doc["msg"]  = msg;
    String out;
    serializeJson(doc, out);
    _ws.broadcastTXT(out.c_str());
    if (_ble) _ble->notifyJson(out);
}

void MilaWebServer::streamRobust(File& f, const String& contentType, bool gzip) {
    size_t fileSize = f.size();
    _http.setContentLength(fileSize);
    if (gzip) _http.sendHeader("Content-Encoding", "gzip");
    _http.send(200, contentType, "");

    WiFiClient client = _http.client();
    uint8_t buf[1024];
    size_t remaining = fileSize;
    while (remaining > 0 && client.connected()) {
        size_t toRead = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t bytesRead = f.read(buf, toRead);
        if (bytesRead == 0) break; // unexpected EOF

        size_t sent = 0;
        uint8_t retries = 50;
        while (sent < bytesRead && retries > 0) {
            size_t n = client.write(buf + sent, bytesRead - sent);
            if (n > 0) {
                sent += n;
            } else {
                retries--;
                delay(10); // let a congested/weak Wi-Fi link drain before retrying
            }
        }
        if (sent < bytesRead) { client.stop(); return; } // couldn't fully send — abort cleanly
        remaining -= bytesRead;
    }
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

    ParamApplyResult r = applyCoreParams(*_cfg, doc);
    bool anyChanged      = r.anyChanged;
    bool discreteChanged = r.discreteChanged;

    // WS-only continuous param
    if (doc.containsKey("ambPollMs")) { _cfg->ambPollMs = doc["ambPollMs"]; anyChanged = true; }

    // WS-only discrete params
    if (doc.containsKey("tvIp"))       { strlcpy(_cfg->tvIp,       doc["tvIp"]       | "", sizeof(_cfg->tvIp));    anyChanged = discreteChanged = true; }
    if (doc.containsKey("ambMapping")) { strlcpy(_cfg->ambMapping, doc["ambMapping"] | "", sizeof(_cfg->ambMapping)); anyChanged = discreteChanged = true; }

    if (anyChanged)      _engine->applyConfig(*_cfg);
    if (discreteChanged) { _store->save(*_cfg); broadcastState(); }
}

void MilaWebServer::handleRestPresets() {
    _http.send(200, "application/json", buildPresetsJson());
}

String MilaWebServer::buildPresetsJson() {
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
    return out;
}

void MilaWebServer::applyStripConfig(JsonDocument& doc) {
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
    if (doc.containsKey("dataPin"))    _cfg->dataPin    = doc["dataPin"];
    if (doc.containsKey("colorOrder")) _cfg->colorOrder = doc["colorOrder"];
    if (doc.containsKey("chipset"))    _cfg->chipset    = doc["chipset"];
    if (doc.containsKey("bleEnabled")) _cfg->bleEnabled = doc["bleEnabled"];
}

bool MilaWebServer::handleBleCommand(const char* json, String& response) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json)) return false;

    const char* action = doc["action"] | "";
    if (!strcmp(action, "presetList")) {
        response = buildPresetsJson();
        String wrapped = String("{\"type\":\"presets\",\"items\":") + response + "}";
        response = wrapped;
        return true;
    }

    if (!strcmp(action, "presetSave")) {
        String safeName = String(doc["name"] | "Unnamed");
        safeName.replace("/", "_");
        safeName.replace("\\", "_");
        LittleFS.mkdir("/presets");
        File f = LittleFS.open(String("/presets/") + safeName + ".json", "w");
        if (!f) { response = "{\"type\":\"ack\",\"action\":\"presetSave\",\"ok\":false}"; return true; }
        serializeJson(doc, f);
        f.close();
        response = String("{\"type\":\"presets\",\"items\":") + buildPresetsJson() + "}";
        return true;
    }

    if (!strcmp(action, "presetDelete")) {
        String name = String(doc["name"] | "");
        name.replace("/", "_");
        name.replace("\\", "_");
        LittleFS.remove(String("/presets/") + name + ".json");
        response = String("{\"type\":\"presets\",\"items\":") + buildPresetsJson() + "}";
        return true;
    }

    if (!strcmp(action, "strip")) {
        applyStripConfig(doc);
        _store->save(*_cfg);
        _pendingRestart = true;
        response = "{\"type\":\"ack\",\"action\":\"strip\",\"ok\":true}";
        return true;
    }

    if (!strcmp(action, "ambilightScan")) {
        if (_scanActive) {
            response = "{\"type\":\"ack\",\"action\":\"ambilightScan\",\"ok\":false}";
            return true;
        }
        _scanActive = true;
        _scanCancel = false;
        _scanIp = 1;
        _scanBase = WiFi.localIP();
        response = "{\"type\":\"ack\",\"action\":\"ambilightScan\",\"ok\":true}";
        return true;
    }

    if (!strcmp(action, "ambilightCancel")) {
        _scanCancel = true;
        response = "{\"type\":\"ack\",\"action\":\"ambilightCancel\",\"ok\":true}";
        return true;
    }

    if (!strcmp(action, "wifiDisconnect")) {
        if (_network) {
            _network->resetSettings();
            WiFi.disconnect();
            broadcastState();
            response = "{\"type\":\"ack\",\"action\":\"wifiDisconnect\",\"ok\":true}";
            return true;
        }
    }

    if (!strcmp(action, "wifiReset")) {
        _pendingWifiReset = true;
        response = "{\"type\":\"ack\",\"action\":\"wifiReset\",\"ok\":true}";
        return true;
    }

    if (!strcmp(action, "wifiScan")) {
        if (_network) {
            _network->cleanScan();
            _network->startScan();
            _bleScanPending = true;
            response = "{\"type\":\"ack\",\"action\":\"wifiScan\",\"ok\":true}";
            return true;
        }
    }

    if (!strcmp(action, "wifiJoin")) {
        if (_network) {
            const char* ssid = doc["ssid"] | "";
            const char* pass = doc["password"] | "";
            _network->startJoin(ssid, pass);
            _bleJoinPending = true;
            response = "{\"type\":\"ack\",\"action\":\"wifiJoin\",\"ok\":true,\"status\":\"connecting\"}";
            return true;
        }
    }

    if (!strcmp(action, "wifiStatus")) {
        if (_network) {
            StaticJsonDocument<256> st;
            st["type"]      = "wifiStatus";
            st["connected"] = _network->isConnected();
            st["isAp"]      = _network->isAp();
            st["ssid"]      = _network->ssid();
            st["ip"]        = _network->localIP();
            serializeJson(st, response);
            return true;
        }
    }


    if (!strcmp(action, "randomizeMac")) {
        LittleFS.remove("/mac.bin"); // deleted → new random MAC generated on next boot
        _pendingRestart = true;
        response = "{\"type\":\"ack\",\"action\":\"randomizeMac\",\"ok\":true}";
        return true;
    }

    return false;
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
    doc["ip"]             = _network ? _network->localIP() : WiFi.localIP().toString();
    doc["ssid"]           = _network ? _network->ssid() : WiFi.SSID();
    doc["wifiConnected"]  = _network ? _network->isConnected() : (WiFi.status() == WL_CONNECTED);
    doc["isAp"]           = _network ? _network->isAp() : false;
    doc["mac"]            = _network ? _network->macAddress() : WiFi.macAddress();

    JsonArray segs = doc["segments"].to<JsonArray>();
    uint16_t physOff = 0;
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_SEGMENTS; i++) {
        if (_cfg->segments[i].count == 0 && activeCount > 0) continue; // inactive after first
        JsonObject seg = segs.createNestedObject();
        seg["count"]     = _cfg->segments[i].count;
        seg["half"]      = _cfg->segments[i].half;
        seg["start"]     = physOff;
        seg["virtCount"] = _cfg->segments[i].half
            ? (_cfg->segments[i].count / 2) : _cfg->segments[i].count;
        physOff += _cfg->segments[i].count;
        activeCount++;
    }
    doc["dataPin"]        = _cfg->dataPin;
    doc["colorOrder"]     = _cfg->colorOrder;
    doc["chipset"]        = _cfg->chipset;
    doc["bleEnabled"]     = _cfg->bleEnabled;
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
