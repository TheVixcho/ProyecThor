#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <cctype>

namespace ProyecThor::UI::Settings {

// ─────────────────────────────────────────────────────────────────────────────
//  Definición de categorías
// ─────────────────────────────────────────────────────────────────────────────
//  El índice de cada entrada es el ID real de la categoría (usado por
//  RenderContent() y por Hub::QuickBtn/m_ActiveTab para saltar a una
//  pestaña concreta) — NO reordenar este arreglo. El orden de *visualización*
//  en el sidebar, agrupado por tema, se controla aparte con k_NavGroups.

// Icono mínimo por categoría, dibujado a mano con primitivas de ImDrawList
// (sin depender de ningún PNG/asset externo) -- ver DrawCategoryIcon.
enum class CatIcon { Palette, Sliders, Monitor, Cast, Speaker, MusicNote, Keyboard, Globe, Download, Storage };

struct Category {
    const char* tag;
    const char* label;
    const char* description;
    CatIcon      icon;
    ImU32        color; // color de identidad de la categoría (icono + acento del ítem)
};

static const Category k_Categories[] = {
    { "UI",  "Apariencia",      "Colores, fuentes y efectos visuales",     CatIcon::Palette,   IM_COL32(185, 130, 245, 255) }, // 0
    { "PRY", "Proyección",      "Monitor, texto y márgenes",               CatIcon::Monitor,   IM_COL32( 70, 195, 220, 255) }, // 1
    { "CNX", "Conexiones",      "Red, app movil, streaming y OSC",         CatIcon::Cast,      IM_COL32(120, 160, 235, 255) }, // 2
    { "STG", "Pantallas",       "Monitor de confianza para el equipo",     CatIcon::Sliders,   IM_COL32( 80, 205, 165, 255) }, // 3
    { "SOU", "Audio",           "Volumen, dispositivo y fade",             CatIcon::Speaker,   IM_COL32(245, 165,  75, 255) }, // 4
    { "SNG", "Canciones",       "Etiquetas y opciones de canciones",       CatIcon::MusicNote, IM_COL32(235, 105, 165, 255) }, // 5
    { "KEY", "Teclas rápidas",  "Atajos de teclado disponibles",           CatIcon::Keyboard,  IM_COL32(230, 190,  70, 255) }, // 6
    { "LNG", "Idioma",          "Idioma de la interfaz",                   CatIcon::Globe,     IM_COL32(100, 205, 110, 255) }, // 7
    { "UPD", "Actualizaciones", "Versión instalada y canales",             CatIcon::Download,  IM_COL32(230, 100,  95, 255) }, // 8
    { "DAT", "Datos",           "Ubicación de archivos y carpetas vinculadas", CatIcon::Storage, IM_COL32(245, 185,  65, 255) }, // 9
};
static constexpr int k_CategoryCount = 10;

// Dibuja un glifo simple y reconocible para 'icon', centrado en 'c', con
// radio aproximado 'r' -- pensado para verse bien a ~8-9px de radio (18px
// de fila) en el color de identidad de cada categoría.
static void DrawCategoryIcon(ImDrawList* dl, CatIcon icon, ImVec2 c, float r, ImU32 color) {
    switch (icon) {
        case CatIcon::Palette: {
            dl->AddCircle(c, r, color, 16, 1.3f);
            float dr = r * 0.30f;
            dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.25f), dr, color);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.30f, c.y - r * 0.35f), dr, color);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.05f, c.y + r * 0.40f), dr, color);
            break;
        }
        case CatIcon::Sliders: {
            float w = r * 1.7f;
            float ys[3] = { c.y - r * 0.65f, c.y, c.y + r * 0.65f };
            float hx[3] = { c.x - w * 0.15f, c.x + w * 0.20f, c.x - w * 0.05f };
            for (int i = 0; i < 3; i++) {
                dl->AddLine(ImVec2(c.x - w * 0.5f, ys[i]), ImVec2(c.x + w * 0.5f, ys[i]), color, 1.4f);
                dl->AddCircleFilled(ImVec2(hx[i], ys[i]), r * 0.16f, color);
            }
            break;
        }
        case CatIcon::Monitor: {
            ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.65f), mx(c.x + r * 0.85f, c.y + r * 0.30f);
            dl->AddRect(mn, mx, color, 2.0f, 0, 1.3f);
            dl->AddLine(ImVec2(c.x, mx.y), ImVec2(c.x, mx.y + r * 0.35f), color, 1.3f);
            dl->AddLine(ImVec2(c.x - r * 0.35f, mx.y + r * 0.35f), ImVec2(c.x + r * 0.35f, mx.y + r * 0.35f), color, 1.3f);
            break;
        }
        case CatIcon::Cast: {
            ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.20f), mx(c.x + r * 0.30f, c.y + r * 0.50f);
            dl->AddRect(mn, mx, color, 2.0f, 0, 1.2f);
            ImVec2 arcCenter(mn.x, mx.y);
            dl->PathArcTo(arcCenter, r * 0.45f, -IM_PI * 0.5f, 0.0f, 8);
            dl->PathStroke(color, false, 1.2f);
            dl->PathClear();
            dl->PathArcTo(arcCenter, r * 0.78f, -IM_PI * 0.5f, 0.0f, 10);
            dl->PathStroke(color, false, 1.2f);
            break;
        }
        case CatIcon::Speaker: {
            ImVec2 bodyMin(c.x - r * 0.80f, c.y - r * 0.22f), bodyMax(c.x - r * 0.30f, c.y + r * 0.22f);
            dl->AddRectFilled(bodyMin, bodyMax, color, 1.0f);
            dl->AddTriangleFilled(
                ImVec2(c.x - r * 0.30f, c.y - r * 0.55f),
                ImVec2(c.x - r * 0.30f, c.y + r * 0.55f),
                ImVec2(c.x + r * 0.20f, c.y), color);
            dl->PathClear();
            dl->PathArcTo(ImVec2(c.x + r * 0.05f, c.y), r * 0.55f, -IM_PI * 0.28f, IM_PI * 0.28f, 8);
            dl->PathStroke(color, false, 1.2f);
            dl->PathClear();
            dl->PathArcTo(ImVec2(c.x + r * 0.05f, c.y), r * 0.85f, -IM_PI * 0.28f, IM_PI * 0.28f, 8);
            dl->PathStroke(color, false, 1.2f);
            break;
        }
        case CatIcon::MusicNote: {
            ImVec2 head(c.x - r * 0.35f, c.y + r * 0.45f);
            dl->AddCircleFilled(head, r * 0.30f, color);
            dl->AddLine(ImVec2(head.x + r * 0.28f, head.y - r * 0.05f), ImVec2(head.x + r * 0.28f, c.y - r * 0.75f), color, 1.5f);
            dl->AddLine(ImVec2(head.x + r * 0.28f, c.y - r * 0.75f), ImVec2(head.x + r * 0.68f, c.y - r * 0.50f), color, 1.5f);
            break;
        }
        case CatIcon::Keyboard: {
            float keyW = r * 0.42f, keyH = r * 0.34f, gap = r * 0.10f;
            for (int row = 0; row < 2; row++) {
                for (int col = 0; col < 3; col++) {
                    ImVec2 p0(c.x - r * 0.72f + col * (keyW + gap), c.y - r * 0.46f + row * (keyH + gap));
                    dl->AddRectFilled(p0, ImVec2(p0.x + keyW, p0.y + keyH), color, 1.0f);
                }
            }
            break;
        }
        case CatIcon::Globe: {
            dl->AddCircle(c, r * 0.85f, color, 20, 1.3f);
            dl->AddLine(ImVec2(c.x - r * 0.85f, c.y), ImVec2(c.x + r * 0.85f, c.y), color, 1.1f);
            dl->PathClear();
            for (int i = 0; i <= 20; i++) {
                float t = (float)i / 20.0f * IM_PI * 2.0f;
                dl->PathLineTo(ImVec2(c.x + cosf(t) * r * 0.32f, c.y + sinf(t) * r * 0.85f));
            }
            dl->PathStroke(color, true, 1.1f);
            break;
        }
        case CatIcon::Download: {
            dl->AddLine(ImVec2(c.x, c.y - r * 0.70f), ImVec2(c.x, c.y + r * 0.10f), color, 1.5f);
            dl->AddTriangleFilled(
                ImVec2(c.x - r * 0.35f, c.y - r * 0.05f),
                ImVec2(c.x + r * 0.35f, c.y - r * 0.05f),
                ImVec2(c.x, c.y + r * 0.35f), color);
            dl->AddLine(ImVec2(c.x - r * 0.6f, c.y + r * 0.65f), ImVec2(c.x + r * 0.6f, c.y + r * 0.65f), color, 1.5f);
            break;
        }
        case CatIcon::Storage: {
            float w = r * 1.30f, h = r * 0.40f;
            float ys[3] = { c.y - r * 0.50f, c.y, c.y + r * 0.50f };
            for (int i = 0; i < 3; i++) {
                dl->AddRectFilled(ImVec2(c.x - w * 0.5f, ys[i] - h * 0.5f),
                                  ImVec2(c.x + w * 0.5f, ys[i] + h * 0.5f), color, 2.0f);
                dl->AddCircleFilled(ImVec2(c.x + w * 0.30f, ys[i]), r * 0.12f, IM_COL32(20, 20, 25, 255));
            }
            break;
        }
    }
}

