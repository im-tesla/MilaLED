# MilaLED Design Spec
**Date:** 2026-06-15
**Status:** Approved

---

## Overview

MilaLED is a WLED-inspired WS2815 LED strip controller running on an ESP8266 (esp12e). It serves a mobile-first web UI from LittleFS, controls LEDs via FastLED, and communicates in real time over WebSocket. The UI is built with React 18 + shadcn/ui + Tailwind CSS and supports both English and Polish.

---

## 1. Hardware

- **Board:** ESP8266 esp12e
- **LED strip:** WS2815, two segments wired in series
  - Segment A: 60 LED/m, 120 physical LEDs
  - Segment B: 30 LED/m, 58 physical LEDs
  - Total physical LEDs: 178
- **Data pin:** configurable (default GPIO2 / D4), stored in config

---

## 2. Firmware Architecture

### 2.1 File Structure

```
src/
├── main.cpp                     # Boot sequence, module wiring, loop()
├── wifi/
│   └── NetworkManager.h/cpp     # Wraps tzapu/WiFiManager: AP+STA, captive portal, mDNS
├── leds/
│   ├── EffectsEngine.h/cpp      # Tick loop, virtual→physical pixel mapping
│   └── effects/
│       ├── EffectBase.h         # Abstract base: tick(), getName(), getParams()
│       ├── SolidEffect.cpp
│       ├── RainbowEffect.cpp
│       ├── CometEffect.cpp
│       ├── CylonEffect.cpp
│       ├── TheaterChaseEffect.cpp
│       ├── RunningLightsEffect.cpp
│       ├── FireEffect.cpp
│       ├── LavaEffect.cpp
│       ├── OceanEffect.cpp
│       ├── TwinkleEffect.cpp
│       ├── MeteorRainEffect.cpp
│       ├── SparkleEffect.cpp
│       ├── BreathingEffect.cpp
│       ├── StrobeEffect.cpp
│       ├── PerlinFlowEffect.cpp
│       ├── ColorTempEffect.cpp
│       └── AmbilightEffect.cpp  # Philips TV Ambilight integration
├── config/
│   └── ConfigStore.h/cpp        # LittleFS JSON — state, presets, ambilight config
└── net/
    └── WebServer.h/cpp          # ESP8266WebServer + WebSocketsServer, REST /api/*
data/                            # LittleFS image (Vite build output, gzipped)
web/                             # React + Vite + shadcn source (not uploaded to ESP)
docs/
└── superpowers/specs/           # Design documents
```

### 2.2 Boot Sequence

1. `ConfigStore` loads `config.json` from LittleFS (seeds defaults on first boot)
2. `EffectsEngine` initialises FastLED with `CRGB leds[178]` and builds the virtual pixel map
3. `NetworkManager` (wrapping tzapu/WiFiManager) attempts STA connection; falls back to AP (`MilaLED` SSID, no password) after 10 seconds
4. `WebServer` starts HTTP on port 80, WebSocket on `/ws`, mDNS as `milaled.local`
5. `loop()` calls `EffectsEngine::tick()` every frame and `WebServer::loop()`

### 2.3 Loop Constraint

ESP8266 is single-threaded. Every effect `tick()` must complete in under 10ms to keep WebSocket responsive. `AmbilightEffect` calls `ESP8266HTTPClient::GET()` synchronously but limits calls to once per poll interval (default 100ms) with a 500ms connection timeout, keeping worst-case loop delay bounded.

---

## 3. Strip Configuration and Virtual Pixel Mapping

### 3.1 Physical Layout

```
[Segment A: phys[0..119] — 60 LED/m, 120 LEDs] → [Segment B: phys[120..177] — 30 LED/m, 58 LEDs]
```

### 3.2 Virtual Pixel Map (default, skip mode on Segment A)

118 virtual pixels are the coordinate system all effects write to.

```
virtual[0]   → phys[0]    (Segment A — skip phys[1])
virtual[1]   → phys[2]    (Segment A — skip phys[3])
...
virtual[59]  → phys[118]  (Segment A — skip phys[119])
virtual[60]  → phys[120]  (Segment B — 1:1)
...
virtual[117] → phys[177]  (Segment B — 1:1)
```

All skipped Segment A LEDs (`phys[1,3,5...119]`) are set to `CRGB::Black` every frame after `mapToPhysical()`.

### 3.3 Segment A Density Toggle

`segA_density` in config: `"half"` (default, skip every other) or `"full"` (1:1, all 120 active).

