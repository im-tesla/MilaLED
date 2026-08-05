import { useEffect, useRef, useCallback } from 'react'
import { useThrottledSender } from './useThrottledSender'

export type WsStatus = 'connecting' | 'open' | 'closed'

export function useWebSocket(
  url: string,
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void; connect: () => void } {
  const wsRef        = useRef<WebSocket | null>(null)
  const statusRef    = useRef<WsStatus>('connecting')
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage

  useEffect(() => {
    const ws = new WebSocket(url)
    wsRef.current = ws
    ws.onopen    = () => { statusRef.current = 'open' }
    ws.onclose   = () => { statusRef.current = 'closed'; setTimeout(() => {
      // simple reconnect
      wsRef.current = new WebSocket(url)
    }, 2000) }
    ws.onmessage = (e) => {
      try { onMessageRef.current(JSON.parse(e.data)) } catch {}
    }
    return () => ws.close()
  }, [url])

  const sendRaw = useCallback((json: string) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(json)
    }
  }, [])

  const send = useThrottledSender(sendRaw)

  // WS auto-connects on mount; exposed as a no-op only so useLedState has a
  // uniform { status, send, connect } shape across both transports.
  const connect = useCallback(() => {}, [])

  return { status: statusRef.current, send, connect }
}
