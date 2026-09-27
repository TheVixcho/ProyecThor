import { useEffect, useState } from 'react'
import logo from '../assets/logo.png'
import { LINKS } from '../links'
import { IconMenu, IconClose } from './Icons'

export default function Navbar({ strings, language, setLanguage, labels }) {
  const [scrolled, setScrolled] = useState(false)
  const [open, setOpen] = useState(false)

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 12)
    onScroll()
    window.addEventListener('scroll', onScroll, { passive: true })
    return () => window.removeEventListener('scroll', onScroll)
  }, [])

  const links = strings.nav.links

  return (
    <header className={`navbar ${scrolled ? 'is-scrolled' : ''}`}>
      <div className="navbar-inner container">
        <a href="#home" className="brand">
          <img src={logo} alt="ProyecThor logo" className="brand-mark" />
          <span>ProyecThor</span>
        </a>

        <nav className="navbar-links">
          {links.map((l) => (
            <a key={l.href} href={l.href}>
              {l.label}
            </a>
          ))}
        </nav>

        <div className="navbar-actions">
          <div className="navbar-lang">
            <label htmlFor="language-select" className="visually-hidden">
              {strings.floating.language}
            </label>
            <select
              id="language-select"
              value={language}
              onChange={(event) => setLanguage(event.target.value)}
            >
              {Object.entries(labels).map(([code, label]) => (
                <option key={code} value={code}>
                  {label}
                </option>
              ))}
            </select>
          </div>
          <a className="btn btn-ghost" href={LINKS.repo} target="_blank" rel="noreferrer">
            {strings.nav.github}
          </a>
          <a className="btn btn-primary" href={LINKS.releases} target="_blank" rel="noreferrer">
            {strings.nav.download}
          </a>
        </div>

        <button
          type="button"
          className="navbar-burger"
          aria-label="Abrir menú"
          onClick={() => setOpen((v) => !v)}
        >
          {open ? <IconClose /> : <IconMenu />}
        </button>
      </div>

      {open && (
        <div className="navbar-mobile">
          {links.map((l) => (
            <a key={l.href} href={l.href} onClick={() => setOpen(false)}>
              {l.label}
            </a>
          ))}
          <a href={LINKS.repo} target="_blank" rel="noreferrer">
            {strings.nav.github}
          </a>
          <a className="btn btn-primary" href={LINKS.releases} target="_blank" rel="noreferrer">
            {strings.nav.download}
          </a>
          <div className="navbar-lang navbar-lang-mobile">
            <select
              value={language}
              onChange={(event) => setLanguage(event.target.value)}
            >
              {Object.entries(labels).map(([code, label]) => (
                <option key={code} value={code}>
                  {label}
                </option>
              ))}
            </select>
          </div>
        </div>
      )}
    </header>
  )
}
