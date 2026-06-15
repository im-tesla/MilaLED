import { useState, useCallback } from 'react'
import { useWebSocket } from './useWebSocket'

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
  segALeds: number
  segBLeds: number
  segAHalf: boolean
  segBHalf: boolean
  dataPin: number
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
  virtualLeds: 118,
  ip: '',
  ssid: '',
  segALeds: 120,
  segBLeds: 58,
  segAHalf: true,
  segBHalf: false,
  dataPin: 2,
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

  const { send, status } = useWebSocket(wsUrl, onMessage)

  const update = useCallback((patch: Partial<LedState>) => {
    setState(s => ({ ...s, ...patch }))
    send(patch)
  }, [send])

  return { state, update, status, scanProgress, foundTvs, send }
}
