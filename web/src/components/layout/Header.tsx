import { useTranslation } from 'react-i18next'
import { Badge } from '@/components/ui/badge'
import { Switch } from '@/components/ui/switch'
import type { LedState } from '@/hooks/useLedState'

interface Props {
  state: LedState
  wsStatus: string
  onPowerToggle: () => void
}

export function Header({ state, wsStatus, onPowerToggle }: Props) {
  const { t } = useTranslation()
  const connected = wsStatus === 'open'

  return (
    <header className="px-4 pt-4 pb-3 border-b border-zinc-800 bg-zinc-950/95 backdrop-blur sticky top-0 z-10">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="font-semibold text-sm tracking-tight text-zinc-100">MilaLED</span>
          <span className={`w-1.5 h-1.5 rounded-full ${connected ? 'bg-emerald-400' : 'bg-zinc-600'}`} />
          <span className="text-[10px] text-zinc-500">{state.ssid || 'milaled.local'}</span>
        </div>
        <div className="flex items-center gap-3">
          <Switch
            checked={state.power}
            onCheckedChange={onPowerToggle}
            className="data-[state=checked]:bg-amber-400 scale-90"
          />
          <Badge
            variant="outline"
            className={`text-[10px] px-2 py-0.5 ${
              state.power
                ? 'border-amber-400/40 text-amber-400'
                : 'border-zinc-700 text-zinc-500'
            }`}
          >
            {state.power ? t('header.on') : t('header.off')}
          </Badge>
        </div>
      </div>
    </header>
  )
}
