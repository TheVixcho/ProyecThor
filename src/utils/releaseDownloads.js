import { useEffect, useState } from 'react'

const RELEASES_API = 'https://api.github.com/repos/TheVixcho/ProyecThor/releases?per_page=10'
const CACHE_KEY = 'proyecthor_latest_downloads_v1'
const CACHE_TTL_MS = 5 * 60 * 1000

export const FALLBACK_DOWNLOADS = {
  'win-x64': {
    url: 'https://github.com/TheVixcho/ProyecThor/releases/latest/download/ProyecThor_Setup.exe',
    tag: '',
    sizeLabel: '',
    isPrerelease: false,
  },
  'win-arm64': {
    url: 'https://github.com/TheVixcho/ProyecThor/releases/latest/download/ProyecThor_Setup_arm64.exe',
    tag: '',
    sizeLabel: '',
    isPrerelease: false,
  },
  linux: {
    url: 'https://github.com/TheVixcho/ProyecThor/releases/latest/download/ProyecThor-x86_64.AppImage',
    tag: '',
    sizeLabel: '',
    isPrerelease: false,
  },
  mac: {
    url: 'https://github.com/TheVixcho/ProyecThor/releases/latest/download/ProyecThor-macos-arm64.dmg',
    tag: '',
    sizeLabel: '',
    isPrerelease: false,
  },
}

const ASSET_MATCHERS = {
  'win-x64': (name) => /^ProyecThor_Setup(?:_[\d.]+(?:-[^.]+)?)?\.exe$/i.test(name),
  'win-arm64': (name) => /_arm64\.exe$/i.test(name),
  linux: (name) => /\.AppImage$/i.test(name),
  mac: (name) => /\.dmg$/i.test(name),
}

function formatSize(bytes) {
  if (!bytes || typeof bytes !== 'number') return ''
  const mb = bytes / (1024 * 1024)
  return `${mb >= 100 ? Math.round(mb) : mb.toFixed(1)} MB`
}

function pickBestAsset(assets, matcher) {
  if (!Array.isArray(assets)) return null
  // Prefer versioned filename when available (clearer download filename), else generic
  const matches = assets.filter((a) => a?.name && a?.browser_download_url && matcher(a.name))
  if (!matches.length) return null
  const versioned = matches.find((a) => /\d+\.\d+\.\d+/.test(a.name))
  return versioned || matches[0]
}

function resolveFromReleases(releases) {
  if (!Array.isArray(releases)) return FALLBACK_DOWNLOADS
  const valid = releases.filter((r) => r && !r.draft && Array.isArray(r.assets))
  const stableReleases = valid.filter((r) => !r.prerelease)
  const searchOrder = [...stableReleases, ...valid.filter((r) => r.prerelease)]

  const resolved = { ...FALLBACK_DOWNLOADS }
  for (const [key, matcher] of Object.entries(ASSET_MATCHERS)) {
    for (const rel of searchOrder) {
      const asset = pickBestAsset(rel.assets, matcher)
      if (asset) {
        resolved[key] = {
          url: asset.browser_download_url,
          tag: rel.tag_name || '',
          sizeLabel: formatSize(asset.size),
          isPrerelease: Boolean(rel.prerelease),
        }
        break
      }
    }
  }
  return resolved
}

let memoryCache = null
let inflightPromise = null

async function fetchLatestDownloads() {
  if (memoryCache) return memoryCache

  if (typeof window !== 'undefined') {
    try {
      const raw = window.sessionStorage.getItem(CACHE_KEY)
      if (raw) {
        const parsed = JSON.parse(raw)
        if (parsed?.ts && Date.now() - parsed.ts < CACHE_TTL_MS && parsed?.data) {
          memoryCache = parsed.data
          return memoryCache
        }
      }
    } catch {
      // Ignore storage errors
    }
  }

  if (!inflightPromise) {
    inflightPromise = fetch(RELEASES_API, {
      headers: { Accept: 'application/vnd.github+json' },
    })
      .then((res) => {
        if (!res.ok) throw new Error(`HTTP ${res.status}`)
        return res.json()
      })
      .then((releases) => {
        const data = resolveFromReleases(releases)
        memoryCache = data
        if (typeof window !== 'undefined') {
          try {
            window.sessionStorage.setItem(CACHE_KEY, JSON.stringify({ ts: Date.now(), data }))
          } catch {
            // Ignore storage errors
          }
        }
        return data
      })
      .catch(() => FALLBACK_DOWNLOADS)
      .finally(() => {
        inflightPromise = null
      })
  }

  return inflightPromise
}

export function useLatestDownloads() {
  const [downloads, setDownloads] = useState(() => memoryCache || FALLBACK_DOWNLOADS)

  useEffect(() => {
    let active = true
    fetchLatestDownloads().then((data) => {
      if (active && data) setDownloads(data)
    })
    return () => {
      active = false
    }
  }, [])

  return downloads
}
