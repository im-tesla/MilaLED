import { useState } from 'react'
import { useTranslation } from 'react-i18next'
import { HexColorPicker } from 'react-colorful'
import { PaletteRow } from '@/components/shared/PaletteRow'
import type { LedState } from '@/hooks/useLedState'

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
}

export function ColorTab({ state, update }: Props) {
  const { t } = useTranslation()
  const [picking, setPicking] = useState<'primary' | 'secondary'>('primary')

  return (
    <div className="space-y-4">
      <div className="flex justify-center">
        <HexColorPicker
          color={picking === 'primary' ? state.colorPrimary : state.colorSecondary}
          onChange={c =>
            update(picking === 'primary' ? { colorPrimary: c } : { colorSecondary: c })
          }
          style={{ width: '100%', maxWidth: 280, height: 200 }}
        />
      </div>

      <div className="grid grid-cols-2 gap-3">
        {(['primary', 'secondary'] as const).map(slot => (
          <button
            key={slot}
            onClick={() => setPicking(slot)}
            className={`rounded-xl p-3 border transition-all text-left ${
              picking === slot ? 'border-amber-400/60' : 'border-zinc-800'
            }`}
          >
            <span className="text-xs text-zinc-500 block mb-2">
              {slot === 'primary' ? t('color.primary') : t('color.secondary')}
            </span>
            <div
              className="h-8 rounded-md border border-zinc-700"
              style={{
                background: slot === 'primary' ? state.colorPrimary : state.colorSecondary,
              }}
            />
          </button>
        ))}
      </div>

      <div className="space-y-2">
        <span className="text-xs text-zinc-500">{t('color.palette')}</span>
        <PaletteRow value={state.palette} onChange={v => update({ palette: v })} />
      </div>
    </div>
  )
}
