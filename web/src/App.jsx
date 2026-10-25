import React, { useState } from "react";
import logo from "./assets/logo.png";

const caracteristicas = [
  {
    titulo: "Arquitectura Multi-Capa",
    descripcion:
      "Gestión independiente y acelerada por GPU para fondos de video, bucles gráficos y capas de texto dinámicas sin interferencia entre elementos.",
    badge: "Motor Gráfico",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
      </svg>
    ),
  },
  {
    titulo: "Despliegue en Doble Pantalla",
    descripcion:
      "Consola de control dedicada para el operador con monitorización en tiempo real, desacoplada de la salida limpia sin bordes para el proyector.",
    badge: "Producción",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Biblioteca Centralizada",
    descripcion:
      "Repertorio de letras, versiones integradas de las Sagradas Escrituras en formato XML estructurado, subtítulos automatizados y archivos de audio.",
    badge: "Gestión",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 6.253v13m0-13C10.832 5.477 9.246 5 7.5 5S4.168 5.477 3 6.253v13C4.168 18.477 5.754 18 7.5 18s3.332.477 4.5 1.253m0-13C13.168 5.477 14.754 5 16.5 5c1.747 0 3.332.477 4.5 1.253v13C19.832 18.477 18.247 18 16.5 18c-1.746 0-3.332.477-4.5 1.253" />
      </svg>
    ),
  },
  {
    titulo: "Streaming RTMP e Integración OSC",
    descripcion:
      "Emisión directa a servidores de retransmisión, distribución en red local y control sincronizado bidireccional mediante el protocolo OSC.",
    badge: "Transmisión",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Shaders y Post-Procesado en Vivo",
    descripcion:
      "Efectos visuales procesados directamente en la GPU: simulación de emulsión analógica, grano cinematográfico, viñeta y desenfoque dinámico.",
    badge: "Efectos GPU",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
      </svg>
    ),
  },
  {
    titulo: "Arquitectura Modular y Plugins",
    descripcion:
      "Ecosistema preparado para la conexión de extensiones personalizadas, automatizaciones avanzadas y utilidades externas de producción.",
    badge: "Extensibilidad",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M11 4a2 2 0 114 0v1a1 1 0 001 1h3a1 1 0 011 1v3a1 1 0 01-1 1h-1a2 2 0 100 4h1a1 1 0 011 1v3a1 1 0 01-1 1h-3a1 1 0 01-1-1v-1a2 2 0 10-4 0v1a1 1 0 01-1 1H7a1 1 0 01-1-1v-3a1 1 0 00-1-1H4a2 2 0 110-4h1a1 1 0 001-1V7a1 1 0 011-1h3a1 1 0 001-1V4z" />
      </svg>
    ),
  },
];

const proAdvantages = [
  {
    title: "Almacenamiento Cloud para 5,000+ Canciones",
    desc: "Capacidad extendida en la nube para sincronizar y respaldar catálogos completos de repertorio con acceso instantáneo desde cualquier estación de trabajo.",
    icon: (
      <svg className="w-5 h-5 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M3 15a4 4 0 004 4h9a5 5 0 10-.1-9.999 5.002 5.002 0 00-9.78 2.096A4.001 4.001 0 003 15z" />
      </svg>
    ),
  },
  {
    title: "Motor de Redacción Asistido por IA",
    desc: "Herramientas de lenguaje natural diseñadas para componer, corregir métrica poética, estructurar estrofas y dar formato inmediato a himnos y notas litúrgicas.",
    icon: (
      <svg className="w-5 h-5 text-indigo-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
      </svg>
    ),
  },
  {
    title: "Asistente y Chat de Voz en Tiempo Real",
    desc: "Canal de interacción por voz que permite al operador solicitar lecturas, transiciones de pantalla o búsquedas temáticas con manos libres durante el directo.",
    icon: (
      <svg className="w-5 h-5 text-sky-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M19 11a7 7 0 01-7 7m0 0a7 7 0 01-7-7m7 7v4m0 0H8m4 0h4m-4-8a3 3 0 01-3-3V5a3 3 0 116 0v6a3 3 0 01-3 3z" />
      </svg>
    ),
  },
  {
    title: "Sincronización Multi-Sala y Multi-Sede",
    desc: "Centralice los cambios editoriales: una modificación en el puesto de preparación se propaga de manera transparente a todas las cabinas de control.",
    icon: (
      <svg className="w-5 h-5 text-emerald-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
      </svg>
    ),
  },
  {
    title: "Respaldos Automáticos con Redundancia",
    desc: "Copias de seguridad automáticas en la nube para garantizar la continuidad operativa de presentaciones y configuraciones críticas ante contingencias de hardware.",
    icon: (
      <svg className="w-5 h-5 text-purple-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z" />
      </svg>
    ),
  },
  {
    title: "Soporte Técnico y Despliegues Dedicados",
    desc: "Canal prioritario de asistencia técnica para auditorios y acceso preferente a compilaciones preliminares y mejoras de rendimiento antes de su publicación general.",
    icon: (
      <svg className="w-5 h-5 text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M18.364 5.636l-3.536 3.536m0 5.656l3.536 3.536M9.172 9.172L5.636 5.636m3.536 9.192l-3.536 3.536M21 12a9 9 0 11-18 0 9 9 0 0118 0zm-5 0a4 4 0 11-8 0 4 4 0 018 0z" />
      </svg>
    ),
  },
];

