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
            28, "1.1.5",
            "VERSIÓN 1.1.5 - NUEVA INTERFAZ SLINT, MATRIZ MULTI-PANTALLA E ISLA DINÁMICA", "LANZAMIENTO OFICIAL",
            "splash_bg7.jpg",
            "### Nueva Interfaz Moderna Slint + libmpv (Open Source)\n"
            "• [NUEVO] Matriz Multi-Pantalla y Salas: Control independiente de Público Principal, LAN / Smart TV (8080), Stage Display y Salas virtuales adicionales con enrutamiento selectivo por pantalla.\n"
            "• [NUEVO] Capas Activas estilo Photoshop: Panel en vivo dentro de Vista en Vivo con miniaturas de cada capa (Tiempo, Aviso, Pleca, Overlay PNG, Letras/Biblia, Fondo y Música) y eliminación individual.\n"
            "• [NUEVO] Isla Dinámica Acoplada con Estudio de Audio: Ondas FFT reales de 256 puntos sincronizadas con la pista, barra de onda interactiva de 56 barras, Ecualizador de 5 Bandas y EQ Dinámica ProyecThor.\n"
            "• [MEJORA] Barra Superior Unificada, Fondos con Árbol de Carpetas, Portadas de Música, Reescalado Real 720p/1080p/16:10/4:3 + FSR, Botón Freeze y Transmisión LAN/Smart TV/Celular independientes.",
            false
        },
        {
            27, "1.1.4",
            "VERSIÓN 1.1.4 - HOTFIX MENORES Y MEJORAS DE RENDIMIENTO GENERAL", "HOTFIX OFICIAL",
            "splash_bg7.jpg",
            "### Hotfixes de Rendimiento y Mejoras Generales\n"
            "• [RENDIMIENTO] Optimizaciones del Motor Gráfico: Hotfix con mejoras de rendimiento general en la tasa de cuadros y reducción de uso de memoria GPU.\n"
            "• [RENDIMIENTO] Sondeo Asíncrono Eficiente: Optimizado el sistema de comprobación de actualizaciones y sincronización en segundo plano con solicitudes HTTP ultraligeras sin bloqueos de interfaz.\n"
            "• [LICENCIAMIENTO] Sesión Única Concurrente (1 PC a la vez): Protección de cuenta institucional con detección y transferencia transparente de sesión activa entre equipos.\n"
            "• [ESTABILIDAD] Hotfixes Menores: Correcciones menores de rendimiento y estabilidad en la carga de bibliotecas, overlays y cambio de escenas.",
            false
        },
        {
            26, "1.1.3",
            "VERSIÓN 1.1.3 - MULTI-CARPETAS PRO, NOTAS EN AJUSTES, UTF-8 & ACTUALIZACIONES EN UN CLIC", "ACTUALIZACIÓN OFICIAL",
            "splash_bg7.jpg",
            "### Gestión Multi-Carpeta Externa (ProyecThor for Professionals)\n"
            "• [NUEVO] Importación y Escaneo Externo sin Duplicar: Los usuarios Pro pueden vincular carpetas completas de música, videos, descargas o imágenes externas sin duplicar archivos en AppData.\n"
            "• [NUEVO] Ramas de Carpetas Independientes: En la biblioteca multimedia y de música, cada carpeta externa se visualiza como una rama independiente junto a la carpeta personal de ProyecThor.\n"
            "• [NUEVO] Creación de Colas y Diálogo 'Mover o Duplicar': Al crear subcarpetas o arrastrar archivos, el sistema permite elegir la ubicación de destino y decidir si mover o duplicar el elemento en la nueva cola.\n"
            "\n"
            "### Experiencia de Usuario, UTF-8 y Diseño Visual\n"
            "• [CORRECCIÓN] Soporte Nativo UTF-8 y Letra 'ñ': Corregida la lectura y renderizado de caracteres especiales, tildes y la letra 'ñ' en títulos de canciones, fondos y menús en Windows.\n"
            "• [MEJORA] Botones de Menú y Barra Superior sin Cortes: Rediseñado el botón de detener cola en el panel lateral con padding armónico; barra superior de búsqueda y controles de reproducción adaptados dinámicamente para no cortarse en resoluciones compactas.\n"
            "• [MEJORA] Modal de Importación Profesional: Simplificado el mensaje del modal de importación a 'Copiar archivo dentro de la carpeta del programa'.\n"
            "\n"
            "### Notas de Versión en Ajustes y Actualizaciones en 1 Clic\n"
            "• [NUEVO] Sección 'Notas de versión' en Ajustes: Historial completo de versiones trasladado de la pantalla de bienvenida a una sección dedicada dentro de Ajustes, con buscador y navegación por categorías.\n"
            "• [MEJORA] Actualización Automática Simplificada: Canal único oficial y estable sin configuraciones complejas; botón 'Comprobar Actualizaciones' que descarga e instala automáticamente con un solo clic.\n"
            "• [MEJORA] Soporte Extendido de Idioma Inglés: Traducción de categorías de ajustes, grupos de navegación y mensajes del sistema al inglés.\n"
            "\n"
            "### Consola de Pads de Escena, Modo Rendimiento y LAN\n"
            "• [NUEVO] Pads Integrados estilo Launchpad: 8 Pads de escenas rápidas en la consola inferior de Vista en Vivo con indicadores LED e iconografía vectorial limpia.\n"
            "• [NUEVO] Modo Rendimiento en Ajustes: Optimizado para equipos de bajos recursos, reduciendo hilos de trabajo y escaneo innecesario.\n"
            "• [MEJORA] Transmisión LAN Gratuita + Canal Alpha Pro: Envío de pantalla por red local para todos los usuarios con salida transparente en versión Pro.",
            false
        },
        {
            25, "1.1.2",
            "VERSIÓN 1.1.2 - NUEVA NAVEGACIÓN, TOOLBARS & GESTIÓN DE COLAS", "ACTUALIZACIÓN OFICIAL",
            "splash_bg7.jpg",
            "### Nueva Experiencia de Navegación e Islas Flotantes\n"
            "• [NUEVO] Islas de herramientas superiores: Estilos, Shaders, Transiciones, Pads, Tiles, Chat, Anuncios, Overlays y Librería ahora residen en la barra superior en cápsulas estilizadas, abriendo sus propias ventanas flotantes independientes.\n"
            "• [NUEVO] Conexiones y Reloj Unificados: El panel de Conexiones (LAN, Chat, Broadcast, Sync, OSC) y el Reloj se trasladaron a la cápsula de emisión junto a 'Proyectar a Público', con una nueva ventana de configuración integrada estilo Ajustes.\n"
            "• [MEJORA] Navegación limpia y solo con iconos: Barra más espaciosa, sin etiquetas de texto fijas, con tooltips explicativos al pasar el cursor y auto-escalado proporcional para evitar solapamientos en cualquier resolución.\n"
            "• [MEJORA] Pestaña Diapositivas renombrada a 'Home', y los plugins/extensiones ahora vienen desactivados por defecto para un arranque más ligero.\n"
            "\n"
            "### Gestión de Colas y Reproducción Multimedia\n"
            "• [NUEVO] Control de Colas Integrado: Botón directo 'Detener Cola' y visualización del estado de cola en la barra superior de Multimedia y Videos.\n"
            "• [MEJORA] Distintivo de Transmisión en Vivo: Indicador visual [EN VIVO] en las miniaturas activas, permitiendo identificar al instante qué medio está saliendo a pantalla.\n"
            "• [MEJORA] Previsualización Rápida (Preview): Controles directos de Proyectar, Preview y Detener en cada tarjeta de video.\n"
            "\n"
            "### Toolbars, Galerías y Miniaturas Pulidas\n"
            "• [MEJORA] Miniaturas compactas por defecto: El tamaño inicial de las cuadrículas en Fondos y Overlays se ajustó al mínimo (zoom 0.65x) para visualizar más contenido simultáneamente.\n"
            "• [MEJORA] Padding y Estética Profesional: Mayor espaciado visual en la barra de búsqueda y en el botón 'Colapsar', logrando una separación armónica con la barra divisoria.\n"
            "• [CORRECCIÓN] Eliminación de Solapamiento en Scroll: Corregido el fallo donde los videos y miniaturas se montaban sobre las barras de herramientas superiores al hacer scroll vertical.\n"
            "\n"
            "### Diálogos y Ventanas Centradas con Foco Activo\n"
            "• [CORRECCIÓN] Centrado Automático de Ventanas: Los nuevos cuadros de diálogo (creación de carpetas, confirmaciones y modales) ahora aparecen siempre en el centro de la pantalla en vez de la esquina.\n"
            "• [CORRECCIÓN] Foco Prioritario de Ventanas: Las ventanas emergentes reciben el foco del sistema inmediatamente al abrirse, evitando que queden ocultas detrás de la ventana principal.\n"
            "\n"
            "### Iconografía Renovada y MuninCloud\n"
            "• [MEJORA] Icono de Nube en Munin: Sustituido el logo anterior por una nube celeste moderna y eliminada la etiqueta restrictiva 'PRO' del encabezado.\n"
            "• [NUEVO] Cancionero en Nube: Nuevo icono representativo de hoja de acordes con nube para el gestor de canciones en Firebase.\n"
            "• [MEJORA] Interfaz de Munin y Hugin alineada con el diseño visual de Ajustes, con miniaturas protegidas para fondos en la nube.\n"
            "\n"
            "### Vista en Vivo Optimizada\n"
            "• [MEJORA] Aprovechamiento Total de Pantalla: El riel lateral de ViewPanel se reubicó en la barra inferior (limpiar texto, fondo, overlay, anuncios, reloj, proporción y ventana flotante), dejando el 100% del ancho para la vista previa.",
            false
        },
        {
            24, "1.1.1",
            "VERSIÓN 1.1.1 - PROYECTHOR PRO & REPARACIONES", "ACTUALIZACIÓN OFICIAL",
            "splash_bg7.jpg",
            "### ProyecThor for Professionals & Ecosistema en la Nube\n"
            "• [NUEVO] Integración Oficial con Canva Connect API: Vinculación directa mediante OAuth 2.0 PKCE para explorar tus presentaciones y diseños de Canva desde la biblioteca e importar diapositivas en alta resolución directamente a Fondos o Multimedia.\n"
            "• [NUEVO] Autenticación y Licenciamiento Cloud con Firebase: Inicio de sesión con correo, actualización de nombre de usuario, restablecimiento de contraseña y sincronización en tiempo real del estado de cuenta y suscripción Pro mediante Google Firebase.\n"
            "• [NUEVO] Pasarela de Pago Integrada con Flow: Contratación y renovación de planes Pro ($9.990 CLP/mes) realizada directamente desde la aplicación de escritorio en Ajustes > Mi Cuenta > Suscripción Pro, con detección automática de Webpay y activación instantánea de licencia.\n"
            "• [SEGURIDAD] Gestión Dinámica de Visibilidad Pro: El botón y sección de Canva se muestran exclusivamente cuando el usuario cuenta con una licencia Pro activa, optimizando el espacio y protegiendo las funciones de suscripción.\n"
            "\n"
            "### Correcciones en el Reloj y Renderizado de Overlays\n"
            "• [CORRECCIÓN] Resolución y Escalado del Reloj en Pantalla: Corregido el problema de dimensionamiento del canvas en capas de reloj de superposición (Overlay Clock Canvas), sincronizando con precisión el ancho y alto del canvas nativo con la resolución real de salida del proyector.\n"
            "• [CORRECCIÓN] Temporizadores y Cuenta Regresiva Precisos: Eliminada la desincronización de escala al alternar entre pantallas de distinta resolución (Monitor local vs Proyector 1080p/4K vs Stage Display).\n"
            "\n"
            "### Estabilidad, Robustez y Prevención de Cierres Inesperados\n"
            "• [ESTABILIDAD] Corrección de Asertos ImGui en Contenedores Hijos: Reemplazo de banderas incompatibles (ImGuiWindowFlags_AlwaysAutoResize) por ImGuiChildFlags en CanvaPanel y VoiceAssistantPanel, erradicando por completo el error de abort() al interactuar con paneles laterales.\n"
            "• [ESTABILIDAD] Cierre de Sesión Atómico y Seguro: Limpieza protegida de sesión y buffers de interfaz al cerrar sesión o cambiar de cuenta sin excepciones de puntero nulo ni fugas de memoria.\n"
            "• [ESTABILIDAD] Sistema de Captura de Errores y Diagnóstico: Incorporación de filtros de excepción no controlada y trazas de depuración nativas para garantizar máxima fiabilidad durante cultos y eventos en vivo.",
            false
        },
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
            "• [RENDIMIENTO] Sincronización Directa de Capas y Reproductores: Optimización del ciclo de activación en BackgroundLayer, MPVBasePlayer y PresentationCore para subir el primer fotograma decodificado a textura OpenGL en cuanto está disponible.\n"
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
            "### ProyecThor for Professionals & Carpetas de Escaneo\n"
            "• [PRO] Carpetas de Escaneo Externas sin Duplicación: Función exclusiva para usuarios PRO que permite añadir bibliotecas y carpetas de medios (ej. Descargas, Mi Música, Videos) para escaneo directo sin copiar ni duplicar archivos en AppData.\n"
            "• [GRATUITO] Almacenamiento Estricto en Assets: Los usuarios gratuitos almacenan sus datos exclusivamente en AppData/proyecthor/assets mediante importación estándar sin duplicación descontrolada ni auto-importaciones en segundo plano.\n"
            "• [NUEVO] Separación de Ramas en Biblioteca: La biblioteca de Videos, Imágenes, Texto (Documentos) y Música ahora presenta la carpeta principal de ProyecThor junto a ramas independientes para cada carpeta externa (ej. 'ProyecThor' y 'Mi Música').\n"
            "• [NUEVO] Creación de Carpetas y Colas con Selector de Ubicación: Al crear una nueva carpeta o cola, los usuarios PRO pueden elegir si crearla dentro de ProyecThor o en una carpeta externa (para usuarios gratuitos se crea de forma directa en ProyecThor sin pasos innecesarios).\n"
            "• [NUEVO] Mover o Duplicar (Copiar y Pegar) en Carpetas: Al agregar o arrastrar archivos hacia carpetas y colas de reproducción, el sistema ofrece ambas opciones: mover el archivo entre listas o duplicarlo/copiarlo.\n"
            "• [NUEVO] Canciones en Carpetas y Subdirectorios: Soporte total para organizar archivos de canciones (.txt) dentro de carpetas y subcarpetas, con detección recursiva, visualización limpia de títulos y asignación a listas.\n"
            "\n"
            "### Sistema de Etiquetas Reparado & Correcciones\n"
            "• [CORRECCIÓN] Sistema de Etiquetas Renovado en Ajustes > Canciones: Reparada la creación, edición y persistencia de etiquetas. Soporte completo de acentos y caracteres especiales, selector interactivo de color, previsualización en vivo estilo chip y eliminación segura sin dejar etiquetas huérfanas.\n"
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
