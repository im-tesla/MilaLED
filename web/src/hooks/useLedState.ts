import { useState, useCallback } from 'react'
import { useWebSocket } from './useWebSocket'
import { useBluetoothTransport } from './useBluetoothTransport'
import { TRANSPORT } from '@/lib/capabilities'

export interface SegmentData {
  count: number
  half: boolean
  virtCount?: number  // computed by firmware, sent in state
  start?: number      // physical offset
}

export interface PresetData {
  name: string
  effect: string
  brightness: number
  palette: string
}

export interface WifiNetwork {
  ssid: string
  rssi: number
  secure: boolean
}

export interface WifiJoinStatus {
  status: 'idle' | 'connecting' | 'connected' | 'failed'
  ssid?: string
  ip?: string
  error?: string
}

export interface LedState {
  power: boolean
  brightness: number
  effect: string
  speed: number
  intensity: number
  colorPrimary: string
  colorSecondary: string
  palette: string
  virtualLeds: number
  ip: string
  ssid: string
  wifiConnected?: boolean
  isAp?: boolean
  segments: SegmentData[]
  dataPin: number
  colorOrder: number
  chipset: number
  bleEnabled: boolean
  version: string
  tvIp: string
  ambPollMs: number
  ambMapping: string
  ambStatus?: string
}

const DEFAULT: LedState = {
  power: true,
  brightness: 180,
  effect: 'rainbow',
  speed: 128,
  intensity: 128,
  colorPrimary: '#FF4500',
  colorSecondary: '#000080',
  palette: 'RainbowColors',
  virtualLeds: 120,
  ip: '',
  ssid: '',
  wifiConnected: false,
  isAp: false,
  segments: [
    { count: 120, half: false },
    { count: 0, half: false },
    { count: 0, half: false },
    { count: 0, half: false },
  ],
  dataPin: 2,
  colorOrder: 2,
  chipset: 2,
  bleEnabled: true,
  version: '',
  tvIp: '',
  ambPollMs: 100,
  ambMapping: 'right',
}

export function useLedState(wsUrl: string) {
  const [state, setState] = useState<LedState>(DEFAULT)
  const [presets, setPresets] = useState<PresetData[]>([])
  const [scanProgress, setScanProgress] = useState<{ pct: number; msg: string } | null>(null)
  const [foundTvs, setFoundTvs] = useState<string[]>([])
  const [wifiNetworks, setWifiNetworks] = useState<WifiNetwork[]>([])
  const [wifiScanning, setWifiScanning] = useState(false)
  const [wifiJoinStatus, setWifiJoinStatus] = useState<WifiJoinStatus>({ status: 'idle' })

  const onMessage = useCallback((data: unknown) => {
    const d = data as Record<string, unknown>
    if (d.type === 'state') {
      setState(s => ({ ...s, ...(d as Partial<LedState>) }))
    } else if (d.type === 'presets') {
      setPresets((d.items as PresetData[]) || [])
    } else if (d.type === 'scanProgress') {
      setScanProgress({ pct: d.pct as number, msg: d.msg as string })
      if ((d.pct as number) >= 100) setScanProgress(null)
    } else if (d.type === 'ambilightFound') {
      setFoundTvs(tvs => [...tvs, d.ip as string])
    } else if (d.type === 'wifiScan') {
      setWifiNetworks((d.networks as WifiNetwork[]) || [])
      setWifiScanning(false)
    } else if (d.type === 'wifiJoinResult') {
      const res = d as unknown as WifiJoinStatus
      setWifiJoinStatus(res)
      if (res.status === 'connected' && res.ssid) {
        setState(s => ({ ...s, ssid: res.ssid || s.ssid, ip: res.ip || s.ip, wifiConnected: true, isAp: false }))
      }
    }
  }, [])

  /* eslint-disable react-hooks/rules-of-hooks -- TRANSPORT is a build-time
     constant (Vite inlines import.meta.env and dead-code-eliminates the
     unused branch), so exactly one of these two hooks ever actually runs
     for a given bundle. A block disable/enable pair is used instead of
     eslint-disable-next-line because the ternary's hook calls span multiple
     lines, which a single-line directive does not cover. */
  const { send, sendImmediate, status, connect, error } = TRANSPORT === 'ble'
    ? useBluetoothTransport(onMessage)
    : { ...useWebSocket(wsUrl, onMessage), sendImmediate: null as ((data: object) => void) | null, error: null as string | null }
  /* eslint-enable react-hooks/rules-of-hooks */

  const sendCommand = sendImmediate || send

  const scanWifi = useCallback(() => {
    setWifiScanning(true)
    if (TRANSPORT === 'ble') {
      sendCommand({ action: 'wifiScan' })
      // Safety timeout in case a BLE notification is dropped
      setTimeout(() => {
        setWifiScanning(scanning => (scanning ? false : false))
      }, 10000)
    } else {
      fetch('/api/wifi/scan', { method: 'POST' })
        .catch(() => {})
        .finally(() => {
          let attempts = 0
          const check = () => {
            fetch('/api/wifi/scan')
              .then(r => r.json())
              .then(data => {
                if (data.scanning && attempts < 12) {
                  attempts++
                  setTimeout(check, 600)
                } else {
                  setWifiNetworks(data.networks || [])
                  setWifiScanning(false)
                }
              })
              .catch(() => setWifiScanning(false))
          }
          setTimeout(check, 800)
        })
    }
  }, [sendCommand])

  const joinWifi = useCallback((ssid: string, password: string) => {
    setWifiJoinStatus({ status: 'connecting', ssid })
    if (TRANSPORT === 'ble') {
      sendCommand({ action: 'wifiJoin', ssid, password })
    } else {
      fetch('/api/wifi/join', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid, password }),
      })
        .then(r => r.json())
        .then(() => {
          let attempts = 0
          const poll = () => {
            fetch('/api/wifi/status')
              .then(r => r.json())
              .then(st => {
                if (st.status === 'connecting' && attempts < 15) {
                  attempts++
                  setTimeout(poll, 1000)
                } else if (st.status === 'connected') {
                  setWifiJoinStatus({ status: 'connected', ssid: st.ssid, ip: st.ip })
                  setState(s => ({ ...s, ssid: st.ssid, ip: st.ip, wifiConnected: true, isAp: false }))
                } else if (st.status === 'failed') {
                  setWifiJoinStatus({ status: 'failed', error: st.error || 'Connection failed' })
                }
              })
              .catch(() => {
                // If device switched channels/networks, connection might be interrupted
              })
          }
          setTimeout(poll, 2000)
        })
        .catch(err => {
          setWifiJoinStatus({ status: 'failed', error: err.message || 'Request failed' })
        })
    }
  }, [sendCommand])

  const clearWifiJoinStatus = useCallback(() => {
    setWifiJoinStatus({ status: 'idle' })
  }, [])

  const update = useCallback((patch: Partial<LedState>) => {
    setState(s => ({ ...s, ...patch }))
    send(patch)
  }, [send])

  return {
    state,
    update,
    status,
    scanProgress,
    foundTvs,
    presets,
    wifiNetworks,
    wifiScanning,
    wifiJoinStatus,
    scanWifi,
    joinWifi,
    clearWifiJoinStatus,
    send,
    sendCommand,
    connect,
    error,
  }
}
