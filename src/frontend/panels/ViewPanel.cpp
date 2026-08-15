#include "ViewPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/views/Announcements.h"
#include "frontend/views/OClock.h"
#include "capture/CapturePanel.h"
#include "TeamChatPanel.h"
#include "MonitorTheme.h"
#include "MonitorDesign.h"
#include "MonitorUIHelpers.h"
#include "frontend/ui/LoadingSpinner.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/home/HomeIcons.h"
#include "frontend/ui/IconRail.h"
#include "frontend/ui/LiveContentRenderer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <iostream>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include "stb_image.h"
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace ProyecThor::UI {

static constexpr float kQuickActionsRailW = 40.0f;
// Franja horizontal de config (RenderQuickActionsConfig), abajo de todo el
// panel -- antes era un segundo riel vertical a la izquierda, movido para
// devolverle ese ancho al video (ver ViewPanel::Render).
static constexpr float kConfigStripH = 34.0f;

namespace {

namespace MT = MonitorTheme;
namespace fs = std::filesystem;
using namespace Design;
using namespace Components;

// ─────────────────────────────────────────────────────────────────────────────
//  Overlays guardados (ver Biblioteca > Overlay / OverlayLibraryTab) — acceso
//  rapido de solo-lectura para RenderOverlaysPopup: misma carpeta que usa
//  OverlayLibraryTab (%APPDATA%\ProyecThor\assets\overlays), sin depender de
//  esa clase (que ademas necesita UIManager para el editor, que este popup
//  no usa -- solo lista y aplica).
// ─────────────────────────────────────────────────────────────────────────────
struct OverlayThumbEntry { std::string name, pngPath; };

fs::path OverlaysDirReadOnly() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    return fs::path(buf) / "ProyecThor" / "assets" / "overlays";
#else
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / ".local" / "share" / "ProyecThor" / "assets" / "overlays";
#endif
}

std::vector<OverlayThumbEntry> ListSavedOverlays() {
    std::vector<OverlayThumbEntry> out;
    std::error_code ec;
    fs::path dir = OverlaysDirReadOnly();
    if (!fs::exists(dir, ec)) return out;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".overlay") continue;
        std::string name = e.path().stem().string();
        fs::path png = dir / (name + ".png");
        if (fs::exists(png)) out.push_back({ name, png.string() });
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

ImTextureID LoadOverlayThumbTex(const char* path) {
    int w, h, n;
    unsigned char* d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    return (ImTextureID)(intptr_t)tex;
}

ImVec4 ToVec4(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }
ImVec4 Brighten(const ImVec4& c, float amount)
{
    return ImVec4(
        std::clamp(c.x + amount, 0.0f, 1.0f),
        std::clamp(c.y + amount, 0.0f, 1.0f),
        std::clamp(c.z + amount, 0.0f, 1.0f),
        c.w);
}

// ─────────────────────────────────────────────────────────────────────────────
//  DrawPadButton — icono simple, circular, en un solo tono por estado (ver
//  llamadas en RenderLiveTransport: gris neutro para las acciones momentaneas
//  -replay/forward/stop-, acento del tema para el estado "encendido" -Play
//  mientras esta en vivo, Mute activo-). "lit" ya no oscurece el color base
//  (antes lo dejaba practicamente invisible en botones que nunca pasan por
//  el estado "prendido"): ahora solo agrega el halo de brillo, para pedir
//  "botón fisico iluminado" sin perder legibilidad en el resto.
// ─────────────────────────────────────────────────────────────────────────────
bool DrawPadButton(const char* iconName, float iconSize, ImVec4 padColor, ImVec2 btnSize, bool lit,
                    DrawIconFn vectorIcon = nullptr)
{
    ImVec4 baseCol = padColor;
    ImVec4 hovCol  = Brighten(baseCol, 0.12f);
    ImVec4 actCol  = Brighten(padColor, -0.10f);

    ImGui::PushStyleColor(ImGuiCol_Button,        baseCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  hovCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   actCol);
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, lit ? 0.40f : 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   std::min(btnSize.x, btnSize.y) * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.3f);

    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::Button("", btnSize);
    bool   isHeld  = ImGui::IsItemActive();
    ImVec2 p1      = { p0.x + btnSize.x, p0.y + btnSize.y };

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Halo suave detras del icono cuando el pad esta prendido -- sensacion
    // de luz interna en vez de un simple resaltado de hover.
    if (lit)
    {
        ImVec2 center = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
        dl->AddCircleFilled(center, btnSize.y * 0.55f,
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)), 24);
    }

    auto it = StyleGeneralApp::Icons.find(iconName);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);

    float  offsetY = isHeld ? 2.0f : 0.0f;
    ImVec2 center  = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f + offsetY };

    // Tinte de icono por contraste contra el propio color del pad (luma de
    // padColor), no blanco fijo -- pedido explicito: con temas claros,
    // padColor (MT::k_NeutBtn, etc) puede quedar CLARO, y un icono blanco
    // fijo se volvia invisible encima. Mismo criterio que HubTheme::OnAccent.
    const float iconLuma = 0.299f * baseCol.x + 0.587f * baseCol.y + 0.114f * baseCol.z;
    const ImU32 iconTint = (iconLuma > 0.55f) ? IM_COL32(20, 20, 24, 255) : IM_COL32(255, 255, 255, 255);

    // Guarda de textura: antes se le pasaba a AddImage un ImTextureID nulo
    // cuando la textura no estaba cargada, y algunos backends lo dibujan
    // como un icono de basura en vez de nada (ver captura del usuario en el
    // pad de mute). Con vectorIcon como respaldo -- mismo criterio que
    // QuickActionButton -- y si tampoco hay uno, se deja el pad solo con su
    // color, sin icono, que es preferible a mostrar basura.
    if (hasTexture)
    {
        dl->AddImage((ImTextureID)(intptr_t)it->second.textureID,
            { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f },
            { center.x + iconSize * 0.5f, center.y + iconSize * 0.5f },
            ImVec2(0, 0), ImVec2(1, 1),
            iconTint);
    }
    else if (vectorIcon)
    {
        ImVec2 origin = { center.x - iconSize * 0.5f, center.y - iconSize * 0.5f };
        vectorIcon(dl, origin, iconSize, iconTint);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return pressed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  HorizontalFader — deslizante estilo canal de mesa de sonido: surco angosto
//  con marcas de escala + un "cap" vertical que se arrastra, en vez de un
//  slider generico. Horizontal (no vertical): en este panel el ancho sobra
//  pero el alto es escaso (fila baja y ancha debajo del video), asi que una
//  columna vertical no entraba sin recortarse -- ver captura del usuario.
//  Click/arrastre mapea directo la posicion X del mouse al valor (mismo
//  criterio inmediato que DS::ModernSlider). Vive aca y no en
//  MonitorUIHelpers porque por ahora solo lo pide este panel.
// ─────────────────────────────────────────────────────────────────────────────
bool HorizontalFader(const char* id, float* value, float lo, float hi, ImVec2 size,
                      ImU32 trackCol, ImU32 fillCol, ImU32 capCol)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;

    const float capW       = 14.0f;
    const float trackLeft  = pos.x + capW * 0.5f;
    const float trackRight = pos.x + size.x - capW * 0.5f;
    const float trackWpx   = std::max(1.0f, trackRight - trackLeft);

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hi > lo)
    {
        float t = std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / trackWpx, 0.0f, 1.0f);
        float newVal = lo + t * (hi - lo);
        if (newVal != *value) { *value = newVal; changed = true; }
    }

    float frac = (hi > lo) ? std::clamp((*value - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    float capX = trackLeft + frac * trackWpx;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    const float trackH  = 6.0f;
    float       cy      = pos.y + size.y * 0.5f;

    dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { trackRight, cy + trackH * 0.5f },
                       trackCol, trackH * 0.5f);

    if (capX - trackLeft > 0.5f)
        dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { capX, cy + trackH * 0.5f },
                           fillCol, trackH * 0.5f);

    // Marcas de escala, como en una consola real.
    for (int i = 0; i <= 4; i++)
    {
        float mx = trackLeft + trackWpx * (float)i / 4.0f;
        dl->AddLine({ mx, cy - size.y * 0.30f }, { mx, cy - trackH * 0.7f },
                     IM_COL32(255, 255, 255, 35), 1.0f);
    }

    float  capHalfH = size.y * 0.40f;
    ImVec2 capMin   = { capX - capW * 0.5f, cy - capHalfH };
    ImVec2 capMax   = { capX + capW * 0.5f, cy + capHalfH };
    ImU32  capBody  = (hovered || active)
        ? ImGui::GetColorU32(Brighten(ToVec4(capCol), 0.10f))
        : capCol;

    dl->AddRectFilled(capMin, capMax, capBody, 3.0f);
    dl->AddRect(capMin, capMax, IM_COL32(0, 0, 0, 110), 3.0f, 0, 1.2f);
    dl->AddLine({ capX, capMin.y + 4.0f }, { capX, capMax.y - 4.0f }, IM_COL32(0, 0, 0, 130), 1.5f);

    return changed;
}

