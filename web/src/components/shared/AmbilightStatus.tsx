import { useTranslation } from 'react-i18next'
import { Badge } from '@/components/ui/badge'

interface Props {
  effect: string
  ambStatus?: string
}

export function AmbilightStatus({ effect, ambStatus }: Props) {
  const { t } = useTranslation()
  const active = effect === 'ambilight'

  if (!active) {
    return (
      <Badge variant="outline" className="text-[10px] border-zinc-700 text-zinc-500">
        {t('ambilight.idle')}
      </Badge>
    )
  }

  if (ambStatus === 'error') {
    return (
      <Badge variant="outline" className="text-[10px] border-red-500/40 text-red-400">
        {t('ambilight.error')}
      </Badge>
    )
  }

  return (
    <Badge variant="outline" className="text-[10px] border-green-500/40 text-green-400 animate-pulse">
      {t('ambilight.polling')}
    </Badge>
  )
}