// Orden y agrupación visual del sidebar (por índice real de k_Categories).
// Reagrupa temas relacionados (p.ej. Stage/Canciones junto a Proyección)
// sin tocar los índices reales, así ningún QuickBtn/m_ActiveTab se rompe.
struct NavGroup { const char* label; const int items[4]; int count; };
static const NavGroup k_NavGroups[] = {
    { "APARIENCIA",      { 0,          }, 1 },
    { "PANTALLAS",       { 1, 2, 3     }, 3 }, // Proyección + Conexiones + Pantallas (Stage)
    { "AUDIO",           { 4, 5        }, 2 },
    { "SISTEMA Y DATOS", { 6, 7, 8, 9  }, 4 }, // Teclas + Idioma + Actualizaciones + Datos
};
static constexpr int k_NavGroupCount = 4;

// Pequeño helper local: convierte un token de color del tema (float[4]) en
// ImVec4, con un multiplicador opcional de alpha.
static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

// Sombra suave apilando rectángulos redondeados semitransparentes con
// desplazamiento creciente hacia abajo — el mismo truco de "blur pobre" que
// ya usan AnimatedProgressBar/SpinnerWidget en este archivo, sin necesidad
// de un shader de blur real. Da la sensación de "isla flotante" a cada
// panel (separada del resto de la ventana).
static inline void DrawFloatingIslandShadow(ImDrawList* dl, ImVec2 pos, ImVec2 size, float rounding) {
    const int   layers    = 5;
    const float maxOffset = 16.0f;
    for (int i = layers; i >= 1; --i) {
        float  t      = (float)i / (float)layers;
        float  offset = maxOffset * t;
        int    alpha  = (int)(30.0f * (1.0f - t * 0.6f));
        ImVec2 p0(pos.x - offset * 0.2f,          pos.y + offset * 0.3f);
        ImVec2 p1(pos.x + size.x + offset * 0.2f, pos.y + size.y + offset * 0.55f);
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, alpha), rounding + offset * 0.25f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

SettingsPanel::SettingsPanel()
    : m_SelectedCategory(0), m_PrevCategory(-1),
      m_SaveTimer(0.0f), m_CheckingAnim(0.0f) {}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::Render(bool* isOpen) {
    if (!*isOpen) { m_WasOpenLastFrame = false; return; }

    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float dt = ImGui::GetIO().DeltaTime;
    m_CheckingAnim += dt * 280.0f;

    // ── Detecta la transición cerrado -> abierto ────────────────────────────
    // Sin fade/pop-in: el panel aparece directo a tamaño y posición final,
    // sin animar nada (ver pedido explícito de sacar animaciones del panel).
    const bool justOpened = !m_WasOpenLastFrame;
    m_WasOpenLastFrame = true;

    // Ancho base subido de 900 a 1040: algunas subcategorias (Conexiones >
    // Red/Mobile/Streaming) embeben tarjetas en grilla (selector de modo a
    // 3 columnas, contenedor de resolucion, QR) pensadas para un panel
    // ancho -- con 900px + sidebar de 240 + padding quedaban aplastadas
    // contra el borde. 1040 les da un ancho de contenido util de ~670px.
    const ImVec2 baseSize(1040.0f, 700.0f);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    // Centrada y a tamaño fijo desde el primer frame -- sin animar el
    // tamaño de la ventana (un "pop-in" de escala hacía que las dos islas
    // se vieran reacomodándose/estirándose durante la apertura). La única
    // animación de apertura es el fade de alpha de más abajo.
    // Esto también evita depender de cualquier posición/tamaño guardado
    // previamente en el .ini (que es lo que hacía que la ventana apareciera
    // en una esquina, a veces fuera de la pantalla, al reabrir el panel).
    if (justOpened) {
        ImGui::SetNextWindowPos(workCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(baseSize, ImGuiCond_Always);
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(820, 560), ImVec2(FLT_MAX, FLT_MAX));

    // Este panel es una utilidad flotante independiente: nunca debe poder
    // acoplarse (dock) a otras ventanas ni aceptar que otras se acoplen a
    // él. DockingAllowUnclassed=false + ImGuiWindowFlags_NoDocking en el
    // Begin() de abajo son cinturón-y-tirantes para el mismo objetivo.
    // Nota: NO forzar ViewportFlagsOverrideSet=TopMost aquí -- con
    // multi-viewport activo eso entra en conflicto con el auto-merge de
    // ImGui (la ventana intenta fusionarse/separarse del viewport principal
    // cada frame) y se ve como un parpadeo/"intento de acople". El "siempre
    // adelante" ya lo cubre BringWindowToDisplayFront más abajo.
    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    // El "chrome" de la ventana (barra de título + botón de cerrar) sigue
    // siendo la ventana ImGui real -- eso es lo que se puede arrastrar y
    // cerrar -- pero su cuerpo queda solo levemente traslúcido (no 100%
    // transparente): el margen alrededor de las islas se ve como un marco
    // tenue en vez de un hueco vacío, mientras el fondo principal lo siguen
    // pintando las dos "islas" (sidebar / contenido) más abajo.
    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ThemeCol(theme.base, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ThemeCol(theme.surface0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ThemeCol(theme.surface1));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0, 0, 0, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // NoSavedSettings: no persistimos pos/tamaño en el .ini, así el panel
    // siempre vuelve a nacer centrado la próxima vez que se abra, sin
    // arrastrar coordenadas obsoletas de una resolución/monitor distinto.
    // NoDocking: ver comentario de floatingClass arriba.
    bool open = ImGui::Begin("Configuraciones", isOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);

    // Comodidad para el usuario: este panel siempre queda por delante de la
    // ventana principal, sin depender de tener el foco (p.ej. si el usuario
    // hace clic en un panel detrás mientras Ajustes sigue abierto, Ajustes
    // no debe quedar tapado).
    //
    // OJO: NO hacer esto mientras un popup NUESTRO esté abierto (combo,
    // color picker, el modal de reiniciar, etc.) -- reordenar la ventana
    // dueña de un popup en pleno vuelo rompe el chequeo interno de ImGui de
    // "sigue siendo la misma ventana en el mismo orden" y el popup se
    // cierra solo en el mismo frame en que se abre (bug real: el combo de
    // fuentes se abría y se cerraba de inmediato).
    //
    // La condición ANTERIOR (bloquear si HABÍA CUALQUIER popup abierto en
    // TODA la app) era demasiado amplia: cualquier tooltip/combo de OTRO
    // panel detrás también nos hacía saltar el reordenamiento ese frame, y
    // por eso el panel "se iba para atrás" o parpadeaba de forma
    // intermitente incluso sin tocar Ajustes. Ahora solo se salta si el
    // popup abierto es nuestro (Ajustes tiene foco/hover, en sí misma o en
    // una ventana hija/popup) -- un popup de cualquier OTRA parte de la app
    // no nos debe frenar de volver al frente.
    bool ownPopupOpen = ImGui::GetCurrentContext()->OpenPopupStack.Size > 0 &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
         ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup));
    if (!ownPopupOpen)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!open) { ImGui::End(); return; }

    ImVec2 avail  = ImGui::GetContentRegionAvail();
    const float margin  = 14.0f;  // aire entre el borde de la ventana y las islas
    const float gap     = 12.0f;  // "división al medio muy pequeña" entre ambas islas
    const float sideW   = 240.0f;
    const float footerH = 70.0f;
    const float islandH  = avail.y - margin * 2.0f;
    const float contentW = avail.x - margin * 2.0f - sideW - gap;

    ImVec2 origin(ImGui::GetCursorScreenPos().x + margin, ImGui::GetCursorScreenPos().y + margin);
    ImVec2 sideOrigin(origin.x, origin.y);
    ImVec2 contentOrigin(origin.x + sideW + gap, origin.y);

    ImDrawList* winDl = ImGui::GetWindowDrawList();
    DrawFloatingIslandShadow(winDl, sideOrigin,    ImVec2(sideW,    islandH), theme.windowRounding);
    DrawFloatingIslandShadow(winDl, contentOrigin, ImVec2(contentW, islandH), theme.windowRounding);

    // Pequeño conector central: una línea sutil a media altura del hueco,
    // que refuerza la lectura de "dos piezas unidas" sin partir la ventana
    // en dos de verdad.
    {
        float midX = sideOrigin.x + sideW + gap * 0.5f;
        float y0   = sideOrigin.y + islandH * 0.30f;
        float y1   = sideOrigin.y + islandH * 0.70f;
        ImU32 segCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.95f));
        winDl->AddLine(ImVec2(midX, y0), ImVec2(midX, y1), segCol, 1.5f);
    }

    // ── Isla derecha: contenido + barra de guardado (se dibuja PRIMERO) ──────
    // Se renderiza antes que el sidebar a propósito: RenderContent() es lo
    // que llena m_SectionAnchors (subcategorías) y calcula
    // m_ActiveSubsection para el frame actual -- si el sidebar se dibujara
    // primero, mostraría la lista de subcategorías de la categoría anterior
    // durante un frame cada vez que se cambia de categoría.
    ImGui::SetCursorScreenPos(contentOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##content_island", ImVec2(contentW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImVec2 islandAvail = ImGui::GetContentRegionAvail();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(45.0f, 40.0f));
    ImGui::BeginChild("##content_scroll", ImVec2(islandAvail.x, islandAvail.y - footerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    RenderContent();
    ImGui::EndChild();

    // La barra de guardado se dibuja mientras seguimos dentro de
    // "##content_island": así toma su rect (GetWindowPos/Size) y sus
    // esquinas inferiores redondeadas coinciden con las de esta isla, en
    // vez de abarcar todo el ancho de la ventana como antes.
    RenderSaveBar();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // ── Isla izquierda: navegación (usa m_SectionAnchors ya frescos) ─────────
    ImGui::SetCursorScreenPos(sideOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 25.0f));
    ImGui::BeginChild("##sidebar", ImVec2(sideW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    RenderSidebar();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::End();

    if (m_SaveTimer > 0.0f) {
        m_SaveTimer -= dt;
        if (m_SaveTimer <= 0.0f) m_SaveStatusMsg = "";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidebar
// ─────────────────────────────────────────────────────────────────────────────

// Minúsculas ASCII simple para el filtro del buscador -- las etiquetas y
// descripciones de k_Categories son español sin acentos raros en las
// palabras clave típicas de búsqueda (osc, red, audio, etc.), así que no
// hace falta nada más elaborado que tolower por byte.
static std::string ToLowerAscii(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return r;
}

void SettingsPanel::RenderSidebar() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImVec4 accent = ThemeCol(theme.accent);

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("ProyecThor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::Text("Versión %s", mgr.GetSettings().updates.currentVersion.c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 18.0f));

    // ── Buscador de configuraciones ─────────────────────────────────────
    // Filtra por etiqueta o descripción (ver k_Categories) -- pedido
    // explícito para no tener que escanear visualmente toda la lista
    // agrupada cuando el usuario ya sabe qué palabra busca.
    const float searchW = ImGui::GetContentRegionAvail().x - 20.0f;
    ImGui::SetCursorPosX(10.0f);
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ThemeCol(theme.surface2, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ThemeCol(theme.surface2, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ThemeCol(theme.surface3, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Text,           ThemeCol(theme.textPrimary));
        ImGui::PushStyleColor(ImGuiCol_Border,         ThemeCol(theme.border, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   9.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,    ImVec2(30.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::SetNextItemWidth(searchW);
        ImGui::InputTextWithHint("##settingsSearch", "Buscar ajustes...", m_SearchBuffer, sizeof(m_SearchBuffer));
        ImVec2 fMin = ImGui::GetItemRectMin();
        ImVec2 fMax = ImGui::GetItemRectMax();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        // Lupa dibujada a mano (mismo criterio que el resto de iconos de
        // este archivo, DrawCategoryIcon) -- circulo + mango diagonal.
        ImU32 glassCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint));
        ImVec2 gC(fMin.x + 15.0f, (fMin.y + fMax.y) * 0.5f);
        dl->AddCircle(gC, 5.2f, glassCol, 12, 1.4f);
        ImVec2 hDir(0.71f, 0.71f);
        dl->AddLine(ImVec2(gC.x + 5.2f * hDir.x,       gC.y + 5.2f * hDir.y),
                    ImVec2(gC.x + 5.2f * hDir.x * 1.85f, gC.y + 5.2f * hDir.y * 1.85f),
                    glassCol, 1.6f);

        // Boton "x" para limpiar, solo si hay texto.
        if (m_SearchBuffer[0] != '\0') {
            ImVec2 xC(fMax.x - 16.0f, (fMin.y + fMax.y) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(xC.x - 9.0f, xC.y - 9.0f));
            if (ImGui::InvisibleButton("##settingsSearchClear", ImVec2(18.0f, 18.0f)))
                m_SearchBuffer[0] = '\0';
            ImU32 xCol = ImGui::ColorConvertFloat4ToU32(
                ThemeCol(theme.textFaint, ImGui::IsItemHovered() ? 1.0f : 0.7f));
            dl->AddLine(ImVec2(xC.x - 3.5f, xC.y - 3.5f), ImVec2(xC.x + 3.5f, xC.y + 3.5f), xCol, 1.5f);
            dl->AddLine(ImVec2(xC.x - 3.5f, xC.y + 3.5f), ImVec2(xC.x + 3.5f, xC.y - 3.5f), xCol, 1.5f);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));

    const float itemH = 45.0f;
    const float itemW = ImGui::GetContentRegionAvail().x - 20.0f; // constante para toda la lista

    // Separamos en dos canales de dibujo: 0 = fondo animado (píldora),
    // 1 = hover/texto. Así la píldora que se desliza entre filas nunca
    // tapa el texto de la fila destino, sin importar el orden de dibujo.
    dl->ChannelsSplit(2);

    float targetPillY = -1.0f;

    // Dibuja una fila de categoría (icono con insignia circular de color +
    // label + subcategorías si está activa) -- extraído a lambda para que
    // tanto la navegación agrupada (k_NavGroups) como los resultados planos
    // del buscador compartan el mismo look, en vez de duplicar el bloque.
    auto RenderCategoryRow = [&](int i) {
        bool selected = (m_SelectedCategory == i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f; // Margen izquierdo

        ImGui::SetCursorPosX(10.0f);

        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));

        char selectId[32];
        snprintf(selectId, sizeof(selectId), "##nav%d", i);

        dl->ChannelsSetCurrent(1);

        bool clicked = ImGui::Selectable(selectId, selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH));
        bool hovered = ImGui::IsItemHovered() && !selected;

        if (clicked && m_SelectedCategory != i) {
            m_SelectedCategory = i;
        }

        if (selected) {
            targetPillY = p.y - ImGui::GetWindowPos().y;
        } else if (hovered) {
            dl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.5f)), 8.0f);
        }

        // Icono de identidad de la categoría, en su color propio (ver
        // k_Categories) -- a todo color cuando está seleccionada, algo
        // apagado en el resto para que no compitan visualmente entre sí.
        // Insignia circular detrás del icono (look "chip de color", estilo
        // Windows 11/macOS Ajustes) -- antes el icono flotaba solo contra
        // el fondo de la fila, se veía plano/aburrido.
        ImVec4 iconColV = ImGui::ColorConvertU32ToFloat4(k_Categories[i].color);
        ImVec2 badgeC(p.x + 22.0f, p.y + itemH * 0.5f);
        ImVec4 badgeFillV = iconColV;
        badgeFillV.w = selected ? 0.24f : (hovered ? 0.16f : 0.10f);
        dl->AddCircleFilled(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeFillV), 20);
        if (selected) {
            ImVec4 badgeRingV = iconColV; badgeRingV.w = 0.55f;
            dl->AddCircle(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeRingV), 20, 1.2f);
        }
        iconColV.w *= selected ? 1.0f : (hovered ? 0.90f : 0.62f);
        DrawCategoryIcon(dl, k_Categories[i].icon, badgeC, 8.5f,
            ImGui::ColorConvertFloat4ToU32(iconColV));

        float labelY = p.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f;
        float labelX = p.x + 42.0f;
        ImVec4 labelColV = selected ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textDim, hovered ? 1.0f : 0.85f);
        dl->AddText(ImVec2(labelX, labelY), ImGui::ColorConvertFloat4ToU32(labelColV), k_Categories[i].label);

        ImGui::PopStyleColor(3);

        // Subcategorías: solo se muestran para la categoría activa (igual
        // que el menú de referencia). Cada una es su propia "página" --
        // clickear una reemplaza por completo el contenido mostrado (ver
        // m_SelectedSubsection, RenderContent/SectionTitle), no un salto de
        // scroll dentro de una página más larga.
        if (selected && !m_SectionAnchors.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            for (size_t si = 0; si < m_SectionAnchors.size(); si++) {
                const std::string& subLabel = m_SectionAnchors[si];
                bool isActiveSub = (subLabel == m_SelectedSubsection);

                ImGui::SetCursorPosX(34.0f);
                ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ThemeCol(theme.surface2, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ThemeCol(theme.surface2, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    isActiveSub ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textFaint));
                ImGui::SetWindowFontScale(0.88f);

                std::string subId = subLabel + "##sub" + std::to_string(i) + "_" + std::to_string(si);
                if (ImGui::Selectable(subId.c_str(), isActiveSub, ImGuiSelectableFlags_None, ImVec2(itemW - 24.0f, 24.0f))) {
                    m_SelectedSubsection = subLabel;
                }

                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopStyleColor(4);
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }
    };

    const bool searching = m_SearchBuffer[0] != '\0';

    if (searching) {
        // Lista plana de resultados, sin encabezados de grupo -- el usuario
        // ya escribió lo que busca, agrupar por tema solo estorbaría.
        std::string query = ToLowerAscii(m_SearchBuffer);
        int matches = 0;
        for (int i = 0; i < k_CategoryCount; i++) {
            std::string label = ToLowerAscii(k_Categories[i].label);
            std::string desc  = ToLowerAscii(k_Categories[i].description);
            if (label.find(query) == std::string::npos && desc.find(query) == std::string::npos)
                continue;
            matches++;
            RenderCategoryRow(i);
        }
        if (matches == 0) {
            ImGui::SetCursorPosX(25.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textFaint));
            ImGui::TextUnformatted("Sin resultados.");
            ImGui::PopStyleColor();
        }
    } else {
        // Navegación agrupada por tema (ver k_NavGroups): en vez de una lista
        // plana de categorías, se muestran en bloques con un encabezado
        // pequeño ("GENERAL", "PANTALLAS", "AUDIO", "SISTEMA", "CONEXIONES"),
        // más fácil de escanear visualmente.
        for (int g = 0; g < k_NavGroupCount; g++) {
            const NavGroup& group = k_NavGroups[g];

            if (g > 0) ImGui::Dummy(ImVec2(0.0f, 16.0f));
            ImGui::SetCursorPosX(25.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textFaint));
            ImGui::SetWindowFontScale(0.82f);
            ImGui::TextUnformatted(group.label);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));

            for (int gi = 0; gi < group.count; gi++)
                RenderCategoryRow(group.items[gi]);
        }
    }

    // Píldora de selección: posición fija en la fila activa, sin animar
    // (ver pedido explícito de sacar animaciones del panel).
    if (targetPillY >= 0.0f) {
        dl->ChannelsSetCurrent(0);

        ImVec2 pillP(ImGui::GetWindowPos().x + 10.0f, ImGui::GetWindowPos().y + targetPillY);

        ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.30f));
        ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
        dl->AddRectFilledMultiColor(pillP, ImVec2(pillP.x + itemW, pillP.y + itemH), colLeft, colRight, colRight, colLeft);

        dl->AddRectFilled(ImVec2(pillP.x, pillP.y + 8.0f), ImVec2(pillP.x + 4.0f, pillP.y + itemH - 8.0f),
                          ImGui::ColorConvertFloat4ToU32(accent), 2.0f);
    }

    dl->ChannelsMerge();

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Área de contenido
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderContent() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    const float dt    = ImGui::GetIO().DeltaTime;
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec4      accent = ThemeCol(theme.accent);

    // Al cambiar de categoría, ninguna subcategoría de la nueva es todavía
    // válida (podría ni existir en la categoría anterior) -- se limpia acá
    // para que SectionTitle() auto-seleccione la primera que aparezca.
    if (m_SelectedCategory != m_PrevCategory) {
        m_PrevCategory = m_SelectedCategory;
        m_SelectedSubsection.clear();
    }

    // Se reconstruye entera cada frame (la vuelve a llenar SectionTitle() a
    // medida que la categoría activa dibuja sus secciones, y decide cuál de
    // ellas dibuja su cuerpo -- ver comentario largo en el .h) -- RenderSidebar()
    // la usa para mostrar las subcategorías de la categoría seleccionada.
    m_SectionAnchors.clear();

    // Cambiar de categoría O de subcategoría arranca esa "página" siempre
    // desde arriba -- sin esto, una página corta podía heredar el scroll a
    // mitad de camino de la página larga que se estaba viendo antes.
    if (m_SelectedSubsection != m_PrevSubsection) {
        m_PrevSubsection = m_SelectedSubsection;
        ImGui::SetScrollY(0.0f);
    }

    // Título de la sección. Sin subtítulo/descripción debajo -- pedido
    // explícito de sacarlo (sobraba: cada categoría ya explica lo suyo en
    // el cuerpo, y el nombre de la categoría + la subcategoría activa en el
    // sidebar ya dicen dónde está parado el usuario).
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(k_Categories[m_SelectedCategory].label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Separador líquido (degradado que se desvanece), color = acento del tema
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImU32 sepAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.6f));
    ImU32 sepFade   = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), sepAccent, sepFade, sepFade, sepAccent);

    ImGui::Dummy(ImVec2(0.0f, 25.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme.frameRounding);

    switch (m_SelectedCategory) {
        case 0: RenderCategoryTheme();       break;
        case 1: RenderCategoryProjection();  break;
        case 2: RenderCategoryConnections(); break;
        case 3: RenderCategoryStage();       break;
        case 4: RenderCategoryAudio();       break;
        case 5: RenderCategorySongs();       break;
        case 6: RenderCategoryShortcuts();   break;
        case 7: RenderCategoryLanguage();    break;
        case 8: RenderCategoryUpdates();     break;
        case 9: RenderCategoryData();        break;
        default: ImGui::TextDisabled("Categoría no implementada."); break;
    }

    ImGui::PopStyleVar(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Footer / barra de guardado
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::RenderSaveBar() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float  barH    = 70.0f;
    float  barY    = winPos.y + winSize.y - barH;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.98f)),
        theme.windowRounding,
        ImDrawFlags_RoundCornersBottom);

    dl->AddLine(
        ImVec2(winPos.x, barY),
        ImVec2(winPos.x + winSize.x, barY),
        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.9f)),
        1.5f);

    float btnY   = winSize.y - barH + (barH - 36.0f) * 0.5f;
    float rightX = winSize.x - 30.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18.0f);

    // Botón Guardar — usa el color de acento del tema activo
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.accent, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.accentLight));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.accentDim));
    ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));

    if (ImGui::Button("Guardar ajustes", ImVec2(160.0f, 36.0f))) {
        mgr.SaveSettings();
        mgr.ApplyTheme();
        mgr.ApplyProjection();
        m_SaveStatusMsg = "Cambios guardados";
        m_SaveTimer     = 3.0f;
    }
    ImGui::PopStyleColor(4);

    // Botón Restablecer
    ImGui::SetCursorPos(ImVec2(rightX - 160.0f - 15.0f - 130.0f, btnY));
    ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.surface2, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.surface3, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.surface1, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textDim));

    if (ImGui::Button("Restablecer", ImVec2(130.0f, 36.0f))) {
        mgr.ResetToDefaults();
        m_SaveStatusMsg = "Ajustes restablecidos";
        m_SaveTimer     = 3.0f;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (!m_SaveStatusMsg.empty()) {
        // Solo se desvanece con el timer al final (no "aparece" con
        // movimiento) -- ver pedido de sacar animaciones del panel.
        float alpha = std::min(1.0f, m_SaveTimer);

        float textY = winSize.y - barH + (barH - ImGui::GetTextLineHeight()) * 0.5f;

        // Antes offset por sideW (240px del sidebar) porque la barra abarcaba
        // toda la ventana; ahora vive solo dentro de la isla de contenido, así
        // que el mensaje arranca desde el borde izquierdo de esa isla.
        ImGui::SetCursorPos(ImVec2(45.0f, textY));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.success, alpha));
        ImGui::TextUnformatted(m_SaveStatusMsg.c_str());
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers Animados y Visuales (Glassy effect)
// ─────────────────────────────────────────────────────────────────────────────

