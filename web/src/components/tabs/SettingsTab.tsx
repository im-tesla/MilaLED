import { useState } from 'react'
import { useTranslation } from 'react-i18next'
import { WifiHigh, ArrowClockwise, MagnifyingGlass } from '@phosphor-icons/react'
import { AmbilightStatus } from '@/components/shared/AmbilightStatus'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import type { LedState } from '@/hooks/useLedState'

const MAPPING_OPTIONS: { value: string; key: string }[] = [
  { value: 'right',   key: 'settings.mappingRight' },
  { value: 'left',    key: 'settings.mappingLeft' },
  { value: 'top',     key: 'settings.mappingTop' },
  { value: 'average', key: 'settings.mappingAvg' },
]

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
  send: (data: object) => void
  scanProgress: { pct: number; msg: string } | null
  foundTvs: string[]
}

export function SettingsTab({ state, update, send, scanProgress, foundTvs }: Props) {
  const { t, i18n } = useTranslation()
  const [otaStatus, setOtaStatus] = useState<string | null>(null)

  const checkOta = async () => {
    setOtaStatus(t('settings.check'))
    try {
      const r = await fetch('/api/ota')
      const d = await r.json() as { version?: string }
      setOtaStatus(d.version ?? 'ok')
    } catch {
      setOtaStatus('error')
    }
  }

  const startScan = () => send({ action: 'scanAmbilight' })

  return (
    <div className="space-y-5">

      {/* Strip */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.strip')}
        </h3>
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 flex justify-between text-sm">
          <span className="text-zinc-400">Virtual LEDs</span>
          <span className="text-zinc-100 tabular-nums">{state.virtualLeds}</span>
        </div>
      </section>

      {/* Network */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.network')}
        </h3>
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 divide-y divide-zinc-800">
          <div className="p-3 flex items-center justify-between text-sm">
            <span className="text-zinc-400 flex items-center gap-1.5">
              <WifiHigh size={14} />
              {t('settings.wifi')}
            </span>
            <span className="text-zinc-100 truncate max-w-[140px]">{state.ssid || '—'}</span>
          </div>
          <div className="p-3 flex items-center justify-between text-sm">
            <span className="text-zinc-400">IP</span>
            <span className="text-zinc-100 tabular-nums">{state.ip || '—'}</span>
          </div>
        </div>
        <div className="flex items-center gap-2">
          <Button
            size="sm"
            variant="outline"
            className="flex-1 border-zinc-700 text-zinc-300 hover:text-amber-400 hover:border-amber-400/40"
            onClick={checkOta}
          >
            <ArrowClockwise size={14} className="mr-1.5" />
            {t('settings.ota')}
          </Button>
          {otaStatus && (
            <span className="text-xs text-zinc-500">{otaStatus}</span>
          )}
        </div>
      </section>

      {/* Ambilight */}
      <section className="space-y-2">
        <div className="flex items-center justify-between">
          <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
            {t('settings.ambilight')}
          </h3>
          <AmbilightStatus effect={state.effect} ambStatus={state.ambStatus} />
        </div>

        <div className="flex gap-2">
          <Input
            value={state.tvIp}
            onChange={e => update({ tvIp: e.target.value })}
            placeholder={t('settings.tvIp')}
            className="bg-zinc-900 border-zinc-700 text-sm h-9 flex-1"
          />
          <Button
            size="sm"
            variant="outline"
            className="border-zinc-700 text-zinc-300 hover:text-amber-400 hover:border-amber-400/40 shrink-0"
            onClick={startScan}
            disabled={!!scanProgress}
          >
            <MagnifyingGlass size={14} className="mr-1" />
            {scanProgress ? t('settings.scanning') : t('settings.scan')}
          </Button>
        </div>

        {scanProgress && (
          <div className="rounded-lg bg-zinc-900 border border-zinc-800 p-2 space-y-1">
            <div className="h-1 bg-zinc-800 rounded-full overflow-hidden">
              <div
                className="h-full bg-amber-400 rounded-full transition-[width] duration-300"
                style={{ width: `${scanProgress.pct}%` }}
              />
            </div>
            <p className="text-xs text-zinc-500">{scanProgress.msg}</p>
          </div>
        )}

        {foundTvs.length > 0 && (
          <div className="space-y-1">
            {foundTvs.map(ip => (
              <button
                key={ip}
                onClick={() => update({ tvIp: ip })}
                className={`w-full text-left text-xs px-3 py-2 rounded-lg border transition-colors ${
                  state.tvIp === ip
                    ? 'border-amber-400/60 text-amber-400 bg-amber-400/5'
                    : 'border-zinc-800 text-zinc-400 hover:border-zinc-700'
                }`}
              >
                {ip}
              </button>
            ))}
          </div>
        )}

        <div className="flex gap-2">
          <Input
            type="number"
            value={state.ambPollMs}
            onChange={e => update({ ambPollMs: Number(e.target.value) })}
            placeholder={t('settings.pollInterval')}
            min={50}
            max={5000}
            className="bg-zinc-900 border-zinc-700 text-sm h-9 flex-1"
          />
          <Select value={state.ambMapping} onValueChange={v => update({ ambMapping: v })}>
            <SelectTrigger className="bg-zinc-900 border-zinc-700 text-sm h-9 flex-1">
              <SelectValue />
            </SelectTrigger>
            <SelectContent className="bg-zinc-900 border-zinc-800">
              {MAPPING_OPTIONS.map(({ value, key }) => (
                <SelectItem key={value} value={value} className="text-zinc-300 focus:bg-zinc-800">
                  {t(key)}
                </SelectItem>
              ))}
            </SelectContent>
          </Select>
        </div>
      </section>

      {/* Language */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.language')}
        </h3>
        <div className="flex gap-2">
          {(['en', 'pl'] as const).map(lang => (
            <button
              key={lang}
              onClick={() => i18n.changeLanguage(lang)}
              className={`flex-1 py-2 rounded-xl border text-sm font-medium transition-colors ${
                i18n.language === lang
                  ? 'border-amber-400/60 text-amber-400 bg-amber-400/5'
                  : 'border-zinc-800 text-zinc-400 hover:border-zinc-700'
              }`}
            >
              {lang.toUpperCase()}
            </button>
          ))}
        </div>
      </section>

    </div>
  )
}
