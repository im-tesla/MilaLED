import { useState } from 'react'
import { useTranslation } from 'react-i18next'
import { Broadcast, CaretDown, CaretUp } from '@phosphor-icons/react'
import { WifiJoinPanel } from './WifiJoinPanel'
import type { WifiNetwork, WifiJoinStatus } from '@/hooks/useLedState'

interface Props {
  networks: WifiNetwork[]
  scanning: boolean
  joinStatus: WifiJoinStatus
  onScan: () => void
  onJoin: (ssid: string, pass: string) => void
  onClearStatus: () => void
  currentIp?: string
}

export function CaptivePortalPanel({
  networks,
  scanning,
  joinStatus,
  onScan,
  onJoin,
  onClearStatus,
  currentIp,
}: Props) {
  const { t } = useTranslation()
  const [collapsed, setCollapsed] = useState(false)

  return (
    <div className="rounded-2xl bg-gradient-to-b from-amber-500/15 via-zinc-900 to-zinc-900 border border-amber-500/30 p-4 shadow-lg space-y-3">
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2.5">
          <div className="p-2 rounded-xl bg-amber-400 text-zinc-950">
            <Broadcast size={20} weight="bold" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-sm font-bold text-zinc-100">{t('settings.captivePortalTitle')}</h2>
              <span className="px-1.5 py-0.5 rounded text-[10px] font-semibold bg-amber-400/20 text-amber-300 uppercase tracking-wider">
                Hotspot
              </span>
            </div>
            <p className="text-xs text-zinc-400 mt-0.5 leading-relaxed">
              {t('settings.captivePortalDesc')}
            </p>
          </div>
        </div>

        <button
          type="button"
          onClick={() => setCollapsed(c => !c)}
          className="text-zinc-400 hover:text-zinc-200 p-1 transition-colors"
          aria-label={collapsed ? t('settings.showPortal') : t('settings.hidePortal')}
        >
          {collapsed ? <CaretDown size={18} /> : <CaretUp size={18} />}
        </button>
      </div>

      {!collapsed && (
        <div className="pt-1">
          <WifiJoinPanel
            networks={networks}
            scanning={scanning}
            joinStatus={joinStatus}
            onScan={onScan}
            onJoin={onJoin}
            onClearStatus={onClearStatus}
            currentIp={currentIp}
            isConnected={false}
            compact
          />
        </div>
      )}
    </div>
  )
}