void SettingsPanel::AnimatedProgressBar(float fraction, ImVec2 size, ImVec4 col) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       t   = (float)ImGui::GetTime();

    // Fondo barra tipo tubo de cristal
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(10, 20, 40, 200), size.y * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 90, 160, 100), size.y * 0.5f, 0, 1.5f);

    if (fraction > 0.0f) {
        float fillW = fraction * size.x;
        if (fillW < size.y) fillW = size.y; // Evitar artefactos gráficos

        // Degradado vertical para simular un cilindro líquido
        ImU32 colTop = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x + 0.2f, col.y + 0.2f, col.z + 0.2f, 0.9f));
        ImU32 colBot = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x - 0.1f, col.y - 0.1f, col.z - 0.1f, 0.9f));

        ImVec2 pMin = pos;
        ImVec2 pMax = ImVec2(pos.x + fillW, pos.y + size.y);

        // ClipRect para asegurar que el relleno respete las curvas
        dl->PushClipRect(pMin, pMax, true);
        dl->AddRectFilledMultiColor(pMin, pMax, colTop, colTop, colBot, colBot);

        // Brillo ondulante (Líquido)
        float wave = sinf(t * 3.0f + pos.x) * 0.5f + 0.5f;
        dl->AddRectFilled(pMin, ImVec2(pMax.x, pMin.y + size.y * 0.3f), IM_COL32(255, 255, 255, (int)(40 + 30 * wave)), size.y * 0.5f);
        dl->PopClipRect();
    }

    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", (int)(fraction * 100.0f));
    ImVec2 textSize = ImGui::CalcTextSize(pctText);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x + 15.0f, pos.y + (size.y - textSize.y) * 0.5f));
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", pctText);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 12.0f));
    ImGui::Dummy(ImVec2(0, 0));
}

