import { useState, useEffect, useRef } from 'react'
import { useTranslation } from 'react-i18next'
import {
  WifiHigh,
  WifiMedium,
  WifiLow,
  LockKey,
  Eye,
  EyeSlash,
  ArrowsClockwise,
  CheckCircle,
  WarningCircle,
  Plus,
  Bluetooth,
} from '@phosphor-icons/react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { TRANSPORT } from '@/lib/capabilities'
import type { WifiNetwork, WifiJoinStatus } from '@/hooks/useLedState'

interface Props {
  networks: WifiNetwork[]
  scanning: boolean
  joinStatus: WifiJoinStatus
  onScan: () => void
  onJoin: (ssid: string, pass: string) => void
  onClearStatus: () => void
  currentSsid?: string
  currentIp?: string
  isConnected?: boolean
  onResetWifi?: () => void
  compact?: boolean
}

export function WifiJoinPanel({
  networks,
  scanning,
  joinStatus,
  onScan,
  onJoin,
  onClearStatus,
  currentSsid,
  currentIp,
  isConnected,
  onResetWifi,
  compact = false,
}: Props) {
  const { t } = useTranslation()
  const [selectedSsid, setSelectedSsid] = useState('')
  const [password, setPassword] = useState('')
  const [showPassword, setShowPassword] = useState(false)
  const [isManual, setIsManual] = useState(false)
  const [manualSsid, setManualSsid] = useState('')
  const [showJoinForm, setShowJoinForm] = useState(!isConnected)
  const [confirmReset, setConfirmReset] = useState(false)
  const hasAutoScannedRef = useRef(false)

  const activeSsid = isManual ? manualSsid : selectedSsid

  // Auto-trigger initial scan once if no networks yet and joining form is open
  useEffect(() => {
    if (showJoinForm && networks.length === 0 && !scanning && !hasAutoScannedRef.current) {
      hasAutoScannedRef.current = true
      onScan()
    }
  }, [showJoinForm, networks.length, scanning, onScan])

  const handleSelectNetwork = (net: WifiNetwork) => {
    setIsManual(false)
    setSelectedSsid(net.ssid)
    onClearStatus()
  }

  const handleConnect = (e: React.FormEvent) => {
    e.preventDefault()
    if (!activeSsid.trim()) return
    onJoin(activeSsid.trim(), password)
  }

  const getSignalIcon = (rssi: number) => {
    if (rssi > -60) return <WifiHigh size={16} className="text-emerald-400 shrink-0" />
    if (rssi > -75) return <WifiMedium size={16} className="text-amber-400 shrink-0" />
    return <WifiLow size={16} className="text-zinc-400 shrink-0" />
  }

  return (
    <div className="space-y-3">
      {/* Current connection status */}
      {isConnected && currentSsid && !showJoinForm && (
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-2">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <span className="relative flex h-2 w-2">
                <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75" />
                <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500" />
              </span>
              <span className="text-sm font-medium text-zinc-100">{currentSsid}</span>
            </div>
            <span className="text-xs text-zinc-400 tabular-nums">{currentIp}</span>
          </div>

          <div className="flex flex-col gap-2 pt-1">
            <div className="flex gap-2">
              <Button
                size="sm"
                variant="outline"
                onClick={() => { setShowJoinForm(true); onScan() }}
                className="flex-1 border-zinc-700 text-xs text-zinc-300 hover:text-amber-400"
              >
                {t('settings.wifiChange')}
              </Button>

              {onResetWifi && !confirmReset && (
                <Button
                  size="sm"
                  variant="outline"
                  onClick={() => setConfirmReset(true)}
                  className="border-zinc-800 text-xs text-zinc-400 hover:text-red-400"
                >
                  {t('settings.disconnectWifi')}
                </Button>
              )}
            </div>

            {confirmReset && onResetWifi && (
              <div className="p-2.5 rounded-lg border border-red-500/30 bg-red-500/10 space-y-2">
                <p className="text-[11px] text-zinc-300">{t('settings.disconnectConfirm')}</p>
                <div className="flex gap-2">
                  <Button
                    size="sm"
                    variant="destructive"
                    onClick={() => { setConfirmReset(false); onResetWifi() }}
                    className="flex-1 text-xs h-7"
                  >
                    {t('settings.confirm')}
                  </Button>
                  <Button
                    size="sm"
                    variant="outline"
                    onClick={() => setConfirmReset(false)}
                    className="flex-1 border-zinc-700 text-xs h-7 text-zinc-400"
                  >
                    {t('settings.cancel')}
                  </Button>
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* Join Wi-Fi Form */}
      {showJoinForm && (
        <div className={`rounded-xl bg-zinc-900 border border-zinc-800 p-3.5 space-y-3 ${compact ? 'text-xs' : ''}`}>
          <div className="space-y-1">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-1.5">
                {TRANSPORT === 'ble' ? (
                  <Bluetooth size={16} className="text-amber-400" />
                ) : (
                  <WifiHigh size={16} className="text-amber-400" />
                )}
                <span className="text-xs font-semibold text-zinc-300 uppercase tracking-wide">
                  {TRANSPORT === 'ble' ? t('settings.wifiBle') : t('settings.wifiJoin')}
                </span>
              </div>
              <Button
                size="sm"
                variant="ghost"
                disabled={scanning}
                onClick={onScan}
                className="h-7 px-2 text-xs text-zinc-400 hover:text-amber-400 hover:bg-zinc-800"
              >
                <ArrowsClockwise size={13} className={`mr-1 ${scanning ? 'animate-spin text-amber-400' : ''}`} />
                {scanning ? t('settings.scanningWifi') : t('settings.scanWifi')}
              </Button>
            </div>

            {TRANSPORT === 'ble' && (
              <p className="text-[11px] text-zinc-400">
                {t('settings.wifiBleDesc')}
              </p>
            )}
          </div>

          {/* Network Selection List */}
          {!isManual ? (
            <div className="space-y-1.5 max-h-48 overflow-y-auto pr-1">
              {networks.length === 0 && !scanning && (
                <p className="text-xs text-zinc-500 py-3 text-center">
                  No Wi-Fi networks found. Click scan to search.
                </p>
              )}

              {networks.map(net => {
                const isSelected = selectedSsid === net.ssid
                return (
                  <button
                    key={net.ssid}
                    type="button"
                    onClick={() => handleSelectNetwork(net)}
                    className={`w-full flex items-center justify-between px-3 py-2 rounded-lg border text-left transition-all ${
                      isSelected
                        ? 'border-amber-400/80 bg-amber-400/10 text-zinc-100'
                        : 'border-zinc-800 hover:border-zinc-700 bg-zinc-950/40 text-zinc-300'
                    }`}
                  >
                    <div className="flex items-center gap-2 truncate">
                      {getSignalIcon(net.rssi)}
                      <span className="text-xs font-medium truncate">{net.ssid}</span>
                    </div>
                    <div className="flex items-center gap-1.5 shrink-0 text-zinc-500">
                      {net.secure && <LockKey size={13} />}
                      <span className="text-[10px] tabular-nums">{net.rssi} dBm</span>
                    </div>
                  </button>
                )
              })}

              <button
                type="button"
                onClick={() => { setIsManual(true); setSelectedSsid(''); onClearStatus() }}
                className="w-full py-2 rounded-lg border border-dashed border-zinc-800 hover:border-zinc-700 text-zinc-400 hover:text-amber-400 text-xs transition-colors flex items-center justify-center gap-1.5"
              >
                <Plus size={13} />
                {t('settings.manualSsid')}
              </button>
            </div>
          ) : (
            <div className="space-y-2">
              <div className="flex items-center justify-between">
                <span className="text-xs text-zinc-400">{t('settings.manualSsid')}</span>
                <button
                  type="button"
                  onClick={() => { setIsManual(false); onClearStatus() }}
                  className="text-[11px] text-amber-400 hover:underline"
                >
                  {t('settings.selectNetwork')}
                </button>
              </div>
              <Input
                value={manualSsid}
                onChange={e => setManualSsid(e.target.value)}
                placeholder={t('settings.ssidPlaceholder')}
                className="bg-zinc-950 border-zinc-700 text-zinc-100 text-xs h-9"
              />
            </div>
          )}

          {/* Password & Connect button (shown when an SSID is picked or manual) */}
          {(selectedSsid || isManual) && (
            <form onSubmit={handleConnect} className="space-y-2.5 pt-1 border-t border-zinc-800/80">
              <div className="space-y-1">
                <label className="text-[11px] text-zinc-400">
                  {t('settings.password')} {activeSsid ? `for "${activeSsid}"` : ''}
                </label>
                <div className="relative">
                  <Input
                    type={showPassword ? 'text' : 'password'}
                    value={password}
                    onChange={e => setPassword(e.target.value)}
                    placeholder={t('settings.passwordPlaceholder')}
                    className="bg-zinc-950 border-zinc-700 text-zinc-100 text-xs h-9 pr-8"
                  />
                  <button
                    type="button"
                    onClick={() => setShowPassword(p => !p)}
                    className="absolute right-2.5 top-1/2 -translate-y-1/2 text-zinc-400 hover:text-zinc-200"
                  >
                    {showPassword ? <EyeSlash size={15} /> : <Eye size={15} />}
                  </button>
                </div>
              </div>

              {/* Status alerts */}
              {joinStatus.status === 'connecting' && (
                <div className="flex items-center gap-2 p-2 rounded-lg bg-amber-500/10 border border-amber-500/20 text-amber-400 text-xs">
                  <ArrowsClockwise size={14} className="animate-spin shrink-0" />
                  <span>{t('settings.connecting')}</span>
                </div>
              )}

              {joinStatus.status === 'connected' && (
                <div className="flex items-center gap-2 p-2.5 rounded-lg bg-emerald-500/10 border border-emerald-500/20 text-emerald-400 text-xs">
                  <CheckCircle size={16} className="shrink-0" />
                  <div className="truncate">
                    <p className="font-semibold">
                      {t('settings.wifiConnectedMsg', { ssid: joinStatus.ssid || activeSsid, ip: joinStatus.ip || '' })}
                    </p>
                  </div>
                </div>
              )}

              {joinStatus.status === 'failed' && (
                <div className="flex items-center gap-2 p-2.5 rounded-lg bg-red-500/10 border border-red-500/20 text-red-400 text-xs">
                  <WarningCircle size={16} className="shrink-0" />
                  <div className="min-w-0 flex-1 break-words">
                    <p className="font-medium">
                      {t('settings.wifiFailedMsg', { error: joinStatus.error || 'Check password' })}
                    </p>
                  </div>
                </div>
              )}

              <div className="flex gap-2">
                <Button
                  type="submit"
                  disabled={!activeSsid.trim() || joinStatus.status === 'connecting'}
                  className="flex-1 bg-amber-400 hover:bg-amber-300 text-zinc-950 font-semibold text-xs h-9"
                >
                  {joinStatus.status === 'connecting'
                    ? t('settings.connecting')
                    : (TRANSPORT === 'ble' ? t('settings.joinViaBle') : t('settings.join'))}
                </Button>
                {isConnected && (
                  <Button
                    type="button"
                    variant="outline"
                    onClick={() => setShowJoinForm(false)}
                    className="border-zinc-700 text-zinc-400 text-xs h-9"
                  >
                    {t('settings.cancel')}
                  </Button>
                )}
              </div>
            </form>
          )}
        </div>
      )}
    </div>
  )
}
