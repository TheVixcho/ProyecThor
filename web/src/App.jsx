import React, { useState, useEffect } from "react";
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
    titulo: "Stage Display (Monitores de Confianza)",
    descripcion:
      "Salida de retorno ultra rápida para escenario: muestra letra actual, próxima estrofa, reloj en vivo y mensajes discretos de cabina.",
    badge: "Escenario",
    icon: (
      <svg className="w-5 h-5 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 18h.01M7 21h10a2 2 0 002-2V5a2 2 0 00-2-2H7a2 2 0 00-2 2v14a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    titulo: "Biblioteca Centralizada",
    descripcion:
      "Repertorio de canciones, versiones integradas de las Sagradas Escrituras en formato XML estructurado, subtítulos y archivos de audio.",
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
    titulo: "Shaders y Post-Procesado en GPU",
    descripcion:
      "Efectos visuales procesados en tiempo real: simulación de emulsión analógica, grano cinematográfico, viñeta y desenfoque dinámico.",
    badge: "Efectos GPU",
    icon: (
      <svg className="w-5 h-5 text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
      </svg>
    ),
  },
];

const proAdvantages = [
  {
    title: "Almacenamiento Cloud para 5,000+ Canciones",
    desc: "Capacidad extendida en la nube para sincronizar y respaldar catálogos completos de repertorio con acceso instantáneo desde cualquier estación de trabajo.",
    badge: "Cloud Repertorio",
    icon: (
      <svg className="w-5 h-5 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M3 15a4 4 0 004 4h9a5 5 0 10-.1-9.999 5.002 5.002 0 00-9.78 2.096A4.001 4.001 0 003 15z" />
      </svg>
    ),
  },
  {
    title: "Stage Display y Monitores de Confianza",
    desc: "Transmisión dedicada en tiempo real para músicos, directores y predicadores con vista de estrofa actual, próxima sección, reloj sincronizado y avisos de cabina.",
    badge: "Retorno Escenario",
    icon: (
      <svg className="w-5 h-5 text-emerald-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
      </svg>
    ),
  },
  {
    title: "Motor de Redacción Asistido por IA",
    desc: "Herramientas de lenguaje natural diseñadas para componer, corregir métrica poética, estructurar estrofas y dar formato inmediato a himnos y notas litúrgicas.",
    badge: "IA Avanzada",
    icon: (
      <svg className="w-5 h-5 text-indigo-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
      </svg>
    ),
  },
  {
    title: "Sincronización Multi-Sala y Multi-Sede",
    desc: "Centralice los cambios editoriales: una modificación en el puesto de preparación se propaga de manera transparente a todas las cabinas de control.",
    badge: "Distribución",
    icon: (
      <svg className="w-5 h-5 text-sky-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
      </svg>
    ),
  },
  {
    title: "Respaldos Automáticos con Redundancia",
    desc: "Copias de seguridad automáticas en la nube para garantizar la continuidad operativa de presentaciones y configuraciones críticas ante contingencias de hardware.",
    badge: "Seguridad Cloud",
    icon: (
      <svg className="w-5 h-5 text-purple-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z" />
      </svg>
    ),
  },
  {
    title: "Soporte Técnico y Despliegues Dedicados",
    desc: "Canal prioritario de asistencia técnica para auditorios y acceso preferente a compilaciones preliminares y mejoras de rendimiento antes de su publicación general.",
    badge: "Prioritario",
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
    detalle: "Instalador NSIS nativo optimizado · Intel Core i5 / 8 GB RAM recomendado (GPU dedicada para 4K)",
  },
  {
    sistema: "GNU/Linux (Arch Linux / CachyOS / Debian / Ubuntu)",
    detalle: "Compilación nativa de alto rendimiento · Eficiencia sobresaliente en consumo de memoria y CPU",
  },
];

