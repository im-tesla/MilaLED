# MilaLED

ESP8266-based Wi-Fi LED strip controller with a modern mobile-first web interface. Control your WS2815 (or compatible) LED strip from any device on your network — no app required.

**Works with:** WS2811, WS2812B, WS2815, WS2813, SK6812 | RGB, GRB, and more

<table align="center"><tr>
<td align="center" width="195">
  <img src="assets/screenshots/effects.png" width="180" alt="Effects tab" style="border-radius:24px;border:3px solid #27272a" /><br/>
  <sub>Effects</sub>
</td>
<td align="center" width="195">
  <img src="assets/screenshots/color.png" width="180" alt="Color tab" style="border-radius:24px;border:3px solid #27272a" /><br/>
  <sub>Color</sub>
</td>
<td align="center" width="195">
  <img src="assets/screenshots/presets.png" width="180" alt="Presets tab" style="border-radius:24px;border:3px solid #27272a" /><br/>
  <sub>Presets</sub>
</td>
<td align="center" width="195">
  <img src="assets/screenshots/settings.png" width="180" alt="Settings tab" style="border-radius:24px;border:3px solid #27272a" /><br/>
  <sub>Settings</sub>
</td>
</tr></table>

## Features

- **17 animated effects** — Rainbow, Fire, Comet, Ocean, Breathing, Strobe, and more
- **Philips Ambilight integration** — mirror your Philips TV's ambient lighting
- **Network scanner** — automatically discover Philips TVs on your LAN
- **Two-segment strip support** — configurable LED counts with half-density skipping
- **Configurable color order** — RGB, GRB, BRG — match whatever your strip expects
- **Presets** — save and recall your favorite setups
- **English / Polish** — auto-detected, toggleable
- **Dark & light theme**
- **No cloud, no app, no account** — self-contained on the ESP8266

## Hardware

| Component | Details |
|-----------|---------|
| **Board** | ESP8266 ESP-12E |
| **Strip** | WS2815 (WS2811/WS2812B/WS2813/SK6812 also supported) |
| **Data pin** | Configurable (GPIO 2, 4, 5, 12, 13, 14) |
| **Color order** | Configurable (RGB, RBG, GRB, GBR, BRG, BGR) |

Default setup: 120 + 58 physical LEDs across two segments, GPIO2 data pin, GRB color order.

## Getting Started

### 1. Set up PlatformIO

Install [PlatformIO](https://platformio.org/install) (VS Code extension or CLI):

```bash
pip install platformio
```

### 2. Build the web UI

```bash
python scripts/build_web.py
```

This runs `npm build` in `web/`, gzips the output, and places it in `data/`.

### 3. Flash the ESP8266

Connect your ESP8266 via USB, then:

```bash
# Flash the web files (LittleFS)
pio run --target uploadfs

# Flash the firmware
pio run --target upload
```

### 4. Connect

On first boot, the ESP8266 creates a Wi-Fi hotspot called **MilaLED**. Connect to it, open a browser, and you'll be guided through connecting it to your home network. Once connected, go to `http://milaled.local` (or the IP shown in settings).

## Development

```bash
# Web UI with hot-reload
cd web && npm run dev      # → http://localhost:5299

# Firmware
pio run                     # compile
pio run --target upload     # flash firmware only
pio device monitor          # serial output (115200 baud)
```

**Project structure:**

```
├── src/              # Arduino firmware
│   ├── config/       # ConfigStore (LittleFS JSON)
│   ├── leds/         # EffectsEngine + 17 effects
│   ├── net/          # HTTP + WebSocket server
│   └── wifi/         # WiFi Manager + mDNS
├── web/              # React 18 + Vite + Tailwind UI
│   └── src/
│       ├── components/  # tabs, layout, shared, ui
│       ├── hooks/       # useLedState, useWebSocket
│       └── i18n/        # English, Polish
├── data/             # gzipped web build (uploaded to ESP)
├── scripts/          # build_web.py
├── test/             # native unit tests
└── platformio.ini
```

## API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/ws` | WebSocket | Real-time control + state sync |
| `/api/strip` | POST | Save LED config (triggers reboot) |
| `/api/presets` | GET/POST/DELETE | Manage saved presets |
| `/api/ambilight/scan` | POST | Start network scan for Philips TVs |
| `/api/ambilight/scan/cancel` | POST | Cancel running scan |
| `/api/wifi/reset` | POST | Erase Wi-Fi credentials, restart in AP mode |

WebSocket commands are simple JSON: `{"power": true}`, `{"brightness": 180}`, `{"effect": "fire2012"}`, etc. The ESP broadcasts full state to all connected clients on connect and after any discrete change.

## License

MIT
