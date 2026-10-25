const caracteristicas = [
  {
    titulo: "Arquitectura multi-capa",
    descripcion:
      "Gestiona fondos (colores, imágenes, video) y texto de forma independiente, sin que los elementos se pisen entre sí.",
  },
  {
    titulo: "Flujo de doble pantalla",
    descripcion:
      "Interfaz de operador dedicada, separada de una salida limpia y sin bordes para el proyector o pantalla en vivo.",
  },
  {
    titulo: "Biblioteca todo-en-uno",
    descripcion:
      "Letras automatizadas, multimedia, Biblias XML gratuitas, subtítulos HTTPS, documentos y audio organizados en un solo lugar.",
  },
  {
    titulo: "Control remoto móvil",
    descripcion:
      "Opera y controla ProyecThor a distancia desde tu teléfono mediante su aplicación dedicada para Android.",
  },
  {
    titulo: "Streaming, red y efectos",
    descripcion:
      "Transmisión RTMP, enrutamiento LAN, control OSC y efectos visuales en vivo (CRT, grano, viñeta, desenfoque y nitidez).",
  },
  {
    titulo: "Conversión y personalización",
    descripcion:
      "Conversor y compresor de video/audio integrado, editor de overlays PNG, temas editables y atajos de teclado tipo Wayland.",
  },
];

const pilares = [
  {
    categoria: "Sin suscripciones",
    detalle: "Todas las funciones disponibles para siempre, sin cuentas ni módulos bloqueados.",
  },
  {
    categoria: "Centrado en el operador",
    detalle: "Interfaz predecible diseñada para minimizar el error humano en momentos críticos.",
  },
  {
    categoria: "Arquitectura Extensible",
    detalle: "Sistema de plugins modular para personalizar y ampliar funcionalidades a medida.",
  },
  {
    categoria: "Rendimiento nativo",
    detalle: "Bajo consumo de memoria, CPU y GPU con respuesta instantánea en vivo.",
  },
];

const plataformas = [
  {
    sistema: "Windows 10 / 11",
    detalle: "Totalmente soportado · Intel i5 / 8 GB RAM mín. (16 GB y GPU dedicada recomendados)",
  },
  {
    sistema: "Linux (Arch / Debian / Ubuntu)",
    detalle: "Totalmente soportado · Perfil optimizado de bajo consumo · Dual-core / 4 GB RAM mín.",
  },
  {
    sistema: "Android (App de Control Remoto)",
    detalle: "Disponible en Google Play (Beta) · Android 12 o superior · Red local Wi-Fi",
  },
];

const enlaces = [
  {
    nombre: "Descargar Gratis",
    url: "https://github.com/TheVixcho/ProyecThor/releases",
    primario: true,
  },
  {
    nombre: "Ver en GitHub",
    url: "https://github.com/TheVixcho/ProyecThor",
  },
  {
    nombre: "App Android",
    url: "https://play.google.com/apps/testing/the.proyecthor.mobile",
  },
  {
    nombre: "Wiki",
    url: "https://github.com/TheVixcho/ProyecThor/wiki",
  },
  {
    nombre: "Canal de WhatsApp",
    url: "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19",
  },
  {
    nombre: "Discord",
    url: "https://discord.gg/RMk8AGC5pn",
  },
];

