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
