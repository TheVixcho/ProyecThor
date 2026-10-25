import React, { useState } from "react";
import logo from "./assets/logo.png";

const caracteristicas = [
  {
    titulo: "Arquitectura Multi-Capa",
    descripcion:
      "Gestiona fondos dinámicos (video, bucles, imágenes de alta resolución) y capas de texto de forma 100% independiente con aceleración GPU.",
    badge: "Motor Gráfico",
    icon: (
      <svg className="w-5 h-5 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
      </svg>
    ),
  },
  {
    titulo: "Flujo de Doble Pantalla",
    descripcion:
      "Panel de control para el operador con previsualización en vivo, desacoplado de la salida de proyección limpia, calibrada y sin bordes.",
    badge: "Producción",
    icon: (
      <svg className="w-5 h-5 text-indigo-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Biblioteca Multimedia Integral",
    descripcion:
      "Repertorio de canciones, Biblias integradas en múltiples versiones, soporte de subtítulos sincronizados, audio multiformato y documentos.",
    badge: "Gestión",
    icon: (
      <svg className="w-5 h-5 text-emerald-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M12 6.253v13m0-13C10.832 5.477 9.246 5 7.5 5S4.168 5.477 3 6.253v13C4.168 18.477 5.754 18 7.5 18s3.332.477 4.5 1.253m0-13C13.168 5.477 14.754 5 16.5 5c1.747 0 3.332.477 4.5 1.253v13C19.832 18.477 18.247 18 16.5 18c-1.746 0-3.332.477-4.5 1.253" />
      </svg>
    ),
  },
  {
    titulo: "Streaming RTMP y Control OSC",
    descripcion:
      "Emisión directa a plataformas de streaming, enrutamiento en red local y control sincronizado con consolas de luces y sonido.",
    badge: "Transmisión",
    icon: (
      <svg className="w-5 h-5 text-purple-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Shaders y Post-Procesado en Vivo",
    descripcion:
      "Efectos visuales acelerados en tiempo real (CRT, grano cinemático, viñeta, desenfoque gaussiano dinámico y corrección de color).",
    badge: "FX Visual",
    icon: (
      <svg className="w-5 h-5 text-pink-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
      </svg>
    ),
  },
  {
    titulo: "Arquitectura Extensible por Plugins",
    descripcion:
      "Capacidad para crear y conectar módulos personalizados, scripts de automatización e integraciones externas con el motor central.",
    badge: "Extensibilidad",
    icon: (
      <svg className="w-5 h-5 text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.75} d="M11 4a2 2 0 114 0v1a1 1 0 001 1h3a1 1 0 011 1v3a1 1 0 01-1 1h-1a2 2 0 100 4h1a1 1 0 011 1v3a1 1 0 01-1 1h-3a1 1 0 01-1-1v-1a2 2 0 10-4 0v1a1 1 0 01-1 1H7a1 1 0 01-1-1v-3a1 1 0 00-1-1H4a2 2 0 110-4h1a1 1 0 001-1V7a1 1 0 011-1h3a1 1 0 001-1V4z" />
      </svg>
    ),
  },
];

const proAdvantages = [
  {
    title: "Más de 5,000 Canciones en la Nube",
    desc: "Almacenamiento y sincronización de todo tu repertorio sin límites. Accede a tus letras, secuencias y acordes al instante desde cualquier PC de producción.",
    icon: "☁️",
  },
  {
    title: "Escritura Potenciada por IA",
    desc: "Asistente inteligente para redactar, estructurar estrofas, corregir métricas y formatear cánticos, avisos y reflexiones en cuestión de segundos.",
    icon: "✨",
  },
  {
    title: "Chat de Voz Potenciado por IA",
    desc: "Comunícate por voz con el asistente en tiempo real durante la transmisión. Solicita versículos, cambia de fondo o busca canciones con manos libres.",
    icon: "🎙️",
  },
  {
    title: "Respaldos Continuos y Multi-Sala",
    desc: "Copias de seguridad automáticas en la nube para no perder configuraciones ni listas de eventos ante fallos de hardware o imprevistos.",
    icon: "⚡",
  },
  {
    title: "Sincronización Instantánea entre Equipos",
    desc: "Edita el repertorio desde tu equipo personal y míralo reflejado automáticamente en la cabina de proyección de tu auditorio.",
    icon: "🔄",
  },
  {
    title: "Soporte Prioritario para Producción",
    desc: "Atención técnica especializada y acceso preferencial a las nuevas herramientas experimentales antes que nadie.",
    icon: "🛡️",
  },
];

const plataformas = [
  {
    sistema: "Windows 10 / 11 (64-bit)",
    detalle: "Instalador NSIS optimizado · Intel Core i5 / 8 GB RAM mín. (GPU dedicada recomendada para 4K)",
  },
  {
    sistema: "Linux (Arch / CachyOS / Debian / Ubuntu)",
    detalle: "Paquetes nativos de alto rendimiento · Bajo consumo de memoria y CPU en producción",
  },
];

export default function App() {
  const [activePlan, setActivePlan] = useState("annual");

  return (
    <div className="min-h-screen flex flex-col bg-neutral-950 text-neutral-200 font-sans selection:bg-cyan-500/30 selection:text-cyan-200">
      {/* Glow de fondo ambiental */}
      <div className="fixed inset-0 pointer-events-none overflow-hidden z-0">
        <div className="absolute -top-40 left-1/2 -translate-x-1/2 w-[700px] h-[400px] bg-gradient-to-tr from-cyan-600/15 via-indigo-600/20 to-purple-600/10 blur-[120px] rounded-full" />
        <div className="absolute top-[45%] -right-40 w-[500px] h-[500px] bg-indigo-500/10 blur-[130px] rounded-full" />
      </div>

      {/* Barra de Navegación Superior */}
      <header className="sticky top-0 z-50 backdrop-blur-md bg-neutral-950/80 border-b border-neutral-800/60 transition-all">
        <div className="max-w-6xl mx-auto px-6 h-16 flex justify-between items-center">
          <div className="flex items-center gap-3">
            <img src={logo} alt="ProyecThor Logo" className="w-8 h-8 object-contain drop-shadow" />
            <div className="flex flex-col">
              <span className="text-base font-semibold tracking-tight text-white leading-none">
                ProyecThor
              </span>
              <span className="text-[10px] text-cyan-400 font-medium tracking-wider uppercase mt-0.5">
                Engine
              </span>
            </div>
          </div>
          <nav className="hidden md:flex items-center gap-7 text-sm font-medium text-neutral-400">
            <a href="#inicio" className="hover:text-white transition-colors">
              Inicio
            </a>
            <a href="#caracteristicas" className="hover:text-white transition-colors">
              Características
            </a>
            <a href="#pro-section" className="text-cyan-400 hover:text-cyan-300 transition-colors flex items-center gap-1.5">
              <span>Pro Version</span>
              <span className="px-1.5 py-0.5 text-[10px] font-bold bg-cyan-500/20 text-cyan-300 rounded border border-cyan-500/30">
                IA
              </span>
            </a>
            <a href="#planes" className="hover:text-white transition-colors">
              Planes
            </a>
            <a href="#plataformas" className="hover:text-white transition-colors">
              Requisitos
            </a>
          </nav>
          <div className="flex items-center gap-3">
            <a
              href="#planes"
              className="px-4 py-2 text-xs font-semibold rounded-lg bg-gradient-to-r from-cyan-500 to-indigo-600 text-white shadow-lg shadow-cyan-500/20 hover:shadow-cyan-500/30 hover:scale-[1.02] active:scale-[0.98] transition-all"
            >
              Obtener Pro
            </a>
          </div>
        </div>
      </header>

      {/* Contenido Principal */}
      <main className="relative z-10 flex-1">
        {/* HERO SECTION */}
        <section id="inicio" className="pt-20 pb-20 px-6 max-w-5xl mx-auto text-center space-y-7">
          <div className="inline-flex items-center gap-2 px-3.5 py-1.5 rounded-full bg-neutral-900/90 border border-neutral-700/60 shadow-inner backdrop-blur-md">
            <span className="flex h-2 w-2 rounded-full bg-cyan-400 animate-pulse" />
            <span className="text-xs font-medium text-neutral-300">
              Disponible ahora: <span className="text-white font-semibold">ProyecThor for Professionals</span> con IA en la nube
            </span>
          </div>

          <h1 className="text-4xl sm:text-6xl font-extrabold tracking-tight text-white leading-tight">
            El motor de proyección creado para{" "}
            <span className="bg-clip-text text-transparent bg-gradient-to-r from-cyan-400 via-indigo-300 to-purple-400">
              producciones en vivo impecables
            </span>
          </h1>

          <p className="text-base sm:text-lg text-neutral-400 max-w-2xl mx-auto leading-relaxed">
            Potencia tu iglesia, teatro o auditorio con respuesta en milisegundos, estabilidad blindada y doble pantalla. <strong className="text-neutral-200">Totalmente gratuito para operar</strong> y con suscripción en la nube para llevar tu producción al siguiente nivel.
          </p>

          <div className="flex flex-wrap justify-center items-center gap-4 pt-3">
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="px-7 py-3.5 rounded-xl font-semibold text-sm bg-white text-neutral-950 hover:bg-neutral-100 shadow-xl shadow-white/10 hover:shadow-white/20 hover:scale-[1.02] active:scale-[0.98] transition-all flex items-center gap-2"
            >
              <svg className="w-4 h-4 text-neutral-950" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
              </svg>
              <span>Descargar Gratis</span>
            </a>
            <a
              href="#pro-section"
              className="px-6 py-3.5 rounded-xl font-semibold text-sm bg-neutral-900/90 text-neutral-200 border border-neutral-700 hover:border-cyan-500/60 hover:text-white transition-all flex items-center gap-2"
            >
              <span>Conoce ProyecThor Pro</span>
              <span className="text-cyan-400">→</span>
            </a>
            <a
              href="https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"
              target="_blank"
              rel="noopener noreferrer"
              className="px-4 py-3.5 rounded-xl font-medium text-xs text-neutral-400 hover:text-neutral-200 transition-colors flex items-center gap-1.5"
            >
              <span>Comunidad WhatsApp</span>
            </a>
          </div>

          {/* Badges de confianza técnica */}
          <div className="pt-8 grid grid-cols-2 sm:grid-cols-4 gap-4 max-w-3xl mx-auto border-t border-neutral-800/50">
            <div className="flex flex-col items-center">
              <span className="text-lg font-bold text-white">0% Caídas</span>
              <span className="text-xs text-neutral-500">Bajo Presión en Vivo</span>
            </div>
            <div className="flex flex-col items-center">
              <span className="text-lg font-bold text-cyan-400">C++ Nativo</span>
              <span className="text-xs text-neutral-500">Sin Lag de Navegador</span>
            </div>
            <div className="flex flex-col items-center">
              <span className="text-lg font-bold text-white">Multi-Capa</span>
              <span className="text-xs text-neutral-500">Video + Texto Independiente</span>
            </div>
            <div className="flex flex-col items-center">
              <span className="text-lg font-bold text-indigo-400">IA Integrada</span>
              <span className="text-xs text-neutral-500">Voz y Escritura en Vivo</span>
            </div>
          </div>
        </section>

        {/* SECCIÓN ESPECIAL: PROYECTHOR FOR PROFESSIONALS (NUBE + IA) */}
        <section id="pro-section" className="py-20 px-6 max-w-6xl mx-auto">
          <div className="relative rounded-3xl p-8 sm:p-12 overflow-hidden border border-cyan-500/30 bg-gradient-to-b from-neutral-900/90 via-neutral-900/60 to-neutral-950 shadow-2xl shadow-cyan-900/10">
            <div className="absolute top-0 right-0 -mt-12 -mr-12 w-96 h-96 bg-gradient-to-bl from-cyan-500/20 via-indigo-500/15 to-transparent blur-3xl pointer-events-none rounded-full" />
            
            <div className="relative z-10 max-w-3xl space-y-4">
              <div className="inline-flex items-center gap-2 px-3 py-1 rounded-md bg-cyan-500/10 border border-cyan-500/30 text-cyan-400 text-xs font-semibold tracking-wider uppercase">
                Suscripción Avanzada
              </div>
              <h2 className="text-3xl sm:text-5xl font-extrabold text-white tracking-tight">
                ProyecThor <span className="bg-clip-text text-transparent bg-gradient-to-r from-cyan-400 to-indigo-400">for Professionals</span>
              </h2>
              <p className="text-neutral-400 text-base sm:text-lg leading-relaxed">
                Diseñado para equipos de medios, iglesias con múltiples sedes y operadores exigentes. Lleva tu repertorio a la nube con inteligencia artificial generativa y control por voz asistido.
              </p>
            </div>

            <div className="mt-12 grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6 relative z-10">
              {proAdvantages.map((adv) => (
                <div
                  key={adv.title}
                  className="rounded-2xl p-6 bg-neutral-950/60 border border-neutral-800/80 hover:border-cyan-500/40 hover:bg-neutral-900/50 transition-all duration-300 group"
                >
                  <div className="text-2xl mb-3 group-hover:scale-110 transition-transform origin-left">
                    {adv.icon}
                  </div>
                  <h3 className="text-base font-bold text-white mb-2 group-hover:text-cyan-300 transition-colors">
                    {adv.title}
                  </h3>
                  <p className="text-xs text-neutral-400 leading-relaxed">
                    {adv.desc}
                  </p>
                </div>
              ))}
            </div>

            <div className="mt-10 pt-8 border-t border-neutral-800/80 flex flex-col sm:flex-row justify-between items-center gap-4">
              <div className="text-center sm:text-left">
                <p className="text-sm font-medium text-neutral-200">
                  ¿Listo para potenciar tu equipo con IA y almacenamiento ilimitado?
                </p>
                <p className="text-xs text-neutral-500">
                  Sincronización instantánea en menos de 2 minutos sin configuraciones complejas.
                </p>
              </div>
              <a
                href="#planes"
                className="px-6 py-3 rounded-xl text-xs font-bold bg-gradient-to-r from-cyan-500 to-indigo-600 text-white hover:opacity-95 shadow-lg shadow-cyan-500/25 transition-all"
              >
                Ver Opciones de Suscripción
              </a>
            </div>
          </div>
        </section>

        {/* CARACTERÍSTICAS TÉCNICAS */}
        <section id="caracteristicas" className="py-16 px-6 max-w-6xl mx-auto space-y-10">
          <div className="text-center max-w-2xl mx-auto space-y-3">
            <h2 className="text-xs uppercase tracking-widest text-cyan-400 font-bold">
              Potencia y Control
            </h2>
            <p className="text-3xl font-extrabold text-white tracking-tight">
              Ingeniería nativa para cero errores
            </p>
            <p className="text-neutral-400 text-sm">
              Cada componente fue programado en C++ para responder en el instante en que el operador hace clic.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {caracteristicas.map((c) => (
              <div
                key={c.titulo}
                className="rounded-2xl p-6 bg-neutral-900/30 border border-neutral-800 hover:border-neutral-700/80 transition-all duration-300 hover:scale-[1.01]"
              >
                <div className="flex items-center justify-between mb-4">
                  <div className="p-2.5 rounded-xl bg-neutral-800/60 border border-neutral-700/50">
                    {c.icon}
                  </div>
                  <span className="text-[10px] uppercase font-bold tracking-wider px-2 py-0.5 rounded bg-neutral-800/80 text-neutral-400">
                    {c.badge}
                  </span>
                </div>
                <h3 className="text-base font-bold text-white mb-2">
                  {c.titulo}
                </h3>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  {c.descripcion}
                </p>
              </div>
            ))}
          </div>
        </section>

        {/* COMPARACIÓN DE PLANES (PRICING) */}
        <section id="planes" className="py-20 px-6 max-w-5xl mx-auto space-y-12">
          <div className="text-center max-w-2xl mx-auto space-y-3">
            <span className="text-xs uppercase tracking-widest text-indigo-400 font-bold">
              Transparencia Total
            </span>
            <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
              Elige el plan ideal para tu auditorio
            </h2>
            <p className="text-neutral-400 text-sm">
              Usa ProyecThor gratis para siempre con todas las funciones locales, o desbloquea la potencia de la nube e inteligencia artificial con el plan profesional.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8 items-stretch">
            {/* PLAN BASE GRATUITO */}
            <div className="rounded-3xl p-8 bg-neutral-900/40 border border-neutral-800 flex flex-col justify-between space-y-6">
              <div className="space-y-4">
                <div className="inline-block px-3 py-1 rounded-md bg-neutral-800 text-neutral-300 text-xs font-semibold uppercase tracking-wider">
                  Plan Base
                </div>
                <div className="flex items-baseline gap-2">
                  <span className="text-4xl font-extrabold text-white">$0</span>
                  <span className="text-xs text-neutral-400">Gratis para siempre</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Ideal para eventos individuales o iglesias que operan completamente de forma local y offline.
                </p>

                <div className="pt-4 border-t border-neutral-800/80 space-y-3 text-xs text-neutral-300">
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Motor nativo C++ de alto rendimiento</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Salida de doble pantalla y vista de operador</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Capas independientes de video, imagen y texto</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Bibliotecas locales de letras, Biblias XML y fondos</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Efectos visuales shaders y streaming RTMP local</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-emerald-400 font-bold">✓</span>
                    <span>Soporte de plugins externos comunitarios</span>
                  </div>
                </div>
              </div>

              <a
                href="https://github.com/TheVixcho/ProyecThor/releases"
                target="_blank"
                rel="noopener noreferrer"
                className="w-full py-3 rounded-xl text-xs font-bold text-center bg-neutral-800 hover:bg-neutral-700 text-white transition-colors"
              >
                Descargar Instalador Gratis
              </a>
            </div>

            {/* PLAN PRO FOR PROFESSIONALS */}
            <div className="rounded-3xl p-8 bg-gradient-to-b from-neutral-900/90 to-neutral-900/40 border-2 border-cyan-500/50 relative flex flex-col justify-between space-y-6 shadow-xl shadow-cyan-500/10">
              <div className="absolute -top-3.5 right-8 px-3 py-1 bg-gradient-to-r from-cyan-500 to-indigo-600 text-white text-[10px] font-extrabold uppercase tracking-widest rounded-full shadow">
                Recomendado
              </div>

              <div className="space-y-4">
                <div className="inline-block px-3 py-1 rounded-md bg-cyan-500/20 text-cyan-300 text-xs font-semibold uppercase tracking-wider">
                  ProyecThor for Professionals
                </div>
                <div className="flex items-baseline gap-2">
                  <span className="text-4xl font-extrabold text-white">Suscripción Pro</span>
                </div>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  Para ministerios, auditorios y equipos de producción que necesitan sincronización en la nube y asistencia IA.
                </p>

                <div className="pt-4 border-t border-neutral-800/80 space-y-3 text-xs text-neutral-200">
                  <div className="flex items-center gap-2.5 font-medium text-white">
                    <span className="text-cyan-400 font-bold">★</span>
                    <span>Guardar más de 5,000 canciones en la nube</span>
                  </div>
                  <div className="flex items-center gap-2.5 font-medium text-white">
                    <span className="text-cyan-400 font-bold">★</span>
                    <span>Escritura inteligente potenciada por IA</span>
                  </div>
                  <div className="flex items-center gap-2.5 font-medium text-white">
                    <span className="text-cyan-400 font-bold">★</span>
                    <span>Chat y comandos de voz potenciados por IA</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-cyan-400 font-bold">✓</span>
                    <span>Sincronización multi-dispositivo en tiempo real</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-cyan-400 font-bold">✓</span>
                    <span>Copias de seguridad continuas ante imprevistos</span>
                  </div>
                  <div className="flex items-center gap-2.5">
                    <span className="text-cyan-400 font-bold">✓</span>
                    <span>Soporte prioritario y acceso anticipado a nuevas funciones</span>
                  </div>
                </div>
              </div>

              <a
                href="https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"
                target="_blank"
                rel="noopener noreferrer"
                className="w-full py-3.5 rounded-xl text-xs font-bold text-center bg-gradient-to-r from-cyan-500 to-indigo-600 text-white hover:opacity-95 shadow-lg shadow-cyan-500/25 transition-all"
              >
                Suscribirse a ProyecThor Pro
              </a>
            </div>
          </div>
        </section>

        {/* PLATAFORMAS Y REQUISITOS TÉCNICOS */}
        <section id="plataformas" className="py-16 px-6 max-w-4xl mx-auto space-y-6">
          <div className="border-b border-neutral-800 pb-3 flex justify-between items-end">
            <div>
              <h2 className="text-xs uppercase tracking-widest text-neutral-400 font-bold">
                Compatibilidad
              </h2>
              <p className="text-xl font-bold text-white mt-1">
                Sistemas Operativos Compatibles
              </p>
            </div>
            <span className="text-xs text-cyan-400 font-medium">64-bit nativo</span>
          </div>

          <div className="divide-y divide-neutral-800/80 border border-neutral-800 rounded-2xl bg-neutral-900/30 overflow-hidden">
            {plataformas.map((plat) => (
              <div
                key={plat.sistema}
                className="p-5 flex flex-col sm:flex-row sm:justify-between sm:items-center gap-2 hover:bg-neutral-800/20 transition-colors"
              >
                <div className="flex items-center gap-3">
                  <span className="text-neutral-400 text-sm">💻</span>
                  <span className="text-sm font-semibold text-white">{plat.sistema}</span>
                </div>
                <span className="text-xs text-neutral-400 sm:text-right">{plat.detalle}</span>
              </div>
            ))}
          </div>
        </section>
      </main>

      {/* FOOTER PROFESIONAL */}
      <footer className="border-t border-neutral-800/80 bg-neutral-950/90 py-10 relative z-10 text-xs text-neutral-500">
        <div className="max-w-6xl mx-auto px-6 flex flex-col sm:flex-row justify-between items-center gap-6">
          <div className="flex items-center gap-3">
            <img src={logo} alt="ProyecThor Logo" className="w-5 h-5 opacity-70" />
            <span>© 2026 ProyecThor · Todos los derechos reservados</span>
          </div>

          <div className="flex flex-wrap items-center gap-6">
            <a
              href="https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"
              target="_blank"
              rel="noopener noreferrer"
              className="text-neutral-400 hover:text-white transition-colors"
            >
              Canal de Noticias WhatsApp
            </a>
            <a
              href="https://ko-fi.com/vixcho"
              target="_blank"
              rel="noopener noreferrer"
              className="text-neutral-400 hover:text-white transition-colors"
            >
              Apoyar en Ko-fi
            </a>
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="text-cyan-400 hover:text-cyan-300 font-medium transition-colors"
            >
              Descargas
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}
