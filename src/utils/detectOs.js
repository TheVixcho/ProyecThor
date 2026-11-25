export function detectUserOS() {
  if (typeof navigator === 'undefined') return 'windows'

  const uadPlatform = navigator.userAgentData?.platform || ''
  const platform = navigator.platform || ''
  const ua = navigator.userAgent || ''
  const combined = `${uadPlatform} ${platform} ${ua}`.toLowerCase()

  if (combined.includes('mac') || combined.includes('iphone') || combined.includes('ipad')) {
    return 'mac'
  }
  if (combined.includes('win')) {
    return 'windows'
  }
  if ((combined.includes('linux') || combined.includes('x11')) && !combined.includes('android')) {
    return 'linux'
  }
  return 'windows'
}

export async function detectWindowsArm64() {
  if (typeof navigator === 'undefined') return false
  try {
    if (navigator.userAgentData?.getHighEntropyValues) {
      const info = await navigator.userAgentData.getHighEntropyValues(['architecture'])
      if (typeof info?.architecture === 'string' && info.architecture.toLowerCase().includes('arm')) {
        return true
      }
    }
  } catch {
    // Fallback to UA inspection below
  }
  const ua = (navigator.userAgent || '').toLowerCase()
  return ua.includes('arm64') || ua.includes('aarch64')
}
