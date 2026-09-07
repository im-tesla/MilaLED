import { useEffect, useState } from 'react'
import { useTranslation } from 'react-i18next'
import { Plus } from '@phosphor-icons/react'
import { PresetCard } from '@/components/shared/PresetCard'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import type { LedState, PresetData } from '@/hooks/useLedState'
import { TRANSPORT } from '@/lib/capabilities'

interface Props {
  state: LedState
  update: (p: Partial<LedState>) => void
  presets: PresetData[]
  sendCommand: (data: object) => void
}

export function PresetsTab({ state, update, presets, sendCommand }: Props) {
  const { t } = useTranslation()
  const [wifiPresets, setWifiPresets] = useState<PresetData[]>([])
  const [newName, setNewName] = useState('')

  const fetchPresets = () => {
    if (TRANSPORT === 'ble') {
      sendCommand({ action: 'presetList' })
      return
    }
    fetch('/api/presets').then(r => r.json()).then(setWifiPresets).catch(() => {})
  }

  useEffect(() => { fetchPresets() }, [])

  const visiblePresets = TRANSPORT === 'ble' ? presets : wifiPresets

  const save = async () => {
    if (!newName.trim()) return
    const p: PresetData = {
      name: newName.trim(),
      effect: state.effect,
      brightness: state.brightness,
      palette: state.palette,
    }
    if (TRANSPORT === 'ble') sendCommand({ action: 'presetSave', ...p })
    else await fetch('/api/presets', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(p),
    })
    setNewName('')
    if (TRANSPORT !== 'ble') fetchPresets()
  }

  const load = (p: PresetData) =>
    update({ effect: p.effect, brightness: p.brightness, palette: p.palette })

  const del = async (name: string) => {
    if (TRANSPORT === 'ble') sendCommand({ action: 'presetDelete', name })
    else await fetch('/api/presets', {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name }),
    })
    if (TRANSPORT !== 'ble') fetchPresets()
  }

  return (
    <div className="space-y-3">
      {visiblePresets.map(p => (
        <PresetCard
          key={p.name}
          name={p.name}
          subtitle={`${p.effect} · ${Math.round((p.brightness / 255) * 100)}% · ${p.palette}`}
          onLoad={() => load(p)}
          onDelete={() => del(p.name)}
        />
      ))}
      <div className="rounded-xl border-2 border-dashed border-zinc-800 p-3 space-y-2">
        <Input
          value={newName}
          onChange={e => setNewName(e.target.value)}
          placeholder={t('presets.name')}
          className="bg-zinc-900 border-zinc-700 text-sm h-9"
          onKeyDown={e => e.key === 'Enter' && save()}
        />
        <Button
          onClick={save}
          size="sm"
          variant="outline"
          className="w-full border-zinc-700 text-zinc-300 hover:text-amber-400 hover:border-amber-400/40"
        >
          <Plus size={14} className="mr-1.5" />
          {t('presets.save')}
        </Button>
      </div>
    </div>
  )
}
