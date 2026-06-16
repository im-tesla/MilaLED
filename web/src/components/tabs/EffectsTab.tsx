import { useTranslation } from 'react-i18next'
import { EffectCard } from '@/components/shared/EffectCard'
import { ParamSlider } from '@/components/shared/ParamSlider'
import { Badge } from '@/components/ui/badge'
import type { LedState } from '@/hooks/useLedState'

const EFFECT_IDS = [
  'solid', 'colortemp', 'rainbow', 'comet', 'cylon',
  'theater', 'running', 'fire2012', 'lava', 'ocean',
  'twinkle', 'meteor', 'sparkle', 'breathing', 'strobe',
  'perlinflow', 'ambilight', 'hyperion',
] as const

type EffectId = typeof EFFECT_IDS[number]

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
}

export function EffectsTab({ state, update }: Props) {
  const { t } = useTranslation()
  const activeId = EFFECT_IDS.includes(state.effect as EffectId) ? state.effect as EffectId : null

  // Effects that don't use speed / intensity sliders
  const noSliders  = activeId === 'ambilight' || activeId === 'hyperion'
  const noSpeed    = noSliders
  const noIntensity = noSliders

  return (
    <div className="space-y-4">
      {activeId && (
        <div className="rounded-xl bg-zinc-900 border border-zinc-800 p-3 space-y-3">
          <div className="flex items-center justify-between">
            <span className="text-sm font-medium text-zinc-100">
              {t(`effects.names.${activeId}`)}
            </span>
            <Badge variant="outline" className="text-[10px] border-amber-400/40 text-amber-400">
              {t('effects.active')}
            </Badge>
          </div>
          {!noSpeed && (
            <ParamSlider
              label={t('effects.speed')}
              value={state.speed}
              onChange={v => update({ speed: v })}
            />
          )}
          {!noIntensity && (
            <ParamSlider
              label={t('effects.intensity')}
              value={state.intensity}
              onChange={v => update({ intensity: v })}
            />
          )}
        </div>
      )}
      <div className="grid grid-cols-2 gap-2">
        {EFFECT_IDS.map(id => (
          <EffectCard
            key={id}
            id={id}
            label={t(`effects.names.${id}`)}
            active={state.effect === id}
            onClick={() => update({ effect: id })}
          />
        ))}
      </div>
    </div>
  )
}
