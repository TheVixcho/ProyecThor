import React from "react";
import logo from "./assets/logo.png";
import { translations } from "./translations";

// Pasarela de pago activa: Flow Chile (Plan PROPROYECTHOR - $9.990 CLP).
const FLOW_SUBSCRIPTION_URL = "https://www.flow.cl/suscripcion/plan/PROPROYECTHOR";
// Descarga directa en 1 clic de la última versión oficial (redirige automáticamente al .exe más reciente sin abrir GitHub)
const DIRECT_DOWNLOAD_URL = "https://github.com/TheVixcho/ProyecThor/releases/latest/download/ProyecThor_Setup.exe";

// Íconos para la sección: En todos lados al mismo tiempo
const everywhereIcons = [
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 18h.01M8 21h8a2 2 0 002-2V5a2 2 0 00-2-2H8a2 2 0 00-2 2v14a2 2 0 002 2z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.905 14.141 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0" />
  </svg>,
];

// Íconos para la sección: Características técnicas base
const specsIcons = [
  <svg className="w-6 h-6 text-neutral-700 dark:text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
  </svg>,
  <svg className="w-6 h-6 text-neutral-700 dark:text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M12 6.253v13m0-13C10.832 5.477 9.246 5 7.5 5S4.168 5.477 3 6.253v13C4.168 18.477 5.754 18 7.5 18s3.332.477 4.5 1.253m0-13C13.168 5.477 14.754 5 16.5 5c1.747 0 3.332.477 4.5 1.253v13C19.832 18.477 18.247 18 16.5 18c-1.746 0-3.332.477-4.5 1.253" />
  </svg>,
  <svg className="w-6 h-6 text-neutral-700 dark:text-neutral-300" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
  </svg>,
];

// Íconos para la sección: Pilares gratuitos
const freeIcons = [
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M18.364 5.636a9 9 0 010 12.728m0 0l-2.829-2.829m2.829 2.829L21 21M15.536 8.464a5 5 0 010 7.072m0 0l-2.829-2.829m-4.243 2.829a4.978 4.978 0 01-1.414-2.83m-1.414 5.658a9 9 0 01-2.167-9.238m7.824-2.178a9 9 0 016.364 2.64m-4.95 2.122a5 5 0 011.414 2.828M3 3l18 18" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M13 10V3L4 14h7v7l9-11h-7z" />
  </svg>,
];

// Íconos para la sección: Ventajas Pro
const proIcons = [
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M3 15a4 4 0 004 4h9a5 5 0 10-.1-9.999 5.002 5.002 0 00-9.78 2.096A4.001 4.001 0 003 15z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M11 5H6a2 2 0 00-2 2v11a2 2 0 002 2h11a2 2 0 002-2v-5m-1.414-9.414a2 2 0 112.828 2.828L11.828 15H9v-2.828l8.586-8.586z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z" />
  </svg>,
  <svg className="w-6 h-6 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M18.364 5.636l-3.536 3.536m0 5.656l3.536 3.536M9.172 9.172L5.636 5.636m3.536 9.192l-3.536 3.536M21 12a9 9 0 11-18 0 9 9 0 0118 0zm-5 0a4 4 0 11-8 0 4 4 0 018 0z" />
  </svg>,
];

