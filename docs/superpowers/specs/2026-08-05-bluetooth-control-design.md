# Bluetooth Control (ESP32) — Design

## Context

MilaLED currently ships one control surface: a React web UI served from LittleFS over WiFi, driven by a WebSocket (state push + continuous/discrete param writes) plus a REST API (presets, strip config + reboot, Ambilight TV scan, WiFi reset). ESP8266 boards have no Bluetooth hardware and are out of scope entirely.

This adds a second, always-on control channel over Bluetooth Low Energy on ESP32 targets (`esp32dev`, `nodemcu-32s`, `esp32-s3-devkitc-1`, `esp32-c6-devkitc-1`, `esp32-c3-supermini`), paired with a statically-hosted copy of the same web UI (GitHub Pages, built via GitHub Actions) that talks to the device directly over Web Bluetooth instead of WiFi — useful when a phone isn't/can't be joined to the strip's WiFi network.

## Goals

- Same UI (1:1 component reuse), reached either by visiting the device's own WiFi-served page (unchanged) or a GitHub Pages URL that connects over BLE via a connect dialog.
- BLE and WiFi control run concurrently on the device — BLE is not a fallback or provisioning-only mode.
- v1 scope is core live control: power, brightness, effect, speed, intensity, colorPrimary, colorSecondary, palette, and segment display. Presets, strip reconfiguration (pin/chipset/segments+reboot), Ambilight TV scan, and WiFi reset are **not** exposed over BLE in v1 — those tabs/actions are hidden in the BLE build.

## Non-goals

- BLE pairing/bonding or encryption beyond what NimBLE defaults provide — matches the existing WiFi surface's security model (no auth), not a hardening pass.
- ESP8266 support — physically impossible, no BLE radio.
- Any attempt to have the static GitHub Pages build try WiFi first — an https static page cannot reach `http://milaled.local` (mixed content is blocked by browsers), so BLE is the only path for that build.

## Architecture

### Firmware (ESP32-only)

**New files:** `src/net/BleServer.h`, `src/net/BleServer.cpp` — entirely wrapped in `#ifdef ESP32`, never compiled into ESP8266 envs.

**Library:** `h2zero/NimBLE-Arduino`, added to `lib_deps` for the five ESP32 envs only (not touched for `esp12e`/`nodemcuv2`/`d1_mini`). Chosen over the stock Arduino `BLEDevice` (Bluedroid) because it uses roughly a third of the RAM — material on the ESP32-C3-supermini, which already runs WiFi + FastLED + WebServer.

**Config:** one new field on `Config` (`config/ConfigStore.h`):
```cpp
bool bleEnabled = true;
```
Persisted/loaded like other fields. Default on, since BLE is meant to be an always-available second channel, but left as a toggle for anyone who wants the radio off.

**GATT service** (custom 128-bit UUID, generated once and hardcoded):
- **Command characteristic** (write, no-response) — accepts the same JSON shape `handleWsMessage` already parses, restricted to core fields: `power`, `brightness`, `effect`, `speed`, `intensity`, `colorPrimary`, `colorSecondary`, `palette`.
- **State characteristic** (notify) — JSON with `type:"state"` plus: `power`, `brightness`, `effect`, `speed`, `intensity`, `colorPrimary`, `colorSecondary`, `palette`, `virtualLeds`, `segments` (count/half/virtCount/start, read-only display), `version`. No WiFi/tvIp/ambilight/dataPin/chipset/colorOrder fields — those belong to hidden tabs.

**Shared command logic (small refactor):** `handleWsMessage` ([WebServer.cpp:335-368](src/net/WebServer.cpp:335)) currently mixes JSON parsing with the continuous-vs-discrete apply/save/broadcast rules. Extract the apply-rules block into a shared function (e.g. `applyCoreParams(Config&, EffectsEngine&, const JsonDocument&) -> {anyChanged, discreteChanged}`) that both `WebServer::handleWsMessage` and `BleServer`'s command handler call. `WebServer` keeps its own save+broadcast around the call (since it also handles WS-only fields like `tvIp`/`ambMapping`); `BleServer` calls it, then does its own save + BLE-notify. This avoids the two channels' param rules drifting apart, and is a direct consequence of adding a second caller — not a speculative cleanup.

**Chunking (required, not optional):** ATT payloads are small (20–247 bytes depending on negotiated MTU), and state JSON can run past that. Both characteristics frame their payload with a 2-byte header (`uint8_t seq`, `uint8_t more`) prepended to each write/notify chunk. The peripheral requests MTU 247 on connect but does not assume the central grants it — chunking works regardless. Reassembly buffer is bounded (matches the `StaticJsonDocument<768>` tx budget already used elsewhere in the codebase).

