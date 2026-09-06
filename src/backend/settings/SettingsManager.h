#pragma once
#ifndef PROYECTHOR_SETTINGS_SETTINGS_MANAGER_H
#define PROYECTHOR_SETTINGS_SETTINGS_MANAGER_H

#include <string>
#include <vector>
#include <imgui.h>
#include "Version.h"
#include "StageLayoutTemplates.h"

namespace ProyecThor::Settings {

    // ── Idiomas ──────────────────────────────────────────────────────────
    enum class Language { Spanish = 0, English, Portuguese, COUNT };

    inline const char* LanguageName(Language l) {
        switch (l) {
            case Language::Spanish:    return "Español";
            case Language::English:    return "English";
            case Language::Portuguese: return "Português";
            default:                   return "Español";
        }
    }

    // ── Proyección ───────────────────────────────────────────────────────
    struct ProjectionSettings {
        int   targetMonitor   = -1;

        // Monitores de salida publica ADICIONALES (opcional) -- todos
        // muestran exactamente lo mismo que targetMonitor. Ver
        // PresentationCore::SetTargetMonitor / UIManager::RenderProjectorOutput.
        std::vector<int> extraMonitors;

        int   outputWidth     = 0;
        int   outputHeight    = 0;
        float contentScale    = 1.0f;
        float marginTop       = 50.0f;
        float marginBottom    = 50.0f;
        float marginLeft      = 50.0f;
        float marginRight     = 50.0f;
        int   aspectRatioMode = 1;
        float customAspectW   = 16.0f;
        float customAspectH   = 9.0f;
        float defaultBgR      = 0.0f;
        float defaultBgG      = 0.0f;
        float defaultBgB      = 0.0f;
        float textSize        = 48.0f;
        float textColorR      = 1.0f;
        float textColorG      = 1.0f;
        float textColorB      = 1.0f;
        float textColorA      = 1.0f;
        int   textAlignment   = 1;
        int   vAlignment      = 1;
        bool  autoScale       = true;

        std::string selectedFont = "Arial.ttf";

        // Efectos visuales del texto proyectado (fondo/borde/sombra/
        // aberracion cromatica/glow/neon/subrayado) -- empaquetados como una
        // sola linea CSV, ver Core::PackTextEffects/UnpackTextEffects
        // (PresentationCore.h) y TextEffectsRenderer.h para el dibujo.
        std::string textEffectsPacked;

        float lineSpacing  = 1.2f;
        bool  fadeEnabled  = true;
        float fadeDuration = 0.3f;
        bool  vsync        = true;
        int   targetFPS    = 60;

        // ── Calidad de salida (video de fondo) ──────────────────────────
        // outputWidth/outputHeight/targetFPS (arriba) se reutilizan como el
        // tamano/fps del modo Custom. outputQualityMode: 0=Auto, 1=Preset,
        // 2=Custom (ver ProjectionQualityPresets.h).
        int   outputQualityMode = 0;
        int   outputPresetIndex = 3; // default: "1080p / 60 FPS"

        // ── Logo (pantalla de carga) ─────────────────────────────────────
        // Imagen que se muestra a la salida REAL (publico) mientras un
        // fondo/video esta cargando (ver PresentationCore::
        // ShouldShowLoadingScreen) en vez de dejar ver un frame
        // entrecortado/viejo. Vacio = sin logo, comportamiento sin cambios.
        std::string loadingLogoPath;

        // ── Fondos: "bucle falso" ─────────────────────────────────────────
        // Ver Ajustes > Proyeccion > Fondos y el comentario largo en
        // BackgroundLayer.h (m_PingPongEnabled). Solo afecta a Fondos
        // (allowAudio=false), nunca a Videos/cola del Monitor.
        bool bgPingPongLoop = false;

