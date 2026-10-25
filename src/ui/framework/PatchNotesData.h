#pragma once
#include <vector>
#include <string>

namespace ProyecThor::UI {

struct UpdateVersionInfo {
    int         id;
    const char* version;
    const char* modalBadge;
    const char* cardBadge;
    const char* coverFile;                       
    const char* summary;
    bool        isBeta = false;
};

inline const std::vector<UpdateVersionInfo>& GetPatchNotesRegistry() {
    static const std::vector<UpdateVersionInfo> s_Registry = {
        {
            23, "1.1.0",
            "GRAN ACTUALIZACIÓN v1.1.0", "GRAN ACTUALIZACIÓN",
            "splash_bg7.jpg",
            "### SplashScreen Renovado, Precarga Real & Arranque Instantáneo\n"
            "• [NUEVO] Nueva Portada y Arte de Inicio (splash_bg7.jpg): Incorporación de nueva fotografía artística en la rotación aleatoria del SplashScreen y nuevo banner panorámico de portada en el visor de Notas de Versión para celebrar la Gran Actualización v1.1.0.\n"
            "• [RENDIMIENTO] Precarga Real Recurso por Recurso en SplashScreen: El inicio ahora carga de verdad en memoria GPU las miniaturas de Multimedia (imágenes y fotogramas de video), Fondos y Overlays mostrando en vivo el porcentaje exacto y el nombre de cada archivo procesado.\n"
            "• [RENDIMIENTO] Caché Unificada de Miniaturas GPU y Shaders FSR: Los paneles de Fondos, Capas, Biblioteca Multimedia y Overlays comparten las texturas precargadas desde el arranque, junto con el cacheo anticipado de cabeceras GLSL de AMD FidelityFX Super Resolution (FSR).\n"
            "• [RENDIMIENTO] Precalentamiento Temprano del Motor de Video VLC: Inicialización anticipada de instancias nativas de libVLC y decodificadores de reproducción en segundo plano durante la pantalla de carga para eliminar bloqueos en frío.\n"
            "• [CORRECCIÓN] Eliminación de Pantalla en Blanco tras el SplashScreen: La ventana principal ahora pre-renderiza un fotograma completo de la interfaz y sus texturas antes de cerrar el SplashScreen, logrando una transición inmediata sin parpadeos ni segundos en blanco.\n"
            "\n"
            "### Motor de Fondos y Video de Respuesta Inmediata\n"
            "• [CORRECCIÓN] Primer Fondo al Instante con un Solo Clic: Solucionada la demora inicial donde el primer fondo de video o imagen tardaba demasiado en aparecer o requería múltiples clics al iniciar el programa.\n"
            "• [RENDIMIENTO] Sincronización Directa de Capas y Reproductores: Optimización del ciclo de activación en BackgroundLayer, VLCBasePlayer y PresentationCore para subir el primer fotograma decodificado a textura OpenGL en cuanto está disponible.\n"
            "• [MEJORA] Refresco Inmediato en Salida Pública, Stage y Red: Actualización instantánea del estado visual en LiveContentRenderer, ViewPanel, Monitor Central, MediaView, DocumentView y servidor de sincronización LAN al alternar fondos o medios.\n"
            "\n"
            "### Sistema Inteligente Anti-Duplicados & Menú de Selección de Medios\n"
            "• [NUEVO] Asistente Interactivo al Detectar Nuevos Archivos: Al detectar videos o imágenes nuevas en 'Descargas' o carpetas vinculadas, ProyecThor ya no los copia en silencio; ahora pregunta si deseas agregarlos (Sí / No) mediante una ventana profesional de selección.\n"
            "• [NUEVO] Selector Individual y Masivo (Multimedia vs Background): Menú interactivo con buscador integrado, casillas por archivo, botones de asignación rápida ('Todo a Multimedia' / 'Todo a Background') y selector individual [ Multimedia ] | [ Background ] para clasificar lotes grandes descargados.\n"
            "• [MEJORA] Vinculación Directa sin Duplicar Archivos en Disco: Los medios aceptados desde 'Descargas' o carpetas externas se reproducen directamente desde su ubicación original sin copiarse a las carpetas internas de la app, ahorrando espacio y evitando clones.\n"
            "• [CORRECCIÓN] Fin a la Duplicación Automática al Importar: Eliminado el desvío oculto que redirigía la vista a carpetas externas tras usar Importar, Arrastrar y Soltar o 'Abrir con ProyecThor', aplicando deduplicación estricta por nombre y ruta canónica en Multimedia, Fondos y Selector de Canciones.\n"
            "• [NUEVO] Desvinculación Segura y Silenciado de Fondos Externos: Opción contextual 'Quitar vínculo' para retirar medios externos de ProyecThor sin borrar el archivo real en 'Descargas', silenciado automático de audio en videos vinculados como Background y nuevos controles en Ajustes > Datos.\n"
            "\n"
            "### Rediseño de Espacio de Trabajo, Toolbars & Islas Flotantes\n"
            "• [INTERFAZ] Barra Izquierda Elevada y Diseño de Ancho Expandido: Reorganización estructural del espacio Clásico donde la Biblioteca y su barra (+ / Importar / Eliminar) suben por encima de la franja inferior, permitiendo que el panel de Diseño pase por debajo de Biblioteca y Home al mismo tiempo.\n"
            "• [INTERFAZ] Barra Inferior Adaptativa sin Solapamientos: Las 8 secciones inferiores (Fondos, Multimedia, Overlays, Estilos, Shaders, Transiciones, Anuncios y Captura) escalan proporcionalmente su ancho y reservan espacio dedicado al botón de transición rápida [⇄].\n"
            "• [NUEVO] Tres Islas Flotantes Superiores con Animación Switch: Reorganización de la barra superior en tres cápsulas traslúcidas con indicador elástico tipo interruptor móvil, botones circulares con etiquetas externas y cápsula de proyección (Público, Stage, Borrar Todo) centrada a la derecha:\n"
            "  - Isla 1 (Izquierda): Contenido y Biblioteca (Canciones, Biblia, Documentos).\n"
            "  - Isla 2 (Centro): Herramientas del Operador (Notas, Voz, Estilos rápidos, Conexiones LAN/Chat/Broadcast/Sync/OSC y Asistente IA).\n"
            "  - Isla 3 (Derecha): Sistema y Ayuda (Configuración global del entorno y menú de Ayuda/Documentación/PatchNotes).\n"
            "• [NUEVO] Colapso Inteligente y Sistema Cromático con Hover: Al elegir Canciones o Documentos la barra inferior se pliega dejando solo sus pestañas; al abrir una sección inferior la lista izquierda colapsa suavemente, acompañado de acentos de color propios en hover y exclusión mutua entre herramientas.\n"
            "\n"
            "### Biblia Ampliada, Gestión Directa de Versiones & Atajos Protegidos\n"
            "• [NUEVO] Botones '+' Importar y Borrar en el Menú de Biblia: El selector desplegable de versiones bíblicas ahora incluye botones integrados '+ Importar' (para añadir biblias XML al vuelo) y 'Borrar' con confirmación segura sobre cada versión instalada.\n"
            "• [INTERFAZ] Modo Lectura Ancha con Auto-Colapso Izquierdo: Al entrar a la sección de Biblia, la columna izquierda de Biblioteca se colapsa automáticamente para otorgar el máximo espacio horizontal a los libros, capítulos, versículos y buscador bíblico.\n"
            "• [CORRECCIÓN] Protección Total de Teclado al Buscar en la Biblia: Corregido el conflicto donde pulsar la letra 'N' abría accidentalmente el panel de Notas de Versión mientras se escribía o buscaba en la Biblia; durante el uso y búsqueda bíblica quedan bloqueados todos los atajos de teclas simples y solo operan combinaciones con Ctrl, Alt o teclas Fn (Notas de Versión pasa a Ctrl+Alt+N).\n"
            "• [MEJORA] Nombres Limpios de Versiones y Biblioteca Pulida: Reconocimiento automático de siglas e idiomas en biblias XML importadas (RVR1960, NVI, NTV, LBLA, TLA, KJV, ARC, etc.), estilo por defecto adaptable en paneles angostos y migración íntegra de metadatos/playlists al renombrar canciones.\n"
            "\n"
            "### Asistente de Voz Inteligente Local (100% Offline)\n"
            "• [NUEVO] Motor Nativo Offline sin Internet ni IA Externa: Reconocimiento de voz continuo 100% local en Windows (SRGS/WinRT) acoplado bajo Vista en Vivo, con control antirrebote de inicio/reinicio, detección de cortes silenciosos y accesos rápidos a permisos de micrófono e idioma Español.\n"
            "• [NUEVO] Tres Modos de Búsqueda Dedicados para el Operador: Control preciso del alcance del asistente en vivo:\n"
            "  - Todos los datos: Búsqueda global de citas bíblicas y letras de canciones en toda la biblioteca.\n"
            "  - Solo Biblia: Reconoce exclusivamente referencias y versículos bíblicos (evita saltos accidentales a canciones durante la predicación).\n"
            "  - Solo canción seleccionada: Avanza automáticamente las estrofas únicamente dentro de la canción activa en el proyector.\n"
            "• [NUEVO] Captura de Audífonos/Escritorio (🎧) y Citas Bíblicas: Detección directa de auriculares y salidas de audio de la PC en el selector de entrada para captar sonido del sistema, junto con el filtro inteligente que convierte números dictados seguidos (ej. 'Génesis 3 4' o 'Génesis 34') en 'Génesis 3:4'.",
            false
        },
        {
            22, "1.0.4",
            "VERSION 1.0.4 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "### Sistema Modular de Plugins & yt-dlp\n"
            "• [NUEVO] Carpeta de Plugins Extensible: Nueva carpeta 'plugins/' donde los usuarios pueden arrastrar y soltar scripts (.py, .sh, .bat, ejecutables) o carpetas con manifiesto 'plugin.json'.\n"
            "• [NUEVO] Instalación por Arrastrar y Soltar (Drag & Drop): Posibilidad de soltar cualquier archivo de plugin o script directamente sobre la ventana de ProyecThor para instalarlo de forma automática.\n"
            "• [NUEVO] Panel de Plugins & Extensiones: Nuevo panel interactivo accesible desde el riel de herramientas y el selector de paneles, con lista de complementos, buscador, argumentos personalizados y consola de terminal en tiempo real con captura de stdout/stderr.\n"
            "• [NUEVO] Desacoplamiento de yt-dlp: La integración con yt-dlp ya no depende de código fijo en el núcleo. Ahora vive como un plugin integrado e independiente en 'plugins/ytdlp/', facilitando su mantenimiento y actualización ante cambios de la plataforma.\n"
            "• [NUEVO] Actualización de yt-dlp con un Clic: Botón dedicado 'Actualizar yt-dlp (-U)' en el panel para mantener el motor al día directamente desde la interfaz.\n"
            "• [NUEVO] Herramientas yt-dlp Integradas: Resolución instantánea de streams de video para VLC y convertidor inteligente de subtítulos a letras limpias de canciones con copia directa.\n"
            "\n"
            "### Novedades en Pantalla Dividida (Tiles)\n"
            "• [NUEVO] Continuidad Inteligente entre Pantallas: Tres modos de flujo continuo para pantallas divididas:\n"
            "  - Historial / Previo: Pantalla 1 muestra el contenido actual mientras Pantalla 2 muestra lo que anteriormente salía en Pantalla 1 (o el versículo/estrofa previo). Al avanzar, el contenido previo se traslada suavemente a Pantalla 2.\n"
            "  - Continuación / Siguiente (Estilo Stage): Pantalla 1 proyecta el contenido actual y Pantalla 2 proyecta la continuación inmediata (siguiente versículo o estrofa) para lectura fluida en paralelo.\n"
            "  - Flujo Inverso: Pantalla 1 muestra el versículo previo y Pantalla 2 el actual para lectura continua de izquierda a derecha.\n"
            "• [NUEVO] Estilos Independientes por Pantalla: Posibilidad de asignar manualmente estilos visuales diferentes a Pantalla 1 y Pantalla 2 desde el panel sin alterar el estilo global del entorno.\n"
            "• [NUEVO] Control de Línea Divisoria: Opción para mostrar u ocultar la línea divisoria de los tiles según la preferencia del operador.\n"
            "• [NUEVO] Integración Completa con Biblias y Canciones: Detección automática y precálculo de versículos y estrofas anteriores y siguientes en tiempo real.\n"
            "\n"
            "### Rendimiento y Organización\n"
            "• [MEJORA] Precarga Instantánea en Pantalla de Inicio: La pantalla de bienvenida ahora precarga el entorno para que al pulsar 'Iniciar Proyección' se muestre de inmediato sin demoras.\n"
            "• [CORRECCIÓN] Remoción de Fondos Protegida: El botón para quitar fondos ahora retira exclusivamente la capa visual de fondo preservando las letras proyectadas intactas.\n"
            "• [MEJORA] Panel de Overlays Predeterminado: Integrado directamente en la barra de herramientas izquierda de la Biblioteca, ubicado estratégicamente después de Multimedia y arriba de Letras.\n"
            "• [MEJORA] Herramienta Reloj en Riel Izquierdo: Acceso unificado a la vista interactiva de reloj, temporizador y cronómetro desde la barra lateral izquierda.",
            false
        },
        {
            21, "1.0.3",
            "VERSION 1.0.3 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "### Correcciones y Estabilidad\n"
            "• [CORRECCIÓN] Salida en Vivo Protegida: Solucionado el problema donde presionar la tecla Escape bloqueaba irreversiblemente la salida en vivo e impedía volver a proyectar.\n"
            "• [CORRECCIÓN] Biblioteca de Letras Limpia: Eliminadas canciones de prueba inexistentes y estandarizado el archivo de bienvenida multilingüe por estrofa (welcome.txt) sin errores de lectura.\n"
            "• [MEJORA] Estabilidad General en Linux: Mejoras de estabilidad en la reproducción multimedia y corrección de bloqueos menores.",
            false
        },
        {
            20, "1.0.2",
            "VERSION 1.0.2 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "### Video y Rendimiento\n"
            "• [MEJORA] Motor de Video VLC Optimizado: Decodificación alineada y carga protegida sin bloqueos en Linux y Windows.\n"
            "• [MEJORA] Gestión Directa de Medios: Administración de videos desde carpetas externas sin clonar ni duplicar archivos en disco (con opción en Ajustes para usuarios que deseen duplicar).\n"
            "• [MEJORA] Panel de Shaders Simplificado: Acceso más directo y accesible a los efectos de post-procesamiento.\n"
            "• [MEJORA] Acceso Directo a Proyección: Eliminación del Hub tradicional en favor del acceso directo a Proyección y notas de versión accesibles desde el inicio.",
            false
        },
        {
            19, "1.0.1",
            "VERSION 1.0.1 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "### Estabilidad y Control\n"
            "• [CORRECCIÓN] Proyección Pantalla Completa en Windows: Solución definitiva para proyección directa e instantánea sin ventanas secundarias vacías.\n"
            "• [NUEVO] Ventana Desacoplable (ViewPanel Flotante): Posibilidad de separar ViewPanel en una ventana independiente flotante para configuraciones multi-monitor del operador.\n"
            "• [MEJORA] Atajo Global de Acople: Botón rápido de acople y atajo global Ctrl+Shift+D.",
            false
        },
        {
            18, "1.0.0",
            "VERSION 1.0.0 ESTABLE", "VERSION 1.0 ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "### Lanzamiento Oficial v1.0.0 Estable\n"
            "• [NUEVO] Edición de Letras en Caliente: Edición directa sobre ViewPanel con sincronización en vivo y guardado automático en .txt.\n"
            "• [NUEVO] Stage Display (Monitor de Escenario): Monitor de escenario completamente funcional para músicos y predicadores.\n"
            "• [NUEVO] Compatibilidad Tipográfica Completa: Soporte total para emojis y símbolos tipográficos en Windows y Linux.\n"
            "• [MEJORA] Biblia 2.0: Buscador inteligente sin restricciones de acentos ni formatos.\n"
            "• [MEJORA] Transición Teatral Iris: Transición Iris por GPU, visor 3D interactivo y Tour Guiado.",
            false
        },
        {
            17, "0.7.1",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "ProyecThor v0.7.1 (Beta): Nuevo visor y catalogo de recursos y modelos 3D (.gltf, .glb, .obj, .stl, .ply) con renderizado acelerado por GPU y proyeccion a pantalla en vivo, miniaturas con tipografia real en Estilos, zoom con control deslizante fluido, boton de transicion rapida renovado con centrado vectorial de precision, alineacion milimetrica en controles del monitor y optimizaciones de rendimiento en todo el sistema.",
            true
        },
        {
            16, "0.7.0",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "Lanzamiento de ProyecThor v0.7.0 (Beta): Nuevo fondo dinámico con ondas topográficas fluidas y partículas astrales, HUD estilizado con tarjetas de acción, panel Web vertical integrado con envío a pantalla pública, barra de filtros multimedia con iconos vectoriales de alta precisión, importación múltiple de archivos a la vez, nueva categoría Datos en Ajustes con carpetas de importe automático, intercambio directo y Drag & Drop entre Fondos y Multimedia, biblioteca de Notas Rápidas permanente, letrero de Anuncios sincronizado, reloj LAN independiente, Asistente de IA fluido (Claude/ChatGPT/Gemini) y eliminación total de oscurecimiento en público.",
            true
        },
        {
            15, "0.7.0-beta.1",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg6.jpg",
            "Asistente de IA en la toolbar de abajo: modo Básica (Claude/ChatGPT/Gemini en un navegador embebido de verdad, inicia sesion normal, ProyecThor no ve tu clave) y modo Avanzada (tu propia API key de Claude, puede listar/crear/editar canciones de la Biblioteca, siempre pide confirmacion antes de guardar algo). Espacio de trabajo \"Video\" renombrado a \"Producción\": ahora un rail izquierdo estilo Biblioteca con Render (conversor de formato), Audio (DAW real: grabar microfono, cortar/mover clips en la linea de tiempo, reproducir todas las pistas juntas, exportar a WAV/MP3/AAC/OGG) y Overlays (galeria+editor). Espacio de trabajo \"Transmisión\" simplificado a solo eso con una lista de capas real. Nuevo panel \"Web\" en la Biblioteca. La Inalámbrica (LAN) ahora se puede clavar en \"Solo reloj\" o \"En blanco\" mientras Público/Stage siguen con lo que este en vivo.",
            true
        },
        {
            14, "0.6.0",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg6.jpg",
            "Notas Rapidas renovado: ahora guarda el texto solo mientras escribis y lo recupera al reabrir la ventana, respeta el tema activo elegido en Apariencia, y queda disponible tanto desde el Hub como proyectando sin cortarse -- atajo nuevo Shift+Z para abrirlo/cerrarlo. Atajos Alt Gr+1/2/3/4 para colapsar y expandir paneles con una animacion prolija. Seccion Multimedia renombrada a \"Medios\", con vista en cuadricula de miniaturas grandes. Nuevo entorno de trabajo \"Biblioteca\". El Preview ahora tiene un boton de pantalla completa de verdad (F11) con controles que se ocultan solos.",
            true
        },
        {
            13, "0.6.0-beta.1",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg6.jpg",
            "Editor de overlays completo en la app movil, panel Render renovado en Biblioteca (codecs H.264/H.265/VP9/AV1, control de compresion), soporte real para Linux/CachyOS, la app ahora respeta el escalado de pantalla de Windows (DPI), Ajustes con categoria \"Conexiones\" y el Editor de Estilos de Letra renovado por completo a pantalla completa.",
            true
        },
        {
            12, "0.5.1",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg5.jpg",
            "Reloj y Contadores ahora es solo \"Contadores\". Nuevo cuadro de reloj dentro del editor de Overlays: lo posicionas y le das estilo una sola vez, y se reemplaza en vivo por la hora/cronometro activo. Overlays con reordenar capas y overlays de reloj predeterminados listos para probar.",
            true
        },
        {
            11, "0.5.0",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg5.jpg",
            "Ajustes reorganizado por completo: cada configuracion ahora es su propia pagina con buscador incluido. Nueva opcion \"Bucle falso\" para Fondos. Nueva seccion de Overlays con capas transparentes. Vista en Vivo renovada con botones planos y menor latencia.",
            true
        },
        {
            10, "0.4.3",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bg_splash3.jpg",
            "Nueva seccion Conexiones (OSC, Red, Chat y Streaming en vivo por RTMP), nueva Biblioteca para gestionar tus archivos con conversor de formato incluido, Biblia a pantalla completa, selector rapido (Alt+Espacio) y nuevo instalador para Windows.",
            true
        },
        {
            9, "0.4.2",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bg_splash3.jpg",
            "Pads de Vista en Vivo arreglados y renovados con escenas de Captura sincronizadas, transporte y volumen rediseñados tipo consola/MIDI, buscador de versiculos por palabras en la Biblia y shaders (NIS, VHS, Cine, TAA).",
            true
        },
        {
            8, "0.4.1",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bg_splash3.jpg",
            "Nuevo panel de Shaders para el video de fondo, miniaturas y vista en grilla/lista en Biblioteca > Videos, escenas rapidas guardadas para Captura y editor de canciones rediseñado.",
            true
        },
        {
            7, "0.4.0",
            "ETAPA BETA", "BETA / PRE-1.0",
            "bg_splash3.jpg",
            "Cola de videos mucho mas estable, nueva seccion de Overlays, Vista en Vivo con acciones rapidas, panel de Rendimiento y rediseño de Fondos y Estilos.",
            true
        },
        {
            6, "0.3.5",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg1.jpg",
            "Audio Rework completo, biblioteca renovada con sistema de etiquetas, soporte oficial para Linux, estadisticas locales y atajos de teclado globales.",
            true
        },
        {
            2, "0.3.0",
            "ETAPA BETA", "BETA / PRE-1.0",
            "splash_bg1.jpg",
            "Nuevas herramientas de transmision, optimizaciones y estabilidad de red.",
            true
        },
    };
    return s_Registry;
}

inline const UpdateVersionInfo* FindPatchNotesVersion(int id) {
    const auto& reg = GetPatchNotesRegistry();
    for (const auto& v : reg)
        if (v.id == id) return &v;
    return reg.empty() ? nullptr : &reg[0];
}

} // namespace ProyecThor::UI
