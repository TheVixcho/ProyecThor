import { useMemo } from 'react'
import { detectUserOS } from '../utils/detectOs'
import { IconWindows, IconLinux, IconMac, IconMobile, IconCheck } from './Icons'

const OS_ICON_MAP = {
  windows: IconWindows,
  linux: IconLinux,
  mac: IconMac,
  android: IconMobile,
}

export default function Platforms({ strings }) {
  const detectedOS = useMemo(() => detectUserOS(), [])

  const orderedItems = useMemo(() => {
    return [...strings.platforms.items].sort((a, b) => {
      if (a.os === detectedOS) return -1
      if (b.os === detectedOS) return 1
      return 0
    })
  }, [strings.platforms.items, detectedOS])

  return (
    <section id="plataformas" className="section section-alt">
      <div className="container">
        <div className="section-head">
          <h2>{strings.platforms.heading}</h2>
          <p className="section-lead">{strings.platforms.lead}</p>
        </div>

        <div className="platforms-grid">
          {orderedItems.map((item) => {
            const Icon = OS_ICON_MAP[item.os] || IconWindows
            const isDetected = item.os === detectedOS
            return (
              <div className={`platform-card ${isDetected ? 'is-detected' : ''}`.trim()} key={item.name}>
                <div className="platform-head">
                  <div className="platform-icon">
                    <Icon width={26} height={26} />
                  </div>
                  <div className="platform-head-copy">
                    <div className="platform-title-row">
                      <h3>{item.name}</h3>
                      {isDetected && strings.platforms.detectedBadge && (
                        <span className="platform-detected-badge">{strings.platforms.detectedBadge}</span>
                      )}
                    </div>
                    <span className="platform-status">
                      <IconCheck width={14} height={14} />
                      {item.status}
                    </span>
                  </div>
                </div>
                <p className="platform-note">{item.note}</p>

                {item.tiers.map((tier) => (
                  <div className="platform-tier" key={tier.label}>
                    <span className="platform-tier-label">{tier.label}</span>
                    <dl className="platform-specs">
                      {tier.specs.map(([k, v]) => (
                        <div key={k}>
                          <dt>{k}</dt>
                          <dd>{v}</dd>
                        </div>
                      ))}
                    </dl>
                  </div>
                ))}
              </div>
            )
          })}
        </div>
      </div>
    </section>
  )
}