// Altavoz — para el pad de Mute del transporte. No hay textura
// "volume_up"/"no_sound" cargada en StyleGeneralApp::Icons (el pad
// terminaba pasandole un ImTextureID nulo a AddImage, que en este backend
// se ve como basura -- ver captura del usuario). Caja + cono triangular a
// mano, mismo criterio que DrawIcon_Disc/DrawIcon_Gear; una linea diagonal
// en vez de las ondas de sonido cuando esta muteado.
void DrawSpeakerShape(ImDrawList* dl, ImVec2 o, float sz, ImU32 col, bool muted)
{
    ImVec2 c = { o.x + sz * 0.5f, o.y + sz * 0.5f };

    float  boxHalfH = sz * 0.16f;
    ImVec2 boxMin   = { c.x - sz * 0.42f, c.y - boxHalfH };
    ImVec2 boxMax   = { c.x - sz * 0.16f, c.y + boxHalfH };
    dl->AddRectFilled(boxMin, boxMax, col, 1.0f);

    ImVec2 apex    = { boxMax.x, c.y };
    ImVec2 baseTop = { c.x + sz * 0.16f, c.y - sz * 0.34f };
    ImVec2 baseBot = { c.x + sz * 0.16f, c.y + sz * 0.34f };
    dl->AddTriangleFilled(apex, baseTop, baseBot, col);

    if (muted)
    {
        dl->AddLine({ o.x + sz * 0.06f, o.y + sz * 0.94f },
                    { o.x + sz * 0.94f, o.y + sz * 0.06f }, col, sz * 0.09f);
    }
    else
    {
        for (int i = 1; i <= 2; i++)
        {
            float r = sz * (0.14f + 0.13f * (float)i);
            dl->PathArcTo({ c.x + sz * 0.10f, c.y }, r, -0.62f, 0.62f, 10);
            dl->PathStroke(col, 0, sz * 0.055f);
        }
    }
}
void DrawIcon_SpeakerOn(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)    { DrawSpeakerShape(dl, o, sz, col, false); }
void DrawIcon_SpeakerMuted(ImDrawList* dl, ImVec2 o, float sz, ImU32 col) { DrawSpeakerShape(dl, o, sz, col, true);  }

// Disco/vinilo — para "Detener disco en vivo" (no hay textura "album" cargada
// en StyleGeneralApp::Icons; se dibuja a mano con el mismo estilo geometrico
// que HomeIcons::DrawIcon_Clock/DrawIcon_Broadcast en vez de agregar un PNG).
void DrawIcon_Disc(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)
{
    ImVec2 center = { o.x + sz * 0.5f, o.y + sz * 0.5f };
    dl->AddCircle(center, sz * 0.40f, col, 24, sz * 0.06f);
    dl->AddCircle(center, sz * 0.24f, col, 20, sz * 0.035f);
    dl->AddCircleFilled(center, sz * 0.07f, col, 12);
}

