import { useMemo } from 'react'
import { LINKS } from '../links'
import { detectUserOS } from '../utils/detectOs'
import { IconWindows, IconLinux, IconMac } from './Icons'

const OS_ITEMS = [
  { id: 'windows', label: 'Windows', Icon: IconWindows },
  { id: 'linux', label: 'Linux', Icon: IconLinux },
  { id: 'mac', label: 'Mac', Icon: IconMac },
]

export default function OsDownloadCta({ variant = 'buttons', label = 'Windows, Linux, Mac', className = '' }) {
  const orderedItems = useMemo(() => {
    const detected = detectUserOS()
    return [...OS_ITEMS].sort((a, b) => (a.id === detected ? -1 : b.id === detected ? 1 : 0))
  }, [])

  if (variant === 'compact') {
    return (
      <a
        className={`btn btn-primary btn-os-compact ${className}`.trim()}
        href={LINKS.releases}
        target="_blank"
        rel="noreferrer"
        aria-label={label}
        title={orderedItems.map((i) => i.label).join(' • ')}
      >
        {orderedItems.map(({ id, Icon }) => (
          <Icon key={id} width={16} height={16} />
        ))}
      </a>
    )
  }

  return (
    <div className={`os-cta-group ${className}`.trim()}>
      {orderedItems.map(({ id, label: osLabel, Icon }, index) => (
        <a
          key={id}
          className={`btn btn-lg btn-os ${index === 0 ? 'btn-primary btn-os-featured' : 'btn-outline btn-os-secondary'}`}
          href={LINKS.releases}
          target="_blank"
          rel="noreferrer"
          aria-label={osLabel}
          title={osLabel}
        >
          <Icon width={19} height={19} />
          <span>{osLabel}</span>
        </a>
      ))}
    </div>
  )
}
