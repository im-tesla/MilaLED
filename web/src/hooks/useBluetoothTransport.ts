import { useCallback, useRef, useState } from 'react'
import { useThrottledSender } from './useThrottledSender'
import type { WsStatus } from './useWebSocket'

const SERVICE_UUID    = '7a2eec00-4b0f-4bde-9f3f-1a7c6d9b2e10'
const CMD_CHAR_UUID   = '7a2eec01-4b0f-4bde-9f3f-1a7c6d9b2e10'
const STATE_CHAR_UUID = '7a2eec02-4b0f-4bde-9f3f-1a7c6d9b2e10'
const CHUNK_PAYLOAD   = 100 // keep in sync with CHUNK_PAYLOAD in BleServer.cpp

export function useBluetoothTransport(
  onMessage: (data: unknown) => void
): { status: WsStatus; send: (data: object) => void; connect: () => void; error: string | null } {
  const [status, setStatus] = useState<WsStatus>('closed')
  const [error, setError]   = useState<string | null>(null)
  const cmdCharRef   = useRef<BluetoothRemoteGATTCharacteristic | null>(null)
  const onMessageRef = useRef(onMessage)
  onMessageRef.current = onMessage
  const rxBufferRef  = useRef<Uint8Array[]>([])

  const handleNotification = useCallback((event: Event) => {
    const target = event.target as BluetoothRemoteGATTCharacteristic
    const value = target.value
    if (!value) return
    const bytes = new Uint8Array(value.buffer)
    if (bytes.length < 2) return
    const seq  = bytes[0]
    const more = bytes[1]
    const payload = bytes.slice(2)

    if (seq === 0) rxBufferRef.current = []
    rxBufferRef.current.push(payload)

    if (!more) {
      const total = rxBufferRef.current.reduce((n, b) => n + b.length, 0)
      const joined = new Uint8Array(total)
      let offset = 0
      for (const chunk of rxBufferRef.current) { joined.set(chunk, offset); offset += chunk.length }
      rxBufferRef.current = []
      try {
        const json = new TextDecoder().decode(joined)
        onMessageRef.current(JSON.parse(json))
      } catch {
        // dropped/corrupt frame train — ignore, the next state push will recover
      }
    }
  }, [])

  const sendRaw = useCallback((json: string) => {
    const cmdChar = cmdCharRef.current
    if (!cmdChar) return
    const bytes = new TextEncoder().encode(json)

    const writeChunks = async () => {
      let offset = 0
      let seq = 0
      do {
        const chunkLen = Math.min(CHUNK_PAYLOAD, bytes.length - offset)
        const more = offset + chunkLen < bytes.length
        const frame = new Uint8Array(2 + chunkLen)
        frame[0] = seq
        frame[1] = more ? 1 : 0
        frame.set(bytes.subarray(offset, offset + chunkLen), 2)
        await cmdChar.writeValueWithoutResponse(frame)
        offset += chunkLen
        seq++
      } while (offset < bytes.length)
    }
    writeChunks().catch(() => { /* write failed — device likely disconnected */ })
  }, [])

  const send = useThrottledSender(sendRaw)

  const connect = useCallback(() => {
    setError(null)
    setStatus('connecting')

    navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    })
      .then(device => {
        device.addEventListener('gattserverdisconnected', () => {
          setStatus('closed')
          cmdCharRef.current = null
        })
        return device.gatt!.connect()
      })
      .then(server => server.getPrimaryService(SERVICE_UUID))
      .then(service => Promise.all([
        service.getCharacteristic(CMD_CHAR_UUID),
        service.getCharacteristic(STATE_CHAR_UUID),
      ]))
      .then(([cmdChar, stateChar]) => {
        cmdCharRef.current = cmdChar
        stateChar.addEventListener('characteristicvaluechanged', handleNotification)
        return stateChar.startNotifications()
      })
      .then(() => setStatus('open'))
      .catch((err: Error) => {
        setStatus('closed')
        setError(err.message || 'Bluetooth connection failed')
      })
  }, [handleNotification])

  return { status, send, connect, error }
}
