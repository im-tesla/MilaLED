import { cn } from '@/lib/utils'

interface Props {
  id: string
  label: string
  active: boolean
  onClick: () => void
}

export function EffectCard({ label, active, onClick }: Props) {
  return (
    <button
      onClick={onClick}
      className={cn(
        'w-full text-left rounded-lg px-3 py-2.5 text-sm font-medium transition-all active:scale-95',
        active
          ? 'bg-amber-400/10 border border-amber-400/40 text-amber-400'
          : 'bg-zinc-900 border border-zinc-800 text-zinc-400 hover:border-zinc-600 hover:text-zinc-200'
      )}
    >
      {label}
    </button>
  )
}
