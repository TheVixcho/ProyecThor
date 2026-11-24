import { IconBolt, IconDualScreen, IconLayers, IconBroadcast } from './Icons'

const PILL_ICONS = [IconBolt, IconBroadcast, IconLayers, IconDualScreen]
const PILL_ANIMS = ['icon-anim-bolt', 'icon-anim-wave', 'icon-anim-float', 'icon-anim-pulse']

export default function Showcase({ strings }) {
  const { showcase } = strings

  return (
    <section className="section section-alt showcase">
      <div className="container">
        <div className="section-head">
          <h2>{showcase.heading}</h2>
          <p className="section-lead">{showcase.lead}</p>
        </div>

        <div className="engine-pills engine-pills-solo">
          {showcase.pills.map((pill, idx) => {
            const Icon = PILL_ICONS[idx % PILL_ICONS.length]
            const animClass = PILL_ANIMS[idx % PILL_ANIMS.length]
            return (
              <div className="engine-pill" key={pill.label}>
                <div className={`engine-pill-icon ${animClass}`}>
                  <Icon width={20} height={20} />
                </div>
                <div>
                  <strong>{pill.label}</strong>
                  <span>{pill.desc}</span>
                </div>
              </div>
            )
          })}
        </div>
      </div>
    </section>
  )
}
