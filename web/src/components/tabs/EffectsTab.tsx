import { useTranslation } from 'react-i18next'
import { EffectCard } from '@/components/shared/EffectCard'
import { ParamSlider } from '@/components/shared/ParamSlider'
import { Badge } from '@/components/ui/badge'
import type { LedState } from '@/hooks/useLedState'

const EFFECTS = [
  { id: 'solid',      label: 'Solid' },
  { id: 'colortemp',  label: 'Color Temp' },
  { id: 'rainbow',    label: 'Rainbow' },
  { id: 'comet',      label: 'Comet' },
  { id: 'cylon',      label: 'Cylon' },
  { id: 'theater',    label: 'Theater Chase' },
  { id: 'running',    label: 'Running Lights' },
  { id: 'fire2012',   label: 'Fire 2012' },
  { id: 'lava',       label: 'Lava' },
  { id: 'ocean',      label: 'Ocean' },
  { id: 'twinkle',    label: 'Twinkle' },
  { id: 'meteor',     label: 'Meteor Rain' },
  { id: 'sparkle',    label: 'Sparkle' },
  { id: 'breathing',  label: 'Breathing' },
  { id: 'strobe',     label: 'Strobe' },
  { id: 'perlinflow', label: 'Perlin Flow' },
  { id: 'ambilight',  label: 'Ambilight' },
] as const

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
}

export function EffectsTab({ state, update }: Props) {
  const { t } = useTranslation()
  const active = EFFECTS.find(e => e.id === state.effect)

  return (
    <div className="space-y-4">
      {active && (
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-3">
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium text-zinc-100">{active.label}</span>
            <Badge variant="outline" className="text-[10px] border-amber-400/40 text-amber-400">
              {t('effects.active')}
            </Badge>
          </div>
          <ParamSlider
            label={t('effects.speed')}
            value={state.speed}
            onChange={v => update({ speed: v })}
          />
          <ParamSlider
            label={t('effects.intensity')}
            value={state.intensity}
            onChange={v => update({ intensity: v })}
          />
        </div>
      )}
      <div className="grid grid-cols-2 gap-2">
        {EFFECTS.map(e => (
          <EffectCard
            key={e.id}
            id={e.id}
            label={e.label}
            active={state.effect === e.id}
            onClick={() => update({ effect: e.id })}
          />
        ))}
      </div>
    </div>
  )
}