        // ── Efectos de post-proceso (salida en vivo, panel "Shaders") ────
        // fsrEnabled/fsrSharpness: ya corren hoy en BackgroundLayer (solo
        // sobre el fondo, para upscale); antes no se persistian ni tenian
        // UI. crt/grain/fxaa corren sobre el COMPOSITE completo de
        // "ProjectorLive" (ver CompositePostChain.h) — filtros 1:1, sin
        // upscale.
        bool  fsrEnabled            = true;
        float fsrSharpness          = 0.2f;
        // Escalador alternativo exclusivo de NVIDIA (ver PostProcessorNIS.h).
        // Mutuamente excluyente con FSR -- activar uno apaga el otro (ver
        // PresentationCore::SetFSREnabled/SetNISEnabled).
        bool  nisEnabled            = false;
        float nisSharpness          = 0.5f;
        bool  crtEnabled            = false;
        float crtScanlineIntensity  = 0.5f;
        bool  grainEnabled          = false;
        float grainIntensity        = 0.15f;
        bool  fxaaEnabled           = false;
        bool  saturationEnabled     = false;
        float saturationAmount      = 1.3f;
        bool  vignetteEnabled       = false;
        float vignetteIntensity     = 0.45f;
        bool  blurEnabled           = false;
        float blurIntensity         = 0.35f;
        bool  sharpenEnabled        = false;
        float sharpenIntensity      = 0.35f;
        bool  bloomEnabled          = false;
        float bloomIntensity        = 0.35f;
        bool  chromaticAberrationEnabled   = false;
        float chromaticAberrationIntensity = 0.35f;
        bool  vhsEnabled            = false;
        float vhsIntensity          = 0.5f;
        bool  cineEnabled           = false;
        float cineIntensity         = 0.5f;
        int   cineTint              = 0; // 0=rojo, 1=verde, 2=azul
        bool  contrastEnabled       = false;
        float contrastAmount        = 1.3f;
        bool  luminosityEnabled     = false;
        float luminosityAmount      = 1.2f;
        bool  taaEnabled            = false;
        float taaIntensity          = 0.5f;

        // ── Nuevos Shaders & Efectos ──
        bool  glitchEnabled         = false;
        float glitchIntensity       = 0.40f;
        float glitchSpeed           = 1.0f;
        int   glitchMode            = 0; // 0=Sutil, 1=Cyberpunk RGB, 2=Cinta Analógica

        bool  colorGradingEnabled   = false;
        float colorGradingIntensity = 0.75f;
        int   colorGradingPreset    = 1; // 0=Cálido, 1=Teal&Orange, 2=Cyber Neón, 3=Sepia, 4=Noir B&W, 5=Matrix, 6=Pastel

        bool  pixelateEnabled       = false;
        float pixelateSize          = 12.0f;
        int   pixelateColorDepth    = 0; // 0=Real, 1=16-bit, 2=8-bit

        bool  radialBlurEnabled     = false;
        float radialBlurIntensity   = 0.35f;

        bool  wavesEnabled          = false;
        float wavesIntensity        = 0.35f;
        float wavesSpeed            = 1.0f;
        float wavesFrequency        = 8.0f;

        bool  mirrorEnabled         = false;
        int   mirrorMode            = 0; // 0=Horizontal, 1=Vertical, 2=Caleidoscopio 4x, 3=Radial 8x

        bool  thermalEnabled        = false;
        float thermalIntensity      = 0.85f;
        int   thermalMode           = 0; // 0=Térmico, 1=Visión Nocturna, 2=Solarizado

        bool  halftoneEnabled       = false;
        float halftoneDotScale      = 10.0f;
        int   halftoneMode          = 0; // 0=Pop-Art Color, 1=Monocromo B&W, 2=Periódico

        // ── Efectos Volumétricos y por Zonas ──
        bool  volumetricFogEnabled         = false;
        float volumetricFogDensity         = 0.50f;
        float volumetricFogSpeed           = 1.0f;
        float volumetricFogScale           = 3.5f;
        int   volumetricFogColorMode       = 0; // 0=Gris, 1=Cian, 2=Fuego, 3=Neón

        bool  volumetricCloudsEnabled      = false;
        float volumetricCloudsCoverage     = 0.55f;
        float volumetricCloudsDensity      = 0.60f;
        float volumetricCloudsSpeed        = 0.80f;
        float volumetricCloudsSunIntensity = 0.65f;

        bool  zonedDistortionEnabled       = false;
        float zonedDistortionIntensity     = 0.45f;
        float zonedDistortionSpeed         = 1.20f;
        int   zonedDistortionZone          = 0; // 0=Inferior, 1=Superior, 2=Centro, 3=Izq, 4=Der
        float zonedDistortionFeather       = 0.35f;

