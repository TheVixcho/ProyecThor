#include "Hub.h"
#include <GL/glew.h>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <filesystem>
#include "settings/SettingsManager.h"
#include "../external/tools/OpenURL.h"
#include "Version.h"
#include "DesignSystem.h"
#include "HubTheme.h"
#include "SongPlayStats.h"
#include "FilePicker.h"

extern GLuint LoadTextureFromFile(const char* filename);

static constexpr float HUB_APPEAR_SPD  = 3.0f;

namespace DS = ProyecThor::UI::DS;
namespace HT = ProyecThor::UI::HubTheme;

namespace ProyecThor::UI {

static ImU32 ColA(ImU32 col, int alpha) {
    alpha = std::clamp(alpha, 0, 255);
    const float styleAlpha = ImGui::GetStyle().Alpha;
    alpha = static_cast<int>(static_cast<float>(alpha) * styleAlpha);
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}
static ImU32 ColAf(ImU32 col, float alpha01) {
    return ColA(col, static_cast<int>(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f));
}

static float HubHoverLerp(ImGuiID id, bool hovered, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* pT = storage->GetFloatRef(id ^ 0x48554248u, 0.0f);
    const float target = hovered ? 1.0f : 0.0f;
    *pT += (target - *pT) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *pT;
}

struct UpdateVersionInfo {
    int         id;
    const char* version;
    const char* modalBadge;
    const char* cardBadge;
    const char* coverFile;                       
    const char* summary;
};

static const std::vector<UpdateVersionInfo> kUpdateRegistry = {
    {
        16, "0.7.0",
        "ACTUALIZACION ESTABLE", "ACTUALIZACION ESTABLE",
        "bin/assets/ui/textures/iniciarpro.jpg",
        "Lanzamiento oficial de ProyecThor v0.7.0: Nuevo fondo dinámico con ondas topográficas fluidas y partículas astrales, "
        "HUD estilizado con tarjetas de acción, panel Web vertical integrado con envío a pantalla pública, barra de filtros "
        "multimedia con iconos vectoriales de alta precisión, importación múltiple de archivos a la vez, nueva categoría Datos en Ajustes "
        "con carpetas de importe automático, intercambio directo y Drag & Drop entre Fondos y Multimedia, biblioteca de Notas Rápidas permanente, "
        "letrero de Anuncios sincronizado, reloj LAN independiente, Asistente de IA fluido (Claude/ChatGPT/Gemini) y eliminación total de "
        "oscurecimiento en público."
    },
    {
        15, "0.7.0-beta.1",
        "BETA", "BETA",
        "splash_bg6.jpg",
        "Asistente de IA en la toolbar de abajo: modo Básica (Claude/ChatGPT/Gemini "
        "en un navegador embebido de verdad, inicia sesion normal, ProyecThor no ve "
        "tu clave) y modo Avanzada (tu propia API key de Claude, puede listar/crear/"
        "editar canciones de la Biblioteca, siempre pide confirmacion antes de "
        "guardar algo). Espacio de trabajo \"Video\" renombrado a \"Producción\": "
        "ahora un rail izquierdo estilo Biblioteca con Render (conversor de formato), "
        "Audio (DAW real: grabar microfono, cortar/mover clips en la linea de tiempo, "
        "reproducir todas las pistas juntas, exportar a WAV/MP3/AAC/OGG) y Overlays "
        "(galeria+editor). Espacio de trabajo \"Transmisión\" simplificado a solo eso "
        "(sin Biblioteca/Home/Diseño de por medio) con una lista de capas real -- "
        "Captura, un Overlay guardado, o la salida en vivo al Público -- para elegir "
        "que se transmite. Nuevo panel \"Web\" en la Biblioteca (junto a Render/"
        "Overlay): navegador embebido con \"Enviar a Público\" para mostrar cualquier "
        "pagina directo en la salida real. La Inalámbrica (LAN) ahora se puede clavar "
        "en \"Solo reloj\" o \"En blanco\" mientras Público/Stage siguen con lo que "
        "este en vivo, elegible desde el selector de vista de Vista en Vivo. Cola del "
        "Monitor con botones animados de verdad (antes planos) y el boton Loop "
        "arreglado (no volvia a repetir el video). Overlay editor: arreglado un "
        "warning de Dear ImGui en la lista de capas."
    },
    {
        14, "0.6.0",
        "ACTUALIZACION ESTABLE", "ACTUALIZACION ESTABLE",
        "splash_bg6.jpg",
        "Notas Rapidas renovado: ahora guarda el texto solo mientras escribis y lo recupera "
        "al reabrir la ventana (nunca mas perder lo que ibas tipeando), respeta el tema "
        "activo elegido en Apariencia, y queda disponible tanto desde el Hub como "
        "proyectando sin cortarse -- atajo nuevo Shift+Z para abrirlo/cerrarlo. Atajos "
        "Alt Gr+1/2/3/4 para colapsar y expandir Biblioteca/Diseño/Vista en Vivo/Home con "
        "una animacion prolija (Alt Gr+0 restablece el entorno completo). Seccion "
        "Multimedia de Biblioteca renombrada a \"Medios\", con vista en cuadricula de "
        "miniaturas grandes ademas de la lista de siempre, e iconos reales por tipo "
        "(video/audio/imagen) en vez de letras sueltas. Nueva categoria Ajustes > "
        "Apariencia > Entorno de trabajo, con 3 disposiciones de paneles para elegir "
        "(Clasico de siempre, Simple al estilo Holyrics con cuatro columnas, y Transmision "
        "con Vista en Vivo como franja superior completa) -- los paneles detectan solos si "
        "quedaron en vertical u horizontal y se acomodan. El reproductor de Audio ahora "
        "puede traer la letra de una cancion pegando la URL de un video (yt-dlp por "
        "debajo) y proyectarla en vivo con un boton de mostrar/ocultar, guardada para esa "
        "pista. ProyecThor ahora se puede abrir con doble click o \"Abrir con\" sobre un "
        "archivo de audio o video (Windows y Linux): lo importa a tu biblioteca y lo deja "
        "listo en Preview, saltando el splash. Nuevo entorno de trabajo \"Biblioteca\" "
        "(junto a Clasico/Simple/Transmision) con solo Biblioteca y Home a la vista, "
        "elegible desde Ajustes o el menu Espacio de trabajo, y recordado entre sesiones. "
        "El Preview ahora tiene un boton de pantalla completa de verdad -- oculta hasta la "
        "barra superior y activa el fullscreen del sistema como F11 -- con controles que "
        "se ocultan solos, volumen propio (mudo por default para no duplicar audio con lo "
        "que ya suena en vivo) y una opcion de FSR para que el video se vea nitido al "
        "agrandarse. Ajustes > Actualizaciones suma \"Desinstalar versiones anteriores\" "
        "(limpia instalaciones viejas sueltas del sistema) y \"Carpeta de datos\" (mover "
        "donde vive tu biblioteca a otro disco, sin perder nada de lo que ya tenias). "
        "Abrir otro archivo con \"Abrir con ProyecThor\" mientras la app ya esta corriendo "
        "ahora lo manda a esa misma ventana en vez de abrir una segunda instancia."
    },
    {
        13, "0.6.0-beta.1",
        "BETA", "BETA",
        "splash_bg6.jpg",
        "Editor de overlays completo en la app movil (mover, redimensionar, rotar, "
        "seleccion multiple con guias de iman, Borrador y Degradado, exportar y "
        "enviar a la PC), panel Render renovado en Biblioteca (codecs H.264/H.265/"
        "VP9/AV1, control de compresion, cancelar a mitad de camino, barra de "
        "progreso real, estimacion y comparacion de peso, elegir donde guardar), "
        "soporte real para Linux/CachyOS (paquete de Arch validado por CI, Wayland "
        "via XWayland), la app ahora respeta el escalado de pantalla de Windows "
        "(150%, etc), Ajustes con una nueva categoria \"Conexiones\" propia y "
        "Actualizaciones simplificado, el Editor de Estilos de Letra renovado "
        "por completo con recuadros de Letras e Indice independientes a pantalla "
        "completa, y efectos de texto nuevos (3D, degradado de color, transparencia "
        "con angulo). Actualizacion grande todavia en curso: revisa el detalle "
        "completo antes de considerarla cerrada."
    },
    {
        12, "0.5.1",
        "BETA", "BETA",
        "splash_bg5.jpg",
        "Reloj y Contadores ahora es solo \"Contadores\". Nuevo cuadro de reloj dentro del "
        "editor de Overlays: lo posicionas y le das estilo una sola vez, y se reemplaza en vivo "
        "por la hora/cronometro activo — la transmision a pantalla ahora depende de que overlay "
        "tengas activo, en vez de un modo aparte. Overlays con reordenar capas y overlays de "
        "reloj predeterminados listos para probar. Corregido un bug por el cual el cuadriculado "
        "de \"sin fondo\" del editor de Overlays podia quedar horneado como fondo opaco al "
        "guardar."
    },
    {
        11, "0.5.0",
        "ACTUALIZACION ESTABLE", "ACTUALIZACION ESTABLE",
        "splash_bg5.jpg",
        "Ajustes reorganizado por completo: cada configuracion ahora es su propia pagina, con "
        "buscador incluido, Proyeccion y Pantallas agrupadas juntas, y Red/Mobile/Streaming/OSC "
        "viviendo dentro de Proyeccion. Nueva opcion \"Bucle falso\" para Fondos, que reproduce "
        "hacia adelante y hacia atras en vez de cortar siempre al mismo frame. Nueva seccion de "
        "Overlays: crea textos, formas e imagenes en un editor a pantalla completa y proyectalos "
        "como una capa transparente encima del fondo y la letra (antes tapaban el fondo por "
        "error). Vista en Vivo renovada: reproductor mas simple, Overlays/Chat/Pads/Reloj ahora "
        "se abren dentro del mismo panel en vez de ventanas flotantes sueltas. Corregidos varios "
        "colores que quedaban fijos sin importar el tema elegido y los fondos de los paneles "
        "ahora son solidos en vez de verse transparentes."
    },
    {
        10, "0.4.3",
        "BETA", "BETA",
        "bg_splash3.jpg",
        "Nueva seccion Conexiones (OSC, Red, Chat y Streaming en vivo por RTMP), nueva "
        "Biblioteca para gestionar tus archivos con conversor de formato incluido, "
        "Biblia a pantalla completa, selector rapido (Alt+Espacio), Monitor de Vista "
        "en Vivo mas compacto, editor de Estilos renovado, nuevo instalador para "
        "Windows, Biblioteca con Biblias y cancion de bienvenida incluidas de entrada, "
        "corregido el titulo de las canciones al guardarlas, y varias correcciones de "
        "estabilidad."
    },
    {
        9, "0.4.2",
        "BETA", "BETA",
        "bg_splash3.jpg",
        "Pads de Vista en Vivo arreglados y renovados con escenas de Captura sincronizadas, "
        "transporte y volumen rediseñados tipo consola/MIDI, buscador de versiculos por "
        "palabras en la Biblia, editor de Estilos acoplado dentro de Home con selector de "
        "fuentes en grilla y nuevos efectos de texto (fondo, borde, sombra, glow, neon, "
        "subrayado), y un monton de efectos nuevos en Shaders: NIS (NVIDIA), VHS, Cine, "
        "Contraste, Luminosidad, Blur, Sharpen, Bloom, Aberracion cromatica y TAA."
    },
    {
        8, "0.4.1",
        "BETA", "BETA",
        "bg_splash3.jpg",
        "Nuevo panel de Shaders (FSR, CRT, grano, saturacion, vinetado y "
        "relleno desenfocado tipo Smart TV) para el video de fondo, miniaturas "
        "y vista en grilla/lista en Biblioteca > Videos, escenas rapidas "
        "guardadas para Captura, fuente de interfaz personalizable, un "
        "motor de renderizado alternativo (libvlc en ventana nativa) para "
        "videos, editor de canciones rediseñado por completo y menu "
        "principal reorganizado, con una correccion importante de "
        "sincronizacion de audio/video en equipos de bajos recursos."
    },
    {
        7, "0.4.0",
        "ACTUALIZACION ESTABLE", "ACTUALIZACION ESTABLE",
        "bg_splash3.jpg",
        "Cola de videos mucho mas estable, nueva seccion de Overlays, "
        "Vista en Vivo con acciones rapidas, panel de Rendimiento y un "
        "rediseño mas compacto de Fondos y Estilos."
    },
    {
        6, "0.3.5",
        "BETA", "BETA",
        "splash_bg1.jpg",
        "Version estable: Audio Rework completo, biblioteca renovada con sistema de "
        "etiquetas, soporte oficial para Linux, estadisticas locales, atajos de "
        "teclado globales y mejoras de estabilidad en toda la aplicacion."
    },
    {
        2, "0.3.0",
        "ACTUALIZACION ESTABLE", "ACTUALIZACION ESTABLE",
        "splash_bg1.jpg",
        "Nuevas herramientas de transmision, optimizaciones y estabilidad de red."
    },
};

static const UpdateVersionInfo* FindUpdateVersion(int id) {
    for (const auto& v : kUpdateRegistry)
        if (v.id == id) return &v;
    return kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
}

struct GLTextureInfo {
    GLuint id     = 0;
    int    width  = 0;
    int    height = 0;
};

static GLTextureInfo GetCoverTexture(const char* filename) {
    static std::unordered_map<std::string, GLTextureInfo> s_Cache;
    auto it = s_Cache.find(filename);
    if (it != s_Cache.end() && it->second.id != 0)
        return it->second;

    GLTextureInfo info;
    info.id = LoadTextureFromFile(filename);

    if (info.id == 0) {
        std::vector<std::string> fallbacks = {
            std::string("assets/") + filename,
            std::string("bin/") + filename,
            std::string("assets/bin/assets/ui/textures/") + filename,
            std::string("bin/assets/ui/textures/") + filename,
            std::string("assets/ui/textures/") + filename
        };
        std::string base = std::filesystem::path(filename).filename().string();
        fallbacks.push_back(base);
        fallbacks.push_back(std::string("assets/") + base);
        fallbacks.push_back(std::string("bin/assets/ui/textures/") + base);
        fallbacks.push_back(std::string("assets/bin/assets/ui/textures/") + base);
        fallbacks.push_back(std::string("assets/ui/textures/") + base);

        for (const auto& fb : fallbacks) {
            info.id = LoadTextureFromFile(fb.c_str());
            if (info.id != 0) break;
        }
    }

    if (info.id != 0) {
        glBindTexture(GL_TEXTURE_2D, info.id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &info.width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &info.height);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    s_Cache[filename] = info;
    return info;
}

struct ParallaxState { float ox = 0.0f, oy = 0.0f, zoom = 1.0f; };

static std::unordered_map<std::string, ParallaxState>& GetParallaxStates() {
    static std::unordered_map<std::string, ParallaxState> s_States;
    return s_States;
}

static void DrawCoverImageCover(ImDrawList* dl, GLuint texId, int texW, int texH,
                                 ImVec2 pMin, ImVec2 pMax,
                                 float rounding, ImDrawFlags roundFlags,
                                 const char* stateKey, float dt,
                                 bool hovered, float maxZoom, float followSpeed = 9.0f)
{
    if (texId == 0 || texW <= 0 || texH <= 0) {
        dl->AddRectFilled(pMin, pMax, ColA(HT::CardAlt, 255), rounding, roundFlags);
        return;
    }

    ParallaxState& st = GetParallaxStates()[stateKey];

    const float boxW = std::max(1.0f, pMax.x - pMin.x);
    const float boxH = std::max(1.0f, pMax.y - pMin.y);

    float targetZoom = hovered ? maxZoom : 1.0f;
    float targetOX   = 0.0f, targetOY = 0.0f;
    if (hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        targetOX = std::clamp(((mouse.x - pMin.x) / boxW) * 2.0f - 1.0f, -1.0f, 1.0f);
        targetOY = std::clamp(((mouse.y - pMin.y) / boxH) * 2.0f - 1.0f, -1.0f, 1.0f);
    }

    const float t = std::clamp(dt * followSpeed, 0.0f, 1.0f);
    st.zoom += (targetZoom - st.zoom) * t;
    st.ox   += (targetOX   - st.ox)   * t;
    st.oy   += (targetOY   - st.oy)   * t;

    const float boxAspect = boxW / boxH;
    const float imgAspect = static_cast<float>(texW) / static_cast<float>(texH);

    float baseUW, baseUH;
    if (imgAspect > boxAspect) {
        baseUH = 1.0f;
        baseUW = boxAspect / imgAspect;
    } else {
        baseUW = 1.0f;
        baseUH = imgAspect / boxAspect;
    }

    const float zoom = std::max(1.0f, st.zoom);
    const float uw = baseUW / zoom;
    const float uh = baseUH / zoom;

    const float marginX = std::max(0.0f, (1.0f - uw) * 0.5f);
    const float marginY = std::max(0.0f, (1.0f - uh) * 0.5f);

    const float centerU = 0.5f + st.ox * marginX;
    const float centerV = 0.5f + st.oy * marginY;

    const ImVec2 uv0(centerU - uw * 0.5f, centerV - uh * 0.5f);
    const ImVec2 uv1(centerU + uw * 0.5f, centerV + uh * 0.5f);

    const int imgAlpha = static_cast<int>(std::clamp(ImGui::GetStyle().Alpha, 0.0f, 1.0f) * 255.0f);
    dl->AddImageRounded((ImTextureID)(intptr_t)texId, pMin, pMax, uv0, uv1,
        IM_COL32(255, 255, 255, imgAlpha), rounding, roundFlags);
}

static float EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

static const char* kHeroCardTextureFile   = "bin/assets/ui/textures/iniciarpro.jpg";
static const char* kConfigCardTextureFile = "bin/assets/ui/textures/20260524_104505.jpg";

Hub::Hub() : m_LastFrameTime(std::chrono::steady_clock::now()) {
    const auto& settings = ProyecThor::Settings::SettingsManager::Get().GetSettings();
    m_SelectedMonitor = settings.projection.targetMonitor;
}

Hub::~Hub() {
    if (m_DownloadSubsThread.joinable())
        m_DownloadSubsThread.join();
}

void Hub::ForceOpen() {
    m_Open                  = true;
    m_Appearing             = true;
    m_AppearProgress        = 0.0f;
    m_LaunchRequested       = false;
    m_OpenSettingsRequested = false;
    m_LastFrameTime         = std::chrono::steady_clock::now();
}

void Hub::UpdateAnimations(float dt) {
    if (m_Appearing) {
        m_AppearProgress += dt * HUB_APPEAR_SPD;
        if (m_AppearProgress >= 1.0f) {
            m_AppearProgress = 1.0f;
            m_Appearing      = false;
        }
    }
}

static void DrawSectionHeader(const char* title, float width) {
    ImGui::SetWindowFontScale(1.2f);
    ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
        ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + width, p.y + 3.5f),
        ColAf(HT::AccentBlue, 0.40f), ColAf(HT::AccentSoft, 0.10f),
        ColAf(HT::AccentSoft, 0.10f), ColAf(HT::AccentBlue, 0.40f));
    ImGui::Dummy(ImVec2(0.0f, 13.0f));
}

