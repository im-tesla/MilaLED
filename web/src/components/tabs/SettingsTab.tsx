import { useState, useEffect } from 'react'
import { useTranslation } from 'react-i18next'
import { WifiHigh, MagnifyingGlass, ArrowCounterClockwise, Stop, Plus, Trash } from '@phosphor-icons/react'
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
import type { LedState, SegmentData } from '@/hooks/useLedState'

const ESP_PINS: { gpio: number; label: string }[] = [
  { gpio: 2,  label: 'GPIO2  (D4)' },
  { gpio: 4,  label: 'GPIO4  (D2)' },
  { gpio: 5,  label: 'GPIO5  (D1)' },
  { gpio: 12, label: 'GPIO12 (D6)' },
  { gpio: 13, label: 'GPIO13 (D7)' },
  { gpio: 14, label: 'GPIO14 (D5)' },
  { gpio: 15, label: 'GPIO15 (D8)' },
  { gpio: 16, label: 'GPIO16 (D0)' },
  { gpio: 21, label: 'GPIO21' },
  { gpio: 22, label: 'GPIO22' },
  { gpio: 23, label: 'GPIO23' },
  { gpio: 25, label: 'GPIO25' },
  { gpio: 26, label: 'GPIO26' },
  { gpio: 27, label: 'GPIO27' },
  { gpio: 32, label: 'GPIO32' },
  { gpio: 33, label: 'GPIO33' },
]

const MAPPING_OPTIONS: { value: string; key: string }[] = [
  { value: 'right',   key: 'settings.mappingRight' },
  { value: 'left',    key: 'settings.mappingLeft' },
  { value: 'top',     key: 'settings.mappingTop' },
  { value: 'average', key: 'settings.mappingAvg' },
]

const COLOR_ORDERS = ['RGB', 'RBG', 'GRB', 'GBR', 'BRG', 'BGR']

const CHIPSETS = ['WS2811', 'WS2812B', 'WS2815', 'WS2813', 'SK6812']

const MAX_SEGMENTS = 4

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
  scanProgress: { pct: number; msg: string } | null
  foundTvs: string[]
}