        // "Rellenado": llena las barras de letterbox/pillarbox con el
        // mismo fondo estirado y muy desenfocado en vez de negro. Ver
        // BackgroundLayer::GetBlurredFillTexture / UIManager.cpp.
        bool  fillBlurEnabled       = false;
        // 0 = negro, 1 = brillo real del fondo desenfocado.
        float fillBlurBrightness    = 0.6f;

        // ── Motor de renderizado del fondo de video ──────────────────────
        // 0 = OpenGL compuesto (fondo + overlays + texto en vivo juntos).
        // 1 = VLC en ventana nativa (el fondo se muestra en una ventana
        // propia con el renderer acelerado de VLC; sin overlays/texto
        // encima ni transicion animada entre clips — ver
        // BackgroundLayer::SetUseNativeEngine). Default libvlc (1): pedido
        // explicito, sin que el operador tenga que ir a configurarlo.
        int videoRenderEngine = 1;
    };

    // ── Audio ────────────────────────────────────────────────────────────
    struct AudioSettings {
        int         masterVolume     = 100;
        int         previewVolume    = 80;
        bool        muted            = false;
        bool        muteOnStop       = true;
        bool        muteOnBlank      = false;
        bool        fadeOnTransition = true;
        float       fadeDuration     = 0.5f;
        std::string audioDevice      = "";
    };

    // ── General ──────────────────────────────────────────────────────────
    struct GeneralSettings {
        bool        startMinimized      = false;
        bool        rememberLayout      = true;
        bool        confirmOnExit       = true;
        bool        autoSave            = true;
        int         autoSaveIntervalSec = 120;
        std::string defaultBiblesFolder = "";
        std::string defaultMediaFolder  = "";
        Language    language            = Language::Spanish;
        std::string dismissedChangelog  = "";
        // Titulos bajo los iconos de los 4 rails (Biblioteca/Home/Control/Diseño).
        // Apagarlo los deja solo-icono para ocupar menos espacio en pantalla.
        bool        showRailLabels      = true;
        // Panel opcional de diagnostico (CPU/RAM/FPS/GPU), activable desde
        // el menu Vista. Apagado por default: es una herramienta puntual,
        // no algo que se quiera ver todo el tiempo.
        bool        showPerfPanel       = false;
        // Riel angosto de botones "Limpiar <tipo>" a la derecha del video en
        // Vista en Vivo (ver ViewPanel::RenderQuickActions). Opcional desde
        // el menu Vista para operadores que no lo necesitan y prefieren mas
        // ancho para el video.
        bool        showViewQuickActions = true;
        // Si es false, se inicia directo en modo Proyector tras la pantalla de carga (omite el Hub)
        bool        openHubOnStartup     = true;

        // Texto de la ventana flotante de Notas (ver QuickNotes) -- se
        // guarda con debounce mientras el operador escribe y se fuerza al
        // cerrar la ventana, para que nunca se pierda lo que iba tipeando
        // aunque cierre la app sin borrarlo a mano.
        std::string quickNotesText = "";
    };

    // ── Tema ─────────────────────────────────────────────────────────────
    // Set reducido de tokens de diseño. ApplyTheme() los expande a todos
    // los colores de ImGui, así que un solo token cambia toda la app.
    enum class ThemePreset {
        Dark, Light, OrangeBlack, Jazz, Kofi, Deadlock, Galaxy, Mek,
        Cyberpunk, Emerald, Crimson, Midnight, Amethyst, Titanium,
        Custom
    };

    const char* ThemePresetName(ThemePreset preset);
    ThemePreset ThemePresetFromString(const std::string& s);

    struct ThemeSettings {
        ThemePreset preset = ThemePreset::Dark;

        // Gris neutro tipo ProPresenter/OBS (ver MakeThemePreset(Dark) para
        // el preset real que se aplica en runtime; estos son solo el
        // fallback de construccion por defecto de la struct).
        float base[4]        = { 0.078f, 0.078f, 0.082f, 1.0f }; // ventana principal
        float surface0[4]    = { 0.098f, 0.098f, 0.102f, 1.0f }; // paneles hijos
        float surface1[4]    = { 0.130f, 0.130f, 0.136f, 1.0f }; // popups / inputs
        float surface2[4]    = { 0.165f, 0.165f, 0.172f, 1.0f }; // hover
        float surface3[4]    = { 0.205f, 0.205f, 0.213f, 1.0f }; // active

