export interface TransportCapabilities {
  presets: boolean
  stripConfig: boolean
  ambilight: boolean
  wifiReset: boolean
}

const isBle = import.meta.env.VITE_TRANSPORT === 'ble'

export const capabilities: TransportCapabilities = isBle
  ? { presets: true, stripConfig: true, ambilight: true, wifiReset: true }
  : { presets: true, stripConfig: true, ambilight: true, wifiReset: true }

export const TRANSPORT: 'wifi' | 'ble' = isBle ? 'ble' : 'wifi'
