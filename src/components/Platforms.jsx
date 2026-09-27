import { IconWindows, IconLinux, IconMobile, IconCheck } from './Icons'

export default function Platforms({ strings }) {
  const platformIcons = [IconWindows, IconLinux, IconMobile]

  return (
    <section id="plataformas" className="section section-alt">
      <div className="container">
        <div className="section-head">
          <h2>{strings.platforms.heading}</h2>
          <p className="section-lead">{strings.platforms.lead}</p>
        </div>

        <div className="platforms-grid">
          {strings.platforms.items.map((item, index) => {
            const Icon = platformIcons[index % platformIcons.length]
            return (
              <div className="platform-card" key={item.name}>
                <div className="platform-head">
                  <div className="platform-icon">
                    <Icon width={26} height={26} />
                  </div>
                  <div>
                    <h3>{item.name}</h3>
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