        float accent[4]      = { 0.550f, 0.560f, 0.580f, 1.0f };
        float accentLight[4] = { 0.720f, 0.730f, 0.750f, 1.0f };
        float accentDim[4]   = { 0.380f, 0.390f, 0.410f, 1.0f };
        float accentFaint[4] = { 0.550f, 0.560f, 0.580f, 0.18f };

        float border[4]      = { 1.000f, 1.000f, 1.000f, 0.08f };
        float borderFaint[4] = { 1.000f, 1.000f, 1.000f, 0.04f };

        float textPrimary[4] = { 0.920f, 0.930f, 0.960f, 1.0f };
        float textDim[4]     = { 0.700f, 0.720f, 0.780f, 1.0f };
        float textFaint[4]   = { 1.000f, 1.000f, 1.000f, 0.28f };

        float danger[4]      = { 0.940f, 0.350f, 0.390f, 1.0f };
        float success[4]     = { 0.320f, 0.880f, 0.630f, 1.0f };

        float windowRounding = 14.0f;
        float frameRounding  =  9.0f;
        float scrollbarSize  =  8.0f;

        // Ruta absoluta a un .ttf/.otf elegido por el usuario para la
        // interfaz de la app (ver CategoryTheme.cpp, seccion "Fuente de la
        // interfaz"). Vacio = usar la fuente por defecto. Se valida antes de
        // cargar (ver IsValidFontFile) y si falla se cae a la default -- ver
        // main.cpp, carga de io.Fonts justo antes de ImGui_ImplGlfw_InitForOpenGL.
        std::string customFontPath = "";
    };

    ThemeSettings MakeThemePreset(ThemePreset preset);

    // ── Entorno de trabajo (Apariencia > Entorno de trabajo) ────────────────
    // Ordenamiento de los 4 paneles dockeados (Biblioteca/Home/Vista en Vivo/
    // Diseño) -- ver UIManager::BeginDockspace, que construye un arbol de
    // DockBuilder distinto segun este valor. Cambiar el preset dispara un
    // reset de layout automatico (UIManager compara contra el ultimo valor
    // visto, ver m_LastWorkspacePreset), no hace falta pedirlo a mano.
    enum class WorkspaceLayoutPreset {
        Classic = 0,   // el de siempre: Biblioteca | Home/Diseño (arriba/abajo) | Vista en Vivo
        Simple,        // estilo Holyrics: Diseño se apila con Vista en Vivo a la derecha,
                       // Home ocupa todo el alto disponible en el centro
        Broadcast,     // Streaming (Captura/Capa/Iniciar, ver StreamingWorkspacePanel) como
                       // franja superior completa en vez de Vista en Vivo; Biblioteca/Home/
                       // Diseño en tres columnas abajo
        Library,       // Biblioteca (bloqueada en Medios) | Home (Preview) -- sin Vista en
                       // Vivo/Diseño, para operar solo reproduciendo contenido de la
                       // biblioteca. Tambien lo usa "Abrir con ProyecThor" para esa sesion
                       // (ver UIManager::EnterLibraryWorkspaceMode), sin pisar este setting.
        Video,         // "Producción" (nombre visible, ver WorkspaceLayoutPresetName) a
                       // pantalla completa (ver VideoEditorPanel): toolbar interna con
                       // Render (conversor de formato, LibraryPanel::RenderConverterSection) /
                       // Colorimetria / Canales de trabajo (placeholders todavia) / Audio
                       // (DAW real, ver AudioDawPanel) / Overlays (galeria+editor, ver
                       // OverlayLibraryTab) -- ABSORBE a los ex-presets "Render", "Audio" e
                       // "Imagen", que ya no existen como espacios de trabajo propios. El
                       // nombre del enum se deja "Video" para no romper el ToKey/FromString
                       // de settings.json ya guardados en disco.
    };

    const char*            WorkspaceLayoutPresetName(WorkspaceLayoutPreset preset);
    WorkspaceLayoutPreset  WorkspaceLayoutPresetFromString(const std::string& s);

    struct WorkspaceSettings {
        WorkspaceLayoutPreset layoutPreset = WorkspaceLayoutPreset::Classic;
    };

