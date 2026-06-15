import { useTranslation } from 'react-i18next'
import { Lightning, PaintBrush, FloppyDisk, GearSix } from '@phosphor-icons/react'

const TABS = [
  { id: 'effects',  Icon: Lightning,  labelKey: 'tabs.effects' },
  { id: 'color',    Icon: PaintBrush, labelKey: 'tabs.color' },
  { id: 'presets',  Icon: FloppyDisk, labelKey: 'tabs.presets' },
  { id: 'settings', Icon: GearSix,    labelKey: 'tabs.settings' },
] as const

export type TabId = typeof TABS[number]['id']

interface Props {
  active: TabId
  onSelect: (id: TabId) => void
}

export function TabBar({ active, onSelect }: Props) {
  const { t } = useTranslation()
  return (
    <nav className="fixed bottom-0 left-0 right-0 bg-zinc-950/95 backdrop-blur border-t border-zinc-800 pb-4 z-10">
      <div className="grid grid-cols-4">
        {TABS.map(({ id, Icon, labelKey }) => {
          const isActive = active === id
          return (
            <button
              key={id}
              onClick={() => onSelect(id)}
              className={`flex flex-col items-center gap-1 py-3 transition-colors active:scale-95 ${
                isActive ? 'text-amber-400' : 'text-zinc-500 hover:text-zinc-300'
              }`}
            >
              <Icon size={22} weight={isActive ? 'fill' : 'regular'} />
              <span className="text-[10px] font-medium">{t(labelKey)}</span>
            </button>
          )
        })}
      </div>
    </nav>
  )
}
