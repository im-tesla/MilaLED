import { useState } from 'react'
import { AnimatePresence, motion } from 'motion/react'
import { useTranslation } from 'react-i18next'
import { Header } from '@/components/layout/Header'
import { BrightnessBar } from '@/components/layout/BrightnessBar'
import { TabBar, type TabId } from '@/components/layout/TabBar'
import { useLedState } from '@/hooks/useLedState'

// Tab content placeholders — replaced in Tasks 19-22
function EffectsTab() { return <div className="text-zinc-400 text-sm">Effects tab</div> }
function ColorTab()   { return <div className="text-zinc-400 text-sm">Color tab</div> }
function PresetsTab() { return <div className="text-zinc-400 text-sm">Presets tab</div> }
function SettingsTab(){ return <div className="text-zinc-400 text-sm">Settings tab</div> }

const TAB_COMPONENTS: Record<TabId, React.ComponentType> = {
  effects:  EffectsTab,
  color:    ColorTab,
  presets:  PresetsTab,
  settings: SettingsTab,
}

const WS_URL = `ws://${window.location.hostname}:81`

export default function App() {
  const [activeTab, setActiveTab] = useState<TabId>('effects')
  const [isDark, setIsDark] = useState(true)
  useTranslation() // ensure i18n is initialized
  const { state, update, status } = useLedState(WS_URL)

  const handleThemeToggle = () => {
    setIsDark(d => !d)
    document.documentElement.classList.toggle('dark')
  }

  const ActiveTab = TAB_COMPONENTS[activeTab]

  return (
    <div className="min-h-[100dvh] flex flex-col bg-zinc-950">
      <Header
        state={state}
        isDark={isDark}
        wsStatus={status}
        onPowerToggle={() => update({ power: !state.power })}
        onThemeToggle={handleThemeToggle}
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
            <ActiveTab />
          </motion.div>
        </AnimatePresence>
      </main>
      <TabBar active={activeTab} onSelect={setActiveTab} />
    </div>
  )
}
