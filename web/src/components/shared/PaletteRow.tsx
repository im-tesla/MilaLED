import { cn } from '@/lib/utils'

const PALETTES = [
  { id: 'RainbowColors', gradient: 'linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f)' },
  { id: 'HeatColors',    gradient: 'linear-gradient(90deg,#000,#800,#f00,#ff0,#fff)' },
  { id: 'OceanColors',   gradient: 'linear-gradient(90deg,#006994,#0099cc,#87ceeb)' },
  { id: 'LavaColors',    gradient: 'linear-gradient(90deg,#000,#400,#c00,#f60,#ff0)' },
  { id: 'CloudColors',   gradient: 'linear-gradient(90deg,#6be,#adf,#fff,#adf,#6be)' },
  { id: 'PartyColors',   gradient: 'linear-gradient(90deg,#f0f,#f00,#ff0,#0f0,#0ff,#00f)' },
  { id: 'ForestColors',  gradient: 'linear-gradient(90deg,#030,#060,#3a3,#6c6,#9f9)' },
] as const

interface Props {
  value: string
  onChange: (id: string) => void
}

export function PaletteRow({ value, onChange }: Props) {
  return (
    <div className="flex gap-2 flex-wrap">
      {PALETTES.map(p => (
        <button
          key={p.id}
          onClick={() => onChange(p.id)}
          title={p.id}
          className={cn(
            'h-7 w-16 rounded-md border transition-all active:scale-95',
            value === p.id
              ? 'border-amber-400 ring-1 ring-amber-400/40'
              : 'border-zinc-700'
          )}
          style={{ background: p.gradient }}
        />
      ))}
    </div>
  )
}