export default function App() {
  const [showFlowSuccess, setShowFlowSuccess] = React.useState(false);
  const [canvaAuth, setCanvaAuth] = React.useState(null);

  // Idioma: inglés por defecto, configurable a español mediante el switch
  const [lang, setLang] = React.useState(() => {
    if (typeof window !== "undefined") {
      const saved = localStorage.getItem("proyecthor_lang");
      if (saved === "es" || saved === "en") return saved;
    }
    return "en"; // Abre en inglés por defecto
  });

  React.useEffect(() => {
    localStorage.setItem("proyecthor_lang", lang);
    document.documentElement.lang = lang;
  }, [lang]);

  const t = translations[lang] || translations.en;

  React.useEffect(() => {
    const checkFlowReturn = () => {
      const hash = window.location.hash || "";
      const search = window.location.search || "";
      if (hash.includes("flow-return") || search.includes("flow-return") || search.includes("token=")) {
        setShowFlowSuccess(true);
      }
      if (search.includes("code=") || search.includes("error=")) {
        const params = new URLSearchParams(search);
        const code = params.get("code");
        const error = params.get("error");
        const errDesc = params.get("error_description");
        const state = params.get("state");

        if (code) {
          setCanvaAuth({ status: "code", code, state });
          try {
            fetch(`http://127.0.0.1:8765/callback?code=${encodeURIComponent(code)}&state=${encodeURIComponent(state || "")}`, { mode: 'no-cors' })
              .catch(() => {});
          } catch (e) {}
        } else if (error) {
          setCanvaAuth({ status: "error", error, errDesc });
        }
      }
    };
    checkFlowReturn();
    window.addEventListener("hashchange", checkFlowReturn);
    return () => window.removeEventListener("hashchange", checkFlowReturn);
  }, []);

  return (
    <div className="min-h-screen flex flex-col bg-slate-50 text-neutral-800 dark:bg-neutral-950 dark:text-neutral-300 font-['Arial',sans-serif] antialiased selection:bg-cyan-500/20 selection:text-cyan-900 dark:selection:bg-neutral-800 dark:selection:text-white relative overflow-x-clip transition-colors duration-300">
      {/* Iluminación de fondo ambiental animada */}
      <div className="fixed inset-0 pointer-events-none overflow-hidden z-0">
        <div className="ambient-glow-1 absolute top-0 left-1/3 -translate-x-1/2 w-[650px] h-[360px] bg-cyan-500/10 dark:bg-cyan-600/15 blur-[160px] rounded-full" />
        <div className="ambient-glow-2 absolute top-1/3 right-1/4 w-[550px] h-[340px] bg-blue-500/10 dark:bg-blue-600/10 blur-[150px] rounded-full" />
        <div className="absolute top-3/4 left-1/2 -translate-x-1/2 w-[700px] h-[300px] bg-cyan-400/5 dark:bg-cyan-900/10 blur-[170px] rounded-full" />
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
              <span className="text-base font-bold tracking-tight text-neutral-900 dark:text-white group-hover:text-cyan-600 dark:group-hover:text-cyan-400 transition-colors">
                ProyecThor
              </span>
              <span className="hidden sm:inline-block text-[11px] text-neutral-500 dark:text-neutral-400 font-medium tracking-wide">
                Engine 1.1.3
              </span>
            </div>
          </div>

          <nav className="hidden lg:flex items-center gap-1.5 text-xs font-medium text-neutral-600 dark:text-neutral-300">
            <a href="#inicio" className="px-3 py-1.5 rounded-lg hover:bg-black/5 dark:hover:bg-white/10 hover:text-neutral-950 dark:hover:text-white transition-all duration-200">
              {t.nav.home}
            </a>
            <a href="#entodoslados" className="px-3 py-1.5 rounded-lg hover:bg-black/5 dark:hover:bg-white/10 hover:text-neutral-950 dark:hover:text-white transition-all duration-200">
              {t.nav.everywhere}
            </a>
            <a href="#gratuito" className="px-3 py-1.5 rounded-lg hover:bg-black/5 dark:hover:bg-white/10 hover:text-cyan-600 dark:hover:text-cyan-400 transition-all duration-200">
              {t.nav.free}
            </a>
            <a href="#especificaciones" className="px-3 py-1.5 rounded-lg hover:bg-black/5 dark:hover:bg-white/10 hover:text-neutral-950 dark:hover:text-white transition-all duration-200">
              {t.nav.specs}
            </a>
            <a href="#compatibilidad" className="px-3 py-1.5 rounded-lg hover:bg-black/5 dark:hover:bg-white/10 hover:text-neutral-950 dark:hover:text-white transition-all duration-200">
              {t.nav.environments}
            </a>
            <a href="#profesionales" className="px-3 py-1.5 rounded-lg text-cyan-600 dark:text-cyan-400 hover:bg-cyan-50 dark:hover:bg-cyan-500/10 hover:text-cyan-700 dark:hover:text-cyan-300 transition-all duration-200 font-semibold">
              {t.nav.professionals}
            </a>
          </nav>

          <div className="flex items-center gap-3">
            {/* Switch Deslizante de Idioma (EN por defecto / ES) */}
            <div
              role="group"
              aria-label="Selector de idioma"
              className="relative inline-flex items-center bg-neutral-200/90 dark:bg-neutral-800 rounded-full p-1 text-xs font-bold select-none border border-neutral-300/70 dark:border-neutral-700"
            >
              <span
                className={`absolute top-1 bottom-1 w-[38px] rounded-full bg-white dark:bg-neutral-900 shadow-sm transition-transform duration-200 ease-in-out ${
                  lang === "es" ? "translate-x-[38px]" : "translate-x-0"
                }`}
              />
              <button
                type="button"
                onClick={() => setLang("en")}
                className={`relative z-10 w-[38px] py-1 text-center transition-colors cursor-pointer rounded-full ${
                  lang === "en" ? "text-neutral-950 dark:text-white" : "text-neutral-500 hover:text-neutral-800 dark:hover:text-neutral-200"
                }`}
                aria-label="Switch to English"
              >
                EN
              </button>
              <button
                type="button"
                onClick={() => setLang("es")}
                className={`relative z-10 w-[38px] py-1 text-center transition-colors cursor-pointer rounded-full ${
                  lang === "es" ? "text-neutral-950 dark:text-white" : "text-neutral-500 hover:text-neutral-800 dark:hover:text-neutral-200"
                }`}
                aria-label="Cambiar a Español"
              >
                ES
              </button>
            </div>

            <a
              href={DIRECT_DOWNLOAD_URL}
              className="px-4 sm:px-5 py-2 sm:py-2.5 text-xs font-semibold rounded-lg bg-neutral-900 text-white hover:bg-neutral-800 shadow-sm dark:bg-white dark:text-neutral-950 dark:hover:bg-neutral-100 dark:hover:shadow-lg dark:hover:shadow-white/10 transition-all duration-200"
            >
              {t.nav.download}
            </a>
          </div>
        </div>
      </header>

      {/* Banner de Retorno Exitoso de Pago Flow */}
      {showFlowSuccess && (
        <div className="fixed top-16 left-0 right-0 z-40 bg-gradient-to-r from-emerald-950/95 via-cyan-950/95 to-emerald-950/95 border-b border-cyan-500/40 px-6 py-3.5 shadow-2xl backdrop-blur-md">
          <div className="max-w-6xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-3 text-sm">
            <div className="flex items-center gap-3 text-cyan-200">
              <span className="w-8 h-8 rounded-full bg-emerald-500/20 text-emerald-400 flex items-center justify-center font-bold text-base shrink-0 border border-emerald-400/30">
                ✓
              </span>
              <div className="text-left">
                <p className="font-semibold text-white">¡Pago procesado con éxito en Flow / Webpay!</p>
                <p className="text-xs text-neutral-300">
                  Tu suscripción ha sido confirmada. Abre tu aplicación de escritorio <strong>ProyecThor</strong> para acceder automáticamente a todas las capacidades de <strong>ProyecThor for Professionals</strong>.
                </p>
              </div>
            </div>
            <button
              onClick={() => setShowFlowSuccess(false)}
              className="px-4 py-1.5 rounded-lg bg-white/10 hover:bg-white/20 text-xs text-white font-medium transition-colors cursor-pointer shrink-0"
            >
              Cerrar aviso
            </button>
          </div>
        </div>
      )}

      {/* Banner de Autorización Canva OAuth */}
      {canvaAuth && canvaAuth.status === "code" && (
        <div className="fixed top-16 left-0 right-0 z-40 bg-gradient-to-r from-cyan-950/95 via-blue-950/95 to-cyan-950/95 border-b border-cyan-400/40 px-6 py-4 shadow-2xl backdrop-blur-md">
          <div className="max-w-6xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4 text-sm">
            <div className="flex items-center gap-3 text-cyan-200">
              <span className="w-8 h-8 rounded-full bg-cyan-500/20 text-cyan-300 flex items-center justify-center font-bold text-base shrink-0 border border-cyan-400/30">
                🎨
              </span>
              <div className="text-left">
                <p className="font-semibold text-white">¡Autorización de Canva Completada!</p>
                <p className="text-xs text-neutral-300">
                  Si ProyecThor está abierto en este equipo, la vinculación se transferirá automáticamente. Si no, copia este código y pégalo en la ventana de Canva de ProyecThor:
                </p>
                <code className="text-xs bg-black/40 px-2 py-1 rounded text-cyan-300 font-mono select-all inline-block mt-1">
                  {canvaAuth.code}
                </code>
              </div>
            </div>
            <div className="flex items-center gap-2 shrink-0">
              <button
                onClick={() => {
                  navigator.clipboard.writeText(canvaAuth.code);
                  alert("Código copiado al portapapeles. Pégalo en ProyecThor.");
                }}
                className="px-4 py-2 rounded-lg bg-cyan-500 hover:bg-cyan-400 text-xs text-black font-semibold transition-colors cursor-pointer"
              >
                Copiar Código
              </button>
              <button
                onClick={() => setCanvaAuth(null)}
                className="px-3 py-2 rounded-lg bg-white/10 hover:bg-white/20 text-xs text-white font-medium transition-colors cursor-pointer"
              >
                Cerrar
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Banner de Error Canva OAuth */}
      {canvaAuth && canvaAuth.status === "error" && (
        <div className="fixed top-16 left-0 right-0 z-40 bg-gradient-to-r from-amber-950/95 via-rose-950/95 to-amber-950/95 border-b border-amber-400/40 px-6 py-4 shadow-2xl backdrop-blur-md">
          <div className="max-w-6xl mx-auto flex flex-col sm:flex-row items-center justify-between gap-4 text-sm">
            <div className="flex items-center gap-3 text-amber-200">
              <span className="w-8 h-8 rounded-full bg-amber-500/20 text-amber-300 flex items-center justify-center font-bold text-base shrink-0 border border-amber-400/30">
                ⚠️
              </span>
              <div className="text-left">
                <p className="font-semibold text-white">Aviso de Configuración de Canva Connect API</p>
                <p className="text-xs text-neutral-300">
                  {canvaAuth.errDesc || canvaAuth.error}
                </p>
                <p className="text-xs text-amber-300 mt-1">
                  💡 Solución: En <strong>Canva Developers</strong> &gt; Tu App &gt; <strong>Redirect URLs</strong>, debes registrar: <code className="bg-black/40 px-1.5 py-0.5 rounded text-white font-mono">http://127.0.0.1:8765/callback</code> o <code className="bg-black/40 px-1.5 py-0.5 rounded text-white font-mono">https://proyecthor.web.app/</code>
                </p>
              </div>
            </div>
            <button
              onClick={() => setCanvaAuth(null)}
              className="px-4 py-2 rounded-lg bg-white/10 hover:bg-white/20 text-xs text-white font-medium transition-colors cursor-pointer shrink-0"
            >
              Cerrar
            </button>
          </div>
        </div>
      )}

      {/* Contenido Principal con Espaciado Generoso y Profesional */}
      <main className="relative z-10 flex-1 pt-16">
        {/* HERO SECTION - Pantalla Completa */}
        <section
          id="inicio"
          className="min-h-[calc(100vh-4rem)] min-h-[calc(100dvh-4rem)] flex flex-col justify-center items-center px-6 py-12 max-w-5xl mx-auto text-center relative"
        >
          <div className="w-full flex flex-col items-center justify-center my-auto space-y-8 sm:space-y-10">
            <h1 className="max-w-4xl mx-auto text-center space-y-2 sm:space-y-3">
              <span className="block text-lg sm:text-2xl lg:text-3xl font-semibold tracking-tight text-neutral-600 dark:text-neutral-300">
                {t.hero.line1}
              </span>
              <span className="block text-4xl sm:text-6xl lg:text-7xl font-extrabold tracking-tight bg-gradient-to-r from-neutral-950 via-neutral-800 to-cyan-600 dark:from-white dark:via-neutral-100 dark:to-cyan-400 bg-clip-text text-transparent leading-[1.1]">
                {t.hero.line2}
              </span>
            </h1>

            <p className="text-base sm:text-xl text-neutral-600 dark:text-neutral-400 max-w-2xl mx-auto leading-relaxed font-normal">
              {t.hero.desc}
            </p>

            <div className="flex flex-wrap justify-center items-center gap-4 sm:gap-5 pt-2">
              <a
                href={DIRECT_DOWNLOAD_URL}
                className="px-8 py-4 rounded-xl text-sm font-semibold bg-neutral-900 text-white hover:bg-neutral-800 shadow-xl shadow-neutral-900/10 dark:bg-white dark:text-neutral-950 dark:hover:bg-neutral-200 dark:shadow-white/10 transition-all duration-200 hover:scale-[1.02] inline-flex items-center gap-2"
              >
                <svg className="w-5 h-5 text-white dark:text-neutral-950" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                </svg>
                <span>{t.hero.downloadBtn}</span>
              </a>
              <a
                href="#profesionales"
                className="px-8 py-4 rounded-xl text-sm font-semibold bg-white border border-neutral-300 text-neutral-800 hover:border-cyan-500 hover:text-cyan-700 shadow-sm dark:bg-neutral-900 dark:border-neutral-700 dark:text-neutral-200 dark:hover:border-cyan-400 dark:hover:text-white transition-all duration-200 hover:scale-[1.02] inline-flex items-center gap-2"
              >
                <span>{t.hero.proBtn}</span>
                <svg className="w-4 h-4 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
                </svg>
              </a>
            </div>
          </div>

          {/* Indicador sutil para explorar hacia abajo */}
          <div className="pt-6 pb-2">
            <a
              href="#entodoslados"
              aria-label="Ver más información"
              className="inline-flex flex-col items-center gap-1.5 text-xs text-neutral-400 hover:text-cyan-600 dark:text-neutral-500 dark:hover:text-cyan-400 transition-colors group"
            >
              <span className="tracking-wide">{t.hero.learnMore}</span>
              <svg className="w-4 h-4 animate-bounce group-hover:text-cyan-600 dark:group-hover:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            </a>
          </div>
        </section>

        {/* NUEVA SECCIÓN: EN TODOS LADOS AL MISMO TIEMPO / EVERYWHERE AT THE SAME TIME */}
        <section id="entodoslados" className="py-28 px-6 max-w-6xl mx-auto space-y-16">
          <div className="max-w-3xl mx-auto space-y-4 text-center">
            <h2 className="text-3xl sm:text-5xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
              {t.everywhere.title}
            </h2>
            <p className="text-neutral-600 dark:text-neutral-400 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto text-center pt-1">
              {t.everywhere.desc}
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-8 max-w-4xl mx-auto">
            {t.everywhere.pillars.map((pillar, idx) => (
              <div
                key={pillar.title}
                className="rounded-2xl border border-neutral-200 bg-white p-7 space-y-4 shadow-sm hover:shadow-md hover:border-cyan-400 hover:-translate-y-1 transition-all duration-300 dark:border-neutral-800 dark:bg-neutral-900/30 dark:hover:border-neutral-700"
              >
                <div className="w-11 h-11 rounded-xl bg-cyan-50 border border-cyan-100 flex items-center justify-center dark:bg-neutral-900 dark:border-neutral-800">
                  {everywhereIcons[idx]}
                </div>
                <h3 className="text-base font-bold text-neutral-900 dark:text-white tracking-tight">
                  {pillar.title}
                </h3>
                <p className="text-xs sm:text-sm text-neutral-600 dark:text-neutral-400 leading-relaxed text-justify">
                  {pillar.desc}
                </p>
              </div>
            ))}
          </div>
        </section>

        {/* SECCIÓN: VERSIÓN GRATUITA */}
        <section id="gratuito" className="py-28 px-6 max-w-6xl mx-auto space-y-20">
          <div className="max-w-3xl mx-auto space-y-6 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
              {t.freeSection.title}
            </h2>
            <p className="text-neutral-600 dark:text-neutral-400 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto text-justify pt-1">
              {t.freeSection.desc}
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            {t.freeSection.pillars.map((p, idx) => (
              <div
                key={p.title}
                className="rounded-2xl border border-neutral-200 bg-white p-8 sm:p-9 flex flex-col justify-between space-y-6 shadow-sm hover:shadow-md hover:border-cyan-400 hover:-translate-y-1 transition-all duration-300 dark:border-neutral-800 dark:bg-neutral-900/30 dark:hover:border-neutral-700"
              >
                <div className="space-y-6">
                  <div className="w-12 h-12 rounded-xl bg-cyan-50 border border-cyan-100 flex items-center justify-center dark:bg-neutral-900 dark:border-neutral-800">
                    {freeIcons[idx]}
                  </div>
                  <h3 className="text-lg font-bold text-neutral-900 dark:text-white tracking-tight">
                    {p.title}
                  </h3>
                  <p className="text-sm text-neutral-600 dark:text-neutral-400 leading-relaxed text-justify">
                    {p.desc}
                  </p>
                </div>
              </div>
            ))}
          </div>

          {/* Resumen de capacidades incluidas gratis */}
          <div className="rounded-3xl border border-neutral-200 bg-white p-8 sm:p-12 space-y-8 shadow-sm dark:border-neutral-800 dark:bg-neutral-950 dark:shadow-xl">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-6 border-b border-neutral-200 dark:border-neutral-800/80 pb-6">
              <div className="space-y-1">
                <span className="text-sm font-bold uppercase tracking-wider text-neutral-900 dark:text-white">
                  {t.freeSection.includedBadge}
                </span>
                <p className="text-xs sm:text-sm text-neutral-500 dark:text-neutral-400">
                  {t.freeSection.includedSubtitle}
                </p>
              </div>
              <a
                href={DIRECT_DOWNLOAD_URL}
                className="inline-flex items-center gap-2 px-5 py-2.5 rounded-xl bg-neutral-900 text-white hover:bg-neutral-800 text-xs font-semibold dark:bg-neutral-900 dark:border dark:border-neutral-700 dark:text-white dark:hover:border-cyan-400 transition-colors w-fit shrink-0 shadow-sm"
              >
                {t.freeSection.downloadBtn}
                <svg className="w-4 h-4 text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M14 5l7 7m0 0l-7 7m7-7H3" />
                </svg>
              </a>
            </div>

            <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-y-5 gap-x-8 text-xs sm:text-sm text-neutral-700 dark:text-neutral-300">
              {t.freeSection.features.map((feature, idx) => (
                <div key={idx} className="flex items-center gap-3">
                  <span className="w-2 h-2 rounded-full bg-cyan-500 dark:bg-cyan-400 shrink-0" />
                  <span>{feature}</span>
                </div>
              ))}
            </div>
          </div>
        </section>

        {/* CARACTERÍSTICAS TÉCNICAS Y ESPECIFICACIONES BASE */}
        <section id="especificaciones" className="py-28 px-6 max-w-6xl mx-auto space-y-16">
          <div className="border-b border-neutral-200 dark:border-neutral-800 pb-8 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
              {t.specsSection.title}
            </h2>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
            {t.specsSection.items.map((c, idx) => (
              <div
                key={c.title}
                className="rounded-2xl border border-neutral-200 bg-white p-8 space-y-5 shadow-sm hover:shadow-md hover:border-cyan-400 hover:-translate-y-1 transition-all duration-300 group dark:border-neutral-800/80 dark:bg-neutral-900/20 dark:hover:border-neutral-700"
              >
                <div className="w-11 h-11 rounded-xl bg-neutral-100 border border-neutral-200 flex items-center justify-center group-hover:border-cyan-500/40 transition-colors dark:bg-neutral-900 dark:border-neutral-800">
                  {specsIcons[idx]}
                </div>
                <h3 className="text-base font-bold text-neutral-900 group-hover:text-cyan-600 dark:text-white dark:group-hover:text-cyan-300 transition-colors">
                  {c.title}
                </h3>
                <p className="text-xs sm:text-sm text-neutral-600 dark:text-neutral-400 leading-relaxed text-justify">
                  {c.desc}
                </p>
              </div>
            ))}
          </div>
        </section>

        {/* PLATAFORMAS COMPATIBLES */}
        <section id="compatibilidad" className="py-28 px-6 max-w-5xl mx-auto space-y-16">
          <div className="border-b border-neutral-200 dark:border-neutral-800 pb-8 text-center">
            <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
              {t.platformsSection.title}
            </h2>
          </div>

          {/* Tabla de Especificaciones para Escritorio / Pantallas Medianas */}
          <div className="hidden md:block rounded-3xl border border-neutral-200 bg-white shadow-md overflow-hidden backdrop-blur-sm dark:border-neutral-800 dark:bg-neutral-950/80 dark:shadow-2xl">
            <table className="w-full text-left text-xs sm:text-sm text-neutral-700 dark:text-neutral-300 border-collapse">
              <thead>
                <tr className="bg-neutral-100 border-b border-neutral-200 text-xs uppercase tracking-wider text-neutral-600 dark:bg-neutral-900/70 dark:border-neutral-800 dark:text-neutral-400 font-semibold">
                  <th scope="col" className="py-5 px-8">{t.platformsSection.th.platform}</th>
                  <th scope="col" className="py-5 px-8">{t.platformsSection.th.distros}</th>
                  <th scope="col" className="py-5 px-8">{t.platformsSection.th.packaging}</th>
                  <th scope="col" className="py-5 px-8">{t.platformsSection.th.requirements}</th>
                  <th scope="col" className="py-5 px-8">{t.platformsSection.th.gpu}</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-neutral-200 dark:divide-neutral-800/70">
                <tr className="hover:bg-neutral-50 dark:hover:bg-neutral-900/40 transition-colors">
                  <td className="py-6 px-8">
                    <div className="flex items-center gap-4">
                      <div className="w-10 h-10 rounded-xl bg-cyan-100/70 border border-cyan-200 dark:bg-cyan-950/50 dark:border-cyan-500/30 flex items-center justify-center shrink-0">
                        <svg className="w-5 h-5 text-cyan-600 dark:text-cyan-400" viewBox="0 0 24 24" fill="currentColor">
                          <path d="M0 3.449L9.75 2.1v9.451H0m10.949-9.602L24 0v11.4H10.949M0 12.6h9.75v9.451L0 20.699M10.949 12.6H24V24l-12.949-1.8" />
                        </svg>
                      </div>
                      <div>
                        <span className="block font-bold text-neutral-900 dark:text-white text-sm">
                          {t.platformsSection.windows.name}
                        </span>
                        <span className="text-xs text-neutral-500 dark:text-neutral-400">
                          {t.platformsSection.windows.arch}
                        </span>
                      </div>
                    </div>
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200">
                    {t.platformsSection.windows.distros}
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200">
                    {t.platformsSection.windows.packaging}
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200 leading-relaxed">
                    {t.platformsSection.windows.req} <br />
                    <span className="text-xs text-neutral-500 dark:text-neutral-400">{t.platformsSection.windows.gpuReq}</span>
                  </td>
                  <td className="py-6 px-8 text-cyan-600 dark:text-cyan-400 font-mono text-xs">
                    {t.platformsSection.windows.gpu}
                  </td>
                </tr>
                <tr className="hover:bg-neutral-50 dark:hover:bg-neutral-900/40 transition-colors">
                  <td className="py-6 px-8">
                    <div className="flex items-center gap-4">
                      <div className="w-10 h-10 rounded-xl bg-amber-100/70 border border-amber-200 dark:bg-amber-950/50 dark:border-amber-500/30 flex items-center justify-center shrink-0">
                        <svg className="w-5 h-5 text-amber-600 dark:text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.8} d="M8 9l3 3-3 3m5 0h3M5 20h14a2 2 0 002-2V6a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" />
                        </svg>
                      </div>
                      <div>
                        <span className="block font-bold text-neutral-900 dark:text-white text-sm">
                          {t.platformsSection.linux.name}
                        </span>
                        <span className="text-xs text-neutral-500 dark:text-neutral-400">
                          {t.platformsSection.linux.arch}
                        </span>
                      </div>
                    </div>
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200">
                    {t.platformsSection.linux.distros}
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200">
                    {t.platformsSection.linux.packaging}
                  </td>
                  <td className="py-6 px-8 text-neutral-800 dark:text-neutral-200 leading-relaxed">
                    {t.platformsSection.linux.req} <br />
                    <span className="text-xs text-neutral-500 dark:text-neutral-400">{t.platformsSection.linux.gpuReq}</span>
                  </td>
                  <td className="py-6 px-8 text-amber-600 dark:text-amber-400 font-mono text-xs">
                    {t.platformsSection.linux.gpu}
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          {/* Tarjetas de Especificaciones para Móvil */}
          <div className="grid grid-cols-1 gap-6 md:hidden">
            {/* Windows Card */}
            <div className="rounded-2xl border border-neutral-200 bg-white p-6 space-y-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-950/80 dark:shadow-xl">
              <div className="flex items-center gap-3 border-b border-neutral-200 dark:border-neutral-800 pb-4">
                <div className="w-10 h-10 rounded-xl bg-cyan-100/70 border border-cyan-200 dark:bg-cyan-950/50 dark:border-cyan-500/30 flex items-center justify-center text-cyan-600 dark:text-cyan-400 shrink-0">
                  <svg className="w-5 h-5 text-cyan-600 dark:text-cyan-400" viewBox="0 0 24 24" fill="currentColor">
                    <path d="M0 3.449L9.75 2.1v9.451H0m10.949-9.602L24 0v11.4H10.949M0 12.6h9.75v9.451L0 20.699M10.949 12.6H24V24l-12.949-1.8" />
                  </svg>
                </div>
                <div>
                  <h3 className="text-base font-bold text-neutral-900 dark:text-white">
                    {t.platformsSection.windows.name}
                  </h3>
                  <span className="text-xs text-neutral-500 dark:text-neutral-400">
                    {t.platformsSection.windows.distros} ({t.platformsSection.windows.arch})
                  </span>
                </div>
              </div>
              <div className="space-y-3 text-xs">
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.installer}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.windows.packaging}</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.hardware}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.windows.req}</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.video}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.windows.gpuReq}</span>
                </div>
                <div className="flex justify-between py-1.5">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.graphics}</span>
                  <span className="text-cyan-600 dark:text-cyan-400 font-mono text-right">{t.platformsSection.windows.gpu}</span>
                </div>
              </div>
            </div>

            {/* Linux Card */}
            <div className="rounded-2xl border border-neutral-200 bg-white p-6 space-y-5 shadow-sm dark:border-neutral-800 dark:bg-neutral-950/80 dark:shadow-xl">
              <div className="flex items-center gap-3 border-b border-neutral-200 dark:border-neutral-800 pb-4">
                <div className="w-10 h-10 rounded-xl bg-amber-100/70 border border-amber-200 dark:bg-amber-950/50 dark:border-amber-500/30 flex items-center justify-center text-amber-600 dark:text-amber-400 shrink-0">
                  <svg className="w-5 h-5 text-amber-600 dark:text-amber-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.8} d="M8 9l3 3-3 3m5 0h3M5 20h14a2 2 0 002-2V6a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" />
                  </svg>
                </div>
                <div>
                  <h3 className="text-base font-bold text-neutral-900 dark:text-white">
                    {t.platformsSection.linux.name}
                  </h3>
                  <span className="text-xs text-neutral-500 dark:text-neutral-400">
                    {t.platformsSection.linux.distros}
                  </span>
                </div>
              </div>
              <div className="space-y-3 text-xs">
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.installer}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.linux.packaging}</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.efficiency}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.linux.req}</span>
                </div>
                <div className="flex justify-between py-1.5 border-b border-neutral-100 dark:border-neutral-900">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.compatibility}</span>
                  <span className="text-neutral-900 dark:text-white font-medium text-right">{t.platformsSection.linux.compat}</span>
                </div>
                <div className="flex justify-between py-1.5">
                  <span className="text-neutral-500">{t.platformsSection.mobileLabels.graphics}</span>
                  <span className="text-amber-600 dark:text-amber-400 font-mono text-right">{t.platformsSection.linux.gpu}</span>
                </div>
              </div>
            </div>
          </div>
        </section>

        {/* SECCIÓN FINAL DE LA LANDING: OPCIÓN PRO COMO ESCALÓN SUPERIOR */}
        <section id="profesionales" className="py-28 px-6 max-w-6xl mx-auto space-y-24">
          {/* Mensaje de transición con espaciado generoso */}
          <div className="rounded-2xl border border-neutral-200 bg-white p-6 sm:p-8 flex flex-col sm:flex-row items-center justify-between gap-6 shadow-sm dark:border-neutral-800 dark:bg-neutral-900/40 dark:shadow-md">
            <div className="space-y-2 text-center sm:text-left">
              <span className="text-xs font-bold uppercase tracking-wider text-cyan-600 dark:text-cyan-400 block">
                {t.proSection.bannerTag}
              </span>
              <p className="text-xs sm:text-sm text-neutral-600 dark:text-neutral-400 leading-relaxed max-w-2xl">
                {t.proSection.bannerDesc}
              </p>
            </div>
            <a
              href="#planes"
              className="px-6 py-3 rounded-xl text-xs font-semibold bg-neutral-900 text-white hover:bg-neutral-800 dark:bg-neutral-800 dark:hover:bg-neutral-700 transition-colors whitespace-nowrap shrink-0 shadow-sm"
            >
              {t.proSection.bannerBtn}
            </a>
          </div>

          <div className="rounded-3xl border border-neutral-200 bg-gradient-to-b from-white to-neutral-50 p-10 sm:p-16 space-y-16 shadow-md relative overflow-hidden backdrop-blur-sm dark:border-neutral-800 dark:bg-gradient-to-b dark:from-neutral-900/60 dark:to-neutral-950/80 dark:shadow-2xl">
            <div className="absolute top-0 right-0 w-80 h-80 bg-cyan-500/10 dark:bg-cyan-500/5 blur-[120px] rounded-full pointer-events-none" />

            <div className="max-w-3xl mx-auto space-y-4 text-center">
              <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
                {t.proSection.title}
              </h2>
              <p className="text-neutral-600 dark:text-neutral-400 text-sm sm:text-base leading-relaxed max-w-2xl mx-auto text-justify pt-1">
                {t.proSection.desc}
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
              {t.proSection.advantages.map((adv, idx) => (
                <div
                  key={adv.title}
                  className="rounded-2xl border border-neutral-200 bg-white p-8 flex flex-col justify-between shadow-sm hover:shadow-md hover:border-cyan-400 hover:-translate-y-1 transition-all duration-300 group space-y-5 dark:border-neutral-800/80 dark:bg-neutral-950/70 dark:hover:border-neutral-700 dark:hover:shadow-xl dark:hover:shadow-black/50"
                >
                  <div className="space-y-5">
                    <div className="w-12 h-12 rounded-xl bg-cyan-50 border border-cyan-100 flex items-center justify-center group-hover:border-cyan-400 transition-colors dark:bg-neutral-900 dark:border-neutral-800">
                      {proIcons[idx]}
                    </div>
                    <h3 className="text-base font-bold text-neutral-900 group-hover:text-cyan-600 dark:text-white dark:group-hover:text-cyan-300 tracking-tight transition-colors">
                      {adv.title}
                    </h3>
                    <p className="text-xs sm:text-sm text-neutral-600 dark:text-neutral-400 leading-relaxed text-justify">
                      {adv.desc}
                    </p>
                  </div>
                </div>
              ))}
            </div>

            <div className="pt-10 mt-6 border-t border-neutral-200 dark:border-neutral-800/80 flex flex-col sm:flex-row items-center justify-between gap-6">
              <span className="text-xs sm:text-sm text-neutral-500 dark:text-neutral-400 text-center sm:text-left">
                {t.proSection.notice}
              </span>
              <a
                href="#planes"
                className="px-7 py-3.5 rounded-xl text-xs font-semibold bg-cyan-500 text-neutral-950 hover:bg-cyan-400 transition-all shadow-md shadow-cyan-500/20 shrink-0"
              >
                {t.proSection.plansBtn}
              </a>
            </div>
          </div>

          {/* COMPARACIÓN DE EDICIONES Y SUSCRIPCIÓN */}
          <div id="planes" className="pt-16 space-y-16">
            <div className="text-center max-w-2xl mx-auto space-y-6">
              <h2 className="text-2xl sm:text-4xl font-extrabold tracking-tight text-neutral-900 dark:text-white leading-tight text-center">
                {t.proSection.comparisonTitle}
              </h2>
              <p className="text-neutral-600 dark:text-neutral-400 text-sm sm:text-base leading-relaxed pt-1">
                {t.proSection.comparisonSubtitle}
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-10 items-stretch max-w-5xl mx-auto">
              {/* PLAN CORE (GRATUITO) */}
              <div className="rounded-3xl border border-neutral-200 bg-white p-10 sm:p-12 flex flex-col justify-between space-y-10 hover:border-neutral-300 transition-all duration-300 shadow-sm hover:shadow-md dark:border-neutral-800 dark:bg-neutral-950 dark:hover:border-neutral-700 dark:shadow-xl">
                <div className="space-y-8">
                  <div className="space-y-3">
                    <div className="text-xs font-semibold uppercase tracking-wider text-neutral-500 dark:text-neutral-400">
                      {t.proSection.basePlan.tag}
                    </div>
                    <div className="text-3xl font-bold text-neutral-900 dark:text-white">
                      {t.proSection.basePlan.title}
                    </div>
                    <div className="text-xs sm:text-sm text-neutral-500">
                      {t.proSection.basePlan.subtitle}
                    </div>
                  </div>

                  <div className="border-t border-neutral-200 dark:border-neutral-900 pt-8 space-y-4 text-xs sm:text-sm text-neutral-700 dark:text-neutral-300">
                    {t.proSection.basePlan.features.map((feat, idx) => (
                      <div key={idx} className="flex items-start gap-3.5">
                        <span className="text-cyan-600 dark:text-cyan-400 font-mono text-sm leading-none font-bold">✓</span>
                        <span>{feat}</span>
                      </div>
                    ))}
                  </div>
                </div>

                <a
                  href={DIRECT_DOWNLOAD_URL}
                  className="w-full py-4 rounded-xl text-xs font-semibold text-center bg-neutral-900 text-white hover:bg-neutral-800 dark:border dark:border-neutral-700 dark:bg-neutral-900 dark:text-white dark:hover:bg-neutral-800 transition-colors shadow-sm"
                >
                  {t.proSection.basePlan.btn}
                </a>
              </div>

              {/* PLAN PRO FOR PROFESSIONALS */}
              <div className="pro-border-glow rounded-3xl border border-cyan-400/40 bg-gradient-to-b from-cyan-50/50 via-white to-cyan-50/20 p-10 sm:p-12 flex flex-col justify-between space-y-10 relative shadow-md transition-all duration-300 dark:from-neutral-900/60 dark:to-neutral-950 dark:shadow-2xl">
                <div className="space-y-8">
                  <div className="space-y-3">
                    <div className="text-xs font-semibold uppercase tracking-wider text-cyan-600 dark:text-cyan-400">
                      {t.proSection.proPlan.tag}
                    </div>
                    <div className="text-3xl font-bold text-neutral-900 dark:text-white">
                      {t.proSection.proPlan.title}
                    </div>
                    <div className="text-xs sm:text-sm text-neutral-600 dark:text-neutral-400">
                      {t.proSection.proPlan.subtitle}
                    </div>
                    <div className="pt-2 flex items-baseline gap-2">
                      <span className="text-3xl sm:text-4xl font-extrabold text-neutral-900 dark:text-white tracking-tight">
                        {t.proSection.proPlan.price}
                      </span>
                      <span className="text-xs uppercase tracking-wider font-semibold text-cyan-600 dark:text-cyan-400">
                        {t.proSection.proPlan.period}
                      </span>
                    </div>
                  </div>

                  <div className="border-t border-neutral-200 dark:border-neutral-800 pt-8 space-y-4 text-xs sm:text-sm text-neutral-700 dark:text-neutral-200">
                    {t.proSection.proPlan.features.map((feat, idx) => (
                      <div key={idx} className="flex items-start gap-3.5">
                        <span className="text-cyan-600 dark:text-cyan-400 font-mono text-sm leading-none font-bold">+</span>
                        <div>
                          <strong className="text-neutral-900 dark:text-white block font-medium">{feat.strong}</strong>
                          <span className="text-neutral-500 dark:text-neutral-400 text-xs">{feat.desc}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                <div className="space-y-4 pt-4">
                  <div className="rounded-xl bg-cyan-50 border border-cyan-200 dark:bg-gradient-to-b dark:from-cyan-950/50 dark:to-neutral-900/80 dark:border-cyan-500/40 p-4 text-center">
                    <div className="inline-flex items-center justify-center gap-2 text-cyan-700 dark:text-cyan-400 font-bold text-sm mb-1.5">
                      <svg className="w-5 h-5 text-cyan-600 dark:text-cyan-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12l2 2 4-4m5.618-4.016A11.955 11.955 0 0112 2.944a11.955 11.955 0 01-8.618 3.04A12.02 12.02 0 003 9c0 5.591 3.824 10.29 9 11.622 5.176-1.332 9-6.03 9-11.622 0-1.042-.133-2.052-.382-3.016z" />
                      </svg>
                      <span>{t.proSection.proPlan.appHireTitle}</span>
                    </div>
                    <p className="text-xs text-neutral-600 dark:text-neutral-300 leading-relaxed mb-3">
                      {t.proSection.proPlan.appHireDesc}
                    </p>
                    <div className="inline-block bg-white dark:bg-neutral-950/80 border border-cyan-300 dark:border-cyan-500/30 rounded-lg px-3.5 py-1.5 text-xs font-mono text-cyan-800 dark:text-cyan-300">
                      {t.proSection.proPlan.appHireRoute}
                    </div>
                  </div>

                  <a
                    href={DIRECT_DOWNLOAD_URL}
                    className="w-full py-3.5 rounded-xl text-xs font-semibold text-center bg-cyan-500 text-neutral-950 hover:bg-cyan-400 transition-all duration-200 shadow-md shadow-cyan-500/20 hover:scale-[1.01] block cursor-pointer"
                  >
                    {t.proSection.proPlan.downloadToHire}
                  </a>

                  <div className="rounded-lg bg-neutral-100 border border-neutral-200 dark:bg-neutral-900/60 dark:border-neutral-800/80 p-3 text-[11px] text-neutral-600 dark:text-neutral-400 space-y-1">
                    <p className="text-cyan-700 dark:text-cyan-400 font-medium">{t.proSection.proPlan.termsTitle}</p>
                    {t.proSection.proPlan.terms.map((term, idx) => (
                      <p key={idx}>• {term}</p>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>
        </section>
      </main>

      {/* FOOTER CORPORATIVO */}
      <footer className="border-t border-neutral-200 bg-neutral-100 dark:border-neutral-800/80 dark:bg-neutral-950 py-12 relative z-10 text-xs text-neutral-500 transition-colors">
        <div className="max-w-6xl mx-auto px-6 flex flex-col sm:flex-row justify-between items-center gap-6">
          <div className="flex items-center gap-3">
            <img src={logo} alt="ProyecThor Logo" className="w-5 h-5 grayscale opacity-60" />
            <span>{t.footer.rights}</span>
          </div>

          <div className="flex items-center gap-6 text-neutral-600 dark:text-neutral-400">
            <a
              href="https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19"
              target="_blank"
              rel="noopener noreferrer"
              className="hover:text-neutral-950 dark:hover:text-white transition-colors"
            >
              {t.footer.channel}
            </a>
            <a
              href="https://ko-fi.com/vixcho"
              target="_blank"
              rel="noopener noreferrer"
              className="hover:text-neutral-950 dark:hover:text-white transition-colors"
            >
              {t.footer.kofi}
            </a>
            <a
              href={DIRECT_DOWNLOAD_URL}
              className="hover:text-neutral-950 dark:hover:text-white transition-colors"
            >
              {t.footer.downloads}
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}
