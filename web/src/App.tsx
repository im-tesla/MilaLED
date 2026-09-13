import { useState, useEffect } from 'react'
import { AnimatePresence, motion } from 'motion/react'
import { Header } from '@/components/layout/Header'
import { BrightnessBar } from '@/components/layout/BrightnessBar'
import { TabBar, type TabId } from '@/components/layout/TabBar'
import { EffectsTab } from '@/components/tabs/EffectsTab'
import { ColorTab } from '@/components/tabs/ColorTab'
import { PresetsTab } from '@/components/tabs/PresetsTab'
import { SettingsTab } from '@/components/tabs/SettingsTab'
import { BleConnectDialog } from '@/components/shared/BleConnectDialog'
import { CaptivePortalPanel } from '@/components/shared/CaptivePortalPanel'
import { useLedState } from '@/hooks/useLedState'
import { TRANSPORT } from '@/lib/capabilities'

const WS_URL = `ws://${window.location.hostname}:81`

export default function App() {
  const [activeTab, setActiveTab] = useState<TabId>('effects')
  const {
    state,
    update,
    status,
    scanProgress,
    foundTvs,
    presets,
    sendCommand,
    connect,
    error,
    wifiNetworks,
    wifiScanning,
    wifiJoinStatus,
    scanWifi,
    joinWifi,
    clearWifiJoinStatus,
  } = useLedState(WS_URL)
  const [isDark, setIsDark] = useState(() => localStorage.getItem('theme') !== 'light')

  useEffect(() => {
    document.documentElement.classList.toggle('dark', isDark)
    localStorage.setItem('theme', isDark ? 'dark' : 'light')
  }, [isDark])

  if (TRANSPORT === 'ble' && status !== 'open') {
    return <BleConnectDialog status={status} error={error} onConnect={connect} />
  }

  // Detect whether we are running in Captive Portal / AP mode
  const isCaptivePortal = Boolean(
    state.isAp ||
    (!state.wifiConnected && TRANSPORT !== 'ble' && typeof window !== 'undefined' &&
      (window.location.hostname === '192.168.4.1' || !state.ssid))
  )

  return (
    <div className="min-h-[100dvh] flex flex-col bg-background">
      <Header
        state={state}
        wsStatus={status}
        onPowerToggle={() => update({ power: !state.power })}
        isDark={isDark}
        onThemeToggle={() => setIsDark(d => !d)}
      />
      <BrightnessBar
        value={state.brightness}
        onChange={v => update({ brightness: v })}
      />
      <main className="flex-1 overflow-y-auto pb-24">
        {/* Captive Portal banner when in AP mode */}
        {isCaptivePortal && (
          <div className="p-4 pb-0">
            <CaptivePortalPanel
              networks={wifiNetworks}
              scanning={wifiScanning}
              joinStatus={wifiJoinStatus}
              onScan={scanWifi}
              onJoin={joinWifi}
              onClearStatus={clearWifiJoinStatus}
              currentIp={state.ip}
            />
          </div>
        )}

        <AnimatePresence mode="wait">
          <motion.div
            key={activeTab}
            initial={{ opacity: 0, y: 6 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -6 }}
            transition={{ duration: 0.18, ease: 'easeOut' }}
            className="p-4 space-y-4"
          >
            {activeTab === 'effects' && (
              <EffectsTab state={state} update={update} />
            )}
            {activeTab === 'color' && (
              <ColorTab state={state} update={update} />
            )}
            {activeTab === 'presets' && (
              <PresetsTab state={state} update={update} presets={presets} sendCommand={sendCommand} />
            )}
            {activeTab === 'settings' && (
              <SettingsTab
                state={state}
                update={update}
                sendCommand={sendCommand}
                scanProgress={scanProgress}
                foundTvs={foundTvs}
                wifiNetworks={wifiNetworks}
                wifiScanning={wifiScanning}
                wifiJoinStatus={wifiJoinStatus}
                onScanWifi={scanWifi}
                onJoinWifi={joinWifi}
                onClearWifiStatus={clearWifiJoinStatus}
              />
            )}
          </motion.div>
        </AnimatePresence>
      </main>
      <TabBar active={activeTab} onSelect={setActiveTab} />
    </div>
  )
}