    // Valida que 'path' sea un archivo de fuente (.ttf/.otf) que ImGui pueda
    // parsear realmente, sin arriesgarse al IM_ASSERT fatal de
    // AddFontFromFileTTF ante un archivo inexistente/corrupto (ver
    // stbtt_InitFont, misma libreria que usa ImGui por debajo).
    bool IsValidFontFile(const std::string& path);

    // ── Actualizaciones ──────────────────────────────────────────────────
    struct UpdatesSettings {
        std::string currentVersion = PROYECTHOR_VERSION_STRING;
        std::string lastChecked    = "";
        std::string updateChannel  = "beta";
        bool        checkOnStartup = true;
        bool        autoDownload   = false;
    };

    // ── Stage Display (monitor de control) ──────────────────────────────
    struct StageDisplaySettings {
        int layoutTemplateIndex = 0; // indice en kStageLayoutTemplates
        int cellWidget[kStageMaxCells] = {
            (int)StageWidgetType::LiveText, (int)StageWidgetType::Clock, 0, 0
        };

        // Antes vivian como miembros efimeros de StageDisplayPanel (se
        // reseteaban a 0/false en cada arranque). Se persisten aca para que
        // el toggle "Stage" de ViewPanel (ver RenderLiveTransport/dots) los
        // pueda usar sin depender de una instancia de StageDisplayPanel.
        int  monitorIndex = -1;    // -1 = sin elegir aun -> default a la pantalla secundaria
        bool useLAN        = false;
        int  lanPort        = 8080;

        // Monitores de Stage ADICIONALES (opcional) -- ver comentario
        // equivalente en ProjectionSettings::extraMonitors.
        std::vector<int> extraMonitors;

        // Si esta activo, Stage ignora la grilla de celdas y muestra
        // exactamente lo mismo que el operador ve en "Vista en Vivo"
        // (fondo+overlay+texto) — ver UI::DrawStageContent.
        bool mirrorPublicOutput = false;
    };

    // ── Sidebar de Biblioteca (Letra/Video/Imagen/Biblia/Doc/Audio) ──────
    // Un color de identidad por categoria; el resto del look (fondo activo,
    // barra lateral, tinte de icono/label) se deriva de este en tiempo real
    // (ver LibrarySidebar.cpp). Los valores por defecto son los mismos tonos
    // que ya se usaban hardcodeados, para no cambiar nada hasta que el
    // usuario decida personalizar.
    // Indices 6/8/9 (Red/Render/Overlay) son un grupo aparte, separado
    // por una linea de las 6 categorias de contenido de arriba — ver
    // LibrarySidebar.cpp. Red se mudo desde ViewToolsSettings, mismo color
    // que tenia alli. Render (indice 8) se mudo desde la seccion
    // "Biblioteca" del workspace (LibraryManagerPanel, retirada), mismo
    // color que tenia ahi. Indice 9 fue Mobile (mudado a Ajustes >
    // Conexiones) y ahora es Overlay -- se reutiliza el slot, no se agrego
    // uno. Indice 7 (Reloj) quedo sin uso: el boton se saco del sidebar por
    // quedar duplicado con el toolbar inline de ViewPanel.
    struct LibrarySidebarSettings {
        float categoryColor[10][4] = {
            { 0.31f, 0.55f, 1.00f, 1.0f }, // Letra
            { 0.86f, 0.24f, 0.24f, 1.0f }, // Video
            { 0.24f, 0.86f, 0.39f, 1.0f }, // Imagen
            { 0.86f, 0.67f, 0.16f, 1.0f }, // Biblia
            { 0.65f, 0.31f, 0.94f, 1.0f }, // Documentos
            { 0.16f, 0.75f, 0.75f, 1.0f }, // Audio
            { 0.30f, 0.80f, 0.85f, 1.0f }, // Red
            { 0.95f, 0.75f, 0.20f, 1.0f }, // Reloj
            { 0.90f, 0.55f, 0.20f, 1.0f }, // Render
            { 0.90f, 0.40f, 0.70f, 1.0f }, // Overlay
        };
    };