const plataformas = [
  {
    sistema: "Microsoft Windows 10 / 11 (64-bit)",
    detalle: "Instalador NSIS optimizado · Intel Core i5 / 8 GB RAM recomendado (GPU dedicada para resoluciones 4K)",
  },
  {
    sistema: "GNU/Linux (Arch Linux / CachyOS / Debian / Ubuntu)",
    detalle: "Compilación nativa de alto rendimiento · Eficiencia sobresaliente en consumo de memoria y CPU",
  },
];

export default function App() {
  return (
    <div className="min-h-screen flex flex-col bg-neutral-950 text-neutral-300 font-sans antialiased selection:bg-neutral-800 selection:text-white">
      {/* Iluminación de fondo sutil y controlada */}
      <div className="fixed inset-0 pointer-events-none overflow-hidden z-0">
        <div className="absolute top-0 left-1/2 -translate-x-1/2 w-[800px] h-[350px] bg-neutral-900/40 blur-[140px] rounded-full" />
      </div>

      {/* Navegación Superior */}
      <header className="sticky top-0 z-50 backdrop-blur-md bg-neutral-950/90 border-b border-neutral-800/80">
        <div className="max-w-6xl mx-auto px-6 h-16 flex justify-between items-center">
          <div className="flex items-center gap-3">
            <img src={logo} alt="ProyecThor Logo" className="w-7 h-7 object-contain" />
            <div className="flex items-baseline gap-2">
              <span className="text-sm font-semibold tracking-tight text-white uppercase">
                ProyecThor
              </span>
              <span className="text-[11px] text-neutral-500 font-medium">
                Professional Audio-Visual Engine
              </span>
            </div>
          </div>
          <nav className="hidden md:flex items-center gap-8 text-xs uppercase tracking-wider text-neutral-400 font-medium">
            <a href="#inicio" className="hover:text-white transition-colors">
              Inicio
            </a>
            <a href="#caracteristicas" className="hover:text-white transition-colors">
              Especificaciones
            </a>
            <a href="#profesionales" className="hover:text-cyan-400 transition-colors">
              Edición Professionals
            </a>
            <a href="#planes" className="hover:text-white transition-colors">
              Planes
            </a>
            <a href="#compatibilidad" className="hover:text-white transition-colors">
              Entornos
            </a>
          </nav>
          <div className="flex items-center gap-3">
            <a
              href="#planes"
              className="px-4 py-2 text-xs font-semibold rounded bg-neutral-100 text-neutral-950 hover:bg-white transition-all shadow-sm"
            >
              Adquirir Suscripción
            </a>
          </div>
        </div>
      </header>

      {/* Contenido Principal */}
      <main className="relative z-10 flex-1">
        {/* HERO SECTION */}
        <section id="inicio" className="pt-24 pb-20 px-6 max-w-5xl mx-auto text-center space-y-8">
          <div className="inline-flex items-center gap-2 px-3 py-1 rounded border border-neutral-800 bg-neutral-900/70 text-neutral-400 text-xs tracking-wide">
            <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
            <span>Nueva Edición: ProyecThor for Professionals con Almacenamiento Cloud e IA</span>
          </div>

          <h1 className="text-4xl sm:text-6xl font-bold tracking-tight text-white max-w-4xl mx-auto leading-tight">
            Motor de proyección audiovisual de alta precisión y baja latencia
          </h1>

          <p className="text-base text-neutral-400 max-w-2xl mx-auto leading-relaxed">
            Diseñado para salas de auditorio, templos y centros de eventos donde la estabilidad operativa es indispensable. Descarga la versión base gratuita o amplía las capacidades de tu equipo mediante la suscripción profesional.
          </p>

          <div className="flex flex-wrap justify-center items-center gap-4 pt-2">
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="px-6 py-3 rounded text-xs font-semibold bg-white text-neutral-950 hover:bg-neutral-200 transition-all shadow"
            >
              Descargar Versión Gratuita
            </a>
            <a
              href="#profesionales"
              className="px-6 py-3 rounded text-xs font-semibold bg-neutral-900 border border-neutral-700 text-neutral-200 hover:border-neutral-500 hover:text-white transition-all"
            >
              Conocer Ventajas Professionals
            </a>
          </div>

          {/* Métricas Técnicas */}
          <div className="pt-12 grid grid-cols-2 md:grid-cols-4 gap-6 max-w-4xl mx-auto border-t border-neutral-900">
            <div className="text-left border-l border-neutral-800 pl-4">
              <span className="block text-2xl font-bold text-white tracking-tight">0 ms</span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">Latencia de Procesado</span>
            </div>
            <div className="text-left border-l border-neutral-800 pl-4">
              <span className="block text-2xl font-bold text-white tracking-tight">C++17</span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">Compilación Nativa</span>
            </div>
            <div className="text-left border-l border-neutral-800 pl-4">
              <span className="block text-2xl font-bold text-white tracking-tight">5,000+</span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">Canciones en Cloud Pro</span>
            </div>
            <div className="text-left border-l border-neutral-800 pl-4">
              <span className="block text-2xl font-bold text-white tracking-tight">IA Vocal</span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">Operación Manos Libres</span>
            </div>
          </div>
        </section>

        {/* SECCIÓN PRO: PROYECTHOR FOR PROFESSIONALS */}
        <section id="profesionales" className="py-20 px-6 max-w-6xl mx-auto">
          <div className="rounded-2xl border border-neutral-800 bg-neutral-900/30 p-8 sm:p-12 space-y-10">
            <div className="max-w-3xl space-y-3">
              <div className="text-xs font-semibold uppercase tracking-widest text-cyan-400">
                Suscripción Institucional y Técnica
              </div>
              <h2 className="text-2xl sm:text-4xl font-bold text-white tracking-tight">
                ProyecThor for Professionals
              </h2>
              <p className="text-neutral-400 text-sm leading-relaxed">
                Una infraestructura integral que complementa el motor local con servicios distribuidos en la nube, optimizada para coordinadores de medios, directores técnicos y ministerios con múltiples auditorios.
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {proAdvantages.map((adv) => (
                <div
                  key={adv.title}
                  className="rounded-xl border border-neutral-800 bg-neutral-950/70 p-6 flex flex-col justify-between hover:border-neutral-700 transition-colors"
                >
                  <div className="space-y-3">
                    <div className="w-9 h-9 rounded bg-neutral-900 border border-neutral-800 flex items-center justify-center">
                      {adv.icon}
                    </div>
                    <h3 className="text-sm font-semibold text-white tracking-tight">
                      {adv.title}
                    </h3>
                    <p className="text-xs text-neutral-400 leading-relaxed">
                      {adv.desc}
                    </p>
                  </div>
                </div>
              ))}
            </div>

            <div className="pt-6 border-t border-neutral-800/80 flex flex-col sm:flex-row items-center justify-between gap-4">
              <span className="text-xs text-neutral-400">
                Activación y enlace inmediato mediante clave de licencia empresarial sin reinstalaciones.
              </span>
              <a
                href="#planes"
                className="px-5 py-2.5 rounded text-xs font-semibold bg-cyan-500 text-neutral-950 hover:bg-cyan-400 transition-colors"
              >
                Consultar Planes y Tarifas
              </a>
            </div>
          </div>
        </section>

        {/* CARACTERÍSTICAS TÉCNICAS */}
        <section id="caracteristicas" className="py-16 px-6 max-w-6xl mx-auto space-y-10">
          <div className="border-b border-neutral-800 pb-4">
            <h2 className="text-xs uppercase tracking-widest text-neutral-500 font-semibold">
              Arquitectura Base
            </h2>
            <p className="text-xl font-bold text-white mt-1">
              Capacidades técnicas del motor local
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {caracteristicas.map((c) => (
              <div
                key={c.titulo}
                className="rounded-xl border border-neutral-800/80 bg-neutral-900/20 p-6 space-y-3 hover:border-neutral-700 transition-colors"
              >
                <div className="flex items-center justify-between">
                  <div className="p-2 rounded bg-neutral-900 border border-neutral-800">
                    {c.icon}
                  </div>
                  <span className="text-[10px] font-semibold uppercase tracking-wider text-neutral-500">
                    {c.badge}
                  </span>
                </div>
                <h3 className="text-sm font-semibold text-white">
                  {c.titulo}
                </h3>
                <p className="text-xs text-neutral-400 leading-relaxed">
                  {c.descripcion}
                </p>
              </div>
            ))}
          </div>
        </section>

        {/* COMPARACIÓN DE EDICIONES Y SUSCRIPCIÓN */}
        <section id="planes" className="py-20 px-6 max-w-5xl mx-auto space-y-12">
          <div className="text-center max-w-2xl mx-auto space-y-2">
            <span className="text-xs uppercase tracking-widest text-neutral-400 font-semibold">
              Estructura de Licenciamiento
            </span>
            <h2 className="text-3xl font-bold text-white tracking-tight">
              Seleccione el modelo operativo de su organización
            </h2>
            <p className="text-neutral-400 text-xs leading-relaxed">
              La edición de escritorio es permanente y sin costo. La suscripción profesional incorpora capacidades en la nube y asistencia por inteligencia artificial.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8 items-stretch">
            {/* PLAN CORE (GRATUITO) */}
            <div className="rounded-2xl border border-neutral-800 bg-neutral-950 p-8 flex flex-col justify-between space-y-8">
              <div className="space-y-6">
                <div className="space-y-2">
                  <div className="text-xs font-semibold uppercase tracking-wider text-neutral-400">
                    Edición de Escritorio
                  </div>
                  <div className="text-3xl font-bold text-white">
                    ProyecThor Base
                  </div>
                  <div className="text-xs text-neutral-500">
                    Licencia de uso gratuito para instalación local
                  </div>
                </div>

                <div className="border-t border-neutral-900 pt-6 space-y-3 text-xs text-neutral-300">
                  <div className="flex items-start gap-3">
                    <span className="text-neutral-400 font-mono text-sm leading-none">—</span>
                    <span>Ejecución nativa en C++ y aceleración por hardware de gráficos</span>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-neutral-400 font-mono text-sm leading-none">—</span>
                    <span>Salida de doble pantalla independiente sin bordes para proyector</span>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-neutral-400 font-mono text-sm leading-none">—</span>
                    <span>Capas simultáneas de video, fondo gráfico y texto dinámico</span>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-neutral-400 font-mono text-sm leading-none">—</span>
                    <span>Bibliotecas locales de canciones, versiones bíblicas XML y multimedia</span>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-neutral-400 font-mono text-sm leading-none">—</span>
                    <span>Streaming local RTMP y compatibilidad con plugins externos</span>
                  </div>
                </div>
              </div>

              <a
                href="https://github.com/TheVixcho/ProyecThor/releases"
                target="_blank"
                rel="noopener noreferrer"
                className="w-full py-3 rounded text-xs font-semibold text-center border border-neutral-700 bg-neutral-900 text-white hover:bg-neutral-800 transition-colors"
              >
                Descargar Instalador Base
              </a>
            </div>

            {/* PLAN PRO FOR PROFESSIONALS */}
            <div className="rounded-2xl border border-cyan-500/40 bg-neutral-900/40 p-8 flex flex-col justify-between space-y-8 relative shadow-lg shadow-neutral-950">
              <div className="absolute top-6 right-6">
                <span className="text-[10px] uppercase font-bold tracking-widest text-cyan-400 border border-cyan-500/40 bg-cyan-950/40 px-2 py-1 rounded">
                  Infraestructura Cloud
                </span>
              </div>

              <div className="space-y-6">
                <div className="space-y-2">
                  <div className="text-xs font-semibold uppercase tracking-wider text-cyan-400">
                    Suscripción Profesional
                  </div>
                  <div className="text-3xl font-bold text-white">
                    For Professionals
                  </div>
                  <div className="text-xs text-neutral-400">
                    Diseñado para auditorios, recintos de culto y eventos de alta demanda
                  </div>
                </div>

                <div className="border-t border-neutral-800 pt-6 space-y-3.5 text-xs text-neutral-200">
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none">+</span>
                    <div>
                      <strong className="text-white block font-medium">Almacenamiento Cloud para 5,000+ canciones</strong>
                      <span className="text-neutral-400">Sincronización continua de catálogo y secuencias entre equipos.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none">+</span>
                    <div>
                      <strong className="text-white block font-medium">Herramientas de redacción asistidas por IA</strong>
                      <span className="text-neutral-400">Estructuración inteligente de estrofas y corrección métrica.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none">+</span>
                    <div>
                      <strong className="text-white block font-medium">Comandos y chat de voz en tiempo real con IA</strong>
                      <span className="text-neutral-400">Asistencia hands-free en directo para el operador de cabina.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none">+</span>
                    <div>
                      <strong className="text-white block font-medium">Copias de seguridad automáticas y redundancia</strong>
                      <span className="text-neutral-400">Protección integral de datos y configuraciones ante incidentes.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none">+</span>
                    <div>
                      <strong className="text-white block font-medium">Soporte técnico preferente para eventos críticos</strong>
                      <span className="text-neutral-400">Canal directo de contacto y asistencia en producción.</span>
                    </div>
                  </div>
                </div>
              </div>

              <a
                href="https://vixcho.lemonsqueezy.com/checkout/buy/e609efc0-4393-46fc-b6ed-d535cb9dd2dc"
                target="_blank"
                rel="noopener noreferrer"
                className="w-full py-3 rounded text-xs font-semibold text-center bg-cyan-400 text-neutral-950 hover:bg-cyan-300 transition-colors shadow-sm"
              >
                Suscribirse a ProyecThor for Professionals
              </a>
            </div>
          </div>
        </section>

        {/* PLATAFORMAS COMPATIBLES */}
        <section id="compatibilidad" className="py-16 px-6 max-w-4xl mx-auto space-y-6">
          <div className="border-b border-neutral-800 pb-3">
            <h2 className="text-xs uppercase tracking-widest text-neutral-500 font-semibold">
              Requerimientos de Sistema
            </h2>
            <p className="text-lg font-bold text-white mt-1">
              Entornos de Ejecución Soportados
            </p>
          </div>

          <div className="divide-y divide-neutral-800/80 border border-neutral-800 rounded-xl bg-neutral-950 overflow-hidden">
            {plataformas.map((plat) => (
              <div
                key={plat.sistema}
                className="p-5 flex flex-col sm:flex-row sm:justify-between sm:items-center gap-2"
              >
                <div className="flex items-center gap-3">
                  <div className="w-2 h-2 rounded-full bg-neutral-600" />
                  <span className="text-sm font-medium text-white">{plat.sistema}</span>
                </div>
                <span className="text-xs text-neutral-400 sm:text-right">{plat.detalle}</span>
              </div>
            ))}
          </div>
        </section>
      </main>

      {/* FOOTER CORPORATIVO */}
      <footer className="border-t border-neutral-800/80 bg-neutral-950 py-8 relative z-10 text-xs text-neutral-500">
        <div className="max-w-6xl mx-auto px-6 flex flex-col sm:flex-row justify-between items-center gap-4">
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
