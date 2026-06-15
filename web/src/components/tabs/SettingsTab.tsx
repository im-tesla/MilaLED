import { useState, useEffect } from 'react'
import { useTranslation } from 'react-i18next'
import { WifiHigh, MagnifyingGlass, ArrowCounterClockwise } from '@phosphor-icons/react'
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

const ESP_PINS: { gpio: number; label: string }[] = [
  { gpio: 5,  label: 'D1 (GPIO5)' },
  { gpio: 4,  label: 'D2 (GPIO4)' },
  { gpio: 2,  label: 'D4 (GPIO2)' },
  { gpio: 14, label: 'D5 (GPIO14)' },
  { gpio: 12, label: 'D6 (GPIO12)' },
  { gpio: 13, label: 'D7 (GPIO13)' },
]

const MAPPING_OPTIONS: { value: string; key: string }[] = [
  { value: 'right',   key: 'settings.mappingRight' },
  { value: 'left',    key: 'settings.mappingLeft' },
  { value: 'top',     key: 'settings.mappingTop' },
  { value: 'average', key: 'settings.mappingAvg' },
]

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
  scanProgress: { pct: number; msg: string } | null
  foundTvs: string[]
}

export function SettingsTab({ state, update, scanProgress, foundTvs }: Props) {
  const { t, i18n } = useTranslation()

  // Local strip config — only applied on "Save & Reboot"
  const [segALeds, setSegALeds] = useState(state.segALeds)
  const [segBLeds, setSegBLeds] = useState(state.segBLeds)
  const [segAHalf, setSegAHalf] = useState(state.segAHalf)
  const [dataPin,  setDataPin]  = useState(state.dataPin)
  const [rebooting, setRebooting] = useState(false)
  const [confirmWifi, setConfirmWifi] = useState(false)
  const [wifiResetting, setWifiResetting] = useState(false)

  // Sync local strip state when WebSocket pushes fresh state (e.g. after reconnect)
  useEffect(() => {
    setSegALeds(state.segALeds)
    setSegBLeds(state.segBLeds)
    setSegAHalf(state.segAHalf)
    setDataPin(state.dataPin)
  }, [state.segALeds, state.segBLeds, state.segAHalf, state.dataPin])

  const saveStrip = async () => {
    setRebooting(true)
    await fetch('/api/strip', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ segALeds, segBLeds, segAHalf, dataPin }),
    }).catch(() => {})
    // ESP restarts — WebSocket will reconnect automatically.
    // Reset rebooting state after a generous timeout in case the restart takes longer.
    setTimeout(() => setRebooting(false), 15000)
  }

  const startScan = () => {
    fetch('/api/ambilight/scan', { method: 'POST' }).catch(() => {})
  }

  const resetWifi = async () => {
    setWifiResetting(true)
    await fetch('/api/wifi/reset', { method: 'POST' }).catch(() => {})
    // ESP will restart into AP mode — connection lost
  }

  return (
    <div className="space-y-5">

      {/* Strip */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.strip')}
        </h3>
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-3">
          <div className="flex items-center justify-between text-sm">
            <span className="text-zinc-400">Virtual LEDs</span>
            <span className="text-zinc-100 tabular-nums">{state.virtualLeds}</span>
          </div>
          <div className="grid grid-cols-2 gap-2">
            <div className="space-y-1">
              <span className="text-xs text-zinc-500">{t('settings.segA')}</span>
              <Input
                type="number"
                value={segALeds}
                onChange={e => setSegALeds(Number(e.target.value))}
                min={1} max={500}
                className="bg-zinc-800 border-zinc-700 text-sm h-9"
              />
            </div>
            <div className="space-y-1">
              <span className="text-xs text-zinc-500">{t('settings.segB')}</span>
              <Input
                type="number"
                value={segBLeds}
                onChange={e => setSegBLeds(Number(e.target.value))}
                min={0} max={500}
                className="bg-zinc-800 border-zinc-700 text-sm h-9"
              />
            </div>
          </div>
          <div className="space-y-1">
            <span className="text-xs text-zinc-500">{t('settings.density')}</span>
            <div className="flex gap-2">
              {[true, false].map(half => (
                <button
                  key={String(half)}
                  onClick={() => setSegAHalf(half)}
                  className={`flex-1 py-2 rounded-lg border text-xs font-medium transition-colors ${
                    segAHalf === half
                      ? 'border-amber-400/60 text-amber-400 bg-amber-400/5'
                      : 'border-zinc-700 text-zinc-400 hover:border-zinc-600'
                  }`}
                >
                  {half ? t('settings.half') : t('settings.full')}
                </button>
              ))}
            </div>
          </div>
          <div className="space-y-1">
            <span className="text-xs text-zinc-500">{t('settings.dataPin')}</span>
            <div className="grid grid-cols-3 gap-1.5">
              {ESP_PINS.map(({ gpio, label }) => (
                <button
                  key={gpio}
                  onClick={() => setDataPin(gpio)}
                  className={`py-2 rounded-lg border text-xs font-medium transition-colors ${
                    dataPin === gpio
                      ? 'border-amber-400/60 text-amber-400 bg-amber-400/5'
                      : 'border-zinc-700 text-zinc-400 hover:border-zinc-600'
                  }`}
                >
                  {label}
                </button>
              ))}
            </div>
          </div>
          <Button
            onClick={saveStrip}
            disabled={rebooting}
            size="sm"
            className="w-full bg-amber-400 hover:bg-amber-300 text-zinc-950 font-semibold"
          >
            <ArrowCounterClockwise size={14} className="mr-1.5" />
            {rebooting ? t('settings.rebooting') : t('settings.saveReboot')}
          </Button>
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
          <div className="p-3 space-y-2">
            {!confirmWifi ? (
              <button
                onClick={() => setConfirmWifi(true)}
                className="w-full text-xs py-2 rounded-lg border border-zinc-700 text-zinc-400 hover:border-zinc-600 hover:text-zinc-300 transition-colors"
              >
                {t('settings.wifiChange')}
              </button>
            ) : (
              <div className="space-y-2">
                <p className="text-[11px] text-zinc-500">{t('settings.wifiChangeConfirm')}</p>
                <div className="flex gap-2">
                  <button
                    onClick={resetWifi}
                    disabled={wifiResetting}
                    className="flex-1 text-xs py-2 rounded-lg border border-red-500/50 text-red-400 hover:bg-red-500/10 transition-colors disabled:opacity-50"
                  >
                    {wifiResetting ? t('settings.rebooting') : t('settings.confirm')}
                  </button>
                  <button
                    onClick={() => setConfirmWifi(false)}
                    className="flex-1 text-xs py-2 rounded-lg border border-zinc-700 text-zinc-400 hover:border-zinc-600 transition-colors"
                  >
                    {t('settings.cancel')}
                  </button>
                </div>
              </div>
            )}
          </div>
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
            min={50} max={5000}
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
              onClick={() => { i18n.changeLanguage(lang); localStorage.setItem('lang', lang) }}
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
