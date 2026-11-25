import { useEffect, useMemo, useRef, useState } from 'react'
import { LINKS } from '../links'
import { detectUserOS, detectWindowsArm64 } from '../utils/detectOs'
import { useLatestDownloads } from '../utils/releaseDownloads'
import { IconWindows, IconLinux, IconMac, IconDownload } from './Icons'

const OS_ITEMS = [
  { id: 'windows', label: 'Windows', Icon: IconWindows },
  { id: 'linux', label: 'Linux', Icon: IconLinux },
  { id: 'mac', label: 'Mac', Icon: IconMac },
]

function buildMetaLine(dl) {
  const parts = []
  if (dl?.tag) parts.push(dl.tag)
  if (dl?.sizeLabel) parts.push(dl.sizeLabel)
  return parts.join(' • ')
}

export default function OsDownloadCta({ variant = 'buttons', label = 'Windows, Linux, Mac', className = '' }) {
  const downloads = useLatestDownloads()
  const [menuOpen, setMenuOpen] = useState(false)
  const [isWinArm64, setIsWinArm64] = useState(false)
  const wrapRef = useRef(null)

  const detectedOS = useMemo(() => detectUserOS(), [])

  const orderedItems = useMemo(() => {
    return [...OS_ITEMS].sort((a, b) => (a.id === detectedOS ? -1 : b.id === detectedOS ? 1 : 0))
  }, [detectedOS])

  const winOptions = useMemo(() => {
    const x64 = {
      key: 'win-x64',
      title: 'Windows x64 (.exe)',
      subtitle: 'PC estándar Intel / AMD',
      dl: downloads['win-x64'],
      recommended: !isWinArm64,
    }
    const arm = {
      key: 'win-arm64',
      title: 'Windows ARM64 (.exe)',
      subtitle: 'Snapdragon / Dispositivos ARM',
      dl: downloads['win-arm64'],
      recommended: isWinArm64,
    }
    return isWinArm64 ? [arm, x64] : [x64, arm]
  }, [downloads, isWinArm64])

  useEffect(() => {
    let active = true
    if (detectedOS === 'windows') {
      detectWindowsArm64().then((arm) => {
        if (active) setIsWinArm64(arm)
      })
    }
    return () => {
      active = false
    }
  }, [detectedOS])

  useEffect(() => {
    if (!menuOpen) return undefined
    const onPointerDown = (e) => {
      if (wrapRef.current && !wrapRef.current.contains(e.target)) {
        setMenuOpen(false)
      }
    }
    const onKeyDown = (e) => {
      if (e.key === 'Escape') setMenuOpen(false)
    }
    document.addEventListener('mousedown', onPointerDown)
    document.addEventListener('keydown', onKeyDown)
    return () => {
      document.removeEventListener('mousedown', onPointerDown)
      document.removeEventListener('keydown', onKeyDown)
    }
  }, [menuOpen])

  if (variant === 'compact') {
    const quickEntries = orderedItems.flatMap(({ id, Icon }) => {
      if (id === 'windows') {
        return winOptions.map((opt) => ({
          key: opt.key,
          title: opt.title,
          meta: buildMetaLine(opt.dl) || opt.subtitle,
          url: opt.dl.url,
          Icon,
        }))
      }
      if (id === 'linux') {
        return [{
          key: 'linux',
          title: 'Linux (.AppImage)',
          meta: buildMetaLine(downloads.linux) || 'x86_64 Portable',
          url: downloads.linux.url,
          Icon,
        }]
      }
      return [{
        key: 'mac',
        title: 'macOS Silicon (.dmg)',
        meta: buildMetaLine(downloads.mac) || 'Apple M1–M4',
        url: downloads.mac.url,
        Icon,
      }]
    })

    return (
      <div className={`os-nav-wrap ${className}`.trim()} ref={wrapRef}>
        <button
          type="button"
          className="btn btn-primary btn-os-compact"
          aria-expanded={menuOpen}
          aria-label={label}
          title={orderedItems.map((i) => i.label).join(' • ')}
          onClick={() => setMenuOpen((v) => !v)}
        >
          {orderedItems.map(({ id, Icon }) => (
            <Icon key={id} width={16} height={16} />
          ))}
        </button>

        {menuOpen && (
          <div className="os-download-menu os-download-menu-right" role="menu">
            {quickEntries.map((entry) => (
              <a
                key={entry.key}
                className="os-download-option"
                href={entry.url}
                download
                onClick={() => setMenuOpen(false)}
              >
                <span className="os-download-icon">
                  <entry.Icon width={17} height={17} />
                </span>
                <span className="os-download-copy">
                  <strong>{entry.title}</strong>
                  <span>{entry.meta}</span>
                </span>
                <IconDownload width={15} height={15} className="os-download-arrow" />
              </a>
            ))}
            <a
              className="os-download-all"
              href={LINKS.releases}
              target="_blank"
              rel="noreferrer"
              onClick={() => setMenuOpen(false)}
            >
              GitHub Releases →
            </a>
          </div>
        )}
      </div>
    )
  }

  return (
    <div className={`os-cta-wrap ${className}`.trim()} ref={wrapRef}>
      <div className="os-cta-group">
        {orderedItems.map(({ id, label: osLabel, Icon }, index) => {
          const btnClass = `btn btn-lg btn-os ${index === 0 ? 'btn-primary btn-os-featured' : 'btn-outline btn-os-secondary'}`
          if (id === 'windows') {
            return (
              <div key={id} className="os-win-anchor">
                <button
                  type="button"
                  className={btnClass}
                  aria-expanded={menuOpen}
                  aria-label="Windows (x64 / ARM64)"
                  title="Windows x64 / ARM64"
                  onClick={() => setMenuOpen((v) => !v)}
                >
                  <Icon width={19} height={19} />
                  <span>{osLabel}</span>
                  <span className="os-chevron" aria-hidden="true">▾</span>
                </button>

                {menuOpen && (
                  <div className="os-download-menu" role="menu">
                    {winOptions.map((opt) => (
                      <a
                        key={opt.key}
                        className={`os-download-option ${opt.recommended ? 'is-recommended' : ''}`.trim()}
                        href={opt.dl.url}
                        download
                        onClick={() => setMenuOpen(false)}
                      >
                        <span className="os-download-icon">
                          <IconWindows width={17} height={17} />
                        </span>
                        <span className="os-download-copy">
                          <strong>{opt.title}</strong>
                          <span>{buildMetaLine(opt.dl) || opt.subtitle}</span>
                        </span>
                        <IconDownload width={15} height={15} className="os-download-arrow" />
                      </a>
                    ))}
                    <a
                      className="os-download-all"
                      href={LINKS.releases}
                      target="_blank"
                      rel="noreferrer"
                      onClick={() => setMenuOpen(false)}
                    >
                      GitHub Releases →
                    </a>
                  </div>
                )}
              </div>
            )
          }

          const dl = downloads[id]
          const meta = buildMetaLine(dl)
          return (
            <a
              key={id}
              className={btnClass}
              href={dl.url}
              download
              aria-label={meta ? `${osLabel} (${meta})` : osLabel}
              title={meta ? `${osLabel} — ${meta}` : osLabel}
            >
              <Icon width={19} height={19} />
              <span>{osLabel}</span>
            </a>
          )
        })}
      </div>
    </div>
  )
}