    // ── Sidebar de Home (Home/Reloj/Anuncios/Notas/Captura/Transmision) ──
    // Mismo mecanismo que LibrarySidebarSettings: un color de identidad por
    // seccion, ver HomeSidebar.cpp.
    struct HomeSidebarSettings {
        float categoryColor[6][4] = {
            { 0.55f, 0.60f, 0.68f, 1.0f }, // Home
            { 0.95f, 0.75f, 0.20f, 1.0f }, // Reloj y Contadores
            { 0.45f, 0.60f, 1.00f, 1.0f }, // Anuncios
            { 0.35f, 0.80f, 0.55f, 1.0f }, // Notas Rapidas
            { 0.90f, 0.35f, 0.45f, 1.0f }, // Captura
            { 0.30f, 0.80f, 0.85f, 1.0f }, // Transmision en Red
        };
    };

    // ── Sidebar del hub de Control (Control/Stage Display) ───────────────
    struct ControlHubSettings {
        float categoryColor[2][4] = {
            { 0.40f, 0.55f, 0.95f, 1.0f }, // Control
            { 0.90f, 0.55f, 0.20f, 1.0f }, // Stage Display
        };
    };

    // ── Sidebar del hub de Diseño (Fondos/Estilos/Shaders/Transiciones/
    //    Anuncios/Captura) ────────────────────────────────────────────────
    struct StylesHubSettings {
        float categoryColor[6][4] = {
            { 0.35f, 0.80f, 0.55f, 1.0f }, // Fondos
            { 0.65f, 0.31f, 0.94f, 1.0f }, // Estilos
            { 0.40f, 0.75f, 0.85f, 1.0f }, // Shaders
            { 0.90f, 0.35f, 0.45f, 1.0f }, // Transiciones
            { 0.45f, 0.60f, 1.00f, 1.0f }, // Anuncios
            { 0.90f, 0.35f, 0.45f, 1.0f }, // Captura
        };
    };

    // ── Sidebar de Herramientas (debajo de Vista en Vivo): Control
    //    Overlays / Notas / Chat / Pads ─────────────────────────────────────
    // Red y Reloj se mudaron al sidebar de Biblioteca — ver
    // LibrarySidebarSettings::categoryColor (indices 6 y 7).
    struct ViewToolsSettings {
        float categoryColor[4][4] = {
            { 0.40f, 0.55f, 0.95f, 1.0f }, // Control Overlays
            { 0.35f, 0.80f, 0.55f, 1.0f }, // Notas
            { 0.75f, 0.40f, 0.90f, 1.0f }, // Chat
            { 0.90f, 0.55f, 0.20f, 1.0f }, // Pads
        };
    };

    // ── Escenas rápidas de Captura ────────────────────────────────────────
    // 8 botones de color: cada uno guarda una configuración completa de
    // captura (fuente + recuadro de posición libre + opacidad) para poder
    // saltar entre "escenas" con un click en vivo -- ver
    // CapturePanel::SaveCurrentAsScene/RecallScene. sourceType guarda el
    // valor numérico de ProyecThor::UI::CaptureSourceType -- no se usa ese
    // enum acá directo para no crear una dependencia de Settings (backend)
    // hacia CapturePanel (frontend/UI).
    struct CaptureSceneSettings {
        bool        assigned     = false;
        int         sourceType   = 0;
        int         sourceIndex  = -1;
        std::string sourceHandle;
        std::string sourceName;
        float       x0 = 0.25f, y0 = 0.25f, x1 = 0.75f, y1 = 0.75f;
        float       opacity = 1.0f;
    };
    static constexpr int kCaptureSceneCount = 8;
    struct CaptureSettings {
        CaptureSceneSettings scenes[kCaptureSceneCount];
    };

    // ── Pads de ViewTools ─────────────────────────────────────────────────
    // 8 botones tipo pad MIDI: cada uno guarda, de forma independiente,
    // una disposicion de Captura (mismos campos que CaptureSceneSettings —
    // ver CapturePanel::SnapshotCurrentCapture/ApplyCaptureScene) y un
    // snapshot directo del estilo+fondo que esta en pantalla en ese momento
    // (no una referencia por nombre a un estilo guardado). hasCapture/
    // hasStyle pueden faltar -- un pad no tiene por que tocar las dos cosas
    // a la vez. Nunca guarda la letra/texto en pantalla.
    struct PadSettings {
        bool assigned  = false;
        int  iconIndex = 0; // indice en la tabla fija de iconos, ver ViewToolsPanel.cpp

