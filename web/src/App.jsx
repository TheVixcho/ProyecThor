import React from "react";
import logo from "./assets/logo.png";

const caracteristicas = [
  {
    titulo: "Arquitectura Multi-Capa",
    descripcion:
      "Gestión independiente y acelerada por GPU para fondos de video, bucles gráficos y capas de texto dinámicas sin interferencia entre elementos.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
      </svg>
    ),
  },
  {
    titulo: "Despliegue en Doble Pantalla",
    descripcion:
      "Consola de control dedicada para el operador con monitorización en tiempo real, desacoplada de la salida limpia sin bordes para el proyector.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Stage Display (Monitores de Retorno)",
    descripcion:
      "Salida de retorno para músicos y púlpito con estrofa actual, próxima estrofa, reloj de cuenta regresiva y mensajes discretos desde la cabina.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 18h.01M7 21h10a2 2 0 002-2V5a2 2 0 00-2-2H7a2 2 0 00-2 2v14a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Biblioteca Centralizada",
    descripcion:
      "Repertorio de canciones, versiones integradas de las Sagradas Escrituras en formato XML estructurado, subtítulos y archivos de audio.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 6.253v13m0-13C10.832 5.477 9.246 5 7.5 5S4.168 5.477 3 6.253v13C4.168 18.477 5.754 18 7.5 18s3.332.477 4.5 1.253m0-13C13.168 5.477 14.754 5 16.5 5c1.747 0 3.332.477 4.5 1.253v13C19.832 18.477 18.247 18 16.5 18c-1.746 0-3.332.477-4.5 1.253" />
      </svg>
    ),
  },
  {
    titulo: "Streaming RTMP e Integración OSC",
    descripcion:
      "Emisión directa a servidores de retransmisión, distribución en red local y control sincronizado bidireccional mediante el protocolo OSC.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Shaders y Post-Procesado en GPU",
    descripcion:
      "Efectos visuales procesados en tiempo real: simulación de emulsión analógica, grano cinematográfico, viñeta y desenfoque dinámico.",
    icon: (
      <svg className="w-6 h-6 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
      </svg>
    ),
  },
];

const pilaresGratuitos = [
  {
    titulo: "100% Funcional Offline",
    desc: "No dependes de conexión a internet para proyectar en tus cultos o eventos. Todo el motor, la biblioteca y los medios funcionan de manera completamente local y autónoma sin bloqueos repentinos.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M18.364 5.636a9 9 0 010 12.728m0 0l-2.829-2.829m2.829 2.829L21 21M15.536 8.464a5 5 0 010 7.072m0 0l-2.829-2.829m-4.243 2.829a4.978 4.978 0 01-1.414-2.83m-1.414 5.658a9 9 0 01-2.167-9.238m7.824-2.178a9 9 0 016.364 2.64m-4.95 2.122a5 5 0 011.414 2.828M3 3l18 18" />
      </svg>
    ),
  },
  {
    titulo: "Cero Marcas de Agua ni Límites",
    desc: "Disfruta de una salida visual limpia para tu congregación. Sin logos forzados en la proyección, sin límites de versículos ni de canciones, y sin fechas de caducidad molestas.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
      </svg>
    ),
  },
  {
    titulo: "Rendimiento Nativo en C++17",
    desc: "A diferencia de aplicaciones pesadas basadas en Electron o navegadores embebidos, ProyecThor se compila directamente para la GPU y el procesador, consumiendo mínimos recursos.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M13 10V3L4 14h7v7l9-11h-7z" />
      </svg>
    ),
  },
];

