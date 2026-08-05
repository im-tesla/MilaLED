import { useTranslation } from 'react-i18next'
import type { WsStatus } from '@/hooks/useWebSocket'

interface Props {
  status: WsStatus
  error: string | null
  onConnect: () => void
}

export function BleConnectDialog({ status, error, onConnect }: Props) {
  const { t } = useTranslation()
  const supported = typeof navigator !== 'undefined' && 'bluetooth' in navigator

  return (
    <div className="min-h-[100dvh] flex flex-col items-center justify-center gap-4 bg-background p-6 text-center">
      <h1 className="text-lg font-semibold text-zinc-100">{t('ble.title')}</h1>
      {!supported ? (
        <p className="text-sm text-zinc-400 max-w-xs">{t('ble.unsupported')}</p>
      ) : (
        <>
          <p className="text-sm text-zinc-400 max-w-xs">{t('ble.description')}</p>
          <button
            onClick={onConnect}
            disabled={status === 'connecting'}
            className="px-6 py-3 rounded-xl bg-amber-400 hover:bg-amber-300 text-zinc-950 font-semibold disabled:opacity-50 transition-colors"
          >
            {status === 'connecting' ? t('ble.connecting') : t('ble.connect')}
          </button>
          {error && <p className="text-sm text-red-400 max-w-xs">{error}</p>}
        </>
      )}
    </div>
  )
}