In `"full"` mode, virtual pixel count becomes 120 + 58 = 178. Recalculated on save, no reflash needed.

### 3.4 Config Fields

```json
{
  "segA_leds": 120,
  "segA_density": "half",
  "segB_leds": 58,
  "virtualLeds": 118,
  "dataPin": 2
}
```

---

## 4. Effects Library

All 17 effects write to `virt[N]`. Universal parameters: **Speed** (0–255), **Intensity** (0–255), **Palette/Color**.

| Category | Effect | Intensity meaning |
|---|---|---|
| Static | Solid Color | — |
| Static | Color Temperature | Warm to cool white blend |
| Chase | Rainbow Cycle | Rainbow band width |
| Chase | Theater Chase | Gap between lit LEDs |
| Chase | Comet | Tail length |
| Chase | Cylon | Eye width |
| Chase | Running Lights | Blur amount |
| Fire/Nature | Fire 2012 | Spark rate |
| Fire/Nature | Lava | Color drift speed |
| Fire/Nature | Ocean | Wave amplitude |
| Twinkle | Twinkle | Number of lit stars |
| Twinkle | Meteor Rain | Meteor count |
| Twinkle | Sparkle | Spark density |
| Pulse | Breathing | Hold time at peak |
| Pulse | Strobe | Duty cycle |
| Noise | Perlin Flow | Noise scale |
| Ambilight | Ambilight (Philips TV) | See Section 6 |

**FastLED palettes available for all non-Ambilight effects:**
`RainbowColors`, `PartyColors`, `HeatColors`, `OceanColors`, `CloudColors`, `LavaColors`, `ForestColors`, plus a custom user palette set via the Color tab.

Effect parameters (speed, intensity, palette, color) are persisted per effect in `config.json`, so switching presets restores exact state.

---

## 5. API Design

### 5.1 WebSocket `/ws` — Real-time Control

**Client to ESP (commands):**
```json
{ "type": "power",        "value": true }
{ "type": "brightness",   "value": 180 }
{ "type": "effect",       "id": "fire2012" }
{ "type": "speed",        "value": 200 }
{ "type": "intensity",    "value": 120 }
{ "type": "color",        "primary": "#FF4500", "secondary": "#000080" }
{ "type": "palette",      "name": "HeatColors" }
{ "type": "preset_load",  "id": "movie-night" }
{ "type": "ambilight_scan_start" }
```

**ESP to client (state push on connect and after any change):**
```json
{
  "power": true,
  "brightness": 180,
  "effect": "fire2012",
  "speed": 200,
  "intensity": 120,
  "color": { "primary": "#FF4500", "secondary": "#000080" },
  "palette": "HeatColors",
  "virtualLeds": 118,
  "ip": "192.168.1.42",
  "ssid": "HomeNetwork",
  "ambilight": { "tvIp": "192.168.1.10", "status": "polling", "mappingMode": "right" }
}
```

**Ambilight scan progress (broadcast to all clients during scan):**
```json
{ "type": "ambilight_scan_progress", "scanned": 45, "total": 254, "found": [{ "ip": "192.168.1.10", "name": "Philips TV" }] }
{ "type": "ambilight_scan_done", "found": [{ "ip": "192.168.1.10" }] }
```

Multiple browser tabs stay in sync — ESP broadcasts state to all connected clients on every change.

### 5.2 REST `/api` — Config and Preset Management

```
GET    /api/presets          list all saved presets
POST   /api/presets          save current state as named preset
DELETE /api/presets/:id      delete preset
GET    /api/config           full device config (LED counts, segment settings, ambilight)
POST   /api/config           update config (triggers LED reinit if LED params changed)
POST   /api/ota              trigger OTA firmware update check
GET    /api/info             firmware version, free heap, uptime
```

### 5.3 Captive Portal

On first boot (AP mode), any HTTP request redirects to the WiFi setup page where the user enters home network credentials. After save, ESP reboots into STA mode. Falls back to AP if STA connection fails.

---

## 6. Philips TV Ambilight Integration

### 6.1 Discovery

Two methods, both available in the Settings tab:

**Manual entry:** User types the TV's IP address directly. Saved to config immediately.

**Network scan:** Triggered by the "Scan network" button. The ESP8266 iterates over all IPs in its local /24 subnet, sending `GET http://<ip>/ambilight/processed` with a 150ms timeout per IP. As each Philips TV is found, the ESP sends a `ambilight_scan_progress` WebSocket event so the UI updates live. Results are saved to config when scan completes.

### 6.2 TV Response Format