const proAdvantages = [
  {
    title: "Almacenamiento Cloud para 5,000+ Canciones",
    desc: "Capacidad extendida en la nube para sincronizar y respaldar catálogos completos de repertorio con acceso instantáneo desde cualquier estación de trabajo.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M3 15a4 4 0 004 4h9a5 5 0 10-.1-9.999 5.002 5.002 0 00-9.78 2.096A4.001 4.001 0 003 15z" />
      </svg>
    ),
  },
  {
    title: "Sincronización Multi-Sala y Multi-Sede",
    desc: "Centralice los cambios editoriales: una modificación en el puesto de preparación se propaga de manera transparente a todas las cabinas de control.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
      </svg>
    ),
  },
  {
    title: "Edición y Estructuración Litúrgica",
    desc: "Herramientas avanzadas para estructurar estrofas, dar formato instantáneo y verificar la concordancia métrica de himnos y cantos.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
      </svg>
    ),
  },
  {
    title: "Respaldos Automáticos con Redundancia",
    desc: "Copias de seguridad automáticas en la nube para garantizar la continuidad operativa de presentaciones y configuraciones críticas ante contingencias de hardware.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z" />
      </svg>
    ),
  },
  {
    title: "Soporte Técnico y Despliegues Dedicados",
    desc: "Canal prioritario de asistencia técnica para auditorios y acceso preferente a compilaciones preliminares y mejoras de rendimiento antes de su publicación general.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M18.364 5.636l-3.536 3.536m0 5.656l3.536 3.536M9.172 9.172L5.636 5.636m3.536 9.192l-3.536 3.536M21 12a9 9 0 11-18 0 9 9 0 0118 0zm-5 0a4 4 0 11-8 0 4 4 0 018 0z" />
      </svg>
    ),
  },
  {
    title: "Continuidad Institucional",
    desc: "Infraestructura robusta pensada para organizaciones religiosas y técnicas que requieren estandarizar múltiples auditorios bajo una misma cuenta centralizada.",
    icon: (
      <svg className="w-6 h-6 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4" />
      </svg>
    ),
  },
];