export default function App() {
  return (
    <div className="min-h-screen flex flex-col bg-neutral-950 text-neutral-300 font-sans antialiased">
      {/* Toolbar Superior */}
      <header className="border-b border-neutral-800/80 bg-neutral-950">
        <div className="max-w-4xl mx-auto px-6 h-16 flex justify-between items-center">
          <span className="text-base text-neutral-100 tracking-wide">ProyecThor</span>
          <nav className="flex gap-6 text-sm text-neutral-400">
            <a href="#aplicacion" className="hover:text-neutral-100">Aplicación</a>
            <a href="#caracteristicas" className="hover:text-neutral-100">Características</a>
            <a href="#filosofia" className="hover:text-neutral-100">Filosofía</a>
            <a href="#plataformas" className="hover:text-neutral-100">Plataformas</a>
          </nav>
        </div>
      </header>

      {/* Contenido Principal */}
      <main className="max-w-4xl w-full mx-auto px-6 py-14 flex-1 space-y-16">
        {/* Presentación de la Aplicación */}
        <section id="aplicacion" className="space-y-4">
          <p className="text-sm uppercase tracking-widest text-neutral-400">
            Motor de Proyección Profesional
          </p>
          <h1 className="text-3xl text-neutral-100 tracking-tight">
            ProyecThor
          </h1>
          <p className="text-neutral-400 max-w-2xl leading-relaxed text-sm">
            Software de proyección gratuito y de código abierto diseñado para ofrecer velocidad, estabilidad y respuestas instantáneas bajo presión en producciones en vivo. Ideal para iglesias, teatros y eventos donde la operación debe ser segura y predecible.
          </p>
          <div className="flex flex-wrap gap-2.5 pt-2">
            {enlaces.map((item) => (
              <a
                key={item.nombre}
                href={item.url}
                target="_blank"
                rel="noopener noreferrer"
                className={
                  item.primario
                    ? "bg-neutral-200 text-neutral-950 hover:bg-white px-3.5 py-1.5 rounded text-xs"
                    : "border border-neutral-800 bg-neutral-900/60 text-neutral-300 hover:border-neutral-700 hover:text-neutral-100 px-3.5 py-1.5 rounded text-xs"
                }
              >
                {item.nombre}
              </a>
            ))}
          </div>
        </section>

        {/* Características Principales */}
        <section id="caracteristicas" className="space-y-5">
          <div className="border-b border-neutral-800/80 pb-3">
            <h2 className="text-sm uppercase tracking-widest text-neutral-400">
              Características Principales
            </h2>
          </div>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            {caracteristicas.map((c) => (
              <div
                key={c.titulo}
                className="bg-neutral-900/40 border border-neutral-800/80 rounded p-5 flex flex-col justify-between"
              >
                <div>
                  <h3 className="text-base text-neutral-100 mb-2">{c.titulo}</h3>
                  <p className="text-neutral-400 text-xs leading-relaxed">
                    {c.descripcion}
                  </p>
                </div>
              </div>
            ))}
          </div>
        </section>

        {/* Filosofía del Proyecto */}
        <section id="filosofia" className="space-y-5">
          <div className="border-b border-neutral-800/80 pb-3">
            <h2 className="text-sm uppercase tracking-widest text-neutral-400">
              Filosofía del Proyecto
            </h2>
          </div>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
            {pilares.map((p) => (
              <div
                key={p.categoria}
                className="bg-neutral-900/40 border border-neutral-800/80 rounded p-4"
              >
                <p className="text-xs uppercase tracking-wider text-neutral-400 mb-1">
                  {p.categoria}
                </p>
                <p className="text-sm text-neutral-200">{p.detalle}</p>
              </div>
            ))}
          </div>
        </section>

        {/* Plataformas y Requisitos */}
        <section id="plataformas" className="space-y-5">
          <div className="border-b border-neutral-800/80 pb-3">
            <h2 className="text-sm uppercase tracking-widest text-neutral-400">
              Plataformas Soportadas
            </h2>
          </div>
          <div className="divide-y divide-neutral-800/80 border border-neutral-800/80 rounded bg-neutral-900/40">
            {plataformas.map((plat) => (
              <div
                key={plat.sistema}
                className="px-5 py-3.5 flex flex-col sm:flex-row sm:justify-between sm:items-center gap-1"
              >
                <span className="text-sm text-neutral-200">{plat.sistema}</span>
                <span className="text-xs text-neutral-500">{plat.detalle}</span>
              </div>
            ))}
          </div>
        </section>
      </main>

      {/* Footer Básico */}
      <footer className="border-t border-neutral-800/80 py-6">
        <div className="max-w-4xl mx-auto px-6 flex justify-between items-center text-xs text-neutral-500">
          <span>© 2026 ProyecThor · Todos los derechos reservados</span>
          <span>
            Puedes apoyar el proyecto en{" "}
            <a
              href="https://ko-fi.com/vixcho"
              target="_blank"
              rel="noopener noreferrer"
              className="text-neutral-300 hover:text-neutral-100 underline underline-offset-4"
            >
              Ko-fi
            </a>
          </span>
        </div>
      </footer>
    </div>
  );
}
