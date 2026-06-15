import { useEffect, useRef, useCallback } from 'react'

export type WsStatus = 'connecting' | 'open' | 'closed'

const THROTTLE_MS = 50 // max one message per 50ms per key to avoid flooding the ESP8266

export function useWebSocket(
  url: string,
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void } {
  const wsRef        = useRef<WebSocket | null>(null)
  const statusRef    = useRef<WsStatus>('connecting')
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage
  // last-sent timestamp per top-level key for throttling
  const lastSentRef  = useRef<Record<string, number>>({})
  const pendingRef   = useRef<Record<string, unknown>>({})
  const timerRef     = useRef<ReturnType<typeof setTimeout> | null>(null)

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

  const flush = useCallback(() => {
    timerRef.current = null
    const pending = pendingRef.current
    if (Object.keys(pending).length === 0) return
    pendingRef.current = {}
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(pending))
    }
  }, [])

  const send = useCallback((data: object) => {
    const now = Date.now()
    const entries = Object.entries(data as Record<string, unknown>)

    const immediate: Record<string, unknown> = {}
    const deferred:  Record<string, unknown> = {}

    for (const [k, v] of entries) {
      const last = lastSentRef.current[k] ?? 0
      if (now - last >= THROTTLE_MS) {
        immediate[k] = v
        lastSentRef.current[k] = now
      } else {
        deferred[k] = v
      }
    }

    if (Object.keys(immediate).length > 0 && wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(immediate))
    }

    if (Object.keys(deferred).length > 0) {
      Object.assign(pendingRef.current, deferred)
      if (!timerRef.current) {
        timerRef.current = setTimeout(flush, THROTTLE_MS)
      }
    }
  }, [flush])

  return { status: statusRef.current, send }
}