void Hub::RenderNovedadesPanel() {
    const float target = m_NovedadesOpen ? 1.0f : 0.0f;
    m_NovedadesAnim += (target - m_NovedadesAnim) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    m_NovedadesAnim = std::clamp(m_NovedadesAnim, 0.0f, 1.0f);
    if (m_NovedadesAnim < 0.001f) m_NovedadesAnim = 0.0f;

    if (!m_NovedadesOpen && m_NovedadesAnim <= 0.0f) return;

    ImGuiViewport* vp    = ImGui::GetMainViewport();
    const float    fadeA = EaseOut(m_NovedadesAnim);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeA);

    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 170));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##NovedadesDim", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    const float scale  = 0.96f + 0.04f * fadeA;
    const float panelW = 720.0f * scale, panelH = 680.0f * scale;

    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ColA(HT::Card, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,   HT::Divider);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   HT::RadiusLg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(28.0f, 24.0f));

    bool vis = ImGui::Begin("##NovedadesPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove);

    if (vis) {
        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            m_NovedadesOpen = false;

        const float headerW = ImGui::GetContentRegionAvail().x;

        ImGui::SetWindowFontScale(1.3f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::TextUnformatted("Novedades");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);

        ImGui::SameLine(headerW - 64.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
        ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
        if (ImGui::Button("Cerrar##novedades", ImVec2(64.0f, 28.0f)))
            m_NovedadesOpen = false;
        ImGui::PopStyleColor(4);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        const UpdateVersionInfo* latest = kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
        if (latest) {
            const GLTextureInfo heroCover = GetCoverTexture(latest->coverFile);
            const float heroW = headerW, heroH = 175.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::CardAlt);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusLg);
            ImGui::BeginChild("##NovedadesHero", ImVec2(heroW, heroH), false, ImGuiWindowFlags_NoScrollbar);

            ImDrawList* hdl  = ImGui::GetWindowDrawList();
            const ImVec2 hMin = ImGui::GetWindowPos();
            const ImVec2 hMax = ImVec2(hMin.x + heroW, hMin.y + heroH);
            const bool heroHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            if (heroCover.id != 0) {
                DrawCoverImageCover(hdl, heroCover.id, heroCover.width, heroCover.height,
                    hMin, hMax, HT::RadiusLg, ImDrawFlags_RoundCornersAll,
                    "novedades_hero", ImGui::GetIO().DeltaTime, heroHovered, 1.05f);
            } else {
                hdl->AddRectFilled(hMin, hMax, ColA(HT::CardAlt, 255), HT::RadiusLg);
            }

            hdl->AddRectFilledMultiColor(
                ImVec2(hMin.x, hMin.y + heroH * 0.15f), hMax,
                IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 230), IM_COL32(0, 0, 0, 230));

            ImGui::SetCursorPos(ImVec2(20.0f, 18.0f));
            ImGui::BeginGroup();

            auto Pill = [&](const char* text, ImU32 bg, ImU32 fg) {
                ImGui::SetWindowFontScale(0.80f);
                const ImVec2 bs = ImGui::CalcTextSize(text);
                ImGui::SetWindowFontScale(1.0f);
                const ImVec2 pad(8.0f, 3.0f);
                const ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(bp.x - pad.x, bp.y - pad.y), ImVec2(bp.x + bs.x + pad.x, bp.y + bs.y + pad.y),
                    bg, HT::RadiusSm);
                ImGui::Dummy(ImVec2(pad.x, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, fg);
                ImGui::SetWindowFontScale(0.80f); ImGui::TextUnformatted(text); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, pad.x + 4.0f);
            };

            Pill(latest->modalBadge, HT::AccentBlue, HT::OnAccent);
            Pill("VERSIÓN ACTUAL", HT::SurfaceHover, HT::TextPri);
            ImGui::NewLine();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::SetWindowFontScale(1.4f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::Text("Actualización v%s", latest->version);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + heroW - 220.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ColA(HT::TextPri, 210));
            std::string heroSummary = latest->summary;
            if (heroSummary.size() > 220) {
                size_t p = heroSummary.find('.', 160);
                if (p != std::string::npos && p < 230) heroSummary = heroSummary.substr(0, p + 1);
                else heroSummary = heroSummary.substr(0, 190) + "...";
            }
            ImGui::TextWrapped("%s", heroSummary.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();

            ImGui::EndGroup();

            ImGui::SetCursorPos(ImVec2(heroW - 190.0f, heroH - 50.0f));
            ImGui::PushStyleColor(ImGuiCol_Button,        HT::AccentBlue);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::AccentSoft);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::AccentBlue);
            ImGui::PushStyleColor(ImGuiCol_Text,          HT::OnAccent);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);
            if (ImGui::Button("Ver todo el detalle >", ImVec2(170.0f, 34.0f))) {
                m_SelectedUpdateVer = latest->id;
                m_IsUpdateModalOpen = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, 18.0f));
        DrawSectionHeader("Historial de versiones", headerW);

        auto RenderUpdateCard = [&](const UpdateVersionInfo& info) {
            const GLTextureInfo cardCover = GetCoverTexture(info.coverFile);
            const float cardH = 115.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, HT::CardAlt);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, HT::RadiusMd);
            ImGui::BeginChild(info.version, ImVec2(headerW, cardH), false, ImGuiWindowFlags_NoScrollbar);

            ImVec2 cardStartPos = ImGui::GetCursorScreenPos();
            ImVec2 cardEndPos   = ImVec2(cardStartPos.x + headerW, cardStartPos.y + cardH);
            const bool cardHovered = ImGui::IsMouseHoveringRect(cardStartPos, cardEndPos);
            const float hoverT = HubHoverLerp(ImGui::GetID(info.version), cardHovered);

            ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));
            ImGui::BeginGroup();

            const float thumbW = 150.0f, thumbH = 95.0f;
            if (cardCover.id != 0) {
                const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
                const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbW, thumbMin.y + thumbH);

                char stateKey[96];
                snprintf(stateKey, sizeof(stateKey), "card_%s", info.version);

                DrawCoverImageCover(ImGui::GetWindowDrawList(), cardCover.id, cardCover.width, cardCover.height,
                    thumbMin, thumbMax, HT::RadiusMd, ImDrawFlags_RoundCornersAll,
                    stateKey, ImGui::GetIO().DeltaTime, cardHovered, 1.08f);

                ImGui::Dummy(ImVec2(thumbW, thumbH));
                ImGui::SameLine(0.0f, 14.0f);
            }

            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, HT::AccentSoft);
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextUnformatted(info.cardBadge);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SetWindowFontScale(1.15f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::Text("Versión v%s", info.version);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
            const float textMaxW = headerW - (cardCover.id != 0 ? (thumbW + 40.0f) : 30.0f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textMaxW);

            std::string summarySnippet = info.summary;
            if (summarySnippet.size() > 160) {
                size_t p = summarySnippet.find('.', 110);
                if (p != std::string::npos && p < 170) summarySnippet = summarySnippet.substr(0, p + 1);
                else summarySnippet = summarySnippet.substr(0, 145) + "...";
            }
            ImGui::TextWrapped("%s", summarySnippet.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::EndGroup();

            ImGui::EndGroup();

            ImGui::SetCursorScreenPos(cardStartPos);
            if (ImGui::InvisibleButton(info.version, ImVec2(headerW, cardH))) {
                m_SelectedUpdateVer = info.id;
                m_IsUpdateModalOpen = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            ImDrawList* cardDl = ImGui::GetWindowDrawList();
            if (hoverT > 0.001f) {
                cardDl->AddRectFilled(cardStartPos, cardEndPos,
                    ColAf(HT::TextPri, 0.04f * hoverT), HT::RadiusMd);
                cardDl->AddRectFilled(cardStartPos, ImVec2(cardStartPos.x + 3.5f, cardEndPos.y),
                    ColAf(HT::AccentBlue, hoverT), HT::RadiusMd, ImDrawFlags_RoundCornersLeft);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
        };

        const float historyH = ImGui::GetContentRegionAvail().y;
        ImGui::BeginChild("##NovedadesHistory", ImVec2(headerW, historyH), false);
        for (const auto& info : kUpdateRegistry)
            RenderUpdateCard(info);
        ImGui::EndChild();
    }

    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(2);
}

bool Hub::Render() {
    if (!m_Open) return false;

    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_LastFrameTime).count();
    m_LastFrameTime = now;
    dt = std::min(dt, 0.05f);

    m_Time += dt;

    static int fpsFrames = 0;
    static float fpsAccum = 0.0f;
    fpsFrames++;
    fpsAccum += dt;
    if (fpsAccum >= 0.5f) {
        const int fps = std::max(1, static_cast<int>(fpsFrames / fpsAccum));
        ProyecThor::UI::RecordPerformanceSample(fps);
        fpsFrames = 0;
        fpsAccum = 0.0f;
    }

    UpdateAnimations(dt);

    if (!m_IsUpdateModalOpen && !m_NovedadesOpen && !m_DownloadSubsOpen) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false))
            m_LaunchRequested = true;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false) || ImGui::IsKeyPressed(ImGuiKey_C, false))
            m_OpenSettingsRequested = true;
    }

    if (!m_IsUpdateModalOpen && !m_DownloadSubsOpen && ImGui::IsKeyPressed(ImGuiKey_N, false))
        m_NovedadesOpen = !m_NovedadesOpen;

    ImGuiViewport* vp = ImGui::GetMainViewport();

    if (!m_BgParticlesInit)
        InitBgParticles(vp->WorkSize.x, vp->WorkSize.y);

    UpdateBgParticles(dt, vp->WorkSize.x, vp->WorkSize.y);
    UpdateNebulas(dt, vp->WorkSize.x, vp->WorkSize.y);

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    constexpr ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;

    ImGui::Begin("##HubRoot", nullptr, rootFlags);

    const float appearA = EaseOut(m_AppearProgress);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, appearA);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      wp = ImGui::GetWindowPos();

    dl->AddRectFilled(wp, ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y), ColAf(HT::BgMain, appearA));
    RenderBgCanvas(dl, wp, vp->WorkSize.x, vp->WorkSize.y);

    static GLuint s_HubBgTex      = 0;
    static bool   s_HubBgTexTried = false;
    if (!s_HubBgTexTried) {
        s_HubBgTexTried = true;
        s_HubBgTex      = LoadTextureFromFile("splash_bg2.jpg");
    }
    if (s_HubBgTex != 0)
        dl->AddImage((ImTextureID)(intptr_t)s_HubBgTex, wp, ImVec2(wp.x + vp->WorkSize.x, wp.y + vp->WorkSize.y),
            ImVec2(0, 0), ImVec2(1, 1), ColAf(IM_COL32_WHITE, HT::BgImageAlpha));

    RenderContent(vp->WorkSize.x, vp->WorkSize.y);

    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);

    RenderNovedadesPanel();
    RenderUpdateDetailModal();
    RenderDownloadSubtitlesPanel();

    if (m_LaunchRequested) {
        m_LaunchRequested = false;
        m_Open            = false;
        return true;
    }

    return false;
}

