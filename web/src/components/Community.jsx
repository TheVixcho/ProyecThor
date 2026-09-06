import { LINKS } from '../links'
import { IconWhatsapp, IconGithub, IconHeart, IconArrowRight } from './Icons'

export default function Community({ strings }) {
  const { community } = strings
  const channels = [
    {
      icon: IconWhatsapp,
      title: community.channels.whatsapp.title,
      text: community.channels.whatsapp.text,
      href: LINKS.whatsapp,
      cta: community.channels.whatsapp.cta,
    },
    {
      icon: IconGithub,
      title: community.channels.github.title,
      text: community.channels.github.text,
      href: LINKS.repo,
      cta: community.channels.github.cta,
    },
  ]

  return (
    <section id="comunidad" className="section section-alt">
      <div className="container">
        <div className="section-head">
          <h2>{community.heading}</h2>
          <p className="section-lead">{community.lead}</p>
        </div>

        <div className="community-grid">
          {channels.map((c) => (
            <a className="community-card" key={c.title} href={c.href} target="_blank" rel="noreferrer">
              <div className="community-icon">
                <c.icon width={22} height={22} />
              </div>
              <h3>{c.title}</h3>
              <p>{c.text}</p>
              <span className="community-link">
                {c.cta}
                <IconArrowRight width={16} height={16} />
              </span>
            </a>
          ))}
        </div>

        <div className="donate-banner">
          <div className="donate-icon">
            <IconHeart width={22} height={22} />
          </div>
          <div className="donate-copy">
            <h3>{community.donateTitle}</h3>
            <p>{community.donateText}</p>
          </div>
          <a className="btn btn-primary" href={LINKS.kofi} target="_blank" rel="noreferrer">
            {community.donateCta}
          </a>
        </div>
      </div>
    </section>
  )
}
