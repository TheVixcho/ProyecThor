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
            20, "1.0.2",
            "VERSION 1.0.2 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "ProyecThor v1.0.2 - Optimización de Video y Experiencia: Nuevo motor de video VLC sin bloqueos en Linux y Windows con decodificación alineada y carga protegida, administración directa de videos desde carpetas externas sin clonar ni duplicar archivos en disco (con opción en Ajustes para usuarios que deseen duplicar), panel de Shaders simplificado y más accesible, eliminación del Hub tradicional en favor del acceso directo a Proyección, nuevas Notas de Versión accesibles directamente desde la pantalla de inicio y mejoras visuales.",
            false
        },
        {
            19, "1.0.1",
            "VERSION 1.0.1 ESTABLE", "ACTUALIZACION ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "ProyecThor v1.0.1 - Actualización de Estabilidad y Control: Solución definitiva en Windows para proyección a pantalla completa directa e instantánea sin ventanas secundarias vacías, nueva función para separar ViewPanel en una ventana independiente flotante (ideal para configuraciones multi-monitor del operador), botón rápido de acople y atajo global Ctrl+Shift+D.",
            false
        },
        {
            18, "1.0.0",
            "VERSION 1.0.0 ESTABLE", "VERSION 1.0 ESTABLE",
            "bin/assets/ui/textures/iniciarpro.jpg",
            "ProyecThor v1.0.0 - PRIMERA VERSION OFICIAL Y COMPLETAMENTE ESTABLE: Lanzamiento definitivo para producción multimedia en vivo. Incluye edición de letras en caliente directamente sobre ViewPanel con sincronización en vivo y guardado automático en .txt, nuevo instalador de Windows inteligente y blindado anti-duplicados, compatibilidad completa con emojis y símbolos tipográficos en Windows y Linux, Stage Display (Monitor de Escenario) completamente funcional, interfaz profesional rediseñada con mayor aprovechamiento del espacio, barra de Asistente de IA opcional, pantalla secundaria en Linux (Wayland/X11) y Windows, Biblia 2.0 con buscador inteligente sin restricciones de acentos ni formatos, transición teatral Iris, efectos atmosféricos por GPU, visor 3D y Tour Guiado interactivo.",
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
