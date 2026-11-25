import OsDownloadCta from './OsDownloadCta'

export default function DownloadCta({ strings }) {
  const { downloadCta } = strings

  return (
    <section className="section">
      <div className="container">
        <div className="download-cta">
          <div className="download-glow" aria-hidden="true" />
          <h2>{downloadCta.heading}</h2>
          <p>{downloadCta.text}</p>
          <OsDownloadCta variant="buttons" />
        </div>
      </div>
    </section>
  )
}
