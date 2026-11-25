import { LINKS } from '../links'
import { IconArrowRight } from './Icons'
import OsDownloadCta from './OsDownloadCta'

export default function Hero({ strings }) {
  const { hero } = strings

  return (
    <section id="home" className="hero">
      <div className="hero-glow" aria-hidden="true" />

      <div className="container hero-inner">
        <div className="hero-copy">
          <h1>
            {hero.titlePrefix} <span className="text-gradient">{hero.titleHighlight}</span>
          </h1>
          <p className="hero-lead">{hero.lead}</p>

          <div className="hero-actions">
            <OsDownloadCta variant="buttons" />
            <a className="btn btn-outline btn-lg" href={LINKS.repo} target="_blank" rel="noreferrer">
              {hero.github}
              <IconArrowRight width={18} height={18} />
            </a>
          </div>
        </div>
      </div>
    </section>
  )
}