export function SettingsTab({ state, update, scanProgress, foundTvs }: Props) {
  const { t, i18n } = useTranslation()

  const [segments, setSegments] = useState<SegmentData[]>(() =>
    state.segments ?? [{ count: 120, half: false }]
  )
  const [dataPin,  setDataPin]  = useState(state.dataPin)
  const [colorOrder, setColorOrder] = useState(state.colorOrder)
  const [chipset,   setChipset]   = useState(state.chipset)
  const [rebooting, setRebooting] = useState(false)
  const [confirmWifi, setConfirmWifi] = useState(false)
  const [wifiResetting, setWifiResetting] = useState(false)

  // Sync from WebSocket on reconnect
  useEffect(() => {
    if (state.segments) setSegments([...state.segments])
    setDataPin(state.dataPin)
    setColorOrder(state.colorOrder)
    setChipset(state.chipset)
  }, [state.segments, state.dataPin, state.colorOrder, state.chipset])

  const localVirt = segments.reduce(
    (sum, s) => sum + Math.floor(s.count / (s.half ? 2 : 1)),
    0
  )

  const localPhys = segments.reduce((sum, s) => sum + s.count, 0)

  const setSegment = (idx: number, patch: Partial<SegmentData>) => {
    setSegments(prev => prev.map((s, i) => i === idx ? { ...s, ...patch } : s))
  }

  const addSegment = () => {
    setSegments(prev => {
      const active = prev.filter(s => s.count > 0)
      if (active.length >= MAX_SEGMENTS) return prev
      return [...prev, { count: 10, half: false }]
    })
  }

  const removeSegment = (idx: number) => {
    setSegments(prev => {
      const active = prev.filter(s => s.count > 0)
      if (active.length <= 1) {
        // Remove last segment — blank it out
        return prev.map((s, i) => i === idx ? { count: 0, half: false } : s)
      }
      return prev.filter((_, i) => i !== idx)
    })
  }

  const saveStrip = async () => {
    setRebooting(true)
    await fetch('/api/strip', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        segments: segments.filter(s => s.count >= 0).slice(0, MAX_SEGMENTS),
        dataPin,
        colorOrder,
        chipset,
      }),
    }).catch(() => {})
    setTimeout(() => setRebooting(false), 15000)
  }

  const startScan = () => {
    fetch('/api/ambilight/scan', { method: 'POST' }).catch(() => {})
  }

  const stopScan = () => {
    fetch('/api/ambilight/scan/cancel', { method: 'POST' }).catch(() => {})
  }

  const resetWifi = async () => {
    setWifiResetting(true)
    await fetch('/api/wifi/reset', { method: 'POST' }).catch(() => {})
  }

  const activeSegs = segments.filter(s => s.count > 0)

  return (
    <div className="space-y-5">

      {/* Strip */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.strip')}
        </h3>
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-3">
          <div className="flex items-center justify-between text-sm">
            <span className="text-zinc-400">{t('settings.virtual')}</span>
            <span className="text-zinc-100 tabular-nums">
              {localVirt} <span className="text-zinc-500 text-xs">/ {localPhys} phys</span>
            </span>
          </div>

          {/* Segment list */}
          <div className="space-y-2">
            {segments.map((seg, idx) => (
              <div key={idx} className={`rounded-lg border p-2 space-y-1.5 ${
                seg.count > 0 ? 'border-zinc-700/60 bg-zinc-800/50' : 'border-zinc-800 border-dashed'
              }`}>
                <div className="flex items-center gap-2">
                  <span className="text-[10px] text-zinc-500 w-12 shrink-0">Seg {idx + 1}</span>
                  <Input
                    type="number"
                    value={seg.count}
                    onChange={e => setSegment(idx, { count: Number(e.target.value) || 0 })}
                    min={0} max={2000}
                    className="bg-zinc-800 border-zinc-700 text-zinc-100 text-sm h-8 w-20 text-center"
                  />
                  <div className="flex gap-1 ml-auto">
                    <button
                      onClick={() => setSegment(idx, { half: false })}
                      className={`px-2 py-1 rounded text-[10px] font-medium transition-colors ${
                        !seg.half ? 'border-amber-400/60 text-amber-400 bg-amber-400/5 border' : 'text-zinc-500'
                      }`}
                    >
                      {t('settings.full')}
                    </button>
                    <button
                      onClick={() => setSegment(idx, { half: true })}
                      className={`px-2 py-1 rounded text-[10px] font-medium transition-colors ${
                        seg.half ? 'border-amber-400/60 text-amber-400 bg-amber-400/5 border' : 'text-zinc-500'
                      }`}
                    >
                      {t('settings.half')}
                    </button>
                    {activeSegs.length > 1 && seg.count > 0 && (
                      <button
                        onClick={() => removeSegment(idx)}
                        className="text-zinc-500 hover:text-red-400 transition-colors px-1"
                      >
                        <Trash size={12} />
                      </button>
                    )}
                  </div>
                </div>
                {seg.count > 0 && (
                  <div className="text-[10px] text-zinc-500 ml-14">
                    {seg.half
                      ? `${Math.floor(seg.count / 2)} virtual (every other LED skipped)`
                      : `${seg.count} virtual (1:1)`}
                  </div>
                )}
              </div>
            ))}
          </div>

          {activeSegs.length < MAX_SEGMENTS && (
            <button
              onClick={addSegment}
              className="w-full py-1.5 rounded-lg border border-dashed border-zinc-700 text-zinc-500 hover:text-amber-400 hover:border-amber-400/40 text-xs transition-colors flex items-center justify-center gap-1"
            >
              <Plus size={12} />
              {t('settings.addSegment')}
            </button>
          )}

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
          <div className="space-y-1">
            <span className="text-xs text-zinc-500">{t('settings.colorOrder')}</span>
            <Select value={String(colorOrder)} onValueChange={v => setColorOrder(Number(v))}>
              <SelectTrigger className="bg-zinc-900 border-zinc-700 text-zinc-100 text-sm h-9 w-full">
                <SelectValue />
              </SelectTrigger>
              <SelectContent className="bg-zinc-900 border-zinc-800">
                {COLOR_ORDERS.map((label, i) => (
                  <SelectItem key={label} value={String(i)} className="text-zinc-300 focus:bg-zinc-800">
                    {label}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>
          <div className="space-y-1">
            <span className="text-xs text-zinc-500">{t('settings.chipset')}</span>
            <Select value={String(chipset)} onValueChange={v => setChipset(Number(v))}>
              <SelectTrigger className="bg-zinc-900 border-zinc-700 text-zinc-100 text-sm h-9 w-full">
                <SelectValue />
              </SelectTrigger>
              <SelectContent className="bg-zinc-900 border-zinc-800">
                {CHIPSETS.map((label, i) => (
                  <SelectItem key={label} value={String(i)} className="text-zinc-300 focus:bg-zinc-800">
                    {label}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
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
            className="bg-zinc-900 border-zinc-700 text-zinc-100 text-sm h-9 flex-1"
          />
          {scanProgress ? (
            <Button
              size="sm"
              variant="outline"
              className="border-red-500/40 text-red-400 hover:border-red-400 hover:text-red-300 shrink-0"
              onClick={stopScan}
            >
              <Stop size={14} className="mr-1" />
              {t('settings.stopScan')}
            </Button>
          ) : (
            <Button
              size="sm"
              variant="outline"
              className="border-zinc-700 text-zinc-300 hover:text-amber-400 hover:border-amber-400/40 shrink-0"
              onClick={startScan}
            >
              <MagnifyingGlass size={14} className="mr-1" />
              {t('settings.scan')}
            </Button>
          )}
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
            className="bg-zinc-900 border-zinc-700 text-zinc-100 text-sm h-9 flex-1"
          />
          <Select value={state.ambMapping} onValueChange={v => update({ ambMapping: v })}>
            <SelectTrigger className="bg-zinc-900 border-zinc-700 text-zinc-100 text-sm h-9 flex-1">
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

      {/* Firmware version */}
      <section className="space-y-2">
        <h3 className="text-xs font-semibold text-zinc-500 uppercase tracking-wide">
          {t('settings.version')}
        </h3>
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 flex items-center justify-between text-sm">
          <span className="text-zinc-400">{t('settings.version')}</span>
          <span className="text-zinc-100 tabular-nums">{state.version || '—'}</span>
        </div>
      </section>

    </div>
  )
}
