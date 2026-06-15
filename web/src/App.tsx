import { useState } from 'react'
import { AnimatePresence, motion } from 'motion/react'
import { Header } from '@/components/layout/Header'
import { BrightnessBar } from '@/components/layout/BrightnessBar'
import { TabBar, type TabId } from '@/components/layout/TabBar'
import { EffectsTab } from '@/components/tabs/EffectsTab'
import { ColorTab } from '@/components/tabs/ColorTab'
import { PresetsTab } from '@/components/tabs/PresetsTab'
import { SettingsTab } from '@/components/tabs/SettingsTab'
import { useLedState } from '@/hooks/useLedState'

const WS_URL = `ws://${window.location.hostname}:81`

export default function App() {
  const [activeTab, setActiveTab] = useState<TabId>('effects')
  const { state, update, status, scanProgress, foundTvs } = useLedState(WS_URL)

  return (
    <div className="min-h-[100dvh] flex flex-col bg-zinc-950">
      <Header
        state={state}
        wsStatus={status}
        onPowerToggle={() => update({ power: !state.power })}
      />
      <BrightnessBar
        value={state.brightness}
        onChange={v => update({ brightness: v })}
      />
      <main className="flex-1 overflow-y-auto pb-24">
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
              <PresetsTab state={state} update={update} />
            )}
            {activeTab === 'settings' && (
              <SettingsTab
                state={state}
                update={update}
                scanProgress={scanProgress}
                foundTvs={foundTvs}
              />
            )}
          </motion.div>
        </AnimatePresence>
      </main>
      <TabBar active={activeTab} onSelect={setActiveTab} />
    </div>
  )
}
