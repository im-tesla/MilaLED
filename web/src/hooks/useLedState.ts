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
  segments: SegmentData[]
  dataPin: number
  colorOrder: number
  chipset: number
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
  segments: [
    { count: 120, half: false },
    { count: 0, half: false },
    { count: 0, half: false },
    { count: 0, half: false },
  ],
  dataPin: 2,
  colorOrder: 2,
  chipset: 2,
  version: '',
  tvIp: '',
  ambPollMs: 100,
  ambMapping: 'right',
}

export function useLedState(wsUrl: string) {
  const [state, setState] = useState<LedState>(DEFAULT)
  const [scanProgress, setScanProgress] = useState<{ pct: number; msg: string } | null>(null)
  const [foundTvs, setFoundTvs] = useState<string[]>([])

  const onMessage = useCallback((data: unknown) => {
    const d = data as Record<string, unknown>
    if (d.type === 'state') {
      setState(s => ({ ...s, ...(d as Partial<LedState>) }))
    } else if (d.type === 'scanProgress') {
      setScanProgress({ pct: d.pct as number, msg: d.msg as string })
      if ((d.pct as number) >= 100) setScanProgress(null)
    } else if (d.type === 'ambilightFound') {
      setFoundTvs(tvs => [...tvs, d.ip as string])
    }
  }, [])

  /* eslint-disable react-hooks/rules-of-hooks -- TRANSPORT is a build-time
     constant (Vite inlines import.meta.env and dead-code-eliminates the
     unused branch), so exactly one of these two hooks ever actually runs
     for a given bundle. A block disable/enable pair is used instead of
     eslint-disable-next-line because the ternary's hook calls span multiple
     lines, which a single-line directive does not cover. */
  const { send, status, connect, error } = TRANSPORT === 'ble'
    ? useBluetoothTransport(onMessage)
    : { ...useWebSocket(wsUrl, onMessage), error: null as string | null }
  /* eslint-enable react-hooks/rules-of-hooks */

  const update = useCallback((patch: Partial<LedState>) => {
    setState(s => ({ ...s, ...patch }))
    send(patch)
  }, [send])

  return { state, update, status, scanProgress, foundTvs, send, connect, error }
}
