import { Trash } from '@phosphor-icons/react'

interface Props {
  name: string
  subtitle: string
  onLoad: () => void
  onDelete: () => void
}

export function PresetCard({ name, subtitle, onLoad, onDelete }: Props) {
  return (
    <div className="rounded-xl border bg-zinc-900 border-zinc-800 p-3 flex items-center justify-between gap-3">
      <button onClick={onLoad} className="flex-1 text-left min-w-0">
        <div className="text-sm font-medium text-zinc-100 truncate">{name}</div>
        <div className="text-xs text-zinc-500 truncate mt-0.5">{subtitle}</div>
      </button>
      <button
        onClick={onDelete}
        className="text-zinc-600 hover:text-red-400 transition-colors p-1 shrink-0"
      >
        <Trash size={16} />
      </button>
    </div>
  )
}
