// Iconos de línea, dibujados a mano en SVG (sin dependencias externas) --
// mismo criterio "sin assets pesados" que usa la app de escritorio.
const base = {
  width: 24,
  height: 24,
  viewBox: '0 0 24 24',
  fill: 'none',
  stroke: 'currentColor',
  strokeWidth: 1.6,
  strokeLinecap: 'round',
  strokeLinejoin: 'round',
}

export function IconLayers(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 3 3 8l9 5 9-5-9-5Z" />
      <path d="m3 13 9 5 9-5" />
      <path d="m3 11 9 5 9-5" />
    </svg>
  )
}

export function IconDualScreen(props) {
  return (
    <svg {...base} {...props}>
      <rect x="2" y="5" width="12" height="9" rx="1.3" />
      <path d="M5.5 17.5h5" />
      <path d="M8 14v3.5" />
      <rect x="15.5" y="8" width="7" height="5.5" rx="1" />
    </svg>
  )
}

export function IconBolt(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12.5 2 4 13.5h6L10.5 22 20 9.5h-6L12.5 2Z" />
    </svg>
  )
}

export function IconWindow(props) {
  return (
    <svg {...base} {...props}>
      <rect x="2.5" y="4" width="19" height="16" rx="2" />
      <path d="M2.5 8.5h19" />
      <circle cx="5.3" cy="6.2" r="0.55" fill="currentColor" stroke="none" />
      <circle cx="7.1" cy="6.2" r="0.55" fill="currentColor" stroke="none" />
    </svg>
  )
}

export function IconEye(props) {
  return (
    <svg {...base} {...props}>
      <path d="M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12Z" />
      <circle cx="12" cy="12" r="3" />
    </svg>
  )
}

export function IconWindows(props) {
  return (
    <svg {...base} {...props}>
      <path d="M3 6.2 10.4 5.2V11.8H3V6.2Z" />
      <path d="M11.4 5.1 21 3.8V11.7H11.4V5.1Z" />
      <path d="M3 12.8H10.4V19.4L3 18.4V12.8Z" />
      <path d="M11.4 12.8H21V20.2L11.4 18.9V12.8Z" />
    </svg>
  )
}

export function IconLinux(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 2.5c-2 0-3 2-3 4.2 0 1.3-.3 2-1 3-1 1.4-2 2.9-2 5 0 3.7 2.7 6.8 6 6.8s6-3.1 6-6.8c0-2.1-1-3.6-2-5-.7-1-1-1.7-1-3 0-2.2-1-4.2-3-4.2Z" />
      <circle cx="9.7" cy="9" r="0.6" fill="currentColor" stroke="none" />
      <circle cx="14.3" cy="9" r="0.6" fill="currentColor" stroke="none" />
      <path d="M9 19.5c1-1 4-1 6 0" />
    </svg>
  )
}

export function IconGithub(props) {
  return (
    <svg {...base} {...props} strokeWidth={1.4}>
      <path d="M9 19c-4.3 1.4-4.3-2.5-6-3m12 5v-3.4c0-1 .1-1.4-.5-2 2.8-.3 5.5-1.4 5.5-6a4.6 4.6 0 0 0-1.3-3.2 4.2 4.2 0 0 0-.1-3.2s-1.1-.3-3.5 1.3a12.3 12.3 0 0 0-6.2 0C6.6 2.9 5.5 3.2 5.5 3.2a4.2 4.2 0 0 0-.1 3.2A4.6 4.6 0 0 0 4.1 9.6c0 4.6 2.7 5.7 5.5 6-.6.6-.6 1.1-.5 2V21" />
    </svg>
  )
}

export function IconDiscord(props) {
  return (
    <svg {...base} {...props} strokeWidth={1.4}>
      <path d="M8 5.5c-2.6.5-4 1.6-4 1.6-2 3-2.5 7.4-2.5 7.4 1.6 2 4 2 4 2l.7-1.1" />
      <path d="M16 5.5c2.6.5 4 1.6 4 1.6 2 3 2.5 7.4 2.5 7.4-1.6 2-4 2-4 2l-.7-1.1" />
      <path d="M8 5.5C9.2 5 10.5 4.8 12 4.8s2.8.2 4 .7" />
      <path d="M5.5 16.5s2.5 1.3 6.5 1.3 6.5-1.3 6.5-1.3" />
      <ellipse cx="9" cy="12.3" rx="1.1" ry="1.4" fill="currentColor" stroke="none" />
      <ellipse cx="15" cy="12.3" rx="1.1" ry="1.4" fill="currentColor" stroke="none" />
    </svg>
  )
}

export function IconWhatsapp(props) {
  return (
    <svg {...base} {...props} strokeWidth={1.4}>
      <path d="M4 20l1.3-3.8A8 8 0 1 1 8.5 19L4 20Z" />
      <path d="M8.7 8.6c.2-.5.4-.5.7-.5h.5c.2 0 .4 0 .6.4.2.5.7 1.6.7 1.7.1.1.1.3 0 .4-.1.2-.2.3-.3.4-.2.2-.3.3-.1.6.2.3.8 1.3 1.7 2 1.2 1 2 1.3 2.3 1.4.3.1.5.1.6-.1.2-.2.7-.8.9-1.1.2-.2.4-.2.6-.1l1.5.7c.2.1.4.2.4.4.1.5-.1 1-.4 1.4-.3.3-.9.6-1.5.6-1.1 0-2.7-.5-4.5-2.1-2.2-1.9-3.4-3.9-3.5-4.1-.1-.2-.7-1-.7-1.8 0-.8.4-1.2.6-1.4Z" fill="currentColor" stroke="none" />
    </svg>
  )
}

