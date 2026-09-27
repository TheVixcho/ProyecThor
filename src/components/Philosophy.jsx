import { IconNoSub, IconUser, IconCode } from './Icons'

export default function Philosophy({ strings }) {
  const { philosophy } = strings
  const icons = [IconNoSub, IconUser, IconCode]

  return (
    <section className="section philosophy">
      <div className="container">
        <div className="section-head">
          <h2>{philosophy.heading}</h2>
          <p className="section-lead">{philosophy.lead}</p>
        </div>

        <div className="philosophy-grid">
          {philosophy.pillars.map((pillar, index) => {
            const Icon = icons[index] ?? IconCode
            return (
              <div className="philosophy-item" key={pillar.title}>
                <div className="philosophy-icon">
                  <Icon width={22} height={22} />
                </div>
                <h3>{pillar.title}</h3>
                <p>{pillar.text}</p>
              </div>
            )
          })}
        </div>
      </div>
    </section>
  )
}