void SettingsPanel::SpinnerWidget(float radius, float thickness, const ImVec4& color) {
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();
    float       cx  = pos.x + radius;
    float       cy  = pos.y + radius;
    float       t   = (float)ImGui::GetTime();

    // Aro de fondo cristalino oscuro
    dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(20, 40, 80, 150), 32, thickness);

    // Segmento rotatorio con resplandor (Neon effect)
    const int   numSegments = 32;
    const float startAngle  = t * 5.0f;
    const float arcSpan     = IM_PI * 1.2f;

    dl->PathClear();
    for (int i = 0; i <= numSegments; i++) {
        float angle = startAngle + arcSpan * ((float)i / numSegments);
        dl->PathLineTo(ImVec2(cx + cosf(angle) * radius, cy + sinf(angle) * radius));
    }

    // Dibujamos dos veces, una más gruesa y transparente para simular el brillo exterior (Glow)
    ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.4f));
    dl->PathStroke(glowCol, false, thickness + 3.0f);

    // Y el núcleo brillante
    for (int i = 0; i <= numSegments; i++) {
        float angle = startAngle + arcSpan * ((float)i / numSegments);
        dl->PathLineTo(ImVec2(cx + cosf(angle) * radius, cy + sinf(angle) * radius));
    }
    dl->PathStroke(ImGui::ColorConvertFloat4ToU32(color), false, thickness);

    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: SectionTitle (usa acento del tema)
