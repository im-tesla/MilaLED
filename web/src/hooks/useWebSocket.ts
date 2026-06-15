import { useEffect, useRef, useCallback } from 'react'

export type WsStatus = 'connecting' | 'open' | 'closed'

export function useWebSocket(
  url: string,
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void } {
  const wsRef = useRef<WebSocket | null>(null)
  const statusRef = useRef<WsStatus>('connecting')
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage

  useEffect(() => {
    const ws = new WebSocket(url)
    wsRef.current = ws
    ws.onopen    = () => { statusRef.current = 'open' }
    ws.onclose   = () => { statusRef.current = 'closed' }
    ws.onmessage = (e) => {
      try { onMessageRef.current(JSON.parse(e.data)) } catch {}
    }
    return () => ws.close()
  }, [url])

  const send = useCallback((data: object) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(data))
    }
  }, [])

  return { status: statusRef.current, send }
}
