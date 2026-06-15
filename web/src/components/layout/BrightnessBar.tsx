import { useTranslation } from 'react-i18next'
import { Slider } from '@/components/ui/slider'
import { Sun } from '@phosphor-icons/react'

interface Props {
  value: number
  onChange: (v: number) => void
}

export function BrightnessBar({ value, onChange }: Props) {
  const { t } = useTranslation()
  return (
    <div className="px-4 py-2.5 border-b border-zinc-800 flex items-center gap-3">
      <Sun size={14} className="text-zinc-400 shrink-0" />
      <span className="text-xs text-zinc-500 shrink-0">{t('brightness')}</span>
      <Slider
        min={0}
        max={255}
        step={1}
        value={[value]}
        onValueChange={([v]) => onChange(v)}
        className="flex-1 [&_[role=slider]]:h-3.5 [&_[role=slider]]:w-3.5 [&_[role=slider]]:border-zinc-400"
      />
      <span className="text-xs text-zinc-400 w-9 text-right tabular-nums">
        {Math.round((value / 255) * 100)}%
      </span>
    </div>
  )
}
