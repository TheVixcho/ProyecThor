import { LINKS } from '../links'
import { IconDownload } from './Icons'

export default function DownloadCta({ strings }) {
  const { downloadCta } = strings

  return (
    <section className="section">
      <div className="container">
        <div className="download-cta">
          <div className="download-glow" aria-hidden="true" />
          <h2>{downloadCta.heading}</h2>
          <p>{downloadCta.text}</p>
          <a className="btn btn-primary btn-lg" href={LINKS.releases} target="_blank" rel="noreferrer">
            <IconDownload width={18} height={18} />
            {downloadCta.cta}
          </a>
        </div>
      </div>
    </section>
  )
}
