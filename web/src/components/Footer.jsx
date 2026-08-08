import logo from '../assets/logo.png'
import { LINKS } from '../links'

export default function Footer({ strings }) {
  const year = new Date().getFullYear()
  const { footer } = strings

  return (
    <footer className="footer">
      <div className="container footer-inner">
        <div className="footer-brand">
          <img src={logo} alt="" className="brand-mark" />
          <div>
            <strong>ProyecThor</strong>
            <p>{footer.brandText}</p>
          </div>
        </div>

        <div className="footer-links">
          <div>
            <h4>{footer.project}</h4>
            <a href={LINKS.repo} target="_blank" rel="noreferrer">{footer.repo}</a>
            <a href={LINKS.releases} target="_blank" rel="noreferrer">{footer.releases}</a>
            <a href={LINKS.wiki} target="_blank" rel="noreferrer">{footer.wiki}</a>
            <a href={LINKS.issues} target="_blank" rel="noreferrer">{footer.issues}</a>
          </div>
          <div>
            <h4>{footer.community}</h4>
            <a href={LINKS.playStore} target="_blank" rel="noreferrer">{footer.mobileApp}</a>
            <a href={LINKS.whatsapp} target="_blank" rel="noreferrer">{footer.whatsapp}</a>
            <a href={LINKS.discord} target="_blank" rel="noreferrer">{footer.discord}</a>
            <a href={LINKS.kofi} target="_blank" rel="noreferrer">{footer.donations}</a>
          </div>
        </div>
      </div>

      <div className="container footer-bottom">
        <span>&copy; {year} {footer.copyright}</span>
        <span>{footer.license}</span>
      </div>
    </footer>
  )
}
