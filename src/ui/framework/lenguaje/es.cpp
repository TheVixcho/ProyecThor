#include "../UIStrings.h"

namespace ProyecThor::UI {

extern const UIStrings kSpanish = {
    // Generales
    "ProyecThor", "Cerrar", "Guardar", "Restablecer", "Cancelar",
    "Editar", "Eliminar", "Importar", "Nuevo", "Actualizar", "Sin señal",

    // Paneles
    "Biblioteca", "Preview", "Inspector de Capas y Fondos", "Control",

    // Menu superior
    "Archivo", "Salir", "Editar", "Configuraciones", "Vista",
    "Restablecer Entorno", "Ayuda", "Web", "Donaciones",
    "Acerca de ProyecThor",

    // About
    "Software profesional para gestión de proyecciones.",
    "Sin fines de lucro. Funcionamos mediante donaciones\ndel equipo de desarrollo y la comunidad.",

    // LibraryPanel
    "Canciones", "Videos", "Imágenes", "Biblia", "Documentos",
    "Buscar por nombre o letra", "Sin reproductor disponible.",

    // BibleView
    "Biblia:", "Libros", "Capítulos",
    "Buscar libro, cap. o vers. (Gn 1:1)", "Limpiar Historial",
    "Error: No se pudo cargar el archivo XML de la Biblia.",
    "Editar", "Guardar en XML",
    "  LIBROS", "  CAPÍTULOS",

    // SongView
    "Tabla de sonidos", "Eliminar letras", "Editar esta canción",

    // MediaView
    "Seleccione un elemento de la biblioteca.",
    "Proyectar Imagen",
    "Video listo: %s",
    "Usa los controles del monitor para proyectar.",

    // TransitionPanel
    "Efectos de Transición",
    "Sin transición",
    "Disolver",
    "Zoom In",
    "Zoom Out",
    "Duración de la transición",
    "Consejo: Las transiciones se aplican al cambiar de estrofa o proyectar nuevo contenido.",

    // DocumentView
    "El documento no tiene páginas generadas.",
    "Documento: %s",
    "Página %d de %d",
    "de",
    "<< Anterior",
    "Siguiente >>",
    "Proyectar Página Actual",

    // MonitorView
    "PREVIEW", "  PREVIEW ",
    "LIVE",   "  ON AIR ",
    "TRANSMITIR##trans",
    "[ Audio desactivado ]",
    "MUTE##lm", "Stp##p",

    // ControlPanel
    "Controles Rápidos",
    "QUITAR LETRA",
    "DETENER VIDEO",
    "Solo se detectó 1 pantalla. Conecta un segundo monitor para proyectar.",
    "Pantallas detectadas: %d",
    "Salida: [%d] %s  (%dx%d)",
    "EMPEZAR PROYECCIÓN",
    "APAGAR PROYECTOR",
    "Proyectando activamente",
    "Proyector inactivo",

    // LayersPanel
    "  Fondos  ", "  Estilos de Letra  ",
    "    Fondos y Videos", "    Recargar    ",
    "Arrastra videos a assets/backgrounds",
    "  + Nuevo Estilo  ", "  Recargar Fuentes  ",
    "Crea tu primer estilo con el botón de arriba",
    "  Ajustes Rápidos (sin guardar)",
    "Fuente", "Color del Texto", "Tamaño  %.0f px",
    "Alineación horizontal", "Alineación vertical",
    "Izq", "Centro", "Der",
    "Arriba", "Centro##v", "Abajo",
    "ACTIVO", "Guardar en XML",

    // OClock (reset reutiliza str.reset = "Restablecer" de Generales)
    "Contadores",
    "Configurar Cuenta Regresiva",
    "Minutos",
    "Segundos",
    "INICIAR",
    "PAUSAR/STOP",
    "Transmitir a Pantalla Principal",

    // QuickNotes
    "Notas Rápidas",
    "Escribe un mensaje para mostrar instantáneamente en pantalla.",
    "Mostrar en Pantalla (F5)",
    "Ocultar Mensaje (ESC)",
    "EN VIVO",

    // SettingsPanel
    "Selecciona el idioma de la interfaz de usuario.",
    "Idioma de la Interfaz",
    "El cambio de idioma se aplica al guardar y reiniciar la aplicación.\nAlgunas cadenas de texto pueden requerir reinicio completo.",
    "Vista Previa de Cadenas",
    "Preferencias",
    "Buscar (Libro Abreviado + 1:1)",
    "Limpiar Historial",

    // Hub
    "Empezar a proyectar",
    "Abrir configuración",
    "Solo el panel de Biblioteca, con Render incluido",
    "Novedades",
    "v%s disponible — tecla N",
    "Descargar subtítulos",
    "Bájalos como .txt desde una URL",
    "Accesos rápidos",
    "Proyecciones totales",
    "FPS promedio",
    "Canción más proyectada",
    "Sin datos aún",
    "Más proyectada (%d)",
    "HISTORIAL DE VERSIONES",
    "Versión v%s",

    // Hub: "Descargar subtitulos"
    "Pega el link de un video. Se buscan sus subtítulos (español primero, si no inglés) y se guardan como un .txt suelto — no crea una canción.",
    "Guardar en",
    "Preguntar cada vez",
    "Carpeta fija",
    "Sin elegir",
    "Elegir",
    "Elegir carpeta para subtítulos descargados",
    "Buscando subtítulos",
    "Descargar",
    "Guardado en: %s",
    "No se pudo escribir el archivo en esa ubicación.",

    // LibrarySongs (Canciones + Playlists)
    "Playlists",
    "Sin playlists todavía",
    "Renombrar",
    "< Volver",
    "%d canción",
    "%d canciones",
    "Esta playlist no tiene canciones todavía",
    "+ Agregar canciones",
    "+ Nueva playlist",
    "Nombre de la playlist",
    "Crear",
    "Nuevo nombre",
    "Agregar canciones",
    "Buscar por título o autor",
    "Agregada",
    "Agregar a la playlist",
    "Sin resultados",
    "Listo",
    "%d canción encontrada",
    "%d canciones encontradas",
    "Asignar etiqueta",
    "Quitar todas las etiquetas",

    // LibrarySidebar
    "Letra",
    "Medios",
    "Doc"
};

} // namespace ProyecThor::UI
