# MilaLED — Claude Code Reference

ESP8266 WS2815 LED strip controller with a React/Vite web UI. Controls 178 physical LEDs across two segments, serves a mobile-first interface from LittleFS over WiFi, and integrates with Philips TV Ambilight.

## Project layout

```
src/           — Arduino firmware (PlatformIO, esp12e board)
  main.cpp            — boot sequence, loop()
  config/             — ConfigStore (LittleFS JSON persistence)
  leds/               — EffectsEngine, PixelMapper, 17 effects
    effects/          — one .cpp per effect (included directly)
  net/                — MilaWebServer (ESP8266WebServer + WebSockets)
  wifi/               — NetworkManager (WiFiManager + mDNS)
  version.h           — MILALED_VERSION macro
web/           — React 18 + Vite + shadcn/ui + Tailwind CSS
  src/hooks/          — useLedState (WS-driven state), useWebSocket (throttled)
  src/components/     — layout/, tabs/, shared/, ui/ (shadcn)
  src/i18n/           — en.json, pl.json
scripts/       — build_web.py (npm build → gzip → data/)
data/          — gzipped LittleFS image (served by ESP)
test/          — native unit tests (PixelMapper)
platformio.ini — esp12e + native envs
```

## Build & flash

```bash
# Build web UI (React → gzip → data/)
python scripts/build_web.py

# Compile firmware
pio run                     # or: python -m platformio run

# Flash filesystem then firmware
pio run --target uploadfs
pio run --target upload

# Serial monitor
pio device monitor
```

**Web dev server (hot-reload):** `cd web && npm run dev` → Vite on port 5299.

## Architecture notes

### Strip config requires reboot
`POST /api/strip` saves segment counts, density toggles, data pin, color order, and chipset to `config.json`, then sets `_pendingRestart = true`. `loop()` calls `ESP.restart()` on the next iteration. These params are used in `EffectsEngine::begin()` to allocate LED arrays and pick FastLED controller — that's why a reboot is needed.

### FastLED template constraint
`FastLED.addLeds<CHIP, PIN, ORDER>()` requires compile-time template params. The `begin()` switch covers the full cross-product of (chipset × color order × data pin) via macros (`LED_PIN_CASES`, `ORDER_CASES`). Adding a new GPIO pin, chipset, or color order means adding a case to the macros in `src/leds/EffectsEngine.cpp`.

### WebSocket message split
The `handleWsMessage()` handler in WebServer.cpp classifies params as:
- **Continuous** (brightness, speed, intensity, colors, ambPollMs): applied to LEDs immediately, NOT saved to flash, NOT echoed back (prevents slider jitter).
- **Discrete** (power, effect, palette, tvIp, ambMapping): saved to flash + broadcast to all clients.

Periodic config save runs every 30s in `main.cpp` to catch continuous changes.

### Virtual pixel map
`PixelMapper` maps a virtual LED index (what effects render to) to a physical strip position. Half-density modes (`segAHalf`, `segBHalf`) skip every other physical LED. The skipped LEDs are set to `CRGB::Black` in `flushVirtualToPhysical()`. Virtual count = `segACount/(segAHalf?2:1) + segBCount/(segBHalf?2:1)`.

### Ambilight scan (non-blocking)
The TV scanner runs as a state machine in `loop()`: one IP per iteration. Cancel sets `_scanCancel = true` and the main loop picks it up immediately. Pre-connects `WiFiClient` before `HTTPClient.begin()` so dead IPs don't stall on lwIP TCP SYN retransmit.

### JSON doc sizes
ArduinoJson documents have fixed capacity. Key limits in the codebase:
- `/api/strip` POST: `StaticJsonDocument<256>`
- WebSocket messages: `StaticJsonDocument<256>` (rx), `StaticJsonDocument<768>` (tx state)
- Ambilight TV JSON: `StaticJsonDocument<2048>` (stream-parsed)
- Presets list: `StaticJsonDocument<2048>`

## i18n
English and Polish. Strings live in `web/src/i18n/{en,pl}.json`. Effect names and palette names are not translated (they're identifiers). The `useTranslation()` hook from react-i18next is used throughout. Language persists in `localStorage('lang')`.

## Theme
Dark mode by default, light mode toggle in Header. Uses shadcn HSL CSS variables. Light mode remaps hardcoded dark Tailwind classes (like `bg-zinc-900`, `text-zinc-100`) via specificity rules in `web/src/index.css`.

## Hardware
- Board: ESP8266 esp12e, 80KB RAM, 1MB flash
- Strip: WS2815 (configurable to WS2811/WS2812B/WS2813/SK6812)
- Data pin: configurable GPIO 2/4/5/12/13/14
- Color order: configurable RGB/RBG/GRB/GBR/BRG/BGR