export function IconHeart(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 20.5s-7.5-4.6-9.7-9C.7 8 2 4.5 5.4 3.7c2-.5 3.9.3 5 1.9a.7.7 0 0 0 1.2 0c1.1-1.6 3-2.4 5-1.9C20 4.5 21.3 8 19.7 11.5c-2.2 4.4-9.7 9-9.7 9Z" />
    </svg>
  )
}

export function IconLock(props) {
  return (
    <svg {...base} {...props}>
      <rect x="4.5" y="10.5" width="15" height="10" rx="1.8" />
      <path d="M7.5 10.5V7a4.5 4.5 0 0 1 9 0v3.5" />
      <circle cx="12" cy="15.2" r="1.4" fill="currentColor" stroke="none" />
      <path d="M12 16.6v2" />
    </svg>
  )
}

export function IconUser(props) {
  return (
    <svg {...base} {...props}>
      <circle cx="12" cy="8" r="3.5" />
      <path d="M4.5 20c1.3-3.6 4.2-5.5 7.5-5.5s6.2 1.9 7.5 5.5" />
    </svg>
  )
}

export function IconNoSub(props) {
  return (
    <svg {...base} {...props}>
      <rect x="3" y="6" width="18" height="13" rx="2" />
      <path d="M3 10.5h18" />
      <path d="M6.5 14.5h4" />
      <path d="m4 3 16 18" />
    </svg>
  )
}

export function IconArrowRight(props) {
  return (
    <svg {...base} {...props}>
      <path d="M4 12h15.5" />
      <path d="m13.5 6 6 6-6 6" />
    </svg>
  )
}

export function IconDownload(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 3v12.5" />
      <path d="m6.5 11 5.5 5.5L17.5 11" />
      <path d="M4 20h16" />
    </svg>
  )
}

export function IconCheck(props) {
  return (
    <svg {...base} {...props}>
      <path d="m4 12.5 5 5L20 6.5" />
    </svg>
  )
}

export function IconMobile(props) {
  return (
    <svg {...base} {...props}>
      <rect x="7" y="2.5" width="10" height="19" rx="2" />
      <path d="M11 18.2h2" />
    </svg>
  )
}

export function IconLibrary(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 5.5c-1.6-1.2-3.8-1.7-5.8-1.4-.9.1-1.7.3-2.2.5v13.5c.5-.2 1.3-.4 2.2-.5 2-.3 4.2.2 5.8 1.4" />
      <path d="M12 5.5c1.6-1.2 3.8-1.7 5.8-1.4.9.1 1.7.3 2.2.5v13.5c-.5-.2-1.3-.4-2.2-.5-2-.3-4.2.2-5.8 1.4V5.5Z" />
    </svg>
  )
}

export function IconOverlay(props) {
  return (
    <svg {...base} {...props}>
      <rect x="2.5" y="7" width="12" height="12" rx="1.6" />
      <path d="M8.5 5h9.5a1.5 1.5 0 0 1 1.5 1.5V15" strokeDasharray="2.6 2.6" />
    </svg>
  )
}

export function IconBroadcast(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 21.5v-8.7" />
      <circle cx="12" cy="11" r="1.4" fill="currentColor" stroke="none" />
      <path d="M8.3 8.7a5.2 5.2 0 0 0 0 4.6" />
      <path d="M15.7 8.7a5.2 5.2 0 0 1 0 4.6" />
      <path d="M5.3 5.7a9.6 9.6 0 0 0 0 10.6" />
      <path d="M18.7 5.7a9.6 9.6 0 0 1 0 10.6" />
    </svg>
  )
}

export function IconPalette(props) {
  return (
    <svg {...base} {...props}>
      <path d="M12 3a9 8 0 1 0 0 16c1 0 1.8-.7 1.8-1.7 0-.5-.2-.9-.5-1.2-.3-.3-.5-.7-.5-1.2 0-1 .8-1.7 1.8-1.7H16a4.5 4 0 0 0 4.5-4C20.5 5.6 16.7 3 12 3Z" />
      <circle cx="7.3" cy="11" r="1" fill="currentColor" stroke="none" />
      <circle cx="9.2" cy="7.3" r="1" fill="currentColor" stroke="none" />
      <circle cx="14" cy="7" r="1" fill="currentColor" stroke="none" />
    </svg>
  )
}

export function IconConvert(props) {
  return (
    <svg {...base} {...props}>
      <path d="M4 8h13.5" />
      <path d="M14 4.3 17.7 8l-3.7 3.7" />
      <path d="M20 16H6.5" />
      <path d="M10 12.3 6.3 16l3.7 3.7" />
    </svg>
  )
}

export function IconQueue(props) {
  return (
    <svg {...base} {...props}>
      <path d="M4 6.5h11" />
      <path d="M4 12h11" />
      <path d="M4 17.5h7" />
      <path d="M17 10.5v7l5-3.5-5-3.5Z" fill="currentColor" stroke="none" />
    </svg>
  )
}

export function IconCode(props) {
  return (
    <svg {...base} {...props}>
      <path d="m8.5 7-5 5 5 5" />
      <path d="m15.5 7 5 5-5 5" />
      <path d="m13.2 5-2.4 14" />
    </svg>
  )
}

export function IconMenu(props) {
  return (
    <svg {...base} {...props}>
      <path d="M4 7h16" />
      <path d="M4 12h16" />
      <path d="M4 17h16" />
    </svg>
  )
}

export function IconClose(props) {
  return (
    <svg {...base} {...props}>
      <path d="m5 5 14 14" />
      <path d="m19 5-14 14" />
    </svg>
  )
}
