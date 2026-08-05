import { useRef, useCallback } from 'react'

const THROTTLE_MS = 50 // max one message per 50ms per key, to avoid flooding the device

/**
 * Per-key throttled sender: at most one message per key per THROTTLE_MS.
 * Keys sent too recently are batched and flushed together after the window.
 */
export function useThrottledSender(sendRaw: (json: string) => void) {
  const lastSentRef = useRef<Record<string, number>>({})
  const pendingRef  = useRef<Record<string, unknown>>({})
  const timerRef    = useRef<ReturnType<typeof setTimeout> | null>(null)

  const flush = useCallback(() => {
    timerRef.current = null
    const pending = pendingRef.current
    if (Object.keys(pending).length === 0) return
    pendingRef.current = {}
    sendRaw(JSON.stringify(pending))
  }, [sendRaw])

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

    if (Object.keys(immediate).length > 0) {
      sendRaw(JSON.stringify(immediate))
    }

    if (Object.keys(deferred).length > 0) {
      Object.assign(pendingRef.current, deferred)
      if (!timerRef.current) {
        timerRef.current = setTimeout(flush, THROTTLE_MS)
      }
    }
  }, [flush])

  return send
}