**Wiring:** `main.cpp`, right after `webServer.begin(...)`:
```cpp
#ifdef ESP32
if (cfg.bleEnabled) bleServer.begin(&cfg, &cfgStore, &engine);
#endif
```
and `bleServer.loop()` (if NimBLE needs polling) added to the main `loop()` next to `webServer.loop()`.

### Frontend (`web/`)

**Transport abstraction:** `useWebSocket` already has a transport-shaped signature (`(url, onMessage) => {status, send}`, [useWebSocket.ts:7-10](web/src/hooks/useWebSocket.ts:7)) including per-key throttling ([useWebSocket.ts:34-71](web/src/hooks/useWebSocket.ts:34)). Extract the throttling/flush logic into a small shared helper parameterized over a raw `sendRaw(json: string)` function, so both `useWebSocket` and the new `useBluetoothTransport` reuse identical throttle behavior instead of duplicating it.

**New file:** `web/src/hooks/useBluetoothTransport.ts` — same return shape as `useWebSocket` (`{status, send}`), same `onMessage` callback contract. Internally: `navigator.bluetooth.requestDevice({filters:[{services:[SERVICE_UUID]}]})`, connect GATT, subscribe to the state characteristic, reassemble frames via the seq/more header (mirrors the firmware framing), write to the command characteristic through the shared throttle helper. On `gattserverdisconnected`, sets `status: 'closed'` — no auto-reconnect attempt (BLE reconnection needs a fresh user-gesture-backed `requestDevice()` picker in most browsers, unlike WS).

**`useLedState.ts`** ([useLedState.ts:61](web/src/hooks/useLedState.ts:61)): picks `useWebSocket(wsUrl, onMessage)` vs `useBluetoothTransport(onMessage)` based on a build-time constant (`import.meta.env.VITE_TRANSPORT === 'ble'`). Everything downstream (`state`, `update`, `status`) is unchanged for consumers.

**Connect dialog:** new component shown at app root only when `VITE_TRANSPORT === 'ble'`, gating the rest of the UI until `status === 'open'`. A button-triggered `requestDevice()` call (Web Bluetooth requires a user gesture), with states for "no Web Bluetooth support" (informs the user to use Chrome/Android or Bluefy/iOS), "picker cancelled", and "connect failed."

**Feature gating:** a `transportCapabilities` object (`{ presets, stripConfig, ambilight, wifiReset }`, all `true` for WiFi build / all `false` for BLE build) read once from the same build-time constant, consumed by the relevant tab components to hide themselves rather than scattering conditionals through the tree.

**Build:** `web/vite.config.ts` gains a second mode; `npm run build:ble` sets `VITE_TRANSPORT=ble`. `scripts/build_web.py` (the firmware-embedding pipeline) is untouched — it keeps using the default WiFi build.

### Deployment

New `.github/workflows/deploy-ble-pages.yml`: on push to `master` touching `web/**`, runs `npm ci && npm run build:ble` in `web/`, deploys `web/dist` via `actions/deploy-pages`. Fully independent of the PlatformIO firmware build.

## Error handling

- **Disconnect:** BLE central disconnect → `status:'closed'` → connect dialog reappears; no silent stale UI.
- **Truncated frame train:** a new `seq:0` chunk always resets the reassembly buffer, so a dropped final chunk can't produce a corrupt concatenation — it's discarded and replaced by the next full train.
- **Unsupported browser:** connect dialog checks `!!navigator.bluetooth` up front and shows a clear message (Chrome/Android or Bluefy/iOS) instead of a cryptic `requestDevice is not a function`.
- **BLE disabled on device:** if `cfg.bleEnabled` is false, the peripheral simply never advertises; the web connect dialog's picker will show no matching device, which is an acceptable (if not maximally friendly) v1 error state.

## Testing

- **Firmware:** manual verification — connect via nRF Connect (or a scratch HTML page) before the real frontend exists; confirm a state payload larger than one ATT chunk (e.g. with multiple active segments) reassembles correctly on the client.
- **Frontend:** unit tests for the transport interface against a mock (matching whatever pattern, if any, currently covers `useWebSocket`); manual test of the connect dialog and full control flow on Chrome/Android and Bluefy/iOS specifically, since Bluefy is a third-party WebKit wrapper with its own quirks.
- No new native (`pio test`) coverage — this feature isn't pure-logic like `PixelMapper`.

## Open items deferred past v1

- Presets / strip reconfig / Ambilight scan / WiFi reset over BLE.
- BLE pairing/bonding.
- Auto-reconnect without a fresh user gesture (browser-dependent; revisit if it becomes annoying in practice).