// ─────────────────────────────────────────────────────────────────────────────
bool SettingsPanel::SectionTitle(const char* label, const char* navGroup) {
    // Registra esta sección como subcategoría navegable (ver RenderSidebar /
    // m_SectionAnchors), agrupando por navGroup si se pasó uno: varios
    // SectionTitle con el mismo grupo comparten UNA sola entrada en el
    // sidebar y se muestran/ocultan juntos -- ver los 3 bloques de
    // Streaming en CategoryProjection.cpp o Temas/Colores/Fuentes/Diseño en
    // CategoryTheme.cpp.
    const char* group = navGroup ? navGroup : label;
    bool alreadyRegistered = false;
    for (const auto& g : m_SectionAnchors) {
        if (g == group) { alreadyRegistered = true; break; }
    }
    if (!alreadyRegistered) {
        m_SectionAnchors.emplace_back(group);
        // La primera subcategoría que aparece en el frame, mientras
        // ninguna esté seleccionada todavía (recién se entró a esta
        // categoría, ver RenderContent), se auto-selecciona -- así la
        // página nunca aparece vacía.
        if (m_SelectedSubsection.empty())
            m_SelectedSubsection = group;
    }

    // Esta sección NO es la subcategoría visible ahora mismo: pedido
    // explícito de que cada subcategoría se vea "una por una" en vez de
    // todas juntas en un scroll largo -- no dibuja ni el título ni el
    // cuerpo que el caller ponga a continuación.
    if (m_SelectedSubsection != group)
        return false;

    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::Dummy(ImVec2(0.0f, 15.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.accentLight));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.55f));
    ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y), ImVec2(p.x + w, p.y + 1.5f), colLeft, colRight, colRight, colLeft);

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: HelpTooltip (tooltip de cristal, usa colores del tema)
// ─────────────────────────────────────────────────────────────────────────────
void SettingsPanel::HelpTooltip(const char* desc) {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.accentDim));
    ImGui::TextDisabled("(?)");
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ThemeCol(theme.surface1, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.accent, 0.55f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, theme.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

        ImGui::BeginTooltip();
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::PushTextWrapPos(280.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndTooltip();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
}

void SettingsPanel::InitializeTheme() {}
} // namespace ProyecThor::UI::Settings