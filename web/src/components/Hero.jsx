import { LINKS } from '../links'
import { IconArrowRight, IconDownload } from './Icons'

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
            <a className="btn btn-primary btn-lg" href={LINKS.releases} target="_blank" rel="noreferrer">
              <IconDownload width={18} height={18} />
              {hero.download}
            </a>
            <a className="btn btn-outline btn-lg" href={LINKS.repo} target="_blank" rel="noreferrer">
              {hero.github}
              <IconArrowRight width={18} height={18} />
            </a>
          </div>
        </div>

        <div className="hero-visual" aria-hidden="true">
          <div className="mock-window">
            <div className="mock-titlebar">
              <span className="mock-dot" />
              <span className="mock-dot" />
              <span className="mock-dot" />
              <span className="mock-title">ProyecThor — Vista en Vivo</span>
            </div>
            <div className="mock-body">
              <div className="mock-rail">
                <span className="mock-rail-item active" />
                <span className="mock-rail-item" />
                <span className="mock-rail-item" />
                <span className="mock-rail-item" />
              </div>
              <div className="mock-stage">
                <span className="mock-stage-text">EN VIVO</span>
                <div className="mock-stage-bar" />
                <div className="mock-stage-bar short" />
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>
  )
}