```json
{
  "layer1": {
    "left":  { "0": {"r":60,"g":61,"b":29}, "1": {"r":74,"g":67,"b":33}, "2": ..., "3": ... },
    "top":   { "0": ..., "1": ..., "2": ..., "3": ..., "4": ..., "5": ..., "6": ..., "7": ... },
    "right": { "0": ..., "1": ..., "2": ..., "3": ... }
  }
}
```

Zones: left = 4 zones, top = 8 zones, right = 4 zones.

### 6.3 Effect Loop

When `AmbilightEffect` is active:

```
every <pollInterval>ms (configurable 50–200ms, default 100ms):
  GET http://<tvIp>/ambilight/processed
  if response ok:
    parse JSON with ArduinoJson
    apply mappingMode → write to virt[0..N-1]
    status = "polling"
  else:
    status = "disconnected"
    hold last frame (or fade to black after 3 failed polls)
```

HTTP polling uses `ESP8266HTTPClient` with a 500ms connection timeout, called at most once per poll interval via a `millis()` timer. The synchronous call holds the loop for up to 500ms on failure, which is acceptable since WebSocket keepalives tolerate brief pauses. On the happy path (local LAN, TV responding) the call typically completes in under 20ms.

### 6.4 Mapping Modes

| Mode | Behavior |
|---|---|
| `proportional` | left[0..3] → virt[0..29], top[0..7] → virt[30..87], right[0..3] → virt[88..117] — for strips wrapping around the TV |
| `left` | left[0..3] interpolated across virt[0..117] — strip mounted on the left of the TV |
| `top` | top[0..7] interpolated across virt[0..117] — strip mounted above the TV |
| `right` | right[0..3] interpolated across virt[0..117] — strip mounted on the right of the TV |
| `average` | average of all zones → solid color across entire strip |

Interpolation between zones uses linear RGB blending to avoid harsh color steps across the strip.

### 6.5 Ambilight Config Fields

```json
{
  "ambilight": {
    "tvIp": "192.168.1.10",
    "pollInterval": 100,
    "mappingMode": "right"
  }
}
```

### 6.6 UI

- Ambilight appears as a distinct effect card in the Effects tab (TV icon, no speed/intensity sliders; shows TV status instead)
- When active, the effect card shows: TV IP, connection status dot (green = polling, red = disconnected), mapping mode selector (5 options)
- Settings tab has an "Ambilight" section: IP input field, "Scan network" button with live scan progress bar, poll interval slider

---

## 7. Web UI

### 7.1 Tech Stack

| Tool | Version | Purpose |
|---|---|---|
| React | 18 | UI framework |
| Vite | 5 | Build tool, outputs to `data/` |
| shadcn/ui | latest | Component library (owned code) |
| Tailwind CSS | v3 | Utility styling |
| react-colorful | ~5.6 | Color wheel picker (~2KB gzipped) |
| react-i18next + i18next | latest | EN/PL i18n |
| Motion (motion/react) | latest | Tab transitions, spring animations |
| Phosphor Icons | latest | Icon library |

Build pipeline: `npm run build` in `web/` → Vite compresses output → PlatformIO `extra_scripts` copies gzipped files to `data/` → `pio run --target uploadfs` pushes to LittleFS.

### 7.2 Design Language

- **Background:** zinc-950 (`#09090b`)
- **Surface:** zinc-900 (`#18181b`)
- **Border:** zinc-800 (`#27272a`)
- **Primary text:** white
- **Muted text:** zinc-500 (`#71717a`)
- **Accent:** amber-400 (`#fbbf24`) — active states, sliders, power toggle, active tab
- **Font:** Geist Sans (self-hosted via `@font-face`)
- **Icons:** Phosphor Icons, `strokeWidth={1.75}`, one family throughout
- **Border radius:** 12–14px cards, 20px pills, 50% circles — consistent scale
- **Touch targets:** 44px minimum
- **Min font size:** 14px (12px for secondary labels only)
- **Light mode:** full shadcn light theme, toggled in Settings, stored in localStorage
- **Animations:** Motion spring physics for tab transitions, scale feedback on tap; all gated behind `prefers-reduced-motion`

### 7.3 Component Structure

