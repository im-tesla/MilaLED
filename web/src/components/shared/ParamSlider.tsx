import { Slider } from '@/components/ui/slider'

interface Props {
  label: string
  value: number
  min?: number
  max?: number
  onChange: (v: number) => void
}

export function ParamSlider({ label, value, min = 0, max = 255, onChange }: Props) {
  return (
    <div className="space-y-1.5">
      <div className="flex justify-between items-center">
        <span className="text-xs text-zinc-400">{label}</span>
        <span className="text-xs text-zinc-300 tabular-nums">{value}</span>
      </div>
      <Slider
        min={min}
        max={max}
        step={1}
        value={[value]}
        onValueChange={([v]) => onChange(v)}
        className="[&_[role=slider]]:h-4 [&_[role=slider]]:w-4 [&_[role=slider]]:border-amber-400"
      />
    </div>
  )
}