export default function App() {
  return (
    <div className="min-h-screen flex flex-col bg-neutral-950 text-neutral-300 font-['Arial',sans-serif] antialiased selection:bg-neutral-800 selection:text-white relative overflow-x-clip">
      {/* Iluminación de fondo ambiental animada */}
      <div className="fixed inset-0 pointer-events-none overflow-hidden z-0">
        <div className="ambient-glow-1 absolute top-0 left-1/3 -translate-x-1/2 w-[650px] h-[360px] bg-cyan-600/15 blur-[160px] rounded-full" />
        <div className="ambient-glow-2 absolute top-1/3 right-1/4 w-[550px] h-[340px] bg-blue-600/10 blur-[150px] rounded-full" />
        <div className="absolute top-3/4 left-1/2 -translate-x-1/2 w-[700px] h-[300px] bg-cyan-900/10 blur-[170px] rounded-full" />
      </div>

      {/* Navegación Superior con Efecto Liquid Glass (Fija Siempre Arriba) */}
      <header className="fixed top-0 left-0 right-0 z-50 liquid-glass transition-all duration-300">
        <div className="max-w-6xl mx-auto px-6 h-16 flex justify-between items-center">
          <div className="flex items-center gap-3 group cursor-pointer">
            <img
              src={logo}
              alt="ProyecThor Logo"
              className="w-8 h-8 object-contain transition-transform duration-300 group-hover:scale-110"
            />
            <div className="flex items-baseline gap-2">
              <span className="text-base font-bold tracking-tight text-white group-hover:text-cyan-400 transition-colors">
                ProyecThor
              </span>
              <span className="hidden sm:inline-block text-[11px] text-neutral-400 font-medium tracking-wide">
                Engine 1.1
              </span>
            </div>
          </div>
          <nav className="hidden md:flex items-center gap-2 text-xs text-neutral-300 font-medium">
            <a href="#inicio" className="px-3.5 py-1.5 rounded-lg hover:bg-white/10 hover:text-white transition-all duration-200">
              Inicio
            </a>
            <a href="#gratuito" className="px-3.5 py-1.5 rounded-lg hover:bg-white/10 hover:text-cyan-400 transition-all duration-200">
              Versión Gratuita
            </a>
            <a href="#especificaciones" className="px-3.5 py-1.5 rounded-lg hover:bg-white/10 hover:text-white transition-all duration-200">
              Especificaciones
            </a>
            <a href="#compatibilidad" className="px-3.5 py-1.5 rounded-lg hover:bg-white/10 hover:text-white transition-all duration-200">
              Entornos
            </a>
            <a href="#profesionales" className="px-3.5 py-1.5 rounded-lg text-cyan-400 hover:bg-cyan-500/10 hover:text-cyan-300 transition-all duration-200 font-semibold">
              Edición Professionals
            </a>
          </nav>
          <div className="flex items-center gap-3">
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="px-5 py-2.5 text-xs font-semibold rounded-lg bg-white/95 text-neutral-950 hover:bg-white hover:shadow-lg hover:shadow-white/10 transition-all duration-200"
            >
              Descargar Gratis
            </a>
          </div>
        </div>
      </header>

      {/* Contenido Principal con Espaciado Generoso y Profesional */}
      <main className="relative z-10 flex-1 pt-16">
        {/* HERO SECTION */}
        <section id="inicio" className="pt-32 pb-28 px-6 max-w-5xl mx-auto text-center space-y-12">
          <h1 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white max-w-4xl mx-auto leading-tight text-center">
            <span className="block mb-2">Proyección profesional para tu auditorio,</span>
            <span className="block bg-gradient-to-r from-white via-neutral-200 to-cyan-400 bg-clip-text text-transparent">
              a la velocidad del rayo
            </span>
          </h1>

          <p className="text-base sm:text-lg text-neutral-400 max-w-2xl mx-auto leading-relaxed font-normal pt-2">
            Un motor nativo en C++17 acelerado por GPU, diseñado para cultos y eventos en vivo. Sin marcas de agua, sin límites de tiempo y con funcionamiento 100% offline. Toda la potencia que tu congregación necesita desde el primer día.
          </p>

          <div className="flex flex-wrap justify-center items-center gap-5 pt-4">
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="px-8 py-4 rounded-xl text-xs font-semibold bg-white text-neutral-950 hover:bg-neutral-200 transition-all duration-200 shadow-md shadow-white/10 hover:scale-[1.02]"
            >
              Descargar Versión Gratuita
            </a>
            <a
              href="#profesionales"
              className="px-8 py-4 rounded-xl text-xs font-semibold bg-neutral-900 border border-neutral-700 text-neutral-200 hover:border-cyan-400 hover:text-white transition-all duration-200 hover:scale-[1.02] inline-flex items-center gap-2"
            >
              <span>Ver versión para profesionales</span>
              <svg className="w-4 h-4 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            </a>
          </div>

          {/* Métricas Técnicas */}
          <div className="pt-20 mt-16 grid grid-cols-2 md:grid-cols-4 gap-8 max-w-4xl mx-auto border-t border-neutral-900">
            <div className="text-left border-l-2 border-neutral-800 pl-5 transition-all hover:border-cyan-400 group space-y-1">
              <span className="block text-3xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                0 ms
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Latencia de Procesado
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-5 transition-all hover:border-cyan-400 group space-y-1">
              <span className="block text-3xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                C++17
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Compilación Nativa
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-5 transition-all hover:border-cyan-400 group space-y-1">
              <span className="block text-3xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                100%
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Offline sin Internet
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-5 transition-all hover:border-cyan-400 group space-y-1">
              <span className="block text-3xl font-bold text-cyan-400 tracking-tight transition-colors">
                Gratis
              </span>
              <span className="text-xs text-neutral-400 uppercase tracking-wider">
                Sin Marcas de Agua
              </span>
            </div>
          </div>
        </section>

        {/* SECCIÓN: LO BUENO QUE ES EL PROGRAMA DE FORMA GRATUITA */}
        <section id="gratuito" className="py-28 px-6 max-w-6xl mx-auto space-y-20">
          <div className="max-w-3xl mx-auto space-y-6 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight text-center">
              La versión gratuita ya es todo lo que tu auditorio necesita
            </h2>
            <p className="text-neutral-400 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto text-justify pt-1">
              ProyecThor se diseñó con una premisa clara: las iglesias y auditorios merecen una herramienta de proyección moderna, rápida y fiable sin tener que pagar licencias prohibitivas ni tolerar marcas de agua molestas.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            {pilaresGratuitos.map((p) => (
              <div
                key={p.titulo}
                className="rounded-2xl border border-neutral-800 bg-neutral-900/30 p-8 sm:p-9 flex flex-col justify-between space-y-6 hover:border-neutral-700 hover:-translate-y-1 transition-all duration-300"
              >
                <div className="space-y-6">
                  <div className="w-12 h-12 rounded-xl bg-neutral-900 border border-neutral-800 flex items-center justify-center">
                    {p.icon}
                  </div>
                  <h3 className="text-lg font-bold text-white tracking-tight">
                    {p.titulo}
                  </h3>
                  <p className="text-sm text-neutral-400 leading-relaxed text-justify">
                    {p.desc}
                  </p>
                </div>
              </div>
            ))}
          </div>

          {/* Resumen de capacidades incluidas gratis */}
          <div className="rounded-3xl border border-neutral-800 bg-neutral-950 p-8 sm:p-12 space-y-8 shadow-xl">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-6 border-b border-neutral-800/80 pb-6">
              <div className="space-y-1">
                <span className="text-sm font-bold uppercase tracking-wider text-white">
                  Incluido de serie en la versión gratuita
                </span>
                <p className="text-xs sm:text-sm text-neutral-400">
                  Todo listo para descargar, instalar y poner en marcha este mismo fin de semana.
                </p>
              </div>
              <a
                href="https://github.com/TheVixcho/ProyecThor/releases"
                target="_blank"
                rel="noopener noreferrer"
                className="inline-flex items-center gap-2 px-5 py-2.5 rounded-xl bg-neutral-900 border border-neutral-700 text-xs font-semibold text-white hover:border-cyan-400 transition-colors w-fit shrink-0"
              >
                Obtener Instalador Gratuito
                <svg className="w-4 h-4 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14 5l7 7m0 0l-7 7m7-7H3" />
                </svg>
              </a>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-y-5 gap-x-8 text-xs sm:text-sm text-neutral-300">
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Salida limpia en doble pantalla sin bordes</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Bibliotecas de canciones locales sin límite</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Sagradas Escrituras en XML estructurado</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Fondos de video y bucles acelerados por GPU</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Stage Display para monitores de escenario</span>
              </div>
              <div className="flex items-center gap-3">
                <span className="w-2 h-2 rounded-full bg-cyan-400 shrink-0" />
                <span>Streaming local mediante LAN</span>
              </div>
            </div>
          </div>
        </section>

        {/* CARACTERÍSTICAS TÉCNICAS Y ESPECIFICACIONES BASE */}
        <section id="especificaciones" className="py-28 px-6 max-w-6xl mx-auto space-y-16">
          <div className="border-b border-neutral-800 pb-8 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight text-center">
              Capacidades técnicas del motor de proyección
            </h2>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
            {caracteristicas.map((c) => (
              <div
                key={c.titulo}
                className="rounded-2xl border border-neutral-800/80 bg-neutral-900/20 p-8 space-y-5 hover:border-neutral-700 hover:-translate-y-1 transition-all duration-300 group"
              >
                <div className="w-11 h-11 rounded-xl bg-neutral-900 border border-neutral-800 flex items-center justify-center group-hover:border-cyan-500/40 transition-colors">
                  {c.icon}
                </div>
                <h3 className="text-base font-bold text-white group-hover:text-cyan-300 transition-colors">
                  {c.titulo}
                </h3>
                <p className="text-xs sm:text-sm text-neutral-400 leading-relaxed text-justify">
                  {c.descripcion}
                </p>
              </div>
            ))}
          </div>
        </section>

        {/* PLATAFORMAS COMPATIBLES */}
        <section id="compatibilidad" className="py-28 px-6 max-w-5xl mx-auto space-y-16">
          <div className="border-b border-neutral-800 pb-8 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight text-center">
              Entornos de Ejecución Soportados
            </h2>
          </div>

          {/* Tabla de Especificaciones para Escritorio / Pantallas Medianas */}
          <div className="hidden md:block rounded-3xl border border-neutral-800 bg-neutral-950/80 shadow-2xl overflow-hidden backdrop-blur-sm">
            <table className="w-full text-left text-xs sm:text-sm text-neutral-300 border-collapse">
              <thead>
                <tr className="bg-neutral-900/70 border-b border-neutral-800 text-xs uppercase tracking-wider text-neutral-400 font-semibold">
                  <th scope="col" className="py-5 px-8">Plataforma</th>
                  <th scope="col" className="py-5 px-8">Distribuciones Verificadas</th>
                  <th scope="col" className="py-5 px-8">Empaquetado</th>
                  <th scope="col" className="py-5 px-8">Requisitos de Hardware</th>
                  <th scope="col" className="py-5 px-8">Aceleración GPU</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-neutral-800/70">
                <tr className="hover:bg-neutral-900/40 transition-colors">
                  <td className="py-6 px-8">
                    <div className="flex items-center gap-4">
                      <div className="w-10 h-10 rounded-xl bg-cyan-950/50 border border-cyan-500/30 flex items-center justify-center shrink-0">
                        <svg className="w-5 h-5 text-cyan-400" viewBox="0 0 24 24" fill="currentColor">
                          <path d="M0 3.449L9.75 2.1v9.451H0m10.949-9.602L24 0v11.4H10.949M0 12.6h9.75v9.451L0 20.699M10.949 12.6H24V24l-12.949-1.8" />
                        </svg>
                      </div>
                      <div>
                        <span className="block font-bold text-white text-sm">Microsoft Windows</span>
                        <span className="text-xs text-neutral-400">64-bit nativo (x86_64)</span>
                      </div>
                    </div>
                  </td>
                  <td className="py-6 px-8 text-neutral-200">
                    Windows 10 / 11
                  </td>
                  <td className="py-6 px-8 text-neutral-200">
                    Instalador NSIS optimizado
                  </td>
                  <td className="py-6 px-8 text-neutral-200 leading-relaxed">
                    Intel Core i5 / 8 GB RAM <br />
                    <span className="text-xs text-neutral-400">(GPU dedicada para 4K)</span>
                  </td>
                  <td className="py-6 px-8 text-cyan-400 font-mono text-xs">
                    DirectX 11 / OpenGL
                  </td>
                </tr>
                <tr className="hover:bg-neutral-900/40 transition-colors">
                  <td className="py-6 px-8">
                    <div className="flex items-center gap-4">
                      <div className="w-10 h-10 rounded-xl bg-amber-950/50 border border-amber-500/30 flex items-center justify-center shrink-0">
                        <svg className="w-5 h-5 text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.8} d="M8 9l3 3-3 3m5 0h3M5 20h14a2 2 0 002-2V6a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" />
                        </svg>
                      </div>
                      <div>
                        <span className="block font-bold text-white text-sm">GNU/Linux</span>
                        <span className="text-xs text-neutral-400">C++17 compilado</span>
                      </div>
                    </div>
                  </td>
                  <td className="py-6 px-8 text-neutral-200">
                    Arch Linux, CachyOS, Debian, Ubuntu
                  </td>
                  <td className="py-6 px-8 text-neutral-200">
                    Compilación nativa de alto rendimiento
                  </td>
                  <td className="py-6 px-8 text-neutral-200 leading-relaxed">
                    Eficiencia sobresaliente <br />
                    <span className="text-xs text-neutral-400">Mínimo consumo de RAM y CPU</span>
                  </td>
                  <td className="py-6 px-8 text-amber-400 font-mono text-xs">
                    OpenGL 4.3 / Vulkan
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          {/* Tarjetas de Especificaciones para Móvil */}
          <div className="grid grid-cols-1 gap-6 md:hidden">
            {/* Windows Card */}
            <div className="rounded-2xl border border-neutral-800 bg-neutral-950/80 p-6 space-y-5 shadow-xl">
              <div className="flex items-center gap-3 border-b border-neutral-800 pb-4">
                <div className="w-10 h-10 rounded-xl bg-cyan-950/50 border border-cyan-500/30 flex items-center justify-center text-cyan-400 shrink-0">
                  <svg className="w-5 h-5 text-cyan-400" viewBox="0 0 24 24" fill="currentColor">
                    <path d="M0 3.449L9.75 2.1v9.451H0m10.949-9.602L24 0v11.4H10.949M0 12.6h9.75v9.451L0 20.699M10.949 12.6H24V24l-12.949-1.8" />
                  </svg>
                </div>
                <div>
                  <h3 className="text-base font-bold text-white">Microsoft Windows</h3>
                  <span className="text-xs text-neutral-400">Windows 10 / 11 (64-bit)</span>
                </div>
              </div>
              <div className="space-y-3 text-xs">
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Instalador</span>
                  <span className="text-white font-medium text-right">NSIS nativo optimizado</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Hardware</span>
                  <span className="text-white font-medium text-right">Intel Core i5 / 8 GB RAM</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Video</span>
                  <span className="text-white font-medium text-right">GPU dedicada para 4K</span>
                </div>
                <div className="flex justify-between py-1.5">
                  <span className="text-neutral-500">Gráficos</span>
                  <span className="text-cyan-400 font-mono text-right">DirectX 11 / OpenGL</span>
                </div>
              </div>
            </div>

            {/* Linux Card */}
            <div className="rounded-2xl border border-neutral-800 bg-neutral-950/80 p-6 space-y-5 shadow-xl">
              <div className="flex items-center gap-3 border-b border-neutral-800 pb-4">
                <div className="w-10 h-10 rounded-xl bg-amber-950/50 border border-amber-500/30 flex items-center justify-center text-amber-400 shrink-0">
                  <svg className="w-5 h-5 text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.8} d="M8 9l3 3-3 3m5 0h3M5 20h14a2 2 0 002-2V6a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" />
                  </svg>
                </div>
                <div>
                  <h3 className="text-base font-bold text-white">GNU / Linux</h3>
                  <span className="text-xs text-neutral-400">Arch, CachyOS, Debian, Ubuntu</span>
                </div>
              </div>
              <div className="space-y-3 text-xs">
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Compilación</span>
                  <span className="text-white font-medium text-right">Nativa C++17 alto rendimiento</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Eficiencia</span>
                  <span className="text-white font-medium text-right">Mínimo consumo RAM y CPU</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-900">
                  <span className="text-neutral-500">Compatibilidad</span>
                  <span className="text-white font-medium text-right">X11 / Wayland verificado</span>
                </div>
                <div className="flex justify-between py-1.5">
                  <span className="text-neutral-500">Gráficos</span>
                  <span className="text-amber-400 font-mono text-right">OpenGL 4.3 / Vulkan</span>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* SECCIÓN FINAL DE LA LANDING: OPCIÓN PRO COMO ESCALÓN SUPERIOR */}
        <section id="profesionales" className="py-28 px-6 max-w-6xl mx-auto space-y-24">
          {/* Mensaje de transición con espaciado generoso */}
          <div className="rounded-2xl border border-neutral-800 bg-neutral-900/40 p-6 sm:p-8 flex flex-col sm:flex-row items-center justify-between gap-6 shadow-md">
            <div className="space-y-2 text-center sm:text-left">
              <span className="text-xs font-bold uppercase tracking-wider text-cyan-400 block">
                ¿Necesitas infraestructura institucional?
              </span>
              <p className="text-xs sm:text-sm text-neutral-400 leading-relaxed max-w-2xl">
                La versión gratuita cubre con creces las necesidades del culto presencial tradicional. Para redes de auditorios, múltiples templos o respaldo masivo en la nube, presentamos la edición superior para profesionales.
              </p>
            </div>
            <a
              href="#planes"
              className="px-6 py-3 rounded-xl text-xs font-semibold bg-neutral-800 text-white hover:bg-neutral-700 transition-colors whitespace-nowrap shrink-0 shadow-sm"
            >
              Ver Comparativa de Planes
            </a>
          </div>

          <div className="rounded-3xl border border-neutral-800 bg-gradient-to-b from-neutral-900/60 to-neutral-950/80 p-10 sm:p-16 space-y-16 shadow-2xl relative overflow-hidden backdrop-blur-sm">
            <div className="absolute top-0 right-0 w-80 h-80 bg-cyan-500/5 blur-[120px] rounded-full pointer-events-none" />

            <div className="max-w-3xl mx-auto space-y-6 text-center">
              <div className="inline-flex items-center justify-center gap-2 text-xs font-semibold uppercase tracking-widest text-cyan-400">
                <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
                Suscripción Institucional y Técnica
              </div>
              <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight text-center">
                ProyecThor for Professionals
              </h2>
              <p className="text-neutral-400 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto text-justify pt-1">
                Una infraestructura integral que complementa el motor local con servicios distribuidos en la nube, optimizada para coordinadores de medios, directores técnicos y ministerios con múltiples auditorios.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
              {proAdvantages.map((adv) => (
                <div
                  key={adv.title}
                  className="rounded-2xl border border-neutral-800/80 bg-neutral-950/70 p-8 flex flex-col justify-between hover:border-neutral-700 hover:-translate-y-1 transition-all duration-300 hover:shadow-xl hover:shadow-black/50 group space-y-5"
                >
                  <div className="space-y-5">
                    <div className="w-12 h-12 rounded-xl bg-neutral-900 border border-neutral-800 flex items-center justify-center group-hover:border-cyan-500/40 transition-colors">
                      {adv.icon}
                    </div>
                    <h3 className="text-base font-bold text-white tracking-tight group-hover:text-cyan-300 transition-colors">
                      {adv.title}
                    </h3>
                    <p className="text-xs sm:text-sm text-neutral-400 leading-relaxed text-justify">
                      {adv.desc}
                    </p>
                  </div>
                </div>
              ))}
            </div>

            <div className="pt-10 mt-6 border-t border-neutral-800/80 flex flex-col sm:flex-row items-center justify-between gap-6">
              <span className="text-xs sm:text-sm text-neutral-400 text-center sm:text-left">
                Activación y enlace inmediato mediante clave de licencia empresarial sin necesidad de reinstalar.
              </span>
              <a
                href="#planes"
                className="px-7 py-3.5 rounded-xl text-xs font-semibold bg-cyan-400 text-neutral-950 hover:bg-cyan-300 transition-all shadow-md shadow-cyan-950/40 shrink-0"
              >
                Consultar Planes y Tarifas
              </a>
            </div>
          </div>

          {/* COMPARACIÓN DE EDICIONES Y SUSCRIPCIÓN */}
          <div id="planes" className="pt-16 space-y-16">
            <div className="text-center max-w-2xl mx-auto space-y-6">
              <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight text-center">
                Elige el modelo adecuado para tu auditorio
              </h2>
              <p className="text-neutral-400 text-sm sm:text-base leading-relaxed pt-1">
                Tanto la versión gratuita como la profesional comparten el mismo motor de alta estabilidad. Elige la versión Pro cuando requieras las mayores capacidades de última generación y especificaciones más puntuales.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-10 items-stretch max-w-5xl mx-auto">
              {/* PLAN CORE (GRATUITO) */}
              <div className="rounded-3xl border border-neutral-800 bg-neutral-950 p-10 sm:p-12 flex flex-col justify-between space-y-10 hover:border-neutral-700 transition-all duration-300 shadow-xl">
                <div className="space-y-8">
                  <div className="space-y-3">
                    <div className="text-xs font-semibold uppercase tracking-wider text-neutral-400">
                      Edición de Escritorio
                    </div>
                    <div className="text-3xl font-bold text-white">
                      ProyecThor Base
                    </div>
                    <div className="text-xs sm:text-sm text-neutral-500">
                      Licencia permanente, libre y sin límites para instalación local
                    </div>
                  </div>

                  <div className="border-t border-neutral-900 pt-8 space-y-4 text-xs sm:text-sm text-neutral-300">
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>Ejecución nativa en C++ y aceleración por hardware de gráficos</span>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>Salida de doble pantalla independiente sin marcas de agua</span>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>100% Funcional sin conexión a internet (offline garantizado)</span>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>Bibliotecas locales de canciones, versiones bíblicas XML y multimedia</span>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>Stage Display local para monitores de retorno en escenario</span>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                      <span>Streaming local mediante LAN</span>
                    </div>
                  </div>
                </div>

                <a
                  href="https://github.com/TheVixcho/ProyecThor/releases"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="w-full py-4 rounded-xl text-xs font-semibold text-center border border-neutral-700 bg-neutral-900 text-white hover:bg-neutral-800 transition-colors shadow-sm"
                >
                  Descargar Versión Gratuita
                </a>
              </div>

              {/* PLAN PRO FOR PROFESSIONALS */}
              <div className="pro-border-glow rounded-3xl border bg-gradient-to-b from-neutral-900/60 to-neutral-950 p-10 sm:p-12 flex flex-col justify-between space-y-10 relative shadow-2xl transition-all duration-300">
                <div className="space-y-8">
                  <div className="space-y-3">
                    <div className="text-xs font-semibold uppercase tracking-wider text-cyan-400">
                      Suscripción Profesional
                    </div>
                    <div className="text-3xl font-bold text-white">
                      For Professionals
                    </div>
                    <div className="text-xs sm:text-sm text-neutral-400">
                      Diseñado para auditorios, recintos de culto y eventos de alta demanda
                    </div>
                  </div>

                  <div className="border-t border-neutral-800 pt-8 space-y-4 text-xs sm:text-sm text-neutral-200">
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                      <div>
                        <strong className="text-white block font-medium">Almacenamiento Cloud para 5,000+ canciones</strong>
                        <span className="text-neutral-400 text-xs">Sincronización continua de catálogo y secuencias entre equipos.</span>
                      </div>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                      <div>
                        <strong className="text-white block font-medium">Sincronización multi-sala y sedes distribuidas</strong>
                        <span className="text-neutral-400 text-xs">Edita en un equipo y propaga cambios a todas las salas del templo.</span>
                      </div>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                      <div>
                        <strong className="text-white block font-medium">Herramientas avanzadas de redacción litúrgica</strong>
                        <span className="text-neutral-400 text-xs">Estructuración rápida de estrofas y verificación métrica.</span>
                      </div>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                      <div>
                        <strong className="text-white block font-medium">Copias de seguridad automáticas y redundancia</strong>
                        <span className="text-neutral-400 text-xs">Protección integral en la nube ante incidentes de hardware.</span>
                      </div>
                    </div>
                    <div className="flex items-start gap-3.5">
                      <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                      <div>
                        <strong className="text-white block font-medium">Soporte técnico preferente para eventos críticos</strong>
                        <span className="text-neutral-400 text-xs">Canal directo de contacto y asistencia en producción.</span>
                      </div>
                    </div>
                  </div>
                </div>

                <a
                  href="https://vixcho.lemonsqueezy.com/checkout/buy/e609efc0-4393-46fc-b6ed-d535cb9dd2dc"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="w-full py-4 rounded-xl text-xs font-semibold text-center bg-cyan-400 text-neutral-950 hover:bg-cyan-300 transition-all duration-200 shadow-md shadow-cyan-950/40 hover:scale-[1.01]"
                >
                  Suscribirse a ProyecThor for Professionals
                </a>
              </div>
            </div>
          </div>
        </section>
      </main>

      {/* FOOTER CORPORATIVO */}
      <footer className="border-t border-neutral-800/80 bg-neutral-950 py-12 relative z-10 text-xs text-neutral-500">
        <div className="max-w-6xl mx-auto px-6 flex flex-col sm:flex-row justify-between items-center gap-6">
          <div className="flex items-center gap-3">
            <img src={logo} alt="ProyecThor Logo" className="w-5 h-5 grayscale opacity-60" />
            <span>© 2026 ProyecThor. Todos los derechos reservados.</span>
          </div>

          <div className="flex items-center gap-6 text-neutral-400">
            <a
              href="https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"
              target="_blank"
              rel="noopener noreferrer"
              className="hover:text-white transition-colors"
            >
              Canal Oficial
            </a>
            <a
              href="https://ko-fi.com/vixcho"
              target="_blank"
              rel="noopener noreferrer"
              className="hover:text-white transition-colors"
            >
              Patrocinio en Ko-fi
            </a>
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="hover:text-white transition-colors"
            >
              Descargas
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}