        bool                  hasCapture = false;
        CaptureSceneSettings  capture;

        bool        hasStyle = false;
        float       styleSize       = 60.0f;
        float       styleColor[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
        int         styleHAlign     = 1;
        int         styleVAlign     = 1;
        float       styleMargins[4] = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool        styleAutoScale  = true;
        std::string styleFontName   = "Predeterminada";
        int         bgType = 0; // espeja PresentationCore::PresentationState::BackgroundType
        std::string bgPath;
        float       bgColor[3] = { 0.0f, 0.0f, 0.0f };
    };
    static constexpr int kPadCount = 8;
    struct PadsSettings {
        PadSettings pads[kPadCount];
    };

    // ── Catalogo de transiciones guardadas (Diseño > Transiciones) ─────────
    // type: valor numerico de ProyecThor::UI::TransitionType -- no se usa
    // ese enum aca directo, mismo criterio que CaptureSceneSettings arriba
    // (backend/settings no depende de frontend/panels).
    struct TransitionPresetSettings {
        std::string name;
        int         type              = 1;    // TransitionType::Fade
        float       duration          = 1.0f;
        bool        affectsBackground = false;
        bool        affectsLyrics     = true;
    };
    struct TransitionSettings {
        std::vector<TransitionPresetSettings> presets;
    };

    // ── Yggdrasil: control de dispositivos externos (luces, etc.) por OSC ──
    // ProyecThor solo emite (no escucha) — ver OSCSender.h. Cada mensaje
    // guardado es una "cue" disparable a mano desde el panel: una direccion
    // OSC (ej. "/cue/1") mas los argumentos, escritos tal cual los tipearia
    // el operador (ver OSCSender::ParseOSCArgs para como se infiere el tipo
    // de cada uno al enviar).
    struct OSCMessageDef {
        std::string label   = "Luz 1";
        std::string address = "/cue/1";
        std::string argsText;  // ej. "1, 0.5, hola" -- vacio = sin argumentos

        // Ultimo resultado de envio (no persistido -- solo para que el
        // operador vea de un vistazo que luz esta respondiendo, ver
        // YggdrasilPanel). false + lastSentAt vacio = todavia no se probo.
        bool        lastSendOk = false;
        std::string lastSentAt;
    };

    // Vincula un parametro en vivo de ProyecThor (ver YggdrasilPanel::
    // GetBindableParams) a una direccion OSC entrante, aprendida con el
    // boton "Aprender" (se guarda la direccion del primer mensaje que
    // llega mientras esa fila esta en modo aprendizaje). paramName debe
    // matchear exactamente el "name" del registro de parametros.
    struct OSCBinding {
        std::string paramName;
        std::string oscAddress;
    };

    struct YggdrasilSettings {
        std::string targetIp    = "127.0.0.1";
        int         targetPort  = 9000;   // hacia donde se envia (luces)
        int         listenPort  = 9001;   // en donde se escucha (Control List)
        bool        autoListen  = false;  // arrancar la escucha sola al abrir la app
        std::vector<OSCMessageDef> messages;
        std::vector<OSCBinding>    bindings;
    };

    // ── Streaming en vivo (RTMP, ver BroadcastPanel/StreamEncoder) ───────
    // serverUrl + streamKey se concatenan como serverUrl + "/" + streamKey
    // para armar la URL RTMP final (mismo criterio que OBS: "Servidor" y
    // "Clave de stream" por separado, asi la clave no queda pegada a mano
    // en una URL larga). videoBitrateKbps sigue la misma convencion de
    // "kbps" que usan las plataformas de streaming (Twitch/YouTube).
    struct StreamingSettings {
        std::string serverUrl        = "rtmp://";
        std::string streamKey        = "";
        int         videoBitrateKbps = 4500;
        int         fps              = 30;
        int         width            = 1280;
        int         height           = 720;
    };

    // ── Sincronizacion LAN con ProyecThor Mobile (ver SyncServer/SyncPanel) ──
    // pairingPin se autogenera (6 digitos) la primera vez que se activa el
    // servidor si esta vacio -- ver SyncPanel::RenderServerControl. Es lo que
    // el celular manda en el header "X-Sync-Token" de cada request.
    struct SyncSettings {
        bool        enabled    = false;
        int         port       = 8090;
        std::string pairingPin = "";
    };