export default function App() {
  const [stageLayout, setStageLayout] = useState("banda"); // "banda", "pulpito", "reloj"
  const [clockTime, setClockTime] = useState("");

  useEffect(() => {
    const updateClock = () => {
      const d = new Date();
      setClockTime(
        d.toLocaleTimeString("es-CL", {
          hour: "2-digit",
          minute: "2-digit",
          second: "2-digit",
          hour12: true,
        })
      );
    };
    updateClock();
    const interval = setInterval(updateClock, 1000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="min-h-screen flex flex-col bg-neutral-950 text-neutral-300 font-sans antialiased selection:bg-neutral-800 selection:text-white relative overflow-x-hidden">
      {/* Iluminación de fondo ambiental animada */}
      <div className="fixed inset-0 pointer-events-none overflow-hidden z-0">
        <div className="ambient-glow-1 absolute top-0 left-1/3 -translate-x-1/2 w-[650px] h-[360px] bg-cyan-600/15 blur-[160px] rounded-full" />
        <div className="ambient-glow-2 absolute top-1/4 right-1/4 w-[550px] h-[340px] bg-emerald-600/10 blur-[150px] rounded-full" />
        <div className="absolute top-2/3 left-1/2 -translate-x-1/2 w-[700px] h-[300px] bg-indigo-900/10 blur-[170px] rounded-full" />
      </div>

      {/* Navegación Superior */}
      <header className="sticky top-0 z-50 backdrop-blur-md bg-neutral-950/85 border-b border-neutral-800/80 transition-all">
        <div className="max-w-6xl mx-auto px-6 h-16 flex justify-between items-center">
          <div className="flex items-center gap-3 group cursor-pointer">
            <img
              src={logo}
              alt="ProyecThor Logo"
              className="w-8 h-8 object-contain transition-transform duration-300 group-hover:scale-110"
            />
            <div className="flex items-baseline gap-2">
              <span className="text-base font-bold tracking-tight text-white uppercase group-hover:text-cyan-400 transition-colors">
                ProyecThor
              </span>
              <span className="hidden sm:inline-block text-[11px] text-neutral-500 font-medium tracking-wide">
                Engine 1.1
              </span>
            </div>
          </div>
          <nav className="hidden md:flex items-center gap-7 text-xs uppercase tracking-wider text-neutral-400 font-medium">
            <a href="#inicio" className="hover:text-white transition-colors py-1">
              Inicio
            </a>
            <a href="#profesionales" className="hover:text-cyan-400 transition-colors py-1">
              Edición Professionals
            </a>
            <a href="#stage" className="text-emerald-400 hover:text-emerald-300 transition-colors py-1 flex items-center gap-1.5 font-semibold">
              <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
              Stage Display
            </a>
            <a href="#caracteristicas" className="hover:text-white transition-colors py-1">
              Especificaciones
            </a>
            <a href="#planes" className="hover:text-white transition-colors py-1">
              Planes
            </a>
            <a href="#compatibilidad" className="hover:text-white transition-colors py-1">
              Entornos
            </a>
          </nav>
          <div className="flex items-center gap-3">
            <a
              href="#planes"
              className="px-4 py-2 text-xs font-semibold rounded-lg bg-white text-neutral-950 hover:bg-neutral-200 hover:shadow-lg hover:shadow-white/10 transition-all duration-200"
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
          <div className="inline-flex items-center gap-2.5 px-4 py-1.5 rounded-full border border-cyan-500/30 bg-cyan-950/40 text-cyan-300 text-xs tracking-wide font-medium shadow-[0_0_20px_rgba(6,182,212,0.15)] transition-all hover:border-cyan-400/50">
            <span className="relative flex h-2 w-2">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-cyan-400 opacity-75"></span>
              <span className="relative inline-flex rounded-full h-2 w-2 bg-cyan-500"></span>
            </span>
            <span>Suscripción Institucional y Técnica · ProyecThor for Professionals</span>
          </div>

          <h1 className="text-4xl sm:text-6xl font-extrabold tracking-tight text-white max-w-4xl mx-auto leading-tight">
            Motor de proyección audiovisual de{" "}
            <span className="bg-gradient-to-r from-white via-neutral-200 to-cyan-400 bg-clip-text text-transparent">
              alta precisión y baja latencia
            </span>
          </h1>

          <p className="text-base sm:text-lg text-neutral-400 max-w-2xl mx-auto leading-relaxed font-normal">
            Diseñado para salas de auditorio, templos y centros de eventos donde la estabilidad operativa es indispensable. Descarga la versión base gratuita o amplía las capacidades de tu equipo mediante la suscripción profesional.
          </p>

          <div className="flex flex-wrap justify-center items-center gap-4 pt-2">
            <a
              href="https://github.com/TheVixcho/ProyecThor/releases"
              target="_blank"
              rel="noopener noreferrer"
              className="px-7 py-3.5 rounded-lg text-xs font-semibold bg-white text-neutral-950 hover:bg-neutral-200 transition-all duration-200 shadow-md shadow-white/10 hover:scale-[1.02]"
            >
              Descargar Versión Gratuita
            </a>
            <a
              href="#stage"
              className="px-7 py-3.5 rounded-lg text-xs font-semibold bg-neutral-900 border border-neutral-700 text-neutral-200 hover:border-cyan-500 hover:text-white transition-all duration-200 hover:scale-[1.02]"
            >
              Ver Stage Display en Vivo
            </a>
          </div>

          {/* Métricas Técnicas */}
          <div className="pt-14 grid grid-cols-2 md:grid-cols-4 gap-6 max-w-4xl mx-auto border-t border-neutral-900">
            <div className="text-left border-l-2 border-neutral-800 pl-4 transition-all hover:border-cyan-400 group">
              <span className="block text-2xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                0 ms
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Latencia de Procesado
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-4 transition-all hover:border-cyan-400 group">
              <span className="block text-2xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                C++17
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Compilación Nativa
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-4 transition-all hover:border-cyan-400 group">
              <span className="block text-2xl font-bold text-white tracking-tight group-hover:text-cyan-400 transition-colors">
                5,000+
              </span>
              <span className="text-xs text-neutral-500 uppercase tracking-wider">
                Canciones en Cloud Pro
              </span>
            </div>
            <div className="text-left border-l-2 border-neutral-800 pl-4 transition-all hover:border-emerald-400 group">
              <span className="block text-2xl font-bold text-emerald-400 tracking-tight transition-colors">
                Stage Display
              </span>
              <span className="text-xs text-neutral-400 uppercase tracking-wider">
                Retorno y Músicos en Vivo
              </span>
            </div>
          </div>
        </section>

        {/* SECCIÓN PRO: PROYECTHOR FOR PROFESSIONALS */}
        <section id="profesionales" className="py-20 px-6 max-w-6xl mx-auto">
          <div className="rounded-2xl border border-neutral-800 bg-gradient-to-b from-neutral-900/60 to-neutral-950/80 p-8 sm:p-12 space-y-10 shadow-2xl relative overflow-hidden backdrop-blur-sm">
            <div className="absolute top-0 right-0 w-80 h-80 bg-cyan-500/5 blur-[120px] rounded-full pointer-events-none" />

            <div className="max-w-3xl space-y-3">
              <div className="inline-flex items-center gap-2 text-xs font-semibold uppercase tracking-widest text-cyan-400">
                <span className="w-1.5 h-1.5 rounded-full bg-cyan-400" />
                Suscripción Institucional y Técnica
              </div>
              <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
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
                  className="rounded-xl border border-neutral-800/80 bg-neutral-950/70 p-6 flex flex-col justify-between hover:border-neutral-700 hover:-translate-y-1 transition-all duration-300 hover:shadow-xl hover:shadow-black/50 group"
                >
                  <div className="space-y-3.5">
                    <div className="flex items-center justify-between">
                      <div className="w-10 h-10 rounded-lg bg-neutral-900 border border-neutral-800 flex items-center justify-center group-hover:border-cyan-500/40 transition-colors">
                        {adv.icon}
                      </div>
                      <span className="text-[10px] font-semibold uppercase tracking-wider text-neutral-500 px-2 py-0.5 rounded bg-neutral-900 border border-neutral-800/60">
                        {adv.badge}
                      </span>
                    </div>
                    <h3 className="text-sm font-semibold text-white tracking-tight group-hover:text-cyan-300 transition-colors">
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
              <span className="text-xs text-neutral-400 text-center sm:text-left">
                Activación y enlace inmediato mediante clave de licencia empresarial sin reinstalaciones.
              </span>
              <a
                href="#planes"
                className="px-6 py-2.5 rounded-lg text-xs font-semibold bg-cyan-400 text-neutral-950 hover:bg-cyan-300 transition-all shadow-md shadow-cyan-950/40"
              >
                Consultar Planes y Tarifas
              </a>
            </div>
          </div>
        </section>

        {/* SECCIÓN DEDICADA: STAGE DISPLAY (MONITOR DE CONFIANZA EN VIVO) */}
        <section id="stage" className="py-20 px-6 max-w-6xl mx-auto space-y-12">
          <div className="text-center max-w-3xl mx-auto space-y-3">
            <div className="inline-flex items-center gap-2 px-3 py-1 rounded-full border border-emerald-500/30 bg-emerald-950/40 text-emerald-300 text-xs font-semibold tracking-wider uppercase">
              <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
              Retorno Dedicado para Músicos y Púlpito
            </div>
            <h2 className="text-3xl sm:text-5xl font-extrabold text-white tracking-tight">
              Stage Display en Tiempo Real
            </h2>
            <p className="text-neutral-400 text-sm leading-relaxed max-w-2xl mx-auto">
              Proporciona a tu banda de alabanza, coro y predicadores una referencia visual desacoplada de la proyección principal. Incluye la estrofa actual, vista previa de la próxima sección, reloj sincronizado y avisos privados de cabina.
            </p>
          </div>

          {/* SIMULADOR INTERACTIVO DE STAGE DISPLAY */}
          <div className="rounded-2xl border border-neutral-800 bg-neutral-950 p-4 sm:p-8 space-y-6 shadow-2xl relative">
            <div className="flex flex-wrap items-center justify-between gap-3 border-b border-neutral-800/80 pb-4">
              <div className="flex items-center gap-2">
                <span className="w-3 h-3 rounded-full bg-red-500/80" />
                <span className="w-3 h-3 rounded-full bg-yellow-500/80" />
                <span className="w-3 h-3 rounded-full bg-emerald-500/80" />
                <span className="ml-2 text-xs font-mono text-neutral-400 uppercase tracking-widest">
                  Simulador de Monitor de Escenario
                </span>
              </div>

              {/* Selector de Layouts Interactivos */}
              <div className="flex items-center gap-1.5 p-1 rounded-lg bg-neutral-900 border border-neutral-800 text-xs font-medium">
                <button
                  onClick={() => setStageLayout("banda")}
                  className={`px-3 py-1 rounded transition-all ${
                    stageLayout === "banda"
                      ? "bg-emerald-500 text-neutral-950 font-semibold shadow-sm"
                      : "text-neutral-400 hover:text-white"
                  }`}
                >
                  Banda / Músicos
                </button>
                <button
                  onClick={() => setStageLayout("pulpito")}
                  className={`px-3 py-1 rounded transition-all ${
                    stageLayout === "pulpito"
                      ? "bg-emerald-500 text-neutral-950 font-semibold shadow-sm"
                      : "text-neutral-400 hover:text-white"
                  }`}
                >
                  Púlpito / Predicador
                </button>
                <button
                  onClick={() => setStageLayout("reloj")}
                  className={`px-3 py-1 rounded transition-all ${
                    stageLayout === "reloj"
                      ? "bg-emerald-500 text-neutral-950 font-semibold shadow-sm"
                      : "text-neutral-400 hover:text-white"
                  }`}
                >
                  Reloj Gigante
                </button>
              </div>
            </div>

            {/* PANTALLA DEL STAGE DISPLAY EN VIVO */}
            <div className="rounded-xl border-2 border-neutral-800 bg-black p-6 sm:p-10 font-mono text-white min-h-[340px] flex flex-col justify-between relative overflow-hidden shadow-inner">
              {/* Barra superior de telemetría de escenario */}
              <div className="flex flex-wrap items-center justify-between gap-4 border-b border-neutral-800 pb-3 text-xs sm:text-sm">
                <div className="flex items-center gap-4">
                  <span className="flex items-center gap-2 text-emerald-400 font-bold tracking-wider">
                    <span className="w-2.5 h-2.5 rounded-full bg-emerald-400 animate-ping" />
                    EN VIVO
                  </span>
                  <span className="text-neutral-400">
                    HORA: <strong className="text-white">{clockTime || "11:45:20 AM"}</strong>
                  </span>
                </div>

                <div className="flex items-center gap-4">
                  <span className="text-neutral-400">
                    CONTEO: <strong className="text-cyan-400">18:42</strong>
                  </span>
                  <span className="px-2 py-0.5 rounded bg-neutral-900 border border-neutral-700 text-amber-300 font-semibold text-xs">
                    {stageLayout === "banda" ? "TONO: G · 72 BPM" : "TEMA: MENSAJE"}
                  </span>
                </div>
              </div>

              {/* Alerta de cabina técnica opcional */}
              <div className="mt-3 py-1 px-3 rounded bg-amber-950/40 border border-amber-500/40 text-amber-300 text-xs flex items-center justify-between">
                <span>NOTIFICACIÓN DE CABINA TÉCNICA:</span>
                <span className="font-sans font-semibold">Repetir Coro 2 antes de la ministración</span>
              </div>

              {/* Contenido según Layout Seleccionado */}
              {stageLayout === "banda" && (
                <div className="my-6 grid grid-cols-1 lg:grid-cols-3 gap-6 items-center">
                  {/* Estrofa Actual (Grande) */}
                  <div className="lg:col-span-2 space-y-2">
                    <span className="text-xs uppercase font-bold tracking-widest text-emerald-400">
                      [ ESTROFA ACTUAL · CORO ]
                    </span>
                    <p className="text-2xl sm:text-4xl font-extrabold tracking-tight text-white leading-tight font-sans">
                      Cuán grande es Dios, cántale, <br />
                      cuán grande es Dios, <br />
                      y todos lo verán, cuán grande es Dios.
                    </p>
                  </div>

                  {/* Próxima Estrofa */}
                  <div className="rounded-lg border border-neutral-800 bg-neutral-900/60 p-4 space-y-2">
                    <span className="text-[11px] uppercase font-bold tracking-widest text-neutral-400">
                      PRÓXIMA SECCIÓN · PUENTE
                    </span>
                    <p className="text-sm text-neutral-300 font-sans leading-relaxed">
                      Y tu nombre sobre todo es, <br />
                      digno de alabar, <br />
                      mi corazón dirá: Cuán grande es Dios.
                    </p>
                  </div>
                </div>
              )}

              {stageLayout === "pulpito" && (
                <div className="my-8 text-center space-y-4 font-sans">
                  <span className="text-xs uppercase font-bold tracking-widest text-cyan-400">
                    REFERENCIA LITÚRGICA EN PANTALLA
                  </span>
                  <h3 className="text-2xl sm:text-4xl font-extrabold text-white max-w-2xl mx-auto">
                    Salmos 23:1 — "El Señor es mi pastor; nada me faltará."
                  </h3>
                  <div className="pt-2 flex justify-center items-center gap-6 text-sm text-neutral-400 font-mono">
                    <span>TIEMPO RESTANTE DE PREDICACIÓN: <strong className="text-amber-400 text-lg">15:00</strong></span>
                  </div>
                </div>
              )}

              {stageLayout === "reloj" && (
                <div className="my-8 text-center space-y-3 font-mono">
                  <span className="text-xs uppercase font-bold tracking-widest text-neutral-400">
                    RELOJ DE ESCENARIO Y CUENTA REGRESIVA
                  </span>
                  <div className="text-5xl sm:text-7xl font-extrabold text-cyan-400 tracking-wider">
                    {clockTime || "11:45:20 AM"}
                  </div>
                  <div className="text-sm text-neutral-400">
                    INICIO DEL SERVICIO EN: <strong className="text-white text-base">08:30 MIN</strong>
                  </div>
                </div>
              )}

              {/* Pie de pantalla de stage */}
              <div className="border-t border-neutral-900 pt-3 flex justify-between items-center text-[11px] text-neutral-500">
                <span>CANAL 01: MONITOR DE PISO PRINCIPAL</span>
                <span>RESOLUCIÓN: 1080p 60FPS · 0.4ms RED LOCAL</span>
              </div>
            </div>

            {/* Ventajas técnicas del Stage Display */}
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4 pt-4">
              <div className="p-4 rounded-xl border border-neutral-800 bg-neutral-900/40 space-y-1.5 hover:border-neutral-700 transition-colors">
                <span className="text-xs font-bold text-white block">Cero Latencia en Red</span>
                <p className="text-[11px] text-neutral-400 leading-relaxed">
                  Transmisión instantánea mediante WebSockets y red local sin demoras audibles ni visuales.
                </p>
              </div>
              <div className="p-4 rounded-xl border border-neutral-800 bg-neutral-900/40 space-y-1.5 hover:border-neutral-700 transition-colors">
                <span className="text-xs font-bold text-white block">Desacople Total</span>
                <p className="text-[11px] text-neutral-400 leading-relaxed">
                  El escenario ve su estructura privada sin comprometer la proyección del público.
                </p>
              </div>
              <div className="p-4 rounded-xl border border-neutral-800 bg-neutral-900/40 space-y-1.5 hover:border-neutral-700 transition-colors">
                <span className="text-xs font-bold text-white block">Mensajes Flash de Cabina</span>
                <p className="text-[11px] text-neutral-400 leading-relaxed">
                  Envía avisos privados directos a los músicos o al predicador en tiempo real.
                </p>
              </div>
              <div className="p-4 rounded-xl border border-neutral-800 bg-neutral-900/40 space-y-1.5 hover:border-neutral-700 transition-colors">
                <span className="text-xs font-bold text-white block">Multi-Dispositivo</span>
                <p className="text-[11px] text-neutral-400 leading-relaxed">
                  Compatible con TVs HDMI, tablets iPad/Android, navegadores web o monitores de confianza.
                </p>
              </div>
            </div>
          </div>
        </section>

        {/* CARACTERÍSTICAS TÉCNICAS */}
        <section id="caracteristicas" className="py-16 px-6 max-w-6xl mx-auto space-y-10">
          <div className="border-b border-neutral-800 pb-4">
            <h2 className="text-xs uppercase tracking-widest text-neutral-500 font-semibold">
              Arquitectura Base
            </h2>
            <p className="text-2xl font-extrabold text-white mt-1">
              Capacidades técnicas del motor local
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {caracteristicas.map((c) => (
              <div
                key={c.titulo}
                className="rounded-xl border border-neutral-800/80 bg-neutral-900/20 p-6 space-y-3 hover:border-neutral-700 hover:-translate-y-1 transition-all duration-300 group"
              >
                <div className="flex items-center justify-between">
                  <div className="p-2 rounded-lg bg-neutral-900 border border-neutral-800 group-hover:border-cyan-500/40 transition-colors">
                    {c.icon}
                  </div>
                  <span className="text-[10px] font-semibold uppercase tracking-wider text-neutral-500">
                    {c.badge}
                  </span>
                </div>
                <h3 className="text-sm font-semibold text-white group-hover:text-cyan-300 transition-colors">
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
            <h2 className="text-3xl sm:text-4xl font-extrabold text-white tracking-tight">
              Seleccione el modelo operativo de su organización
            </h2>
            <p className="text-neutral-400 text-xs sm:text-sm leading-relaxed">
              La edición de escritorio es permanente y sin costo. La suscripción profesional incorpora capacidades en la nube, retorno de escenario Stage Display y asistencia por inteligencia artificial.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8 items-stretch">
            {/* PLAN CORE (GRATUITO) */}
            <div className="rounded-2xl border border-neutral-800 bg-neutral-950 p-8 flex flex-col justify-between space-y-8 hover:border-neutral-700 transition-all duration-300">
              <div className="space-y-6">
                <div className="space-y-2">
                  <div className="text-xs font-semibold uppercase tracking-wider text-neutral-400">
                    Edición de Escritorio
                  </div>
                  <div className="text-3xl font-bold text-white">
                    ProyecThor Base
                  </div>
                  <div className="text-xs text-neutral-500">
                    Licencia de uso permanente para instalación local
                  </div>
                </div>

                <div className="border-t border-neutral-900 pt-6 space-y-3.5 text-xs text-neutral-300">
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
                className="w-full py-3.5 rounded-lg text-xs font-semibold text-center border border-neutral-700 bg-neutral-900 text-white hover:bg-neutral-800 transition-colors"
              >
                Descargar Instalador Base
              </a>
            </div>

            {/* PLAN PRO FOR PROFESSIONALS */}
            <div className="pro-border-glow rounded-2xl border bg-gradient-to-b from-neutral-900/60 to-neutral-950 p-8 flex flex-col justify-between space-y-8 relative shadow-2xl transition-all duration-300">
              <div className="absolute top-6 right-6">
                <span className="text-[10px] uppercase font-bold tracking-widest text-cyan-400 border border-cyan-500/40 bg-cyan-950/60 px-2.5 py-1 rounded-full">
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
                    <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                    <div>
                      <strong className="text-white block font-medium">Almacenamiento Cloud para 5,000+ canciones</strong>
                      <span className="text-neutral-400">Sincronización continua de catálogo y secuencias entre equipos.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-emerald-400 font-mono text-sm leading-none font-bold">+</span>
                    <div>
                      <strong className="text-white block font-medium">Stage Display y Monitores de Confianza en vivo</strong>
                      <span className="text-neutral-400">Retorno de escenario dedicado con próxima estrofa, reloj y avisos técnicos.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                    <div>
                      <strong className="text-white block font-medium">Herramientas de redacción asistidas por IA</strong>
                      <span className="text-neutral-400">Estructuración inteligente de estrofas y corrección métrica.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                    <div>
                      <strong className="text-white block font-medium">Copias de seguridad automáticas y redundancia</strong>
                      <span className="text-neutral-400">Protección integral de datos y configuraciones ante incidentes.</span>
                    </div>
                  </div>
                  <div className="flex items-start gap-3">
                    <span className="text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
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
                className="w-full py-3.5 rounded-lg text-xs font-semibold text-center bg-cyan-400 text-neutral-950 hover:bg-cyan-300 transition-all duration-200 shadow-md shadow-cyan-950/40 hover:scale-[1.01]"
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

          <div className="divide-y divide-neutral-800/80 border border-neutral-800 rounded-xl bg-neutral-950 overflow-hidden shadow-md">
            {plataformas.map((plat) => (
              <div
                key={plat.sistema}
                className="p-5 flex flex-col sm:flex-row sm:justify-between sm:items-center gap-2 hover:bg-neutral-900/30 transition-colors"
              >
                <div className="flex items-center gap-3">
                  <div className="w-2 h-2 rounded-full bg-cyan-500" />
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