```
web/src/
├── App.tsx                       # Root: theme provider, WS context, tab router
├── i18n/
│   ├── en.json                   # English strings
│   └── pl.json                   # Polish strings
├── hooks/
│   ├── useWebSocket.ts           # Connection, reconnect, message dispatch
│   └── useLedState.ts            # Global LED state from WS messages
├── components/
│   ├── layout/
│   │   ├── Header.tsx            # App name, connection dot, power toggle
│   │   ├── BrightnessBar.tsx     # Always-visible brightness slider
│   │   └── TabBar.tsx            # Bottom navigation, 4 tabs
│   ├── tabs/
│   │   ├── EffectsTab.tsx        # Active effect card + category pills + effect grid
│   │   ├── ColorTab.tsx          # react-colorful wheel + swatches + palette list
│   │   ├── PresetsTab.tsx        # Preset cards + save button
│   │   └── SettingsTab.tsx       # Strip config, WiFi, Ambilight, OTA, theme, language
│   └── shared/
│       ├── EffectCard.tsx        # Individual effect card (FastLED or Ambilight variant)
│       ├── PresetCard.tsx        # Preset card with long-press-to-delete
│       ├── ParamSlider.tsx       # Speed/Intensity slider (shadcn Slider)
│       ├── PaletteRow.tsx        # Palette selector with gradient previews
│       └── AmbilightStatus.tsx   # TV connection status + mapping mode picker
```

### 7.4 Tab Breakdown

**Effects tab:**
- Persistent header + brightness slider always visible
- Active effect card: effect name, category, speed slider, intensity slider, ACTIVE badge (amber)
- Category filter pills: All / Chase / Fire / Twinkle / Pulse / Noise / Ambilight
- Scrollable 2-column grid of all 17 effects; tap to activate instantly via WebSocket

**Color tab:**
- react-colorful HSV color wheel
- Hex input field (shadcn Input)
- Primary and Secondary color swatches
- Palette list with full-width gradient preview bars + name

**Presets tab:**
- List of saved presets showing effect name, brightness, palette
- Tap to load (sends `preset_load` via WebSocket)
- Long-press (500ms) to delete with confirmation
- "Save current state" button at bottom with name input dialog (shadcn Dialog)

**Settings tab (sections):**
- Strip: Segment A LED count, Segment A density toggle (Half/Full), Segment B LED count, computed virtual LED count (read-only), data pin
- Ambilight: TV IP input, "Scan network" button + live progress, poll interval slider, mapping mode selector
- Network: current SSID, IP address, mDNS hostname, "Change WiFi" button (opens captive portal)
- System: OTA update button, firmware version, free heap (read-only)
- Appearance: theme toggle (Light/Dark), language selector (English/Polski)

### 7.5 i18n

Library: `react-i18next`. Language stored in `localStorage`. Toggle in Settings tab.

Translated: all UI chrome — tab names, labels, button text, status messages, error copy, WiFi setup, ambilight section.

Not translated: FastLED effect names (`Fire 2012`, `Rainbow Cycle`, etc.), palette names (`HeatColors`, `PartyColors`), and preset names (user-defined).

Polish translation examples:
```
tabs.effects     → Efekty
tabs.color       → Kolor
tabs.presets     → Presety
tabs.settings    → Ustawienia
ui.brightness    → Jasność
ui.speed         → Prędkość
ui.intensity     → Intensywność
ui.active        → AKTYWNY
ui.power         → Zasilanie
settings.segA    → Segment A
settings.density → Gęstość
settings.half    → Połowa (co drugi)
settings.full    → Pełna
ambilight.scan   → Skanuj sieć
ambilight.tvIp   → Adres IP telewizora
ambilight.mapping → Tryb mapowania
ambilight.left   → Lewa strona
ambilight.top    → Górna strona
ambilight.right  → Prawa strona
ambilight.proportional → Proporcjonalnie
ambilight.average → Uśredniony kolor
ambilight.connected   → Połączono
ambilight.disconnected → Rozłączono
```

---

## 8. Build Pipeline

```
web/          ← npm run build (Vite)
    ↓
data/         ← gzipped HTML, JS, CSS (served from LittleFS)
    ↓
platformio.ini extra_scripts: pre:scripts/build_web.py
    ↓
pio run --target uploadfs   ← uploads data/ to LittleFS
pio run --target upload     ← uploads firmware
```

`build_web.py` script: runs `npm run build` in `web/`, gzips the output, copies to `data/`.

---

## 9. PlatformIO Dependencies

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino
board_build.filesystem = littlefs
lib_deps =
  fastled/FastLED
  links2004/WebSockets
  bblanchon/ArduinoJson
  tzapu/WiFiManager
monitor_speed = 115200
```

---

## 10. Out of Scope

- MQTT
- Schedules / timers
- Multi-device sync
- Music reactivity
- Native mobile app