    // ── Asistente de IA (chat + edicion de canciones con confirmacion) ───
    // Por ahora solo Anthropic Claude (Messages API) -- pedido explicito de
    // arrancar con un solo proveedor; Gemini/ChatGPT quedan para una pasada
    // futura si hace falta (por eso no hay un enum de "proveedor" todavia,
    // seria una UI de elegir entre una sola opcion). apiKey vive en
    // settings.json igual que streamKey (ver StreamingSettings) -- ese
    // archivo ya esta en .gitignore por guardar credenciales de usuario.
    struct AISettings {
        bool        enabled = false;
        std::string apiKey  = "";
        std::string model   = "claude-sonnet-5";
    };

    // ── Almacenamiento y Carpetas de Datos (Ajustes > Datos) ───────────
    struct WatchedFolder {
        std::string path;
        bool        copyToDataDir = false; // false = reproducir original; true = copiar a AppData/carpeta de datos
        bool        enabled       = true;
    };

    struct StorageSettings {
        std::string                customDataRoot = "";
        std::vector<WatchedFolder> watchedFolders;
    };

    struct AppSettings {
        ProjectionSettings     projection;
        AudioSettings          audio;
        GeneralSettings        general;
        ThemeSettings          theme;
        WorkspaceSettings      workspace;
        UpdatesSettings        updates;
        StageDisplaySettings   stageDisplay;
        LibrarySidebarSettings librarySidebar;
        HomeSidebarSettings    homeSidebar;
        ControlHubSettings     controlHub;
        StylesHubSettings      stylesHub;
        ViewToolsSettings      viewTools;
        CaptureSettings        capture;
        PadsSettings           pads;
        YggdrasilSettings      yggdrasil;
        StreamingSettings      streaming;
        SyncSettings           sync;
        TransitionSettings     transitions;
        AISettings             ai;
        StorageSettings        storage;
    };

    class SettingsManager {
    public:
        static SettingsManager& Get() {
            static SettingsManager instance;
            return instance;
        }

        AppSettings&       GetSettings()       { return m_Settings; }
        const AppSettings& GetSettings() const { return m_Settings; }

        void SaveSettings();
        void LoadSettings();
        void ResetToDefaults() { m_Settings = AppSettings{}; }
        void Save() { SaveSettings(); }

        // Aplica el tema activo (m_Settings.theme) a todo ImGui y a
        // DesignSystem (paneles "glass"). Se llama al iniciar y al guardar.
        void ApplyTheme();

        // Aplica un preset y lo deja como tema activo (sin guardar a disco).
        // Para Mek, ademas intenta usar la fuente de waybar (Linux) -- ver
        // implementacion en SettingsManager.cpp.
        void ApplyPreset(ThemePreset preset);

        void ApplyProjection();

        // Pedido de reinicio (p.ej. tras elegir una fuente nueva -- ver
        // CategoryTheme.cpp). NO reinicia nada por si solo: solo levanta la
        // bandera; el loop principal en main.cpp la revisa cada frame y
        // cierra la ventana normalmente (glfwSetWindowShouldClose), asi
        // corre TODO el shutdown existente (VLC, GL, ImGui) antes de
        // relanzar el proceso -- ver RestartApplication().
        void RequestRestart()        { m_RestartRequested = true; }
        bool IsRestartRequested() const { return m_RestartRequested; }

    private:
        SettingsManager()                                  = default;
        ~SettingsManager()                                 = default;
        SettingsManager(const SettingsManager&)            = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        AppSettings m_Settings;
        bool        m_RestartRequested = false;
    };

    // Relanza el ejecutable actual como un proceso nuevo e independiente.
    // Se debe llamar SOLO despues de que el proceso actual ya termino su
    // shutdown limpio (ImGui/GLFW/VLC ya destruidos) -- ver el final de
    // main(). En Linux usa fork()+exec() (el padre no hace exit() acá, eso
    // lo hace el return normal de main()); en Windows, CreateProcess.
    void RestartApplication();

} // namespace ProyecThor::Settings

#endif // PROYECTHOR_SETTINGS_SETTINGS_MANAGER_H