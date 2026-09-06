import {
  IconLayers,
  IconDualScreen,
  IconLibrary,
  IconOverlay,
  IconBroadcast,
  IconBolt,
  IconPalette,
  IconConvert,
  IconQueue,
  IconMobile,
} from './Icons'

export default function Features({ strings }) {
  const featureIcons = [
    IconLayers,
    IconDualScreen,
    IconLibrary,
    IconOverlay,
    IconBroadcast,
    IconBolt,
    IconPalette,
    IconConvert,
    IconQueue,
    IconMobile,
  ]
  const features = strings.features.map((feature, index) => ({
    ...feature,
    icon: featureIcons[index % featureIcons.length],
  }))

  return (
    <section id="caracteristicas" className="section">
      <div className="container">
        <div className="section-head">
          <h2>{strings.featuresHeading}</h2>
          <p className="section-lead">{strings.featuresLead}</p>
        </div>

        <div className="features-grid">
          {features.map(({ icon: Icon, title, text }) => (
            <div className="feature-card" key={title}>
              <div className="feature-icon">
                <Icon width={22} height={22} />
              </div>
              <h3>{title}</h3>
              <p>{text}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  )
}