// Boton de icono simple para las franjas de acciones rapidas (riel derecho +
// franja de config) — flat, esquinas cuadradas (sin redondeo, pedido
// explicito), separado del resto por espaciado en vez de la antigua grilla
// tipo hoja de calculo (celdas pegadas + linea negra de 1px entre cada una).
// Un chip de acento fino abajo del icono marca el estado activo/encendido en
// vez de la barra lateral que usaba la version anterior.
//
// iconKey busca una textura en StyleGeneralApp::Icons; si no hay ninguna
// registrada con ese nombre, se usa vectorIcon (dibujado a mano, ver
// AppIcons.h/HomeIcons.h/DrawIcon_Disc/DrawIcon_Gear) en su lugar — el
// pedido explicito fue "no quiero letras", así que el glifo de texto ya no
// se usa como ultimo recurso salvo que ninguno de los dos este disponible.
bool QuickActionButton(const char* id, const char* iconKey, DrawIconFn vectorIcon,
                       const char* fallbackGlyph, const char* tooltip, ImVec2 size,
                       ImVec4 bgColor, ImVec4 hoverColor, ImVec4 activeColor, ImVec4 tint,
                       bool toggledOn)
{
    ImVec4 restColor = toggledOn ? activeColor : bgColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        restColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text,          tint);

    auto it = StyleGeneralApp::Icons.find(iconKey);
    bool hasTexture = (it != StyleGeneralApp::Icons.end() && it->second.textureID != nullptr);
    bool hasIcon    = hasTexture || vectorIcon != nullptr;
    std::string label = (hasIcon ? "" : std::string(fallbackGlyph)) + "##" + id;

    bool clicked = ImGui::Button(label.c_str(), size);

    ImVec2 bMin = ImGui::GetItemRectMin();
    ImVec2 bMax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (hasTexture)
    {
        const float iconSide = std::min(size.x, size.y) * 0.42f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 pMin    = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        const ImVec2 pMax    = { center.x + iconSide * 0.5f, center.y + iconSide * 0.5f };

        dl->AddImage(it->second.textureID, pMin, pMax,
            ImVec2(0, 0), ImVec2(1, 1),
            ImGui::ColorConvertFloat4ToU32(tint));
    }
    else if (vectorIcon)
    {
        const float iconSide = std::min(size.x, size.y) * 0.48f;
        const ImVec2 center  = { (bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f };
        const ImVec2 origin  = { center.x - iconSide * 0.5f, center.y - iconSide * 0.5f };
        vectorIcon(dl, origin, iconSide, ImGui::ColorConvertFloat4ToU32(tint));
    }

    // Chip de acento fino, centrado abajo del icono, cuando el estado esta
    // activo/encendido -- reemplaza la barra lateral + linea de celda negra
    // que tenia la versión "hoja de calculo" anterior.
    if (toggledOn)
    {
        ImU32 accent = ImGui::ColorConvertFloat4ToU32(tint);
        float chipW = (bMax.x - bMin.x) * 0.36f;
        float cx = (bMin.x + bMax.x) * 0.5f;
        dl->AddRectFilled({ cx - chipW * 0.5f, bMax.y - 4.0f }, { cx + chipW * 0.5f, bMax.y - 2.0f },
                          accent, 1.0f);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", tooltip);

    return clicked;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pads (ver RenderPadsPopup) — movido tal cual desde ViewToolsPanel (mismo
//  comportamiento). Tabla de iconos elegibles: reusa dibujos vectoriales ya
//  existentes (AppIcons/HomeIcons, misma firma en los dos headers), no hace
//  falta agregar assets nuevos. PadSettings::iconIndex es la posicion en
//  esta tabla (no un nombre), asi que el orden importa para la persistencia.
// ─────────────────────────────────────────────────────────────────────────────
using PadIconDrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);
struct PadIconEntry { const char* name; PadIconDrawFn draw; };

static const PadIconEntry kPadIcons[] = {
    { "Mixer",      AppIcons::DrawIcon_Mixer     },
    { "Monitor",    AppIcons::DrawIcon_Monitor   },
    { "Capas",      AppIcons::DrawIcon_Layers    },
    { "Paleta",     AppIcons::DrawIcon_Palette   },
    { "Overlay",    AppIcons::DrawIcon_Overlay   },
    { "Tipografia", AppIcons::DrawIcon_TextAa    },
    { "Transición", AppIcons::DrawIcon_Swap      },
    { "Shader",     AppIcons::DrawIcon_Shader    },
    { "Home",       HomeIcons::DrawIcon_Home     },
    { "Reloj",      HomeIcons::DrawIcon_Clock    },
    { "Anuncios",   HomeIcons::DrawIcon_Megaphone},
    { "Notas",      HomeIcons::DrawIcon_Notepad  },
    { "Camara",     HomeIcons::DrawIcon_Camera   },
    { "Red",        HomeIcons::DrawIcon_Broadcast},
    { "Chat",       HomeIcons::DrawIcon_Chat     },
};
static constexpr int kPadIconCount = (int)(sizeof(kPadIcons) / sizeof(kPadIcons[0]));

const PadIconEntry& PadIconFor(int index)
{
    return kPadIcons[std::clamp(index, 0, kPadIconCount - 1)];
}

// Grilla de selección de icono, usada dentro del submenu "Elegir icono" del
// menu contextual de cada pad. Devuelve true si el usuario eligio uno nuevo.
bool RenderPadIconGrid(int& iconIndex)
{
    bool changed = false;
    const int   cols    = 5;
    const float cellSz  = 34.0f;
    const float spacing = 6.0f;

    for (int i = 0; i < kPadIconCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        const bool sel = (i == iconIndex);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? MT::k_PrevBtn : ImVec4(1,1,1,0.04f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_PrevBtnAct);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        bool clicked = ImGui::Button("##padIcon", ImVec2(cellSz, cellSz));
        ImVec2 p = ImGui::GetItemRectMin();
        ImVec2 s = ImGui::GetItemRectSize();
        float  iconSz = cellSz * 0.55f;
        kPadIcons[i].draw(ImGui::GetWindowDrawList(),
                          { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, ImGui::GetColorU32(ImVec4(1,1,1,0.92f)));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", kPadIcons[i].name);

        if (clicked) { iconIndex = i; changed = true; }
        ImGui::PopID();
    }
    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Guardar/aplicar un Pad — junta las dos partes independientes (Captura,
//  Estilo+Fondo) via las APIs ya existentes de cada subsistema. Nunca toca
//  la letra/texto en pantalla (PresentationState::currentText) a proposito
//  -- eso es lo unico que un Pad no guarda.
// ─────────────────────────────────────────────────────────────────────────────
using PadSettings = ProyecThor::Settings::PadSettings;

void SavePad(PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();

    if (auto* cap = core.GetCapturePanelRef())
        pad.hasCapture = cap->SnapshotCurrentCapture(pad.capture);
    else
        pad.hasCapture = false;

    auto state = core.GetState();
    pad.hasStyle       = true;
    pad.styleSize      = state.textSize;
    for (int c = 0; c < 4; c++) pad.styleColor[c] = state.textColor[c];
    pad.styleHAlign    = state.textAlignment;
    pad.styleVAlign    = state.vAlignment;
    for (int c = 0; c < 4; c++) pad.styleMargins[c] = state.margins[c];
    pad.styleAutoScale = state.autoScale;
    pad.styleFontName  = state.selectedFont;
    pad.bgType = (int)state.bgType;
    pad.bgPath = state.bgPath;
    for (int c = 0; c < 3; c++) pad.bgColor[c] = state.bgColor[c];

    pad.assigned = pad.hasCapture || pad.hasStyle;
    ProyecThor::Settings::SettingsManager::Get().Save();
}

void ApplyPad(const PadSettings& pad)
{
    auto& core = Core::PresentationCore::Get();
    using BgType = Core::PresentationState::BackgroundType;

    if (pad.hasCapture) {
        if (auto* cap = core.GetCapturePanelRef())
            cap->ApplyCaptureScene(pad.capture);
    }

    if (pad.hasStyle) {
        // Los Pads solo capturan la caja de Letras (SavePad arriba lee los
        // campos planos de PresentationState, que son espejo de lyricsBox
        // -- ver ApplyLyricsBoxToState). Se reconstruye una caja
        // (TextBoxStyle) desde los margenes planos guardados. El Indice no
        // es parte de un Pad -- queda deshabilitado, igual que el default
        // de un SavedStyle nuevo.
        Core::TextBoxStyle box;
        box.sizeW    = std::max(0.02f, (1920.0f - pad.styleMargins[0] - pad.styleMargins[2]) / 1920.0f);
        box.sizeH    = std::max(0.02f, (1080.0f - pad.styleMargins[1] - pad.styleMargins[3]) / 1080.0f);
        box.posX     = pad.styleMargins[0] / 1920.0f + box.sizeW * 0.5f;
        box.posY     = pad.styleMargins[1] / 1080.0f + box.sizeH * 0.5f;
        box.textSize = pad.styleSize;
        for (int c = 0; c < 4; c++) box.color[c] = pad.styleColor[c];
        box.hAlign    = pad.styleHAlign;
        box.vAlign    = pad.styleVAlign;
        box.autoScale = pad.styleAutoScale;
        box.fontName  = pad.styleFontName;

        Core::SavedStyle snap;
        snap.lyrics = box;
        core.ApplyStyleSnapshot(snap);

        switch ((BgType)pad.bgType) {
            case BgType::SolidColor:
                core.SetLayer0_Color(pad.bgColor[0], pad.bgColor[1], pad.bgColor[2]);
                break;
            case BgType::Video:
                core.SetBackgroundMedia(pad.bgPath, true, false);
                break;
            case BgType::Audio:
                core.SetBackgroundAudio();
                break;
        }
    }
}

} // namespace

void ViewPanel::Render()
{
    // Alt Gr + 3: si Vista en Vivo esta colapsada (o pasando el punto medio
    // de la animacion), no dibujar la ventana ni su rail de acciones -- la
    // salida real al publico/Stage no depende de esto (ver
    // RenderLiveOutputWindows, siempre corre aparte).
    if (m_UIManager && m_UIManager->IsPanelCollapsedForRender(GetName()))
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, MT::k_Bg3);

    bool visible = ImGui::Begin("Vista en Vivo");

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(1);

    if (visible)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Children con padding cero — el estilo global usa WindowPadding
        // (22,18), que aquí sólo recortaría el video y el riel angosto.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        // Modo compacto horizontal (ver RenderCompactWide) -- el panel quedo
        // notablemente mas ancho que alto (ej. franja superior completa en
        // Ajustes > Apariencia > Entorno de trabajo > Transmisión). Apilar
        // video/transporte/config verticalmente como en el modo de siempre
        // dejaria un video minusculo dentro de una franja baja.
        const bool wideShort = avail.x > avail.y * 1.8f && avail.y > 8.0f;

        if (wideShort)
        {
            const bool showQuickActionsCompact = ProyecThor::Settings::SettingsManager::Get()
                                                        .GetSettings().general.showViewQuickActions;
            RenderCompactWide(avail, showQuickActionsCompact);
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }

        // Riel/franja opcional desde Vista > "Botones de limpieza (Vista en
        // Vivo)" — apagado, el video se queda con todo el espacio. Solo el
        // riel de "Limpiar <tipo>" es vertical (a la derecha, junto al
        // video); config es una franja horizontal abajo de TODO el panel,
        // asi no le resta ancho al video por los dos costados.
        const bool  showQuickActions = ProyecThor::Settings::SettingsManager::Get()
                                            .GetSettings().general.showViewQuickActions;
        // En una columna angosta (ej. "Vista en Vivo" en Ajustes > Apariencia
        // > Entorno de trabajo > Simple), el video es 16:9 y su alto se
        // deriva de su ancho -- restarle 40px mas al riel de "Limpiar" lo
        // encogia todavia mas sin necesidad real (esos 40px valen mucho mas
        // ahi que en un panel ancho). Se oculta el riel en vez de encimarlo,
        // igual criterio que RenderCompactWide sacrifica la herramienta
        // inline cuando el panel es ancho-y-bajo.
        const bool  narrowColumn = avail.x < 340.0f;
        const bool  showClearRail = showQuickActions && !narrowColumn;
        const float railW      = showClearRail ? kQuickActionsRailW : 0.0f;
        const float stripH     = showQuickActions ? kConfigStripH : 0.0f;
        // Ya no se le resta stripH aca -- la franja de config se movio DENTRO
        // de la columna de video (pegada debajo del transporte, ver mas
        // abajo), en vez de vivir anclada al borde inferior de TODO el panel
        // (pedido explicito: la toolbar va arriba del contenido que abre,
        // Overlays/Chat/Pads, no abajo).
        const float topAreaH   = avail.y;
        const float contentW   = std::max(0.0f, avail.x - railW);

        if (contentW > 8.0f && topAreaH > 8.0f)
        {
            ImGui::BeginChild("##viewVideoArea", ImVec2(contentW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Relacion de aspecto real de la salida (la del monitor
            // configurado en Ajustes > Proyeccion) — se calcula antes de
            // todo porque tanto el video principal como la tira de Stage
            // (mas abajo) letterboxean contra la MISMA proporcion, sea 16:9
            // o cualquier otra resolución "rara" que use el operador.
            float srcAspect = 1920.0f / 1080.0f;
            {
                int monitorCount = 0;
                GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
                auto state = Core::PresentationCore::Get().GetState();
                if (monitors && monitorCount > 0 &&
                    state.targetMonitorIndex >= 0 && state.targetMonitorIndex < monitorCount) {
                    if (const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
                        mode && mode->width > 0 && mode->height > 0) {
                        srcAspect = (float)mode->width / (float)mode->height;
                    }
                }
            }

            // Puntos "Público"/"Stage" + "Borrar Todo" -- se mudaron a la
            // toolbar superior (ver UIManager::RenderModeToolbarStatusActions),
            // pedido explicito para liberarle este espacio a "Vista en Vivo".
            const float dotsH = 0.0f;

            // La tira de preview de Stage (mostrada arriba de Publico en
            // simultaneo) se retiro -- pedido explicito: era redundante con
            // "vaPreviewSource", que ya permite alternar TODO el recuadro
            // entre Publico/Stage sin pararse frente al segundo monitor.
            const float topReservedH = dotsH;

            // Se reserva stripH aca (no restando del topAreaH general, ver
            // arriba) para que la franja de config quede DENTRO de esta
            // columna, pegada debajo del transporte -- ver uso mas abajo.
            const float remain2   = std::max(0.0f, topAreaH - topReservedH - stripH);
            const float minVideoH = 40.0f;

            // El video nunca se lleva mas del 65% de lo que queda, aunque
            // "quisiera" mas (relacion de aspecto muy vertical) — así el
            // transporte siempre conserva un piso usable. std::clamp() en
            // este libstdc++ hace assert si hi < lo, y con remain2 chico
            // (panel muy bajo) "remain2*0.65f" puede quedar por debajo de
            // minVideoH — de ahi el std::max() en cada limite superior, para
            // que el clamp nunca reciba un rango invertido pase lo que pase
            // con el alto disponible.
            float naturalVideoH = contentW / std::max(0.1f, srcAspect);
            float videoH     = std::clamp(naturalVideoH, minVideoH, std::max(minVideoH, remain2 * 0.65f));
            // FIX: un panel angosto y muy alto (poco ancho -> poco alto
            // "natural" de 16:9, pero mucho remain2 vertical) hacia que ANTES
            // se le devolviera TODO el sobrante a videoH, mucho mas alla de
            // lo que su aspecto realmente necesita — RenderContent letterboxea
            // puertas adentro, asi que ese alto de mas no sumaba video, solo
            // franjas negras enormes arriba/abajo del recuadro real (el
            // operador lo veia como "espacio roto" entre el video y el
            // transporte).
            //
            // El transporte SIEMPRE usa su alto minimo/fijo -- ya no se
            // estira para "rellenar" el sobrante (eso hacia que la franja de
            // config, pegada debajo, se moviera de lugar segun hubiera o no
            // una herramienta inline abierta; pedido explicito: la toolbar
            // tiene que quedar SIEMPRE en la misma posicion). Lo que sobre
            // despues del transporte se le da a la herramienta inline
            // (Overlays/Chat/Pads) si hay una abierta; si no, se deja en
            // blanco al fondo del panel, debajo de la toolbar -- nunca
            // "flotando" entre el transporte y la franja, que es lo único
            // que se pidio evitar.
            const bool  toolActive   = (m_ActiveTool != InlineTool::None);
            const float sobrante     = std::max(0.0f, remain2 - videoH);
            float transportH = std::min(kLiveTransportMinH, sobrante);
            float inlineToolH = toolActive ? std::max(0.0f, sobrante - transportH) : 0.0f;

            ImGui::SetCursorPosY(topReservedH);
            RenderContent(contentW, videoH);

            // RenderContent centra el video (letterbox) y puede dejar el
            // cursor antes de videoH — se fuerza la posicion para que cada
            // seccion arranque justo donde corresponde, sin importar cuanto
            // del alto reservado ocupo el letterbox.
            ImGui::SetCursorPosY(topReservedH + videoH);
            RenderLiveTransport(contentW, transportH);

            // Franja de config, pegada debajo del transporte (antes vivia
            // anclada al borde inferior de TODO el panel, debajo de
            // cualquier herramienta inline abierta -- pedido explicito: la
            // toolbar va ARRIBA del contenido que abre, no abajo).
            if (showQuickActions)
            {
                ImGui::SetCursorPosY(topReservedH + videoH + transportH);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
                ImGui::BeginChild("##viewQuickActionsConfigStrip", ImVec2(contentW, stripH), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                RenderQuickActionsConfig(stripH);
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }

            if (inlineToolH > 8.0f)
            {
                ImGui::SetCursorPosY(topReservedH + videoH + transportH + stripH);
                RenderInlineTool(contentW, inlineToolH);
            }

            ImGui::EndChild();
        }

        if (showClearRail)
        {
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
            ImGui::BeginChild("##viewQuickActions", ImVec2(railW, topAreaH), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            RenderQuickActionsClear(railW);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
    }

    ImGui::End();
}

void ViewPanel::RenderQuickActionsClear(float railW)
{
    auto& core = Core::PresentationCore::Get();

    auto* announcements= core.GetAnnouncementsRef();
    auto* oclock       = core.GetOClockRef();
    auto* capturePanel = core.GetCapturePanelRef();

    // Cada uno de estos refleja si TODAVIA hay algo de ESE tipo especifico
    // para limpiar — el boton se resalta (amarillo) mientras la funcion que
    // "elimina" sigue activa, y se apaga solo apenas se limpia. En vez de
    // resaltar por hover como el resto de la app, ver pedido original.
    bool showText  = core.GetState().showText;
    bool discLive  = core.GetState().bgType == Core::PresentationState::BackgroundType::Audio;
    bool bgLive    = core.GetState().bgType != Core::PresentationState::BackgroundType::SolidColor;
    bool annLive   = announcements && announcements->IsLive();
    bool clockLive = oclock && oclock->IsLive();
    bool capLive   = capturePanel && capturePanel->IsLive();
    bool overlayLive = core.HasOverlay();

    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeContent= Design::k_EQ_Yellow;
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);
    ImVec4 tintOnYellow = ImVec4(0.10f, 0.09f, 0.06f, 1.0f);
    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);

    struct ActionDef {
        const char* id;
        const char* icon;          // clave en StyleGeneralApp::Icons (textura), o "" si no hay
        DrawIconFn  vectorIcon;    // dibujado a mano — se usa si icon no tiene textura cargada
        const char* fallbackGlyph; // ultimo recurso si ninguno de los dos aplica (no deberia pasar)
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    // "Limpiar <tipo especifico>" — uno por cada capa de contenido que puede
    // estar en vivo. Pedido explicito: solo iconos, nada de letras — donde
    // no habia una textura ya cargada (album/imagen/campana/reloj/camara) se
    // reusan los iconos vectoriales ya dibujados a mano en otras partes de
    // la app (AppIcons.h/HomeIcons.h) o se agregan nuevos chicos aca mismo
    // (Disco) — ver DrawIcon_Disc arriba.
    ActionDef actions[7] = {
        { "vaClearText", "", AppIcons::DrawIcon_TextAa, "Aa", "Limpiar texto",
          hoverClear, activeContent, showText,  showText  ? tintOnYellow : textPrimary },
        { "vaClearDisc", "", DrawIcon_Disc, "Dsc", "Detener disco en vivo",
          hoverClear, activeContent, discLive,  discLive  ? tintOnYellow : textPrimary },
        { "vaClearBg",   "delete", nullptr, "BG",  "Quitar fondo",
          hoverClear, activeContent, bgLive,    bgLive    ? tintOnYellow : textPrimary },
        { "vaClearOverlay", "", AppIcons::DrawIcon_Overlay, "Ovl", "Quitar overlay",
          hoverClear, activeContent, overlayLive, overlayLive ? tintOnYellow : textPrimary },
        { "vaClearAnn",  "", HomeIcons::DrawIcon_Megaphone, "Anc", "Detener anuncios",
          hoverClear, activeContent, annLive,   annLive   ? tintOnYellow : textPrimary },
        { "vaClearClock","", HomeIcons::DrawIcon_Clock, "Rlj", "Quitar reloj",
          hoverClear, activeContent, clockLive, clockLive ? tintOnYellow : textPrimary },
        { "vaClearCap",  "", HomeIcons::DrawIcon_Camera, "Cap", "Detener captura",
          hoverClear, activeContent, capLive,   capLive   ? tintOnYellow : textPrimary },
    };

    // Iconos chicos y espaciados, no celdas pegadas de hoja de calculo.
    const float  cellPad = 4.0f;
    const ImVec2 cellSize(railW - cellPad * 2.0f, railW - cellPad * 2.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
    ImGui::Dummy(ImVec2(railW, 4.0f));

    for (int i = 0; i < 7; i++)
    {
        ImGui::SetCursorPosX(cellPad);
        if (QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn))
        {
            if (i == 0)      core.ClearLayer2();
            else if (i == 1) core.StopBackgroundMedia();
            else if (i == 2) core.StopBackgroundMedia();
            else if (i == 3) core.ClearOverlay();
            else if (i == 4 && announcements) announcements->SetLive(false);
            else if (i == 5 && oclock)        oclock->StopTransmitting();
            else if (i == 6 && capturePanel)  capturePanel->Stop();
        }
    }

    ImGui::PopStyleVar();
}

void ViewPanel::RenderQuickActionsConfig(float stripH)
{
    auto& core = Core::PresentationCore::Get();
    bool stretchOn = core.GetStretchToFill();

    ImVec4 baseFill     = ToVec4(DS::BtnDefaultFill);
    ImVec4 hoverClear   = ToVec4(DS::AccentColorDim);
    ImVec4 activeStretch= ToVec4((DS::AccentColor & 0x00FFFFFFu) | (140u << 24));
    ImVec4 textPrimary  = ToVec4(DS::TextPrimary);

    struct ActionDef {
        const char* id;
        const char* icon;
        DrawIconFn  vectorIcon;
        const char* fallbackGlyph;
        const char* tooltip;
        ImVec4      hoverColor;
        ImVec4      activeColor;
        bool        toggledOn;
        ImVec4      tint;
    };

    // Utilidades de vista/configuración -- separadas de "Limpiar <tipo>"
    // (riel derecho) a pedido explicito, para no mezclar accion destructiva
    // con ajuste de vista. Mute/Desmute se saco de aca (pedido explicito,
    // sobraba: el mismo control ya esta en RenderLiveTransport).
    const bool previewingAlt = (m_PreviewSource != PreviewSource::Publico);
    const char* previewSourceLabel[4] = { "Público", "Stage", "Transmisión", "Inalámbrica" };
    int previewSourceIdx = static_cast<int>(m_PreviewSource);
    int nextPreviewSourceIdx = (previewSourceIdx + 1) % 4;
    char previewSourceTooltip[96];
    snprintf(previewSourceTooltip, sizeof(previewSourceTooltip), "Viendo: %s (click para ver %s)",
             previewSourceLabel[previewSourceIdx], previewSourceLabel[nextPreviewSourceIdx]);

    // Overlays/Chat/Pads ya no abren un popup flotante: alternan que se
    // muestra en la herramienta inline de abajo (ver m_ActiveTool /
    // RenderInlineTool en Render()) -- toggledOn refleja si esa herramienta
    // es la que esta abierta ahora mismo, para que el chip de acento marque
    // el boton activo como cualquier otro toggle de esta franja.
    const bool overlaysOn = (m_ActiveTool == InlineTool::Overlays);
    const bool chatOn     = (m_ActiveTool == InlineTool::Chat);
    const bool padsOn     = (m_ActiveTool == InlineTool::Pads);
    const bool clockOn    = (m_ActiveTool == InlineTool::Clock);

    // "Ajustes" se saco de aca (pedido explicito: "quita configuraciones y
    // mueve reloj ahi") -- ya esta a un click en la toolbar superior/menu,
    // asi que no hacia falta duplicarlo aca. En su lugar, acceso rapido al
    // Reloj (ver LibraryPanel::LibrarySideMode::Clock / OClock), mismo
    // patron inline que Overlays/Chat/Pads.
    ActionDef actions[6] = {
        { "vaStretch",   stretchOn ? "original_screen" : "fit_screen", nullptr, stretchOn ? "1:1" : "Fit",
          "Alternar proporción", hoverClear, activeStretch, stretchOn, textPrimary },
        { "vaClock",     "", HomeIcons::DrawIcon_Clock, "Rlj", "Reloj",
          hoverClear, activeStretch, clockOn, textPrimary },
        { "vaPreviewSource", "", AppIcons::DrawIcon_Swap, "Vis",
          previewSourceTooltip,
          hoverClear, activeStretch, previewingAlt, textPrimary },
        { "vaOverlays",  "", AppIcons::DrawIcon_Overlay, "Ovl", "Overlays",
          hoverClear, activeStretch, overlaysOn, textPrimary },
        { "vaChat",      "", HomeIcons::DrawIcon_Chat, "Cht", "Chat",
          hoverClear, activeStretch, chatOn, textPrimary },
        { "vaPads",      "", AppIcons::DrawIcon_Pads, "Pds", "Pads",
          hoverClear, activeStretch, padsOn, textPrimary },
    };

    // Rectangulos ESTIRADOS a lo ancho, pegados unos a otros sin huecos
    // (pedido explicito: "no que se vean separados sino que el fondo
    // alargado como rectangulos") -- el alto se mantiene chico (mismo que
    // los pads de Reproduccion), asi que el icono (basado en min(w,h), ver
    // QuickActionButton) no crece aunque el rectangulo sea mas ancho.
    constexpr int kCount = 6;
    constexpr float kGap = 2.0f;
    const float   btnH   = std::min(stripH - 4.0f, 28.0f);
    const float   totalW = ImGui::GetContentRegionAvail().x;
    const float   cellW  = (totalW - kGap * (kCount - 1)) / (float)kCount;
    const ImVec2  cellSize(cellW, btnH);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::SetCursorPosY((stripH - btnH) * 0.5f);

    for (int i = 0; i < kCount; i++)
    {
        if (i > 0) ImGui::SameLine(0.0f, kGap);

        bool clicked = QuickActionButton(actions[i].id, actions[i].icon, actions[i].vectorIcon, actions[i].fallbackGlyph,
                               actions[i].tooltip, cellSize, baseFill, actions[i].hoverColor,
                               actions[i].activeColor, actions[i].tint, actions[i].toggledOn);

        if (clicked)
        {
            if (i == 0)      core.SetStretchToFill(!stretchOn);
            else if (i == 1) m_ActiveTool = clockOn    ? InlineTool::None : InlineTool::Clock;
            else if (i == 2) m_PreviewSource = static_cast<PreviewSource>(nextPreviewSourceIdx);
            else if (i == 3) m_ActiveTool = overlaysOn ? InlineTool::None : InlineTool::Overlays;
            else if (i == 4) m_ActiveTool = chatOn     ? InlineTool::None : InlineTool::Chat;
            else if (i == 5) m_ActiveTool = padsOn     ? InlineTool::None : InlineTool::Pads;
        }
    }

    ImGui::PopStyleVar();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderCompactWide — ver comentario en Render() y en el header. Toda la
//  toolbar (transporte + config + limpiar) se apila en UNA columna angosta a
//  la izquierda con scroll propio, en vez de reservar altura debajo del
//  video -- asi el video usa el 100% del alto de una franja baja y ancha.
//  No soporta la herramienta inline (Overlays/Chat/Pads, ver
//  RenderInlineTool): una franja de este tipo (top strip de "Transmisión")
//  no tiene alto libre para abrirla igual, y es un caso de uso raro ahi.
// ─────────────────────────────────────────────────────────────────────────────
void ViewPanel::RenderCompactWide(ImVec2 avail, bool showQuickActions)
{
    const float leftW  = showQuickActions ? 220.0f : 0.0f;
    const float videoW = std::max(0.0f, avail.x - leftW);

    if (showQuickActions && leftW > 8.0f && avail.x > leftW + 40.0f)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
        ImGui::BeginChild("##viewCompactToolbar", ImVec2(leftW, avail.y), false,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        const float transportH = std::min(kLiveTransportMinH, std::max(60.0f, avail.y));
        RenderLiveTransport(leftW, transportH);

        ImGui::SetCursorPosY(transportH);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
        ImGui::BeginChild("##viewCompactConfig", ImVec2(leftW, kConfigStripH), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        RenderQuickActionsConfig(kConfigStripH);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(transportH + kConfigStripH + 6.0f);
        ImGui::SetCursorPosX(std::max(0.0f, (leftW - kQuickActionsRailW) * 0.5f));
        ImGui::BeginGroup();
        RenderQuickActionsClear(kQuickActionsRailW);
        ImGui::EndGroup();

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 0.0f);
    }

    ImGui::BeginChild("##viewCompactVideo", ImVec2(videoW, avail.y), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderContent(videoW, avail.y);
    ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderInlineTool — contenido de Overlays/Chat/Pads, dibujado EN EL MISMO
//  panel (no en un popup flotante aparte, pedido explicito) dentro de un
//  child con scroll propio, ocupando el espacio libre entre el transporte y
//  la franja de config (ver Render()).
// ─────────────────────────────────────────────────────────────────────────────
void ViewPanel::RenderInlineTool(float w, float h)
{
    if (m_ActiveTool == InlineTool::None) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MT::k_PadLg, MT::k_Pad));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleColor(ImGuiCol_Border,  MT::k_BorderSubtle);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   MT::k_R);

    ImGui::BeginChild("##viewInlineTool", { w, h }, true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    switch (m_ActiveTool)
    {
        case InlineTool::Overlays: RenderOverlaysContent(); break;
        case InlineTool::Chat:     RenderChatContent();     break;
        case InlineTool::Pads:     RenderPadsContent();     break;
        case InlineTool::Clock:    RenderClockContent();    break;
        default: break;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// Mismo OClock que Biblioteca > Reloj (ver LibraryPanel::LibrarySideMode::
// Clock y PresentationCore::GetOClockRef) -- una unica instancia real,
// dibujada aca "en linea" para poder arrancarla/pararla sin salir de Vista
// en Vivo.
void ViewPanel::RenderClockContent()
{
    auto* oclock = Core::PresentationCore::Get().GetOClockRef();
    if (oclock && m_UIManager)
        oclock->Render(m_UIManager->GetGlassRenderer());
    else
        ImGui::TextDisabled("Reloj no disponible.");
}

void ViewPanel::RenderOverlaysContent()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("OVERLAYS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    std::vector<OverlayThumbEntry> overlays = ListSavedOverlays();

    if (overlays.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
        ImGui::TextWrapped("Sin overlays guardados todavia. Creá uno desde Biblioteca > Overlay.");
        ImGui::PopStyleColor();
        return;
    }

    constexpr float kCardW = 84.0f, kCardH = 52.0f, kGap = 8.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const int   cols   = std::max(1, (int)((availW + kGap) / (kCardW + kGap)));
    int col = 0;

    for (const auto& ov : overlays)
    {
        ImGui::PushID(ov.name.c_str());

        auto it = m_OverlayThumbCache.find(ov.pngPath);
        if (it == m_OverlayThumbCache.end())
            it = m_OverlayThumbCache.emplace(ov.pngPath, LoadOverlayThumbTex(ov.pngPath.c_str())).first;
        ImTextureID thumb = it->second;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = { p0.x + kCardW, p0.y + kCardH };
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(MT::k_Bg2), MT::k_R);
        if (thumb) dl->AddImageRounded(thumb, p0, p1, { 0, 0 }, { 1, 1 }, IM_COL32_WHITE, MT::k_R);
        dl->AddRect(p0, p1, ImGui::GetColorU32(MT::k_BorderSubtle), MT::k_R);

        std::string dn = ov.name.length() > 12 ? ov.name.substr(0, 10) + "..." : ov.name;
        ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
        dl->AddRectFilled({ p0.x, p1.y - 16.0f }, p1, IM_COL32(0, 0, 0, 170), MT::k_R, ImDrawFlags_RoundCornersBottom);
        dl->AddText({ p0.x + (kCardW - ns.x) * 0.5f, p1.y - 15.0f }, ImGui::GetColorU32(MT::k_TextWhite), dn.c_str());

        if (ImGui::InvisibleButton("##ovApply", { kCardW, kCardH }))
        {
            Core::PresentationCore::Get().SetOverlayMedia(ov.pngPath);
            m_ActiveTool = InlineTool::None;
        }

        col++;
        if (col < cols) ImGui::SameLine(0.0f, kGap);
        else { col = 0; ImGui::Dummy(ImVec2(0.0f, kGap)); }

        ImGui::PopID();
    }
}

void ViewPanel::RenderChatContent()
{
    // El child "##viewInlineTool" que envuelve esto ya tiene un alto fijo,
    // recalculado desde cero cada frame en Render() (no acumulado) -- a
    // diferencia del viejo popup, ImGui::GetContentRegionAvail() dentro de
    // TeamChatPanel::RenderContent() no puede retroalimentarse en un loop de
    // "mas contenido -> ventana mas alta", así que no hace falta forzar un
    // tamaño fijo con ImGuiCond_Always como antes.
    if (m_TeamChatPanelRef)
        m_TeamChatPanelRef->RenderContent();
    else
        ImGui::TextDisabled("Chat no disponible.");
}

// Pads — movido tal cual desde ViewToolsPanel::RenderPads (mismo
// comportamiento, ver los helpers en el namespace anonimo de arriba).
void ViewPanel::RenderPadsContent()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentColor));
    ImGui::TextUnformatted("PADS");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextSecondary));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 290.0f);
    ImGui::TextWrapped("Click: aplicar. Click derecho: guardar lo que hay en pantalla "
                       "(captura + estilo/fondo + overlay activo, no la letra) o elegir icono.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    auto& padsArr = ProyecThor::Settings::SettingsManager::Get().GetSettings().pads.pads;

    const int   cols    = 4;
    const float btnSize = 56.0f;
    const float spacing = 10.0f;

    for (int i = 0; i < ProyecThor::Settings::kPadCount; i++)
    {
        if (i % cols != 0) ImGui::SameLine(0.0f, spacing);

        auto& pad = padsArr[i];
        const auto& icon = PadIconFor(pad.iconIndex);

        ImVec4 fillCol = pad.assigned ? MT::k_PrevBtn : ImVec4(MT::k_PrevBtn.x, MT::k_PrevBtn.y, MT::k_PrevBtn.z, 0.12f);
        ImVec4 bordCol = pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.35f) : MT::k_BorderSubtle;

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button,        fillCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   MT::k_PrevBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Border,         bordCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   10.0f);

        bool clicked = ImGui::Button("##pad", ImVec2(btnSize, btnSize));

        ImVec2 p       = ImGui::GetItemRectMin();
        ImVec2 s       = ImGui::GetItemRectSize();
        float  iconSz  = btnSize * 0.42f;
        ImU32  iconCol = ImGui::GetColorU32(pad.assigned ? ImVec4(1.0f, 1.0f, 1.0f, 0.92f) : MT::k_TextDim);
        icon.draw(ImGui::GetWindowDrawList(),
                  { p.x + (s.x - iconSz) * 0.5f, p.y + (s.y - iconSz) * 0.5f }, iconSz, iconCol);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (clicked && pad.assigned) ApplyPad(pad);

        if (ImGui::BeginPopupContextItem("##padCtx")) {
            if (ImGui::MenuItem(pad.assigned ? "Guardar aquí (reemplazar)" : "Guardar aquí"))
                SavePad(pad);

            if (ImGui::BeginMenu("Elegir icono")) {
                if (RenderPadIconGrid(pad.iconIndex))
                    ProyecThor::Settings::SettingsManager::Get().Save();
                ImGui::EndMenu();
            }

            if (pad.assigned) {
                ImGui::Separator();
                if (ImGui::MenuItem("Borrar pad")) {
                    pad = PadSettings{};
                    ProyecThor::Settings::SettingsManager::Get().Save();
                }
            }
            ImGui::EndPopup();
        }

        if (pad.assigned && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            std::string tip = "Pad " + std::to_string(i + 1);
            if (pad.hasCapture) tip += "\n- Captura";
            if (pad.hasStyle)   tip += "\n- Estilo y fondo";
            ImGui::SetTooltip("%s", tip.c_str());
        }

        ImGui::PopID();
    }

    if (auto* cap = Core::PresentationCore::Get().GetCapturePanelRef())
        cap->RenderSceneButtons();
}

// NOTA: RenderStatusDots/StatusDotToggle/ToggleAudience/ToggleStageQuick y
// el botón "Borrar Todo" que vivian aca se mudaron a UIManager.cpp
// (RenderModeToolbarStatusActions), pedido explicito para subirlos a la
// toolbar superior y liberarle este espacio a "Vista en Vivo".

// ─────────────────────────────────────────────────────────────────────────────
//  RenderLiveTransport — transporte + VU meters del player "general" (bg,
//  el que va a público). Portado de MonitorLiveControls::RenderLiveControls
//  (ver historial de Monitor), reflowado de una columna angosta/alta a una
//  barra ancha/baja para vivir debajo del video en vez de al costado.
// ─────────────────────────────────────────────────────────────────────────────
void ViewPanel::RenderLiveTransport(float w, float h)
{
    auto& core = Core::PresentationCore::Get();
    Core::VLCBasePlayer* bg = core.GetBackgroundPlayer();

    m_LiveMuted   = core.GetLiveMute();
    m_LiveVolume  = static_cast<float>(core.GetLiveVolume()) * 0.01f;
    m_LivePlaying = bg && !bg->IsPaused();

    int64_t liveLenMs = bg ? bg->GetLength() : 0;

    if (m_LivePlaying && bg && liveLenMs > 0)
    {
        int64_t curMs = bg->GetTime();
        float   fpos  = (liveLenMs > 0)
            ? static_cast<float>(curMs) / static_cast<float>(liveLenMs)
            : 0.0f;

        if (fpos >= 0.995f && core.GetLiveLoop())
        {
            bg->SetPosition(0.0f);
            bg->SetPause(false);
        }
    }

    // Sin borde ni esquinas redondeadas (pedido explicito: "directamente sin
    // bordes para tener mas espacio aun") -- solo el fondo plano ya alcanza
    // para separarlo visualmente del video de arriba y la toolbar de abajo.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { MT::k_PadLg, MT::k_Pad });
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::BeginChild("##viewLiveTransport", { w, h }, false, ImGuiWindowFlags_NoScrollbar);

    const float innerW = w - MT::k_PadLg * 2.0f;

    // Sin cabecera "PROGRAM — ON AIR" (pedido explicito: solo transporte,
    // igual que la referencia) -- el estado "en vivo" ya se ve en el propio
    // boton de Play/Pausa (se pone del color de acento cuando esta sonando).
    ImGui::Spacing();

    // ── Barra de progreso ─────────────────────────────────────────────────────
    int64_t liveCurMs = bg ? bg->GetTime()   : 0;
    int64_t liveLen   = bg ? bg->GetLength() : 0;
    float   livePos   = (liveLen > 0)
        ? std::clamp(static_cast<float>(liveCurMs) / static_cast<float>(liveLen), 0.0f, 1.0f)
        : 0.0f;

    ImGui::SetCursorPosX(MT::k_PadLg);
    float displayPos = livePos;
    if (BMSlider("##vp_tl_live", &displayPos, 0.0f, 1.0f, "",
                 MT::k_LiveTrack, MT::k_LiveGrab,
                 { MT::k_LiveGrab.x * 1.1f, MT::k_LiveGrab.y * 1.1f, MT::k_LiveGrab.z * 1.1f, 1.0f },
                 innerW))
    {
        core.SetLivePosition(displayPos);
        liveCurMs = static_cast<int64_t>(displayPos * static_cast<float>(liveLen));
    }

    DrawTimeRow(innerW, MT::k_PadLg, liveCurMs, liveLen);
    ImGui::Spacing();

    // ── Transporte (pads MIDI) + fader horizontal de volumen, en una fila ────
    // Todo en una sola fila: pads de colores (estilo controlador MIDI, un
    // color fijo por accion) + mute + fader. Se probo con el fader en una
    // columna vertical a la derecha, pero en este panel el ancho sobra y el
    // alto es el que esta justo (fila baja debajo del video) -- una columna
    // vertical no entraba sin recortarse. Ademas, si el ancho disponible es
    // chico (panel angosto, riel de acciones activado), los pads y el fader
    // se ACHICAN en vez de cortarse: todo se calcula a partir de innerW en
    // vez de usar tamaños fijos.
    // Pads chicos (pedido explicito: "achica los de reproducción") -- antes
    // llegaban hasta 56px, ahora quedan bien por debajo de los botones de la
    // franja de config de abajo.
    const float rowH      = std::clamp(ImGui::GetContentRegionAvail().y, 24.0f, 36.0f);
    const float gap       = MT::k_Gap * 1.5f;
    const float muteW     = std::clamp(rowH, 22.0f, 28.0f);
    const float minFaderW = 50.0f;
    const float minPad    = 20.0f;
    const float maxPad    = rowH;

    // 4 pads + mute + fader = 6 elementos => 5 espacios entre ellos.
    const float gapsTotal = gap * 5.0f;
    const float padSize   = std::clamp((innerW - gapsTotal - muteW - minFaderW) / 4.0f, minPad, maxPad);
    const float faderW    = std::max(minFaderW, innerW - gapsTotal - muteW - padSize * 4.0f);

    // Un solo tono neutro (tema) para las acciones momentaneas, acento del
    // tema solo para lo que tiene un estado real de encendido/apagado (Play
    // en vivo, Mute activo) -- pedido explicito: "mas simple, con iconos, no
    // botones" en vez del esquema anterior de un color fijo por acción
    // (ambar/rojo/azul/rojo) sin relacion con el tema.
    const ImVec4 kNeutral = MT::k_NeutBtn;
    const ImVec4 kLive    = MT::k_LiveBtn;

    ImGui::SetCursorPosX(MT::k_PadLg);

    ImGui::PushID("vp_pad_replay");
    if (DrawPadButton("replay_10", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        float np = livePos - (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::max(0.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    const char* mainIcon = m_LivePlaying ? "pause" : "play";
    ImGui::PushID("vp_pad_main");
    if (DrawPadButton(mainIcon, padSize * 0.40f, m_LivePlaying ? kLive : kNeutral, { padSize, padSize }, m_LivePlaying)) {
        if (bg) {
            if (m_LivePlaying) {
                bg->SetPause(true);
            } else {
                core.SetLiveMute(m_LiveMuted);
                core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
                bg->SetPause(false);
            }
        }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_fwd");
    if (DrawPadButton("forward_10", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        float np = livePos + (liveLen > 0 ? 10000.0f / static_cast<float>(liveLen) : 0.0f);
        core.SetLivePosition(std::min(1.0f, np));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImGui::PushID("vp_pad_stop");
    if (DrawPadButton("stop", padSize * 0.34f, kNeutral, { padSize, padSize }, false)) {
        core.SetLivePosition(0.0f);
        if (bg) { bg->SetPosition(0.0f); bg->SetPause(true); }
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    bool        isDanger  = (m_LiveVolume > 1.0f);
    DrawIconFn  speakerFn = m_LiveMuted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn;

    ImGui::PushID("vp_pad_mute");
    if (DrawPadButton(m_LiveMuted ? "no_sound" : "volume_up", muteW * 0.44f, m_LiveMuted ? kLive : kNeutral,
                      { muteW, padSize }, m_LiveMuted, speakerFn)) {
        m_LiveMuted = !m_LiveMuted;
        core.SetLiveMute(m_LiveMuted);
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }
    ImGui::PopID();
    ImGui::SameLine(0.0f, gap);

    ImU32 trackCol = ImGui::GetColorU32(MT::k_NeutBtn);
    ImU32 fillCol  = isDanger ? IM_COL32(235, 70, 70, 255) : ImGui::GetColorU32(MT::k_LiveGrab);
    ImU32 capCol   = isDanger ? IM_COL32(255, 90, 90, 255) : IM_COL32(225, 228, 235, 255);

    if (HorizontalFader("##vp_vol_fader", &m_LiveVolume, 0.0f, 2.0f, { faderW, padSize },
                         trackCol, fillCol, capCol)) {
        core.SetLiveVolume(m_LiveMuted ? 0 : static_cast<int>(m_LiveVolume * 100.0f));
    }

    // Los medidores VU ya no van aca abajo (le comian ~48px fijos a este
    // panel, siempre a lo ancho completo): ahora se dibujan chicos, pegados
    // al borde izquierdo del video en RenderContent, mas comodos y sin
    // robarle alto al transporte. m_AudioMeters.Update() se llama desde ahi.

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

// "Control Overlays" se movio a ViewToolsPanel (nuevo panel debajo de
// "Vista en Vivo", junto con Red/Notas/Reloj) — ver ViewToolsPanel.cpp.

void ViewPanel::RenderContent(float panelW, float panelH)
{
    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── 1. Resolución de referencia del proyector ────────────────────────
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    float srcW = 1920.0f;
    float srcH = 1080.0f;

    if (monitors && monitorCount > 0 && state.targetMonitorIndex >= 0 &&
        state.targetMonitorIndex < monitorCount)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
        if (mode && mode->width > 0 && mode->height > 0)
        {
            srcW = (float)mode->width;
            srcH = (float)mode->height;
        }
    }

    // ── 2. Calcular "Lo justo y necesario" ────────────────────────────────
    float srcRatio = srcW / srcH;
    float drawW = panelW;
    float drawH = panelW / srcRatio;

    // Si el alto calculado supera el alto disponible, ajustamos en base al alto
    if (drawH > panelH)
    {
        drawH = panelH;
        drawW = panelH * srcRatio;
    }

    // Centrar horizontal y verticalmente desplazando el cursor interno de ImGui
    float offsetX = (panelW - drawW) * 0.5f;
    float offsetY = (panelH - drawH) * 0.5f;

    if (offsetX > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    }
    if (offsetY > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
    }

    // Puntos exactos del área de dibujo
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + drawW, p0.y + drawH);

    // ── 3+4. Contenido: Público (fondo+overlay+texto) o Stage (grilla/mirror)
    // Movido a UI::DrawPublicContent/DrawStageContent para poder reusarlo
    // desde el Monitor de Control (ver LiveContentRenderer.h) — el operador
    // elige la fuente con el botón "vaPreviewSource" del riel derecho.
    if (m_PreviewSource == PreviewSource::Publico)
    {
        UI::DrawPublicContent(dl, p0, p1, drawW, drawH);
    }
    else if (m_PreviewSource == PreviewSource::Stage)
    {
        UI::DrawStageContent(dl, p0, p1);
    }
    else if (m_PreviewSource == PreviewSource::Lan)
    {
        // Refleja EXACTAMENTE lo que hoy manda NetworkStreamServer (ver
        // PresentationCore::WireNetworkServerProviders/RenderProjectorToFBO):
        // "En vivo" es un espejo real de Publico, "Solo reloj"/"En blanco"
        // son la misma logica de OutputContentMode que ya aplica del lado
        // del servidor, asi el operador ve exactamente lo que sale por LAN.
        auto lanMode = core.GetLanContentMode();
        if (lanMode == Core::OutputContentMode::Live)
        {
            UI::DrawPublicContent(dl, p0, p1, drawW, drawH);
        }
        else
        {
            dl->AddRectFilled(p0, p1, IM_COL32(10, 10, 12, 255));
            if (lanMode == Core::OutputContentMode::ClockOnly)
            {
                std::time_t now = std::time(nullptr);
                std::tm lt{};
#ifdef _WIN32
                localtime_s(&lt, &now);
#else
                localtime_r(&now, &lt);
#endif
                char buf[16];
                std::strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
                float fontSize = std::clamp(drawH * 0.20f, 24.0f, 160.0f);
                ImFont* f  = ImGui::GetFont();
                ImVec2  ts = f->CalcTextSizeA(fontSize, FLT_MAX, FLT_MAX, buf);
                dl->AddText(f, fontSize, { p0.x + (drawW - ts.x) * 0.5f, p0.y + (drawH - ts.y) * 0.5f },
                            IM_COL32(235, 235, 240, 255), buf);
            }
            else // Blank
            {
                const char* msg = "En blanco";
                ImVec2 ts = ImGui::CalcTextSize(msg);
                dl->AddText({ p0.x + (drawW - ts.x) * 0.5f, p0.y + (drawH - ts.y) * 0.5f },
                            IM_COL32(110, 110, 118, 255), msg);
            }
            dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);
        }

        // Selector de "Contenido" de LAN -- 3 pastillas chicas pegadas al
        // borde inferior del video, solo visibles en esta pestaña (mismo
        // criterio que el resto del riel: la accion vive donde tiene efecto).
        {
            const char* pillLabel[3] = { "En vivo", "Solo reloj", "En blanco" };
            const float pillH = 24.0f, pillGap = 4.0f, pillPad = 8.0f;
            float pillY = p1.y - pillH - pillPad;
            float pillTotalW = drawW - pillPad * 2.0f;
            float pillW = (pillTotalW - pillGap * 2.0f) / 3.0f;
            for (int pi = 0; pi < 3; pi++)
            {
                bool active = (static_cast<int>(lanMode) == pi);
                ImVec2 pillPos = { p0.x + pillPad + pi * (pillW + pillGap), pillY };
                ImGui::SetCursorScreenPos(pillPos);
                ImGui::PushStyleColor(ImGuiCol_Button, active
                    ? ImVec4(0.35f, 0.55f, 0.95f, 0.85f) : ImVec4(0.0f, 0.0f, 0.0f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active
                    ? ImVec4(0.40f, 0.60f, 1.00f, 0.90f) : ImVec4(0.0f, 0.0f, 0.0f, 0.70f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.50f, 0.90f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Text, active
                    ? ImVec4(1, 1, 1, 1) : ImVec4(0.75f, 0.76f, 0.80f, 1.0f));
                ImGui::PushID(pi);
                if (ImGui::Button(pillLabel[pi], { pillW, pillH }))
                    core.SetLanContentMode(static_cast<Core::OutputContentMode>(pi));
                ImGui::PopID();
                ImGui::PopStyleColor(4);
            }
        }
    }
    else // Transmision -- RTMP no tiene contenido propio que previsualizar
         // aca (usa Captura, ver BroadcastPanel), asi que esta pestaña solo
         // ofrece un atajo directo al espacio de trabajo dedicado.
    {
        dl->AddRectFilled(p0, p1, IM_COL32(10, 10, 12, 255));
        const char* msg = "La Transmisión (RTMP) se controla desde su espacio de trabajo";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 msgPos = { p0.x + (drawW - ts.x) * 0.5f, p0.y + drawH * 0.44f - ts.y * 0.5f };
        dl->AddText(msgPos, IM_COL32(160, 160, 170, 255), msg);
        dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);

        const char* btnLabel = "Ir a Transmisión";
        ImVec2 btnSize = { 180.0f, 30.0f };
        ImGui::SetCursorScreenPos({ p0.x + (drawW - btnSize.x) * 0.5f, msgPos.y + ts.y + 14.0f });
        if (ImGui::Button(btnLabel, btnSize))
        {
            auto& workspace = ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace;
            workspace.layoutPreset = ProyecThor::Settings::WorkspaceLayoutPreset::Broadcast;
            ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    // ── 5b. Medidor VU chico, pegado al borde izquierdo del video ─────────
    // Antes vivia en RenderLiveTransport como una barra horizontal fija de
    // 48px de alto x todo el ancho, debajo del video. Se movio aca, chico y
    // vertical, para no robarle alto al transporte y quedar "encima" del
    // visor como en un mixer, sin estorbar. Solo tiene sentido mientras se
    // previsualiza Publico (Stage no tiene audio propio).
    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting)
    {
        Core::VLCBasePlayer* liveBg = core.GetBackgroundPlayer();
        bool liveMuted   = core.GetLiveMute();
        float liveVolume = static_cast<float>(core.GetLiveVolume()) * 0.01f;
        bool livePlaying = liveBg && !liveBg->IsPaused();

        m_AudioMeters.Update(liveBg, true, livePlaying, liveMuted, liveVolume);

        const float meterW = 22.0f;
        const float meterPad = 6.0f;
        float meterH = std::min(drawH - meterPad * 2.0f, 110.0f);
        if (meterH > 20.0f)
        {
            ImVec2 meterPos = { p0.x + meterPad, p0.y + meterPad };
            m_AudioMeters.RenderVertical(dl, meterPos, meterW, meterH);
        }
    }

    // ── 5c. Indicador de carga (solo operador) ─────────────────────────────
    // Chico, esquina inferior derecha del preview: NUNCA se muestra en la
    // salida real al publico (ver PresentationCore::ShouldShowLoadingScreen
    // para el equivalente que si se ve el publico, con el logo configurado
    // en Ajustes > Proyeccion). Este es solo feedback para el operador de
    // que un fondo/video esta cargando en standby.
    if (m_PreviewSource == PreviewSource::Publico && core.IsBackgroundSwapPending())
    {
        const float spinR = 11.0f;
        ImVec2 spinCenter = { p1.x - spinR - 14.0f, p1.y - spinR - 14.0f };
        DrawLoadingSpinner(dl, spinCenter, spinR);

        char etaBuf[32];
        snprintf(etaBuf, sizeof(etaBuf), "~%.1fs", core.GetBackgroundSwapEta());
        ImVec2 etaSz = ImGui::CalcTextSize(etaBuf);
        ImVec2 etaPos = { spinCenter.x - spinR - 6.0f - etaSz.x, spinCenter.y - etaSz.y * 0.5f };
        dl->AddRectFilled({ etaPos.x - 5.0f, etaPos.y - 3.0f }, { etaPos.x + etaSz.x + 5.0f, etaPos.y + etaSz.y + 3.0f },
                          IM_COL32(0, 0, 0, 150), 4.0f);
        dl->AddText(etaPos, IM_COL32(230, 230, 235, 230), etaBuf);
    }

    // ── 6. Indicador de Red (Solo cuando transmite) ───────────────────────
    if (m_PreviewSource == PreviewSource::Publico && state.isProjecting && state.isStreamingNet)
    {
        // Puedes cambiar "WIFI" por un icono de FontAwesome si tu proyecto lo soporta (ej. u8"\uf1eb")
        const char* wifiStr = "online"; 
        ImVec2 wifiSize = ImGui::CalcTextSize(wifiStr);
        
        // Posicionado en la esquina superior derecha del área de proyección
        ImVec2 wifiPos = ImVec2(p1.x - wifiSize.x - 12.0f, p0.y + 8.0f);

        // Fondo oscuro semitransparente para que contraste con cualquier video/imagen de fondo
        dl->AddRectFilled(
            ImVec2(wifiPos.x - 6.0f, wifiPos.y - 4.0f),
            ImVec2(wifiPos.x + wifiSize.x + 6.0f, wifiPos.y + wifiSize.y + 4.0f),
            IM_COL32(0, 0, 0, 160), 4.0f);

        // Dibujar el icono
        dl->AddText(wifiPos, IM_COL32(0, 255, 100, 255), wifiStr);
    }

    // Registramos que solo consumimos el tamaño de la pantalla
    ImGui::Dummy(ImVec2(drawW, drawH));
}

} // namespace ProyecThor::UI