void Hub::RenderContent(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##HubContent", ImVec2(w, h), false, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Dimensiones responsivas y centradas ──────────────────────────────
    const float contentW = std::clamp(w * 0.74f, 760.0f, 980.0f);
    const float contentX = (w - contentW) * 0.5f;

    const float totalH = 580.0f;
    const float startY = std::max(18.0f, (h - totalH) * 0.36f);

    ImGui::SetCursorPos(ImVec2(contentX, startY));
    ImGui::BeginGroup();

    // Factor de respiración armónico suave (ciclo de ~3.2 segundos)
    const float breathe = 0.5f + 0.5f * sinf(m_Time * 1.95f);

    // ── 1. Cabecera con Branding y Versión ───────────────────────────────
    {
        ImFont* font = ImGui::GetFont();
        const float logoFontSize = ImGui::GetFontSize() * 1.65f;

        const ImVec2 sizeProyec = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Proyec");
        const ImVec2 sizeThor   = font->CalcTextSizeA(logoFontSize, FLT_MAX, 0.0f, "Thor");
        const float  totalLogoW = sizeProyec.x + sizeThor.x;

        ImGui::SetCursorPosX(contentX + (contentW - totalLogoW) * 0.5f);
        const ImVec2 logoScreenPos = ImGui::GetCursorScreenPos();
        const ImVec2 posProyec = logoScreenPos;
        const ImVec2 posThor   = ImVec2(logoScreenPos.x + sizeProyec.x, logoScreenPos.y);

        dl->AddText(font, logoFontSize, posProyec, HT::TextPri, "Proyec");
        dl->AddText(font, logoFontSize, posThor,   HT::AccentSoft, "Thor");

        ImGui::Dummy(ImVec2(totalLogoW, logoFontSize));

        // Subtítulo con tag y versión
        const std::string tagText = "Software profesional de proyección y producción multimedia";
        const ImVec2 tagSz = ImGui::CalcTextSize(tagText.c_str());

        const std::string verText = std::string("v") + PROYECTHOR_VERSION_STRING;
        const ImVec2 verSz = ImGui::CalcTextSize(verText.c_str());

        const float rowW = tagSz.x + verSz.x + 24.0f;
        ImGui::SetCursorPosX(contentX + (contentW - rowW) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted(tagText.c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 10.0f);

        // Badge de versión limpio
        const ImVec2 badgePos = ImGui::GetCursorScreenPos();
        const ImVec2 badgePad(8.0f, 3.0f);
        const ImVec2 badgeMin(badgePos.x, badgePos.y - 1.0f);
        const ImVec2 badgeMax(badgePos.x + verSz.x + badgePad.x * 2.0f, badgePos.y + verSz.y + badgePad.y * 2.0f);
        dl->AddRectFilled(badgeMin, badgeMax, ColA(HT::CardAlt, 230), HT::RadiusSm);
        dl->AddRect(badgeMin, badgeMax, ColAf(HT::AccentBlue, 0.35f), HT::RadiusSm, 0, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(badgePos.x + badgePad.x, badgePos.y + badgePad.y - 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::AccentSoft);
        ImGui::TextUnformatted(verText.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // ── 2. Fila Principal: 2 Tarjetas Verticales de Acción (Deadlock Hero Cards) ──
    const float mainGap = 20.0f;
    const float cardW   = (contentW - mainGap) * 0.5f;
    const float cardH   = 320.0f;

    const ImVec2 row1Pos = ImGui::GetCursorScreenPos();

    // ── Helper para dibujar brackets estilizados en las esquinas ──
    auto DrawCornerBrackets = [&](const ImVec2& min, const ImVec2& max, ImU32 col, float len = 12.0f, float th = 1.8f) {
        // Top-left
        dl->AddLine(min, ImVec2(min.x + len, min.y), col, th);
        dl->AddLine(min, ImVec2(min.x, min.y + len), col, th);
        // Top-right
        dl->AddLine(ImVec2(max.x, min.y), ImVec2(max.x - len, min.y), col, th);
        dl->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + len), col, th);
        // Bottom-left
        dl->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + len, max.y), col, th);
        dl->AddLine(ImVec2(min.x, max.y), ImVec2(min.x, max.y - len), col, th);
        // Bottom-right
        dl->AddLine(max, ImVec2(max.x - len, max.y), col, th);
        dl->AddLine(max, ImVec2(max.x, max.y - len), col, th);
    };

    // ── Card 1: Empezar a proyectar (Hero Poster con Aura Esmeralda / Cian) ──
    {
        const ImVec2 cMin = ImVec2(contentX + ImGui::GetWindowPos().x, row1Pos.y);
        const ImVec2 cMax = ImVec2(cMin.x + cardW, cMin.y + cardH);

        ImGui::SetCursorScreenPos(cMin);
        ImGui::InvisibleButton("##heroBtn", ImVec2(cardW, cardH));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) m_LaunchRequested = true;

        const float hoverT = HubHoverLerp(ImGui::GetID("##heroBtn"), hovered);

        // Halo de respiración ambiental continuo + expansión interactiva en hover
        const float glowIntensity = 0.09f + 0.09f * breathe + hoverT * 0.25f;
        for (int i = 3; i >= 1; i--) {
            const float pad = (static_cast<float>(i) * 3.5f) + (breathe * 2.0f) + (hoverT * 6.0f);
            dl->AddRectFilled(
                ImVec2(cMin.x - pad, cMin.y - pad),
                ImVec2(cMax.x + pad, cMax.y + pad),
                ColAf(HT::AccentBlue, glowIntensity * (1.0f - static_cast<float>(i - 1) / 3.0f)),
                HT::RadiusLg + pad * 0.35f);
        }

        // Fondo con imagen y parallax
        const GLTextureInfo heroTex = GetCoverTexture(kHeroCardTextureFile);
        DrawCoverImageCover(dl, heroTex.id, heroTex.width, heroTex.height, cMin, cMax,
            HT::RadiusLg, ImDrawFlags_RoundCornersAll,
            "hub_hero_card", ImGui::GetIO().DeltaTime, hovered, 1.05f, 2.8f);

        // Scrim oscuro degradado
        dl->AddRectFilled(cMin, cMax, ColA(HT::Card, 120), HT::RadiusLg);
        dl->AddRectFilledMultiColor(
            ImVec2(cMin.x, cMin.y + cardH * 0.18f), cMax,
            ColA(IM_COL32(0, 0, 0, 0), 0), ColA(IM_COL32(0, 0, 0, 0), 0),
            ColA(HT::BgMain, 252), ColA(HT::BgMain, 252));

        // Borde interactivo con cristal fino
        const ImU32 cardBorderCol = ColAf(HT::AccentBlue, 0.25f + 0.25f * breathe + hoverT * 0.55f);
        dl->AddRect(cMin, cMax, cardBorderCol, HT::RadiusLg, 0, hovered ? 1.8f : 1.2f);
        DrawCornerBrackets(cMin, cMax, ColAf(HT::AccentSoft, 0.30f + hoverT * 0.60f), 14.0f, 2.0f);

        // Contenido de la tarjeta (Padding 20px)
        const float padX = 20.0f, padY = 18.0f;

        // Tag superior
        const ImVec2 tagPos(cMin.x + padX, cMin.y + padY);
        dl->AddRectFilled(tagPos, ImVec2(tagPos.x + 136.0f, tagPos.y + 22.0f),
            ColAf(HT::AccentBlue, 0.28f + hoverT * 0.20f), HT::RadiusSm);
        dl->AddText(ImVec2(tagPos.x + 8.0f, tagPos.y + 3.0f),
            HT::AccentSoft, "PROYECCIÓN EN VIVO");

        // Icono Play central flotante con halo de respiración
        const float playR = 25.0f + hoverT * 3.0f + breathe * 1.5f;
        const ImVec2 playCenter(cMin.x + cardW * 0.5f, cMin.y + cardH * 0.38f);
        dl->AddCircleFilled(playCenter, playR + 8.0f + breathe * 3.0f, ColAf(HT::AccentBlue, 0.14f + 0.14f * breathe + hoverT * 0.20f));
        dl->AddCircleFilled(playCenter, playR, HT::AccentBlue);
        const float pTriW = playR * 0.70f, pTriH = playR * 0.85f;
        dl->AddTriangleFilled(
            ImVec2(playCenter.x - pTriW * 0.35f, playCenter.y - pTriH * 0.5f),
            ImVec2(playCenter.x - pTriW * 0.35f, playCenter.y + pTriH * 0.5f),
            ImVec2(playCenter.x + pTriW * 0.65f, playCenter.y),
            HT::OnAccent);

        // Textos inferiores
        const float textW = cardW - padX * 2.0f;

        // Título
        ImGui::SetCursorScreenPos(ImVec2(cMin.x + padX, cMin.y + cardH - 120.0f));
        ImGui::SetWindowFontScale(1.26f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::TextUnformatted("Empezar a proyectar");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);

        // Subtítulo
        ImGui::SetCursorScreenPos(ImVec2(cMin.x + padX, cMin.y + cardH - 90.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(HT::TextPri, 195));
        ImGui::PushTextWrapPos(cMin.x + padX + textW);
        ImGui::TextWrapped("Canciones, Biblia, videos, fondos, notas, overlays y capturas.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Botón indicador inferior
        const ImVec2 btnMin(cMin.x + padX, cMin.y + cardH - 38.0f);
        const ImVec2 btnMax(cMax.x - padX, cMin.y + cardH - 12.0f);
        dl->AddRectFilled(btnMin, btnMax, ColAf(HT::AccentBlue, 0.16f + 0.10f * breathe + hoverT * 0.25f), HT::RadiusSm);
        dl->AddRect(btnMin, btnMax, ColAf(HT::AccentBlue, 0.32f + 0.20f * breathe + hoverT * 0.45f), HT::RadiusSm, 0, 1.2f);
        const char* hintTxt = "Abrir Proyector";
        const ImVec2 hsz = ImGui::CalcTextSize(hintTxt);
        dl->AddText(ImVec2(btnMin.x + (btnMax.x - btnMin.x - hsz.x) * 0.5f, btnMin.y + (btnMax.y - btnMin.y - hsz.y) * 0.5f),
            HT::AccentSoft, hintTxt);
    }

    // ── Card 2: Ajustes y Configuración (Hero Companion con Aura Robótica) ──
    {
        const ImVec2 cMin = ImVec2(contentX + ImGui::GetWindowPos().x + cardW + mainGap, row1Pos.y);
        const ImVec2 cMax = ImVec2(cMin.x + cardW, cMin.y + cardH);

        ImGui::SetCursorScreenPos(cMin);
        ImGui::InvisibleButton("##cfgCardHit", ImVec2(cardW, cardH));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) m_OpenSettingsRequested = true;

        const float hoverT = HubHoverLerp(ImGui::GetID("##cfgCardHit"), hovered);

        // Halo de respiración sutil ambiental
        const float glowIntensity = 0.06f + 0.06f * breathe + hoverT * 0.20f;
        for (int i = 3; i >= 1; i--) {
            const float pad = (static_cast<float>(i) * 3.0f) + (breathe * 1.5f) + (hoverT * 5.0f);
            dl->AddRectFilled(
                ImVec2(cMin.x - pad, cMin.y - pad),
                ImVec2(cMax.x + pad, cMax.y + pad),
                ColAf(HT::AccentSoft, glowIntensity * (1.0f - static_cast<float>(i - 1) / 3.0f)),
                HT::RadiusLg + pad * 0.35f);
        }

        // Fondo con imagen y parallax
        const GLTextureInfo cfgTex = GetCoverTexture(kConfigCardTextureFile);
        DrawCoverImageCover(dl, cfgTex.id, cfgTex.width, cfgTex.height, cMin, cMax,
            HT::RadiusLg, ImDrawFlags_RoundCornersAll,
            "hub_cfg_card", ImGui::GetIO().DeltaTime, hovered, 1.05f, 2.8f);

        // Scrim oscuro degradado
        dl->AddRectFilled(cMin, cMax, ColA(HT::Card, 140), HT::RadiusLg);
        dl->AddRectFilledMultiColor(
            ImVec2(cMin.x, cMin.y + cardH * 0.18f), cMax,
            ColA(IM_COL32(0, 0, 0, 0), 0), ColA(IM_COL32(0, 0, 0, 0), 0),
            ColA(HT::BgMain, 252), ColA(HT::BgMain, 252));

        // Borde interactivo con cristal
        const ImU32 cardBorderCol = ColAf(HT::AccentSoft, 0.18f + 0.18f * breathe + hoverT * 0.45f);
        dl->AddRect(cMin, cMax, cardBorderCol, HT::RadiusLg, 0, hovered ? 1.6f : 1.2f);
        DrawCornerBrackets(cMin, cMax, ColAf(HT::TextPri, 0.25f + hoverT * 0.50f), 14.0f, 2.0f);

        // Contenido de la tarjeta (Padding 20px)
        const float padX = 20.0f, padY = 18.0f;

        // Header de la tarjeta: Tag
        const ImVec2 tagPos(cMin.x + padX, cMin.y + padY);
        dl->AddRectFilled(tagPos, ImVec2(tagPos.x + 110.0f, tagPos.y + 22.0f),
            ColA(HT::Surface, 230), HT::RadiusSm);
        dl->AddText(ImVec2(tagPos.x + 8.0f, tagPos.y + 3.0f),
            HT::TextMuted, "CONFIGURACIÓN");

        // Icono de engranaje central flotante
        const float iconR = 25.0f + hoverT * 2.0f + breathe * 1.2f;
        const ImVec2 iconCenter(cMin.x + cardW * 0.5f, cMin.y + cardH * 0.38f);
        dl->AddCircleFilled(iconCenter, iconR + 8.0f + breathe * 2.0f, ColAf(HT::AccentSoft, 0.09f + 0.09f * breathe + hoverT * 0.16f));
        dl->AddCircleFilled(iconCenter, iconR, ColA(HT::Surface, 235));
        dl->AddCircle(iconCenter, iconR, ColAf(HT::AccentSoft, 0.28f + 0.22f * breathe), 20, 1.4f);
        dl->AddCircle(iconCenter, iconR * 0.42f, HT::TextPri, 12, 2.0f);

        // Textos inferiores
        const float textW = cardW - padX * 2.0f;

        // Título
        ImGui::SetCursorScreenPos(ImVec2(cMin.x + padX, cMin.y + cardH - 120.0f));
        ImGui::SetWindowFontScale(1.26f);
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::TextUnformatted("Ajustes del sistema");
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);

        // Subtítulo
        ImGui::SetCursorScreenPos(ImVec2(cMin.x + padX, cMin.y + cardH - 90.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(HT::TextPri, 195));
        ImGui::PushTextWrapPos(cMin.x + padX + textW);
        ImGui::TextWrapped("Pantallas de salida, conexiones LAN, temas, shaders y atajos.");
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Botón indicador inferior
        const ImVec2 btnMin(cMin.x + padX, cMin.y + cardH - 38.0f);
        const ImVec2 btnMax(cMax.x - padX, cMin.y + cardH - 12.0f);
        dl->AddRectFilled(btnMin, btnMax, ColA(HT::Surface, hovered ? 245 : 185), HT::RadiusSm);
        dl->AddRect(btnMin, btnMax, ColAf(HT::AccentSoft, 0.22f + 0.15f * breathe + hoverT * 0.35f), HT::RadiusSm, 0, 1.2f);
        const char* hintTxt = "Abrir Ajustes";
        const ImVec2 hsz = ImGui::CalcTextSize(hintTxt);
        dl->AddText(ImVec2(btnMin.x + (btnMax.x - btnMin.x - hsz.x) * 0.5f, btnMin.y + (btnMax.y - btnMin.y - hsz.y) * 0.5f),
            HT::TextPri, hintTxt);
    }

    ImGui::SetCursorScreenPos(ImVec2(row1Pos.x, row1Pos.y + cardH + 16.0f));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    // ── 3. Fila de Acompañamiento: 3 Tarjetas Compactas (Estilo Deadlock Bottom Trio) ──
    const float trioGap = 14.0f;
    const float trioW   = (contentW - trioGap * 2.0f) / 3.0f;
    const float trioH   = 84.0f;
    const ImVec2 row2Pos = ImGui::GetCursorScreenPos();

    auto RenderCompanionTile = [&](const ImVec2& uMin, const char* id, const char* title,
                                   const char* subtitle, const char* tag, bool isAccent, auto onClick)
    {
        const ImVec2 uMax(uMin.x + trioW, uMin.y + trioH);

        ImGui::SetCursorScreenPos(uMin);
        ImGui::InvisibleButton(id, ImVec2(trioW, trioH));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) onClick();

        const float hoverT = HubHoverLerp(ImGui::GetID(id), hovered);

        // Fondo del tile
        dl->AddRectFilled(uMin, uMax, ColAf(HT::CardAlt, 0.95f), HT::RadiusMd);
        if (hoverT > 0.001f)
            dl->AddRectFilled(uMin, uMax, ColAf(isAccent ? HT::AccentBlue : HT::TextPri, 0.06f * hoverT), HT::RadiusMd);

        // Borde interactivo con cristal suave
        dl->AddRect(uMin, uMax,
            hovered ? ColAf(isAccent ? HT::AccentBlue : HT::TextPri, 0.40f + hoverT * 0.35f) : ColAf(HT::Divider, 0.60f),
            HT::RadiusMd, 0, hovered ? 1.4f : 1.0f);

        // Indicador lateral izquierdo al hover
        if (hoverT > 0.001f) {
            dl->AddRectFilled(uMin, ImVec2(uMin.x + 3.5f, uMax.y),
                ColAf(isAccent ? HT::AccentBlue : HT::AccentSoft, hoverT),
                HT::RadiusMd, ImDrawFlags_RoundCornersLeft);
        }

        // Contenido
        const float padX = 14.0f;
        const float iconSize = 38.0f;
        const ImVec2 iconPos(uMin.x + padX, uMin.y + (trioH - iconSize) * 0.5f);

        // Icon Box
        dl->AddRectFilled(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            ColAf(isAccent ? HT::AccentBlue : HT::Surface, 0.30f + hoverT * 0.20f), HT::RadiusSm);
        dl->AddRect(iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            ColAf(isAccent ? HT::AccentBlue : HT::Divider, 0.50f), HT::RadiusSm);

        // Icono gráfico decorativo
        if (isAccent) {
            const ImVec2 ic(iconPos.x + iconSize * 0.5f, iconPos.y + iconSize * 0.5f);
            dl->AddCircleFilled(ic, 4.5f, HT::AccentSoft);
            dl->AddLine(ImVec2(ic.x - 8.0f, ic.y), ImVec2(ic.x + 8.0f, ic.y), HT::AccentSoft, 1.8f);
            dl->AddLine(ImVec2(ic.x, ic.y - 8.0f), ImVec2(ic.x, ic.y + 8.0f), HT::AccentSoft, 1.8f);
        } else {
            const ImVec2 ic(iconPos.x + iconSize * 0.5f, iconPos.y + iconSize * 0.5f);
            dl->AddRect(ImVec2(ic.x - 6.0f, ic.y - 7.0f), ImVec2(ic.x + 6.0f, ic.y + 7.0f), HT::TextPri, 2.0f, 0, 1.4f);
            dl->AddLine(ImVec2(ic.x - 3.0f, ic.y - 2.0f), ImVec2(ic.x + 3.0f, ic.y - 2.0f), HT::AccentSoft, 1.4f);
            dl->AddLine(ImVec2(ic.x - 3.0f, ic.y + 2.0f), ImVec2(ic.x + 3.0f, ic.y + 2.0f), HT::AccentSoft, 1.4f);
        }

        // Textos del tile
        const float textStartX = iconPos.x + iconSize + 12.0f;
        const float textMaxW   = trioW - (textStartX - uMin.x) - 14.0f;

        // Fila 1: Título + Tag pill
        ImGui::SetCursorScreenPos(ImVec2(textStartX, uMin.y + 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
        ImGui::SetWindowFontScale(1.02f);
        ImGui::TextUnformatted(title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        if (tag && tag[0] != '\0') {
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, isAccent ? HT::AccentSoft : HT::TextMuted);
            ImGui::SetWindowFontScale(0.78f);
            ImGui::TextUnformatted(tag);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        // Fila 2: Subtítulo con wrapping
        ImGui::SetCursorScreenPos(ImVec2(textStartX, uMin.y + 40.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::PushTextWrapPos(textStartX + textMaxW);
        ImGui::TextWrapped("%s", subtitle);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    };

    // Tile 1: Novedades
    const UpdateVersionInfo* latestVer = kUpdateRegistry.empty() ? nullptr : &kUpdateRegistry[0];
    std::string verTag = latestVer ? (std::string("v") + latestVer->version) : "";
    RenderCompanionTile(ImVec2(contentX + ImGui::GetWindowPos().x, row2Pos.y),
        "##utilNovedades", "Novedades",
        "Registro de versiones y parches.",
        verTag.c_str(), true, [this]() { m_NovedadesOpen = true; });

    // Tile 2: Subtítulos
    RenderCompanionTile(ImVec2(contentX + ImGui::GetWindowPos().x + trioW + trioGap, row2Pos.y),
        "##utilSubs", "Subtítulos",
        "Descargador de letras desde URL.",
        "TXT", false, [this]() { m_DownloadSubsOpen = true; });

    // Tile 3: Ajustes Rápidos / Pantallas
    RenderCompanionTile(ImVec2(contentX + ImGui::GetWindowPos().x + (trioW + trioGap) * 2.0f, row2Pos.y),
        "##utilQuickCfg", "Configuración",
        "Salidas de video, temas y LAN.",
        "SYS", false, [this]() { m_OpenSettingsRequested = true; });

    ImGui::SetCursorScreenPos(ImVec2(row2Pos.x, row2Pos.y + trioH + 20.0f));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    // ── 4. Footer con atajos rápidos ─────────────────────────────────────
    {
        const std::string footerLeft = "[ Enter ] Proyectar   •   [ S ] Ajustes   •   [ N ] Novedades   •   [ F11 ] Pantalla completa";
        const ImVec2 leftSz = ImGui::CalcTextSize(footerLeft.c_str());

        ImGui::SetCursorPosX(contentX + (contentW - leftSz.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ColA(HT::TextMuted, 160));
        ImGui::TextUnformatted(footerLeft.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void Hub::InitBgParticles(float w, float h) {
    std::mt19937 rng(static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(m_BgParticles.data()) ^ 0xDEADBEEF));

    auto frand = [&](float lo, float hi) -> float {
        return lo + (hi - lo) * (static_cast<float>(rng()) / static_cast<float>(rng.max()));
    };

    for (auto& p : m_BgParticles) {
        p.x      = frand(0.0f, w);
        p.y      = frand(0.0f, h);
        p.vx     = frand(-0.15f, 0.15f);
        p.vy     = frand(-0.10f, -0.38f);
        p.r      = frand(1.2f, 3.2f);
        p.phase  = frand(0.0f, 6.28318530717958647f);
        p.isCyan = (frand(0.0f, 1.0f) > 0.55f);
    }

    m_BgParticlesInit = true;
}

void Hub::UpdateBgParticles(float dt, float w, float h) {
    for (auto& p : m_BgParticles) {
        p.x += (p.vx + sinf(m_Time * 0.8f + p.phase) * 0.12f) * dt * 60.0f;
        p.y += p.vy * dt * 60.0f;

        if (p.x < 0.0f) p.x += w;
        if (p.x > w)    p.x -= w;
        if (p.y < 0.0f) {
            p.y += h;
            p.x = fmodf(p.x + sinf(p.phase) * 100.0f + w, w);
        }
        if (p.y > h)    p.y -= h;
    }
}

void Hub::InitNebulas(float w, float h) {
    std::mt19937 rng(static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(m_Nebulas.data()) ^ 0x9E3779B9u));
    auto frand = [&](float lo, float hi) -> float {
        return lo + (hi - lo) * (static_cast<float>(rng()) / static_cast<float>(rng.max()));
    };
    for (auto& n : m_Nebulas) {
        n.x  = frand(0.0f, w);
        n.y  = frand(0.0f, h);
        n.r  = frand(160.0f, 320.0f);
        n.vx = frand(-0.04f, 0.04f);
        n.vy = frand(-0.03f, 0.03f);
    }
    m_NebulasInit = true;
}

void Hub::UpdateNebulas(float dt, float w, float h) {
    for (auto& n : m_Nebulas) {
        n.x += n.vx * dt * 60.0f;
        n.y += n.vy * dt * 60.0f;
        if (n.x < -n.r) n.x = w + n.r;
        if (n.x > w + n.r) n.x = -n.r;
        if (n.y < -n.r) n.y = h + n.r;
        if (n.y > h + n.r) n.y = -n.r;
    }
}

void Hub::RenderBgCanvas(ImDrawList* dl, ImVec2 origin, float w, float h) {
    // ── 1. Fondo Oscuro Profundo ──
    const ImU32 baseBg = ColA(HT::BgMain, 255);
    dl->AddRectFilled(origin, ImVec2(origin.x + w, origin.y + h), baseBg);

    // ── 2. Ondas y Telaraña Astral Estilo Deadlock (Topographic Flowing Waves & Astral Rings) ──
    const float t = m_Time * 0.16f; // Avance suave y lento de las olas

    // Nodos astrales de referencia
    const ImVec2 centerL(origin.x + w * 0.18f, origin.y + h * 0.38f);
    const ImVec2 centerR(origin.x + w * 0.82f, origin.y + h * 0.52f);

    // Anillos concéntricos y ondas expansivas continuas
    for (int r = 1; r <= 8; r++) {
        const float radL = (static_cast<float>(r) * 85.0f) + fmodf(m_Time * 14.0f, 85.0f);
        const float aL   = std::max(0.0f, 0.045f * (1.0f - radL / 720.0f));
        dl->AddCircle(centerL, radL, ColAf(HT::AccentBlue, aL), 64, 1.1f);

        const float radR = (static_cast<float>(r) * 95.0f) + fmodf(m_Time * 11.0f + 45.0f, 95.0f);
        const float aR   = std::max(0.0f, 0.040f * (1.0f - radR / 780.0f));
        dl->AddCircle(centerR, radR, ColAf(HT::AccentSoft, aR), 64, 1.1f);
    }

    // Rayos geométricos tenues hacia los nodos
    const int rayCount = 16;
    for (int i = 0; i < rayCount; i++) {
        const float ang = (static_cast<float>(i) / static_cast<float>(rayCount)) * 6.2831853f + m_Time * 0.015f;
        const ImVec2 ptL(centerL.x + cosf(ang) * 550.0f, centerL.y + sinf(ang) * 550.0f);
        dl->AddLine(centerL, ptL, ColAf(HT::AccentBlue, 0.022f), 1.0f);

        const float angR = (static_cast<float>(i) / static_cast<float>(rayCount)) * 6.2831853f - m_Time * 0.012f;
        const ImVec2 ptR(centerR.x + cosf(angR) * 600.0f, centerR.y + sinf(angR) * 600.0f);
        dl->AddLine(centerR, ptR, ColAf(HT::AccentSoft, 0.018f), 1.0f);
    }

    // ── 3. Malla de Olas Topográficas Transparentes que Fluyen Lentamente ──
    const int waveCount = 14;
    for (int wIdx = 0; wIdx < waveCount; wIdx++) {
        const float baseRatio = static_cast<float>(wIdx) / static_cast<float>(waveCount - 1);
        const float baseY = h * (0.06f + baseRatio * 0.88f);
        const float freq1 = 0.0022f + baseRatio * 0.0010f;
        const float freq2 = 0.0048f - baseRatio * 0.0012f;
        const float amp1  = 26.0f + 16.0f * sinf(t * 0.7f + baseRatio * 2.8f);
        const float amp2  = 14.0f + 9.0f * cosf(t * 1.1f - baseRatio * 1.9f);
        const float speed = t * 1.4f + baseRatio * 1.5f;

        const float alpha = 0.035f + 0.030f * sinf(t * 0.8f + baseRatio * 3.0f);
        const ImU32 waveCol = ColAf((wIdx % 2 == 0) ? HT::AccentBlue : HT::AccentSoft, alpha);

        ImVec2 prevPt;
        const int steps = 54;
        for (int s = 0; s <= steps; s++) {
            const float px = origin.x + (w * static_cast<float>(s) / static_cast<float>(steps));
            const float py = origin.y + baseY
                + sinf((px - origin.x) * freq1 + speed) * amp1
                + cosf((px - origin.x) * freq2 - speed * 0.65f) * amp2
                + sinf(((px - origin.x) + baseY) * 0.003f + t * 0.5f) * 12.0f;

            if (s > 0) {
                dl->AddLine(prevPt, ImVec2(px, py), waveCol, 1.2f);
            }
            prevPt = ImVec2(px, py);
        }
    }

    // ── 4. Partículas / Chispas Luminosas en Suspensión ──
    for (const auto& p : m_BgParticles) {
        const float sinVal = sinf(m_Time * 1.1f + p.phase);
        const float alpha  = 0.28f + 0.26f * sinVal;
        const ImU32 particleTint = p.isCyan ? HT::ParticleA : HT::ParticleB;
        const ImU32 col    = ColAf(particleTint, alpha);
        const ImVec2 pos = ImVec2(origin.x + p.x, origin.y + p.y);

        // Halos de resplandor multicapa
        for (int layer = 3; layer >= 1; layer--) {
            const float layerT = static_cast<float>(layer) / 3.0f;
            const float haloR  = p.r * (1.8f + layerT * 2.5f);
            const float haloA  = alpha * 0.18f * (1.0f - layerT * 0.7f);
            dl->AddCircleFilled(pos, haloR, ColAf(particleTint, haloA), 12);
        }
        dl->AddCircleFilled(pos, p.r, col, 10);
    }
}

void Hub::RenderDownloadSubtitlesPanel() {
    bool resultReady = false;
    ProyecThor::Core::SubtitleFetchResult resultCopy;
    {
        std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
        if (m_DownloadSubsResult.has_value() && !m_DownloadSubsRunning) {
            resultCopy  = *m_DownloadSubsResult;
            resultReady = true;
            m_DownloadSubsResult.reset();
        }
    }
    if (resultReady) {
        if (m_DownloadSubsThread.joinable())
            m_DownloadSubsThread.join();

        if (resultCopy.success) {
            std::string savePath;
            if (m_DownloadSubsAskEachTime || m_DownloadSubsPresetFolder.empty()) {
                std::string suggested = resultCopy.title.empty() ? "subtitulos" : resultCopy.title;
                savePath = ProyecThor::UI::PickSaveTextPath(
                    (m_DownloadSubsPresetFolder.empty() ? suggested : (m_DownloadSubsPresetFolder + "/" + suggested)) + ".txt");
            } else {
                std::string base = resultCopy.title.empty() ? "subtitulos" : resultCopy.title;
                std::string candidate = m_DownloadSubsPresetFolder + "/" + base + ".txt";
                int suffix = 2;
                while (std::filesystem::exists(candidate)) {
                    candidate = m_DownloadSubsPresetFolder + "/" + base + " (" + std::to_string(suffix) + ").txt";
                    ++suffix;
                }
                savePath = candidate;
            }

            if (!savePath.empty()) {
                std::ofstream f(savePath, std::ios::binary);
                if (f.is_open()) {
                    f << "\xEF\xBB\xBF" << resultCopy.lyrics;
                    f.close();
                    m_DownloadSubsSavedPath = savePath;
                    m_DownloadSubsLastError.clear();
                } else {
                    m_DownloadSubsLastError = "No se pudo escribir el archivo en esa ubicacion.";
                }
            }
        } else {
            m_DownloadSubsLastError = resultCopy.error;
        }
    }

    if (!m_DownloadSubsOpen) return;

    // FIX (Wayland): esta ventana usaba AlwaysAutoResize + un ancho de
    // contenido derivado de GetContentRegionAvail() (ver el InputText de la
    // carpeta fija mas abajo, "SetNextItemWidth(GetContentRegionAvail().x -
    // 96.0f)"). En Wayland el tamaño real de ventana que devuelve GLFW llega
    // con un frame de latencia (el compositor negocia el resize, no es
    // inmediato como en X11) -- eso arma un circulo: el contenido pide un
    // ancho basado en el tamaño de la ventana, la ventana se autoajusta a
    // ese contenido, y el proximo frame el tamaño "real" que reporta GLFW ya
    // cambio, asi que el contenido vuelve a pedir un ancho distinto. Crece
    // sin limite. Pasaba justo al tocar "Carpeta fija" (la opcion de abajo)
    // porque ese es el radio button que agrega el InputText problematico.
    // Con tamaño FIJO (sin AlwaysAutoResize) no hay nada que reajustar en
    // base al contenido, asi que el circulo no puede arrancar.
    const ImVec2 baseSize(480.0f, 320.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(workCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(baseSize, ImGuiCond_Appearing);

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    bool open = ImGui::Begin("Descargar subtitulos", &m_DownloadSubsOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoResize);

    if (open) {
        ImGui::TextWrapped("Pega el link de un video. Se buscan sus subtitulos (español primero, si "
                            "no ingles) y se guardan como un .txt suelto -- no crea una cancion.");
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        ImGui::BeginDisabled(m_DownloadSubsRunning);
        ImGui::SetNextItemWidth(-1.0f);
        bool enterPressed = ImGui::InputTextWithHint("##dlSubsUrl", "https://www.youtube.com/watch?v=...",
            m_DownloadSubsUrlBuf, sizeof(m_DownloadSubsUrlBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
        ImGui::TextUnformatted("Guardar en");
        ImGui::PopStyleColor();
        if (ImGui::RadioButton("Preguntar cada vez", m_DownloadSubsAskEachTime))
            m_DownloadSubsAskEachTime = true;
        if (ImGui::RadioButton("Carpeta fija", !m_DownloadSubsAskEachTime))
            m_DownloadSubsAskEachTime = false;

        if (!m_DownloadSubsAskEachTime) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            char folderBuf[512];
            std::snprintf(folderBuf, sizeof(folderBuf), "%s",
                m_DownloadSubsPresetFolder.empty() ? "Sin elegir..." : m_DownloadSubsPresetFolder.c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 96.0f);
            ImGui::InputText("##dlSubsPresetFolder", folderBuf, sizeof(folderBuf), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Elegir...", ImVec2(86.0f, 0.0f))) {
                std::string chosen = ProyecThor::UI::PickFolder("Elegir carpeta para subtitulos descargados");
                if (!chosen.empty()) m_DownloadSubsPresetFolder = chosen;
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        bool wantStart = false;
        if (m_DownloadSubsRunning) {
            ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Buscando subtitulos...");
        } else {
            if (ImGui::Button("Descargar", ImVec2(120.0f, 32.0f)))
                wantStart = true;
            if (enterPressed)
                wantStart = true;
            ImGui::SameLine();
            if (ImGui::Button("Cerrar", ImVec2(100.0f, 32.0f))) {
                m_DownloadSubsOpen = false;
                m_DownloadSubsLastError.clear();
            }
        }

        if (!m_DownloadSubsLastError.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("%s", m_DownloadSubsLastError.c_str());
            ImGui::PopStyleColor();
        }
        if (!m_DownloadSubsSavedPath.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, HT::Success);
            ImGui::TextWrapped("Guardado en: %s", m_DownloadSubsSavedPath.c_str());
            ImGui::PopStyleColor();
        }

        if (wantStart && !m_DownloadSubsRunning && m_DownloadSubsUrlBuf[0] != '\0') {
            if (m_DownloadSubsThread.joinable()) m_DownloadSubsThread.join();
            m_DownloadSubsLastError.clear();
            m_DownloadSubsSavedPath.clear();
            m_DownloadSubsRunning = true;
            {
                std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
                m_DownloadSubsResult.reset();
            }
            std::string urlCopy = m_DownloadSubsUrlBuf;
            m_DownloadSubsThread = std::thread([this, urlCopy]() {
                ProyecThor::Core::SubtitleFetchResult res = ProyecThor::Core::FetchSubtitlesAsLyrics(urlCopy);
                std::lock_guard<std::mutex> lk(m_DownloadSubsMutex);
                m_DownloadSubsResult  = std::move(res);
                m_DownloadSubsRunning = false;
            });
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void Hub::RenderUpdateDetailModal() {
    const int selectedUpdateVer = m_SelectedUpdateVer;

    {
        const float target = m_IsUpdateModalOpen ? 1.0f : 0.0f;
        m_UpdateModalAnim += (target - m_UpdateModalAnim) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
        m_UpdateModalAnim = std::clamp(m_UpdateModalAnim, 0.0f, 1.0f);
        if (m_UpdateModalAnim < 0.001f) m_UpdateModalAnim = 0.0f;
    }

    if (m_IsUpdateModalOpen || m_UpdateModalAnim > 0.0f) {
        const UpdateVersionInfo* selInfo = FindUpdateVersion(selectedUpdateVer);
        const GLTextureInfo modalCover = selInfo ? GetCoverTexture(selInfo->coverFile) : GLTextureInfo{};

        ImGuiViewport* vp     = ImGui::GetMainViewport();
        const float     fadeA = EaseOut(m_UpdateModalAnim);

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fadeA);

        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 170));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##DimOverlay", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        const float scale         = 0.96f + 0.04f * fadeA;
        const float modalW        = 780.0f * scale, modalH = 660.0f * scale;
        const float headerH       = 200.0f * scale, footerH = 62.0f * scale;
        const float modalRounding = HT::RadiusLg;

        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ColA(HT::Card, 255));
        ImGui::PushStyleColor(ImGuiCol_Border,   HT::Divider);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   modalRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));

        bool vis = ImGui::Begin("##UpdateModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

        if (vis) {
            ImDrawList* dl     = ImGui::GetWindowDrawList();
            ImVec2      winP   = ImGui::GetWindowPos();
            const ImU32 modalBg = ColA(HT::Card, 255);

            const ImVec2 headerMin = winP;
            const ImVec2 headerMax = ImVec2(winP.x + modalW, winP.y + headerH);

            if (modalCover.id != 0) {
                char stateKey[96];
                snprintf(stateKey, sizeof(stateKey), "modal_%s", selInfo ? selInfo->version : "none");
                const bool headerHovered = ImGui::IsMouseHoveringRect(headerMin, headerMax);

                DrawCoverImageCover(dl, modalCover.id, modalCover.width, modalCover.height,
                    headerMin, headerMax, modalRounding, ImDrawFlags_RoundCornersTop,
                    stateKey, ImGui::GetIO().DeltaTime, headerHovered, 1.06f);
                ImGui::Dummy(ImVec2(modalW, headerH));
            } else {
                dl->AddRectFilled(headerMin, headerMax, ColA(HT::CardAlt, 255),
                    modalRounding, ImDrawFlags_RoundCornersTop);
                ImGui::Dummy(ImVec2(modalW, headerH));
            }

            dl->AddRectFilledMultiColor(
                ImVec2(winP.x, winP.y+headerH-60), ImVec2(winP.x+modalW, winP.y+headerH),
                ColA(modalBg, 0), ColA(modalBg, 0), modalBg, modalBg);

            ImGui::SetCursorPos(ImVec2(0, headerH));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
            ImGui::BeginChild("##ModalScroll", ImVec2(modalW, modalH-headerH-footerH), false);

            const float mg = 36.0f, cw = modalW - mg*2;
            ImGui::SetCursorPos(ImVec2(mg, 18.0f));
            ImGui::BeginGroup();

            auto DrawPillBadge = [&](const char* text) {
                ImGui::SetWindowFontScale(0.8f);
                const ImVec2 bs = ImGui::CalcTextSize(text);
                ImGui::SetWindowFontScale(1.0f);
                const ImVec2 pad(8.0f, 3.0f);
                const ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(bp.x - pad.x, bp.y - pad.y), ImVec2(bp.x + bs.x + pad.x, bp.y + bs.y + pad.y),
                    HT::AccentBlue, HT::RadiusSm);
                ImGui::Dummy(ImVec2(pad.x, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, HT::OnAccent);
                ImGui::SetWindowFontScale(0.8f); ImGui::Text("%s", text); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, pad.x);
            };
            DrawPillBadge(selInfo ? selInfo->modalBadge : "BETA");
            ImGui::SameLine(0, 40);

            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
            ImGui::Text("HISTORIAL DE VERSIONES");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,6));

            ImGui::SetWindowFontScale(1.7f);
            ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
            ImGui::Text("Actualizacion v%s", selInfo ? selInfo->version : "?");
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Dummy(ImVec2(0,20));

            auto Cat = [&](const char* t) {
                ImGui::PushStyleColor(ImGuiCol_Text, HT::TextPri);
                ImGui::SetWindowFontScale(1.05f); ImGui::Text("%s",t); ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor();
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(p.x,p.y+1), ImVec2(p.x+cw,p.y+1), HT::BorderFaint);
                ImGui::Dummy(ImVec2(0,10));
            };
            auto Bul = [&](const char* t) {
                ImVec2 bp = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(bp.x+6, bp.y+ImGui::GetTextLineHeight()*0.5f), 2.5f, HT::AccentBlue);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+18);
                ImGui::PushStyleColor(ImGuiCol_Text, HT::TextMuted);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+cw-22);
                ImGui::TextWrapped("%s",t);
                ImGui::PopTextWrapPos(); ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0,4));
            };

            if (selectedUpdateVer == 16) {
                Cat("HUD Estilizado y Fondo de Ondas Dinámicas");
                Bul("Nuevo fondo atmosférico con ondas topográficas fluidas en movimiento lento y partículas astrales en suspensión.");
                Bul("Tarjetas de inicio tipo póster con iluminación reactiva, brackets angulares y respiración ambiental armónica.");
                Bul("Cabecera tipográfica nítida de dos tonos ProyecThor sin desenfoques.");
                ImGui::Dummy(ImVec2(0, 12));

                Cat("Navegador Web Integrado y Filtros Multimedia");
                Bul("Panel Web rediseñado en formato vertical para integrarse perfectamente en el panel lateral de la Biblioteca.");
                Bul("Envío de sitios web en vivo a la pantalla pública en pantalla completa con un solo clic.");
                Bul("Importación múltiple de archivos a la vez en todas las secciones de la Biblioteca.");
                Bul("Intercambio rápido entre Fondos (Backgrounds) y Multimedia: menú con clic derecho para mover/copiar y soporte para Drag & Drop (arrastrar y soltar) bidireccional.");
                Bul("Barra de filtros de medios con iconos vectoriales homogéneos (Todos, Videos, Audios, Imágenes, Recargar, Importar, Cuadrícula y Lista).");
                Bul("Corrección de colisión de controles: separación estricta entre el botón de importar archivos y el control de tamaño de miniaturas.");
                ImGui::Dummy(ImVec2(0, 12));

                Cat("Gestión de Datos y Carpetas de Importe Automático");
                Bul("Nueva categoría Datos en Ajustes: personaliza la ubicación de almacenamiento de AppData en cualquier unidad o disco.");
                Bul("Carpetas de Importe Automático (Watched Folders): vincula carpetas externas (ej. Descargas) con modo de reproducción directa o copia automática.");
                ImGui::Dummy(ImVec2(0, 12));

                Cat("Biblioteca de Notas Rápidas y Anuncios");
                Bul("Sistema permanente de guardado y carga de notas con persistencia en disco JSON.");
                Bul("Etiquetado inteligente por categorías: Anuncios, Avisos, Urgente, Culto y General.");
                Bul("Sincronización directa con el letrero de anuncios rodante y selección de destinos (Público, LAN, Ambos).");
                ImGui::Dummy(ImVec2(0, 12));

                Cat("Proyección a Público y Rendimiento");
                Bul("Corrección de oscurecimiento: la pantalla pública y los monitores secundarios nunca se oscurecen al abrir paneles o modales.");
                Bul("Puntero del mouse oculto automáticamente al proyectar sobre la pantalla secundaria.");
                Bul("Reloj y contadores LAN desacoplados: personalización de temas inalámbricos sin alterar la salida pública.");
                ImGui::Dummy(ImVec2(0, 12));

                Cat("Asistente de IA y Espacios de Trabajo");
                Bul("Acceso directo y fluido a Claude, ChatGPT y Gemini en navegador embebido.");
                Bul("Liberación inmediata de foco de teclado al cerrar la ventana del asistente web.");
                Bul("Espacio de trabajo Producción enfocado en Render y Overlays.");
                ImGui::Dummy(ImVec2(0, 12));
            } else if (selectedUpdateVer == 15) {
                Cat("Asistente de IA Integrado");
                Bul("Nuevo Asistente de IA accesible directamente desde la barra inferior de la aplicación.");
                Bul("Modo Básica: Navegador embebido real con Claude, ChatGPT o Gemini para consultar y generar contenido sin exponer tus credenciales.");
                Bul("Modo Avanzada: Conexión directa mediante API Key propia de Claude. Permite listar, redactar, estructurar y editar canciones de la Biblioteca con confirmación explícita antes de guardar.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Espacio de Trabajo \"Producción\"");
                Bul("El antiguo espacio de Video se transforma en Producción: un entorno completo para creación y procesado multimedia.");
                Bul("Audio DAW multipista: graba desde tu micrófono, corta y mueve clips en la línea de tiempo, reproduce todas las pistas sincronizadas y exporta en WAV, MP3, AAC u OGG.");
                Bul("Conversor Render: compresión y conversión optimizada de videos y audios entre formatos modernos (H.264, H.265, VP9, AV1).");
                Bul("Galería y Editor de Overlays integrados en el riel de navegación lateral.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Transmisión y Salidas");
                Bul("Espacio de Transmisión simplificado y enfocado: selección directa entre Captura, Overlays guardados o la salida al Público.");
                Bul("Navegador Web en Biblioteca: navega cualquier sitio web y envíalo en vivo a la pantalla pública.");
                Bul("Salida Inalámbrica (LAN): posibilidad de fijar la salida de red en \"Solo reloj\" o \"En blanco\" mientras el proyector principal sigue en vivo.");
                Bul("Independización del estilo de reloj: el tema del reloj por LAN no interfiere con el estilo de la pantalla pública.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Mejoras de Rendimiento y UI");
                Bul("Cola del Monitor con botones animados interactivos y corrección en el modo Loop de repetición.");
                Bul("Editor de Overlays: correcciones en la lista de capas y mayor estabilidad general.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 14) {
                Cat("Notas Rápidas y Atajos");
                Bul("Notas Rápidas rediseñado con persistencia automática de texto: nunca más perderás lo que estabas escribiendo al cerrar la ventana.");
                Bul("Nuevo atajo Shift+Z para alternar Notas Rápidas en cualquier momento.");
                Bul("Atajos Alt Gr+1/2/3/4 para colapsar y expandir paneles con transiciones suaves (Alt Gr+0 restaura el entorno completo).");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Gestión de Medios y Biblioteca");
                Bul("Sección Multimedia renombrada a \"Medios\", con vista en cuadrícula de miniaturas grandes e iconos descriptivos por tipo de archivo.");
                Bul("Importación inteligente: el reproductor de Audio puede importar letras de canciones directamente desde enlaces de YouTube.");
                Bul("Nuevo entorno de trabajo \"Biblioteca\" para concentrarse en la gestión de contenidos.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Pantalla Completa y Ajustes");
                Bul("El Preview cuenta con modo de pantalla completa real (F11) con controles auto-ocultables y soporte para escalado FSR.");
                Bul("Ajustes > Actualizaciones añade herramientas para desinstalar versiones anteriores y mover la carpeta de datos de la biblioteca.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 13) {
                Cat("App movil: editor de overlays");
                Bul("Edicion completa de overlays desde el celular: mover, redimensionar y rotar capas con gestos, igual que en la PC.");
                Bul("Seleccion multiple con recuadro de arrastre (rubber-band), guias de iman para alinear capas entre si, y una barra con el tamaño en pixeles mientras moves o redimensionas.");
                Bul("Panel de capas reordenable arrastrando (igual que en la PC), y dos herramientas nuevas: Borrador y Degradado, con edicion real de pixeles.");
                Bul("Boton \"Enviar al PC\": exporta el overlay y lo sube directo a la app de escritorio sin pasar por USB ni un cable.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Render");
                Bul("Eleccion de codec de video al convertir (H.264, H.265, VP9 o AV1) y un control deslizante de compresion.");
                Bul("Boton para cancelar una conversion a mitad de camino, con una barra de progreso real en vez de una animacion generica.");
                Bul("Estimacion del peso final antes de convertir, y comparacion exacta de antes/despues una vez termina.");
                Bul("Podes elegir si guardar siempre en una carpeta fija o que te pregunte cada vez, con el mismo dialogo nativo de siempre.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte real para Linux y CachyOS");
                Bul("ProyecThor ahora compila y corre en Linux de verdad: paquete para Arch/CachyOS validado automaticamente en cada version.");
                Bul("Funciona tanto en X11 como en Wayland (via XWayland), incluyendo en escritorios como el de CachyOS.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Windows: escalado de pantalla (DPI)");
                Bul("La app ahora respeta el porcentaje de escalado de Windows (125%, 150%, etc.): en laptops con pantallas de alta densidad, la letra y los botones ya no se ven diminutos.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Ajustes reorganizados");
                Bul("Nueva categoria propia \"Conexiones\", con Red (LAN), Mobile, Streaming y OSC como subcategorias separadas -- antes vivian sueltas o mezcladas dentro de Proyeccion.");
                Bul("Actualizaciones sigue siendo su propia categoria, pero sus 3 subcategorias (Version instalada, Estado, Configuracion) se unificaron en una sola pagina.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Estilos de Letra, renovado por completo");
                Bul("Ahora es un editor visual a pantalla completa: arrastras y redimensionas directamente sobre una vista previa 16:9, en vez de tocar numeros de margen a mano.");
                Bul("El cuerpo de un versiculo biblico usa el mismo diseno que las canciones (recuadro de Letras) -- ya no hay un recuadro aparte para el texto del versiculo.");
                Bul("Nuevo recuadro opcional \"Indice\": si lo activas, muestra solo la referencia (ej. \"Genesis 1:1\") en la posicion, tamano y estilo que quieras, totalmente independiente de las Letras.");
                Bul("Menu de opciones arriba (Fuente/Alinear, Efectos, Fondo de pantalla), mismo lenguaje visual que el editor de Overlays pero pensado para texto.");
                Bul("Cada recuadro puede tener su propio fondo de imagen opcional, elegido de tu biblioteca de fondos, con control de opacidad -- transparente por defecto.");
                Bul("Los estilos guardados con la version anterior se migran solos al nuevo formato al abrirlos, sin perder la configuracion previa.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Efectos de texto nuevos");
                Bul("Texto 3D: extrusion solida detras de la letra, con color y profundidad configurables.");
                Bul("Degradado de color: interpola entre dos colores a lo largo de un angulo, para letras multicolor.");
                Bul("Transparencia con angulo: desvanece el texto de un extremo al otro segun el angulo elegido, en vez de una opacidad pareja.");
                Bul("Los tres se combinan con el resto de efectos (sombra, borde, glow, neon, etc.) y se editan desde la misma tarjeta de Efectos.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Aviso");
                Bul("Esta es una actualizacion grande y todavia esta en beta / en construccion: pueden aparecer ajustes y correcciones adicionales en las proximas versiones menores.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 12) {
                Cat("Contadores (antes \"Reloj y Contadores\")");
                Bul("Se acorto el nombre de la seccion a secas \"Contadores\" en el sidebar de Home.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Reloj dentro de Overlays");
                Bul("Nuevo cuadro de Reloj en el editor de Overlays (boton junto a Texto/Forma/Imagen): lo arrastras, le das tamaño y estilo de texto (fuente, color, sombra, contorno, fondo) una sola vez, como una capa mas.");
                Bul("Ese cuadro es solo un marcador de posicion: al proyectar el overlay que lo contiene, se reemplaza en vivo por la hora o el cronometro activo — nunca queda \"horneado\" como texto fijo en el overlay guardado.");
                Bul("La transmision a pantalla del reloj ya no es un modo aparte a elegir: aparece automaticamente si el overlay que tenes activo incluye un cuadro de Reloj. El panel de Contadores muestra un aviso si el overlay activo no tiene uno.");
                Bul("La transmision a dispositivos en red (LAN) sigue siendo un interruptor propio (Apagado / Solo LAN), independiente del overlay.");
                Bul("Se agregaron overlays de reloj predeterminados (barra inferior, esquina y centrado) listos para probar de una, sin tener que armar uno desde cero.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Overlays");
                Bul("Las capas ahora se pueden reordenar (subir/bajar) desde la lista lateral, para elegir cual queda encima de cual.");
                Bul("Encabezado del editor mas plano y compacto (se saco el degradado de color) y menos relleno en los margenes, para un look mas minimalista.");
                Bul("Corregido: el cuadriculado que indica \"sin fondo\" en el editor podia terminar guardado como fondo opaco (gris/negro) en el PNG del overlay en vez de quedarse transparente, sobre todo en overlays sin capas de imagen.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones en Contadores");
                Bul("El aviso de \"overlay activo sin cuadro de reloj\" y otros textos largos ya no se cortaban contra el borde del panel: ahora se ajustan en varias lineas.");
                Bul("Corregido un icono roto en el boton \"Avanzar\" del titulo/mensaje del reloj.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 11) {
                Cat("Ajustes reorganizado");
                Bul("Cada configuracion ahora es su propia pagina: al elegir una subcategoria en el menu de la izquierda, se ve sola en vez de tener que scrollear una lista larga con todo junto.");
                Bul("Nuevo buscador arriba del menu de Ajustes, para encontrar una configuracion por nombre sin tener que navegar categoria por categoria.");
                Bul("Proyeccion y Pantallas ahora estan agrupadas juntas en el menu, y Red, Mobile, Streaming y OSC pasaron a vivir DENTRO de Proyeccion en vez de tener su propia categoria aparte.");
                Bul("Se saco la categoria General (Inicio, Guardado automatico, Carpetas por defecto): no se usaba.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Fondos: bucle falso");
                Bul("Nueva opcion en Ajustes > Proyeccion > Fondos: en vez de cortar siempre al mismo frame inicial al repetir, el fondo reproduce hacia adelante y despues \"hacia atras\", dando sensacion de bucle continuo sin el salto de siempre.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Render");
                Bul("El conversor de formato tiene un diseño mas moderno, con el texto que antes se cortaba contra el borde del panel ahora bien acomodado.");
                Bul("Se saco el boton de Reloj del sidebar de Biblioteca: ya estaba disponible en la barra inferior de Vista en Vivo, quedaba duplicado.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva seccion: Overlays");
                Bul("Crea overlays (textos, formas e imagenes) en un editor nuevo a pantalla completa, desde Biblioteca > Overlay.");
                Bul("Un overlay se guarda como imagen PNG con transparencia real: al mostrarlo, se proyecta como una capa aparte ENCIMA del fondo y la letra, dejando ver lo que haya debajo — antes, por error, lo reemplazaba todo como si fuera un fondo mas.");
                Bul("El editor tiene una barra flotante para agregar texto, formas o imagenes, lista de capas, y boton de Eliminar para la capa seleccionada.");
                Bul("Acceso rapido tambien desde Vista en Vivo (boton Overlays de la barra inferior), con galeria de miniaturas para aplicar uno sin salir de la pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Vista en Vivo renovada");
                Bul("Reproductor mas simple: se saco el encabezado \"PROGRAM - ON AIR\" y los botones de transporte pasaron a iconos chicos y planos, mas parecidos al resto de apps de proyeccion.");
                Bul("Overlays, Chat, Pads y Reloj ahora se abren DENTRO del mismo panel de Vista en Vivo (con scroll propio si hay mucho contenido), en vez de ventanas flotantes sueltas que quedaban desconectadas del boton que las abria.");
                Bul("La barra de botones de abajo quedo pegada justo debajo del reproductor, sin espacio vacio en el medio, y con los botones mas parejos entre si.");
                Bul("Se saco la tira de Stage que aparecia arriba del video: quedaba duplicada con el boton que ya permite alternar toda la vista entre Publico y Stage.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva seccion: Pantallas");
                Bul("La configuracion de Stage (que monitor usa, si es por red, que muestra cada pantalla) ahora tiene su propio menu \"Pantallas\" arriba de todo, en vez de estar mezclada con Proyeccion.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones de tema y apariencia");
                Bul("Varias ventanas y menus (el menu superior, los popups de Estilos y el selector rapido Alt+Espacio, el Monitor de Control) ignoraban el tema elegido en Ajustes > Apariencia y se quedaban siempre con los mismos colores fijos — ahora todos respetan el tema.");
                Bul("Los fondos de los paneles eran levemente transparentes y dejaban ver lo que hubiera atras, dando un aspecto \"lavado\" o inconsistente segun el tema — ahora son solidos.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 10) {
                Cat("Nueva seccion: Conexiones");
                Bul("Toolbar nueva arriba de todo (Hub / Proyector / Conexiones / Biblioteca / Biblia) para saltar entre secciones completas de la app, opcional segun Vista.");
                Bul("OSC: enviar mensajes a luces/controladores externos con direccion IP y puerto configurables, mas \"Aprender\" (OSC Learn) para vincular un fader externo a parametros en vivo como opacidad, velocidad, escala, color o intensidad de los shaders.");
                Bul("Red y Chat, disponibles ahora en dos lugares a la vez (Conexiones y su ubicacion original en Biblioteca/Herramientas): es la misma conexion y el mismo chat, no hay que elegir uno.");
                Bul("Streaming en vivo real por RTMP (Twitch, YouTube, Facebook, etc.), con captura de camara/pantalla, preview y control de capas tipo OBS, todo integrado en el mismo rail.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nueva seccion: Biblioteca");
                Bul("Ver, renombrar y borrar tus archivos de Video, Imagen y Audio ya importados, separado de Vista en Vivo para no arriesgar nada de lo que este proyectando.");
                Bul("Nuevo panel \"Render\": convierte tus videos y audios a otros formatos aprovechando ffmpeg, sin instalar nada aparte.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia a pantalla completa");
                Bul("El mismo buscador de Biblia de siempre, ahora tambien como su propia seccion a pantalla completa: libros/capitulos a la izquierda, texto grande a la derecha.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Selector rapido y novedades");
                Bul("Alt+Espacio abre un selector para saltar entre Hub, Conexiones, Biblioteca y Biblia con el teclado, sin tocar el mouse.");
                Bul("Al abrir una version nueva de ProyecThor aparece un carrusel de novedades en el Hub, en vez de tener que buscarlas en esta misma pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Monitor de Vista en Vivo, mas compacto");
                Bul("El panel de Preview del Monitor ocupaba mucho mas alto del que en realidad necesitaba: se redujo para darle bastante mas espacio al video.");
                Bul("El boton de Play/Pausa se integro en la misma fila que Inicio / -10s / +10s / Detener, en vez de tener su propia fila completa aparte.");
                Bul("Botones e iconos del Preview mas chicos y prolijos; la columna central (Transmitir/Loop) ahora se achica sola si el espacio disponible es menor al habitual, en vez de cortarse.");
                Bul("Sacado el boton de Contener/Estirar de esa columna: ya estaba disponible a la derecha de Vista en Vivo, no hacia falta duplicarlo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Estilos renovado");
                Bul("Se le bajo el tono \"arcoiris\" que tenia (cada pestaña/tarjeta con un color distinto) a favor de un solo acento consistente con el resto de la app.");
                Bul("Encabezado, bordes y esquinas mas sobrios y rectos, en linea con el resto de los paneles en vez de un look aparte tipo Canva.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo instalador para Windows");
                Bul("ProyecThor ahora se instala con un instalador moderno: mas rapido, mas prolijo y con menos falsos positivos de antivirus.");
                Bul("Si ya tenias ProyecThor instalado con una version anterior (aunque sea de un instalador viejo), no hace falta que la desinstales a mano: el instalador nuevo la detecta y la reemplaza solo, sin dejar archivos sueltos de la version vieja.");
                Bul("Corregido: el icono de la aplicacion no se veia bien (aparecia en blanco) en el acceso directo y en el instalador.");
                Bul("Las actualizaciones automaticas de esta pantalla tambien se actualizaron para descargar el instalador nuevo correctamente.");
                Bul("Nuevo aviso en Ajustes > Actualizaciones, con un icono de informacion que te recuerda revisar \"Agregar o quitar programas\" si sospechas que quedo mas de una version instalada.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca con contenido de entrada");
                Bul("Canciones y Biblias ya no arrancan vacias en una instalacion nueva: se cargan solas una cancion de bienvenida y varias Biblias (español, ingles y portugues) para tener algo con que probar de una.");
                Bul("Corregido: al ponerle Titulo a una cancion nueva (o cambiarselo a una ya existente) desde el editor, ahora se ve reflejado en la lista, el buscador y las playlists — antes quedaba guardado por dentro pero la Biblioteca seguia mostrando el nombre viejo (\"Nueva cancion\").");
                Bul("Corregido: renombrar una cancion desde el menu contextual ya no le hace perder el autor, las etiquetas, el estilo/fondo preferido ni las playlists en las que estaba.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Correcciones de estabilidad");
                Bul("Corregido un cierre inesperado de la app relacionado con ffmpeg: antes podia abrir brevemente una consola negra y cerrarse sin avisar el motivo; ahora corre oculto y muestra el error real si algo falla (por ejemplo, al convertir un video en Biblioteca > Render).");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 9) {
                Cat("Pads de Vista en Vivo");
                Bul("Corregido el problema por el cual guardar un pad (click derecho > Guardar aqui) podia no aplicar nada al presionarlo despues: ahora siempre captura estilo, fondo y captura de pantalla tal cual estan en pantalla.");
                Bul("El panel de Pads se reorganizo en dos secciones: \"General\" (los pads de siempre) y \"Captura\", que ahora muestra las mismas escenas rapidas del panel Captura, sincronizadas — guardar o aplicar una desde cualquiera de los dos lados es lo mismo.");
                Bul("El texto de ayuda de \"Escenas rapidas\" se reemplazo por un icono de informacion, para no saturar el panel de letra.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Transporte y volumen de Vista en Vivo");
                Bul("Los botones de Play/Pausa, Retroceder, Avanzar y Detener ahora son pads de colores tipo controlador MIDI, con el boton de reproduccion iluminado en rojo mientras esta en vivo.");
                Bul("El control de volumen pasa a ser un fader horizontal estilo consola de sonido en vez del slider de siempre.");
                Bul("Corregido un icono roto en el boton de silenciar (mute) de Vista en Vivo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia: buscador por palabras");
                Bul("Nuevo boton (lupa + \"Aa\") junto al buscador rapido: permite escribir una o mas palabras y muestra todos los versiculos de la Biblia activa que las contienen, para cuando no te acordas la cita exacta.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Editor de Estilos renovado");
                Bul("El editor de un estilo ya no abre una ventana flotante encima de todo: ahora se muestra acoplado dentro de Home, ocupando todo ese espacio, como una seccion mas de la Biblioteca.");
                Bul("El selector de fuente pasa de una lista de texto a una grilla con la vista previa real de cada tipografia.");
                Bul("Nueva pestaña \"Efectos\": fondo, borde, sombra, aberracion cromatica, glow (bloom), neon y subrayado, todo configurable por separado para el texto proyectado.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Shaders: muchos efectos nuevos");
                Bul("NIS: escalador alternativo a FSR, exclusivo para placas NVIDIA (se detecta automaticamente).");
                Bul("VHS: sangrado de color, scanlines, bamboleo y ruido de estatica, como una cinta de video vieja.");
                Bul("Cine: gradacion de color tipo cine, con tinte a elegir entre rojo, verde o azul.");
                Bul("Contraste y Luminosidad: ajuste directo de contraste y brillo de la salida en vivo.");
                Bul("Blur, Sharpen, Bloom y Aberracion cromatica: desenfoque, nitidez, resplandor de brillos y desfase de color, respectivamente.");
                Bul("TAA (antialiasing temporal): suaviza bordes mezclando con el frame anterior, a costa de un poco de desenfoque de movimiento.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 8) {
                Cat("Editor de canciones (rediseño total)");
                Bul("Editar una cancion ya no abre una ventana flotante encima: el mismo panel de Canciones pasa a modo edicion, con letra a la izquierda (mucho mas grande) y preview de las diapositivas a la derecha.");
                Bul("Titulo y Autor quedan siempre a la vista; Nota, Derechos de autor y Extra se movieron detras de un boton de informacion para no restarle espacio a la letra.");
                Bul("Todo se guarda solo mientras se escribe (sin boton Guardar), con indicador de estado y botones de Deshacer/Rehacer del ultimo cambio.");
                Bul("Nuevo filtro de \"Lineas por diapositiva\" (1/2/3): separa la letra de verdad, insertando lineas en blanco reales dentro de cada estrofa, para que la division se vea en el propio texto y no solo en el preview.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Menu principal reorganizado");
                Bul("Nuevo menu \"ProyecThor\" (primero, a la izquierda) con Preferencias y Salir.");
                Bul("Archivo ahora es la categoria Importar, con una opcion nueva: \"Importar cancion desde portapapeles\" (crea la cancion y pega el contenido del portapapeles de una).");
                Bul("\"Base de datos\" y \"Wiki\" se movieron al menu Ayuda.");
                Bul("Nuevo menu \"Ventana\" con Pantalla completa (tambien con la tecla F11).");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Efectos de video (rediseñado + nuevos)");
                Bul("El panel de Shaders (al lado de Overlays, en Diseño) ahora se ve como tarjetas con icono, descripcion y control de intensidad propio para cada efecto, en vez de una lista de switches.");
                Bul("Dos efectos nuevos: Saturacion (colores mas vivos o hasta blanco y negro) y Vinetado (oscurece los bordes para enfocar el centro), sumados a FSR, CRT, grano de pelicula y FXAA.");
                Bul("Nuevo efecto \"Rellenado\" (recomendado): llena las barras negras de letterbox/pillarbox con el mismo fondo, estirado y muy desenfocado, en vez de dejarlas negras — el efecto tipo Spotify Canvas / Smart TV.");
                Bul("Cada efecto se prende o apaga por separado y se ve reflejado al instante en la salida en vivo.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Videos");
                Bul("Los videos ahora muestran una miniatura real (un frame del video), igual que ya pasaba con los Fondos.");
                Bul("Nuevo boton para alternar entre vista en lista y vista en grilla con miniaturas grandes, mas un control para agrandar o achicar las miniaturas.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca > Playlists");
                Bul("El panel de \"Agregar canciones\" a una playlist es mas grande y las canciones se listan en orden alfabetico, con un boton \"+\" bien visible para agregar y una insignia verde \"Agregada\" para las que ya estan.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Captura (camara / pantalla)");
                Bul("Nuevas \"Escenas rapidas\": 8 botones de color donde guardar una fuente + recuadro + opacidad ya armados, para saltar entre encuadres con un solo click durante el evento.");
                Bul("Click derecho sobre un boton para guardar la posicion libre actual ahi o borrarla; quedan guardadas entre sesiones.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Ajustes > Apariencia");
                Bul("Nueva fuente de interfaz personalizable: se puede importar una tipografia propia (.ttf/.otf/.ttc) ademas de elegir entre las que ya trae la app, con reinicio guiado para aplicarla.");
                Bul("El menu de Ajustes se reordeno con iconos por categoria y subcategorias navegables, para ubicar cada opcion mas rapido.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo motor de video (experimental)");
                Bul("En Ajustes > Proyeccion, opcion para elegir el motor con el que se reproducen los Videos: el de siempre (OpenGL) o uno nuevo (libvlc) que usa una ventana propia con reproduccion acelerada.");
                Bul("Pensado para equipos con poca placa de video — los Fondos (loops decorativos) siempre siguen mostrandose como hasta ahora, con overlays y texto encima.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estabilidad");
                Bul("Corregido un problema por el cual el video de fondo podia irse desincronizando del audio con el correr de los minutos en computadoras mas lentas.");
                Bul("Corregido: el control de FSR en Ajustes > Proyeccion y el del panel de Shaders podian mostrar y guardar valores distintos entre si.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 7) {
                Cat("Cola de videos y video en vivo");
                Bul("La cola de videos es mucho mas confiable: los clips pasan de uno a otro sin cortes ni pantallas de carga de por medio.");
                Bul("Corregido: la app ya no se traba si hacias clic varias veces seguidas sobre el mismo video.");
                Bul("Los videos de la cola ahora siempre arrancan desde el principio, nunca aparecen a mitad de camino.");
                Bul("Corregido un cierre inesperado de la app en Windows al usar la Vista Previa mientras habia algo en vivo.");
                Bul("La Vista Previa de la Biblioteca ya no puede trabar ni afectar al video que esta en vivo para el publico.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Nuevo panel de Rendimiento");
                Bul("Panel opcional (menu Vista > Rendimiento) que muestra en vivo el uso de CPU, memoria RAM y los FPS de la app — util para saber si la computadora esta exigida durante un evento.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Overlays (nuevo)");
                Bul("Nueva seccion para crear tus propios overlays: imagenes con texto que podes acomodar libremente arrastrandolo por la pantalla.");
                Bul("Guardá tus overlays y usalos despues con un solo clic, igual que un fondo.");
                Bul("Podes editar o borrar los overlays guardados desde un menu rapido.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Vista en Vivo");
                Bul("Nuevos botones rapidos al costado de Vista en Vivo para limpiar el texto, quitar el fondo, ajustar la proporcion o silenciar el audio sin buscar en menus.");
                Bul("El panel de Control quedo mas simple: solo iniciar/detener la proyeccion y elegir la pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Fondos y Estilos");
                Bul("Los Fondos ahora se organizan en carpetas, mas faciles de navegar.");
                Bul("Nuevo control para agrandar o achicar las miniaturas y ver mas fondos o estilos a la vez.");
                Bul("Animaciones mas suaves al pasar el mouse y cambiar de seccion.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Interfaz general");
                Bul("Los 4 menus de iconos (Biblioteca, Control, Home y Diseño) se ven mas prolijos y del mismo tamaño entre si.");
                Bul("Podes ocultar los titulos debajo de los iconos (menu Vista) para ganar espacio en pantalla.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca y fuentes");
                Bul("Corregido: al importar una fuente nueva la app se ponia en negro y habia que reiniciarla para que se viera.");
                Bul("Al cambiar de categoria en la Biblioteca (Letra, Video, Biblia, etc.) la busqueda se limpia sola, para que un resultado vacio no se confunda con contenido que desaparecio.");
                Bul("El fondo de cada cancion ahora se elige de tu biblioteca de Fondos en vez de buscar un archivo suelto en la computadora.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Monitor de Control (Stage)");
                Bul("Nuevo boton en Vista en Vivo para alternar la previsualizacion entre Publico y Stage, y tener a la vista ambas salidas sin un segundo monitor.");
                Bul("[Experimental] Opcion para que el Monitor de Control muestre exactamente lo mismo que ve el operador en Vista en Vivo, en vez de la grilla de reloj/texto.");
                ImGui::Dummy(ImVec2(0,12));
            } else if (selectedUpdateVer == 6) {
                Cat("Audio");
                Bul("Sonido renovado: nueva pantalla de audio, portada por cancion, ecualizador y control de volumen.");
                Bul("Ahora podes asignar autores a las canciones.");
                Bul("Cambiar de cancion es mas rapido y con menos cortes de audio.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Reproduccion y previsualizacion");
                Bul("La Vista Previa y el video en vivo ahora son totalmente independientes: uno ya no afecta al otro.");
                Bul("Corregidas las pantallas negras en el segundo monitor y videos con la proporcion incorrecta.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Cola de reproduccion");
                Bul("La cola avanza de forma mas confiable entre videos, incluso si hay algun archivo eliminado o roto.");
                Bul("Corregidos casos donde la cola podia desincronizarse de lo que realmente se estaba mostrando.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblioteca");
                Bul("Biblioteca renovada, con listas y playlists mas faciles de usar.");
                Bul("Nuevo sistema de etiquetas de colores para organizar tus canciones.");
                Bul("Busqueda mejorada y navegacion con las flechas del teclado mas prolija.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Biblia");
                Bul("Nuevos atajos de teclado para buscar libro, capitulo o versiculo mas rapido (Ctrl+F, Ctrl y Alt).");
                Bul("Nueva seccion en Ajustes con todos los atajos disponibles.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Control de proyeccion");
                Bul("Mejor soporte para varios monitores (proyector y stage).");
                Bul("Panel de control mas simple, todo en una sola fila de botones.");
                Bul("El mute y el volumen ahora se mantienen sincronizados entre el control y el monitor.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Red local y streaming");
                Bul("Transmision por red local (LAN) mas estable, con menos cortes.");
                Bul("Corregidos errores de imagen y de marca de agua en la transmision.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estadisticas locales");
                Bul("Nuevo resumen en el Hub con el total de proyecciones y las canciones mas usadas.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte para Linux");
                Bul("ProyecThor ahora funciona de forma nativa en Linux, probado en Arch Linux y derivados.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Atajos de teclado globales");
                Bul("Ctrl+P, F1 y Alt+F4 ahora funcionan desde cualquier pantalla de la app (Preferencias, Ayuda y Cerrar).");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Interfaz y experiencia");
                Bul("Nuevo logo y mejoras visuales en varias secciones de la app.");
                Bul("Animaciones mas fluidas en el Hub principal.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Sistema y ajustes");
                Bul("Tus ajustes y preferencias se guardan y cargan correctamente entre sesiones.");
                Bul("Podes personalizar el idioma y la apariencia de la app.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Estabilidad general");
                Bul("Multiples correcciones para evitar que la app se cuelgue en biblioteca, streaming y multi-monitor.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y comunidad");
                Bul("Canal oficial de comunicacion y soporte en WhatsApp y Discord.");
            } else {
                Cat("General");
                Bul("Nuevo Hub central para administrar la app.");
                Bul("Codigo QR automatico para ver la transmision desde el celular.");
                Bul("Nuevas pantallas de bienvenida al iniciar la app.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Multimedia y Streaming");
                Bul("Mejoras en la transmision LAN y en la conexion de dispositivos.");
                Bul("Estilos de letras predeterminados segun el tipo de lista.");
                Bul("Reproduccion de video mas fluida.");
                Bul("Nueva opcion para transmitir fondos con la orientacion correcta.");
                Bul("Mejor rendimiento en la biblioteca y la vista previa.");
                ImGui::Dummy(ImVec2(0,12));

                Cat("Soporte y Estabilidad");
                Bul("Mejor manejo de archivos y mas estabilidad general.");
                Bul("Podes editar canciones sin perder el foco en pantalla.");
                Bul("Correcciones en la cola de reproduccion y en las transiciones.");
                Bul("Varias correcciones para evitar que la app se cuelgue.");
            }

            ImGui::Dummy(ImVec2(0,24));
            ImGui::EndGroup();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SetCursorPos(ImVec2(0, modalH-footerH));
            ImVec2 flp = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(flp.x,flp.y), ImVec2(flp.x+modalW,flp.y), HT::Divider, 1.0f);

            const float bw=130, bh=34;
            ImGui::SetCursorPos(ImVec2((modalW-bw)*0.5f, (modalH-footerH)+(footerH-bh)*0.5f));
            ImGui::PushStyleColor(ImGuiCol_Button,        HT::Surface);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HT::SurfaceHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  HT::SurfaceActive);
            ImGui::PushStyleColor(ImGuiCol_Text,          HT::TextPri);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, HT::RadiusSm);
            if (ImGui::Button("Cerrar", ImVec2(bw, bh)))
                m_IsUpdateModalOpen = false;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }

        ImGui::End();
        ImGui::PopStyleVar(4);
        ImGui::PopStyleColor(2);
    }
}

}
