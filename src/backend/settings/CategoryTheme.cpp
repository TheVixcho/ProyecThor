#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/panels/biblio/LibraryIcons.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <array>
#endif

namespace ProyecThor::UI::Settings {

using namespace ProyecThor::Settings;

#ifndef _WIN32
// Selector de archivos para Linux/macOS: no hay dialogo nativo unico en
// estos sistemas, asi que se delega en zenity/kdialog (lo que este
// instalado). Mismo enfoque que TabTypography::OpenFontFileDialogUnix
// (reimplementado localmente aca, no se comparte cabecera entre ambos por
// ser un helper chico y de un solo uso en cada archivo).
static std::string OpenFontFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar fuente de la interfaz\" "
        "--file-filter=\"Fuentes | *.ttf *.otf *.ttc\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.ttf *.otf *.ttc|Fuentes\" 2>/dev/null"
    };

    for (const char* cmd : commands) {
        std::array<char, 1024> buffer{};
        std::string result;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;

        while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
            result += buffer.data();

        int status = pclose(pipe);
        if (status != 0) continue; // el usuario cancelo o la herramienta no existe

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        if (!result.empty())
            return result;
    }
    return {};
}
#endif

// Abre el dialogo nativo (Windows) o zenity/kdialog (Linux/macOS) para
// elegir un archivo de fuente. Devuelve la ruta absoluta, o vacio si el
// usuario cancelo / no hay herramienta disponible.
static std::string PickFontFileDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn       = {};
    ofn.lStructSize         = sizeof(ofn);
    ofn.hwndOwner           = NULL;
    ofn.lpstrFilter         = "Fuentes\0*.ttf;*.otf;*.ttc\0Todos los archivos\0*.*\0";
    ofn.lpstrFile           = filename;
    ofn.nMaxFile            = MAX_PATH;
    ofn.Flags               = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&ofn)) return {};
    return filename;
#else
    return OpenFontFileDialogUnix();
#endif
}

// Resuelve un nombre de fuente (stem, sin extension) a su ruta completa
// dentro de assets/fonts. Misma lógica que
// PresentationCore::ResolveFontFilePath, reimplementada acá porque ese
// método es privado (solo lo usa PresentationCore internamente para
// resolver la fuente activa de las diapositivas).
static std::string ResolveFontPathByName(const std::string& fontName) {
    if (fontName.empty() || fontName == "Predeterminada") return "";

    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
    for (const char* ext : { ".ttf", ".otf", ".ttc" }) {
        std::filesystem::path candidate = fontsDir / (fontName + ext);
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec))
            return candidate.string();
    }
    return "";
}

// Progreso animado (0..1) de hover/seleccion por-item -- mismo patron que
// ControlWidgets::AnimHoverT / IconRail.cpp (ImGuiStorage + lerp con
// DeltaTime), reutilizado acá para las tarjetas de Apariencia.
static float AnimT(ImGuiID baseId, ImU32 salt, bool target, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(baseId ^ salt, target ? 1.0f : 0.0f);
    float dst = target ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// Tamaño de la miniatura de preview de PresetSwatch -- también usado por
// RenderCategoryTheme() para calcular el tamaño de columna/fila de la
// grilla, así que vive como constante compartida en vez de un número mágico
// duplicado en dos sitios.
static constexpr float kSwatchCardW = 108.0f;
static constexpr float kSwatchCardH = 74.0f;

// Tarjeta de preset de tema: mini "screenshot" del tema (barra superior +
// botón de acento + par de líneas de texto simuladas) en vez de un simple
// cuadrado de color -- da una idea real de cómo se va a ver la app, no solo
// el tono de acento. Nombre debajo. Dibujada con posición ABSOLUTA
// (origin/colW) en vez de fluir con ImGui::SameLine() -- antes cada swatch
// avanzaba según el ancho real de su propio texto, y un nombre largo como
// "Naranja y Negro" (mucho más ancho que el cuadrado anterior de 52px)
// corría a todos los swatches siguientes y rompía la alineación de la
// grilla entre filas. Ahora cada swatch ocupa siempre exactamente una
// columna de ancho colW, y el texto se envuelve adentro de esa columna en
// vez de desbordarse.
static bool PresetSwatch(const char* label, ThemePreset preset, ThemePreset active,
                          ImVec2 origin, float colW) {
    ThemeSettings preview = MakeThemePreset(preset);
    ImVec4 accent = ImVec4(preview.accent[0],   preview.accent[1],   preview.accent[2],   1.0f);
    ImVec4 base   = ImVec4(preview.base[0],     preview.base[1],     preview.base[2],     1.0f);
    ImVec4 bar    = ImVec4(preview.surface1[0], preview.surface1[1], preview.surface1[2], 1.0f);
    ImVec4 dim    = ImVec4(preview.textDim[0],  preview.textDim[1],  preview.textDim[2],  1.0f);
    bool   selected = (preset == active);

    const float cardW = kSwatchCardW, cardH = kSwatchCardH;
    const float cardX = origin.x + (colW - cardW) * 0.5f;

    ImGui::PushID(label);
    ImGui::SetCursorScreenPos(ImVec2(cardX, origin.y));
    ImGui::InvisibleButton("##swatch", ImVec2(cardW, cardH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    ImGuiID id      = ImGui::GetID("##swatch");
    float   hoverT  = AnimT(id, 0xA1u, hovered, 14.0f);
    float   selectT = AnimT(id, 0xB2u, selected, 9.0f);

    // Leve "levitacion" en hover: sube un par de px y agranda la sombra.
    float lift = hoverT * 2.5f;
    ImVec2 p0(cardX, origin.y - lift);
    ImVec2 p1(p0.x + cardW, p0.y + cardH);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 3; i >= 1; --i) {
        float t   = (float)i / 3.0f;
        float off = (5.0f + hoverT * 5.0f) * t;
        int   a   = (int)((24.0f + hoverT * 12.0f) * (1.0f - t * 0.5f));
        dl->AddRectFilled(ImVec2(p0.x - off * 0.3f, p0.y + off * 0.45f),
                          ImVec2(p1.x + off * 0.3f, p1.y + off * 0.8f),
                          IM_COL32(0, 0, 0, a), 13.0f + off * 0.2f);
    }

    // ── Mini "screenshot" del tema ──────────────────────────────────────
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(base), 13.0f);

    const float barH = 18.0f;
    dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + barH),
        ImGui::ColorConvertFloat4ToU32(bar), 13.0f, ImDrawFlags_RoundCornersTop);
    // "Semáforo" de ventana (3 puntitos) en la barra, como cualquier título
    // de ventana real -- ancla visual de que esto es una miniatura de UI.
    for (int i = 0; i < 3; i++)
        dl->AddCircleFilled(ImVec2(p0.x + 11.0f + i * 9.0f, p0.y + barH * 0.5f), 2.6f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(dim.x, dim.y, dim.z, 0.55f)));
    // Identidad de color del preset, esquina superior derecha de la barra.
    ImVec2 dotC(p1.x - 13.0f, p0.y + barH * 0.5f);
    dl->AddCircleFilled(dotC, 5.5f, ImGui::ColorConvertFloat4ToU32(accent));

    // Cuerpo: un "botón" de acento + un par de líneas de texto simuladas,
    // para sugerir contenido real en vez de un rectángulo vacío.
    float bodyY = p0.y + barH + 10.0f;
    dl->AddRectFilled(ImVec2(p0.x + 10.0f, bodyY), ImVec2(p0.x + 42.0f, bodyY + 9.0f),
        ImGui::ColorConvertFloat4ToU32(accent), 4.0f);
    float lineY = bodyY + 17.0f;
    dl->AddRectFilled(ImVec2(p0.x + 10.0f, lineY), ImVec2(p1.x - 14.0f, lineY + 3.0f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(dim.x, dim.y, dim.z, 0.55f)), 1.5f);
    dl->AddRectFilled(ImVec2(p0.x + 10.0f, lineY + 8.0f), ImVec2(p1.x - 32.0f, lineY + 11.0f),
        ImGui::ColorConvertFloat4ToU32(ImVec4(dim.x, dim.y, dim.z, 0.35f)), 1.5f);

    // Anillo: gris tenue en reposo, se funde al acento del preset a medida
    // que selectT avanza, con un pulso suave mientras esta seleccionado.
    float  pulse = selected ? (0.85f + 0.15f * std::sin((float)ImGui::GetTime() * 2.4f)) : 1.0f;
    ImVec4 idle(1.0f, 1.0f, 1.0f, 0.14f);
    ImVec4 ring(
        idle.x + (accent.x - idle.x) * selectT,
        idle.y + (accent.y - idle.y) * selectT,
        idle.z + (accent.z - idle.z) * selectT,
        (idle.w + (1.0f - idle.w) * selectT) * pulse);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(ring), 13.0f, 0, 1.0f + selectT * 1.8f);

    // Marca de seleccionado (check), aparece/desaparece con el mismo fade
    // que el anillo.
    if (selectT > 0.02f) {
        ImVec2 c(p1.x - 13.0f, p1.y - 13.0f);
        ImU32  badgeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, selectT));
        ImU32  checkCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, selectT));
        dl->AddCircleFilled(c, 8.0f, badgeCol);
        dl->AddCircle(c, 8.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(0, 0, 0, 0.25f * selectT)), 16, 1.0f);
        dl->PathLineTo(ImVec2(c.x - 3.6f, c.y));
        dl->PathLineTo(ImVec2(c.x - 0.7f, c.y + 2.9f));
        dl->PathLineTo(ImVec2(c.x + 4.0f, c.y - 3.6f));
        dl->PathStroke(checkCol, false, 1.7f);
    }

    // Nombre del preset: centrado si entra en una linea, envuelto dentro de
    // la columna (nunca desbordando al swatch vecino) si no entra.
    ImGui::SetCursorScreenPos(ImVec2(origin.x, p1.y + lift + 8.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, selected
        ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.78f, 0.80f, 0.86f, 1.0f));
    float fullW = ImGui::CalcTextSize(label).x;
    if (fullW <= colW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (colW - fullW) * 0.5f);
        ImGui::TextUnformatted(label);
    } else {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + colW);
        ImGui::TextWrapped("%s", label);
        ImGui::PopTextWrapPos();
    }
    ImGui::PopStyleColor();
    ImGui::PopID();

    return clicked;
}

// Diagramas a mano de cada Entorno de trabajo (ver UIManager::
// BuildWorkspaceLayout*, que arma el DockBuilder real con las mismas
// proporciones) -- se dibujan adentro de la tarjeta de WorkspacePresetCard
// para que el usuario vea la disposicion antes de elegirla, en vez de un
// nombre suelto. "a"/"b" son la esquina superior-izquierda/inferior-derecha
// del area disponible dentro de la tarjeta; accentCol resalta el panel
// "Vista en Vivo" (el que mas cambia de lugar entre presets).
static void DrawWorkspaceDiagramClassic(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    float w = b.x - a.x, h = b.y - a.y, g = 3.0f;
    float leftW = w * 0.22f, rightW = w * 0.28f;
    float midW  = w - leftW - rightW - g * 2.0f;
    float homeH = h * 0.62f;

    dl->AddRectFilled({a.x, a.y}, {a.x + leftW, b.y}, panelCol, 2.0f); // Biblioteca
    dl->AddRectFilled({a.x + leftW + g, a.y}, {a.x + leftW + g + midW, a.y + homeH}, panelCol, 2.0f); // Home
    dl->AddRectFilled({a.x + leftW + g, a.y + homeH + g}, {a.x + leftW + g + midW, b.y}, panelCol, 2.0f); // Diseño
    dl->AddRectFilled({b.x - rightW, a.y}, {b.x, b.y}, accentCol, 2.0f); // Vista en Vivo
}

static void DrawWorkspaceDiagramSimple(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    // Cuatro columnas de alto completo, nada apilado: Biblioteca | Home |
    // Vista en Vivo | Diseño (Diseño se corre TODO a la derecha).
    float w = b.x - a.x, g = 3.0f;
    float leftW = w * 0.22f, vivW = w * 0.32f, rightW = w * 0.16f;
    float midW  = w - leftW - vivW - rightW - g * 3.0f;

    float x = a.x;
    dl->AddRectFilled({x, a.y}, {x + leftW, b.y}, panelCol, 2.0f); x += leftW + g; // Biblioteca
    dl->AddRectFilled({x, a.y}, {x + midW, b.y}, panelCol, 2.0f); x += midW + g; // Home
    dl->AddRectFilled({x, a.y}, {x + vivW, b.y}, accentCol, 2.0f); x += vivW + g; // Vista en Vivo
    dl->AddRectFilled({x, a.y}, {b.x, b.y}, panelCol, 2.0f); // Diseño
}

static void DrawWorkspaceDiagramBroadcast(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 /*panelCol*/, ImU32 accentCol) {
    // Transmisión a pantalla completa, sola -- ver UIManager::
    // BuildWorkspaceLayoutBroadcast ("elimina todo lo relacionado a
    // proyeccion", ya no reparte Biblioteca/Home/Diseño abajo).
    dl->AddRectFilled(a, b, accentCol, 2.0f);
}

static void DrawWorkspaceDiagramLibrary(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 panelCol, ImU32 accentCol) {
    // Biblioteca | Home -- sin Vista en Vivo/Diseño (ver
    // UIManager::BuildWorkspaceLayoutLibrary).
    float w = b.x - a.x, g = 3.0f;
    float leftW = w * 0.30f;
    float mainW = w - leftW - g;

    dl->AddRectFilled({a.x, a.y}, {a.x + leftW, b.y}, panelCol, 2.0f); // Biblioteca
    dl->AddRectFilled({a.x + leftW + g, a.y}, {a.x + leftW + g + mainW, b.y}, accentCol, 2.0f); // Home
}

// "Producción" (enum WorkspaceLayoutPreset::Video): una sola ventana a
// pantalla completa (ver VideoEditorPanel, que ahora absorbe Render/
// Colorimetria/Canales/Audio(DAW)/Overlays por pestañas internas) -- se
// dibuja un icono generico de "produccion" (camara/claqueta) centrado
// adentro para que no sea un rectangulo solido pelado.
static void DrawWorkspaceDiagramMediaStub(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 accentCol,
                                          void (*drawIcon)(ImDrawList*, ImVec2, float, ImU32)) {
    dl->AddRectFilled(a, b, accentCol, 2.0f);
    float sz = std::min(b.x - a.x, b.y - a.y) * 0.36f;
    ImVec2 o = { (a.x + b.x) * 0.5f - sz * 0.5f, (a.y + b.y) * 0.5f - sz * 0.5f };
    drawIcon(dl, o, sz, IM_COL32(18, 18, 22, 220));
}
static void DrawWorkspaceDiagramVideo(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 /*panelCol*/, ImU32 accentCol) {
    DrawWorkspaceDiagramMediaStub(dl, a, b, accentCol, ProyecThor::Library::DrawIcon_Video);
}

using WorkspaceDiagramFn = void (*)(ImDrawList*, ImVec2, ImVec2, ImU32, ImU32);

// Tarjeta con el diagrama de arriba en vez de un swatch de color -- lo que
// cambia entre presets de Entorno de trabajo es la DISPOSICION de los
// paneles, no una paleta (ver PresetSwatch, mismo patron de seleccion).
static bool WorkspacePresetCard(const char* label, WorkspaceLayoutPreset preset,
                                WorkspaceLayoutPreset active, WorkspaceDiagramFn drawDiagram)
{
    const ImVec4 accent = ImVec4(0.45f, 0.60f, 1.00f, 1.0f);
    const ImVec4 base   = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
    bool selected = (preset == active);

    ImGui::PushID(label);
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(base.x + 0.04f, base.y + 0.04f, base.z + 0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, base);
    ImGui::PushStyleColor(ImGuiCol_Border, selected ? accent : ImVec4(1, 1, 1, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 2.0f : 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool clicked = ImGui::Button("##wscard", ImVec2(150.0f, 96.0f));
    bool hovered = ImGui::IsItemHovered();

    ImVec2      p0 = ImGui::GetItemRectMin();
    ImVec2      p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 panelCol  = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.10f));
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(selected ? accent : ImVec4(1, 1, 1, 0.30f));
    drawDiagram(dl, { p0.x + 10.0f, p0.y + 10.0f }, { p1.x - 10.0f, p1.y - 10.0f }, panelCol, accentU32);

    // Glow suave en hover + anillo con pulso mientras esta seleccionada --
    // mismo criterio de animacion que PresetSwatch, para que "Temas" y
    // "Entorno de trabajo" se sientan parte de la misma seccion.
    ImGuiID id     = ImGui::GetID("##wscard");
    float   hoverT = AnimT(id, 0xC3u, hovered, 14.0f);
    if (hoverT > 0.01f)
        dl->AddRectFilled(p0, p1,
            ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.07f * hoverT)), 8.0f);
    if (selected) {
        float pulse = 0.7f + 0.3f * std::sin((float)ImGui::GetTime() * 2.2f);
        dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, pulse)),
            8.0f, 0, 2.0f);
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    ImGui::PopID();

    return clicked;
}

// ── Chips de color (grilla compacta para "Personalizar colores") ───────────
// Antes cada color editable vivia en su propia fila "ColorEdit4 + etiqueta"
// -- con ~33 colores editables entre la paleta base y las 4 paletas de
// identidad de sidebar, la subcategoria "Colores" quedaba larguisima y
// obligaba a scrollear sin parar. ColorChip/ColorChipGroup los agrupan en
// grillas cortas que envuelven, varios chips por fila (mismo criterio de
// grilla por posicion absoluta que PresetSwatch: nunca se desalinea sin
// importar el largo de la etiqueta).
static bool ColorChip(const char* uid, const char* label, float* col,
                       ImGuiColorEditFlags flags, ImVec2 origin, float colW, const char* desc) {
    const float swSize = 34.0f;
    ImGui::PushID(uid);

    ImGui::SetCursorScreenPos(ImVec2(origin.x + (colW - swSize) * 0.5f, origin.y));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 9.0f));
    ImVec2 swPos = ImGui::GetCursorScreenPos();
    bool changed = ImGui::ColorEdit4("##c", col, flags | ImGuiColorEditFlags_NoLabel);
    ImGui::PopStyleVar(2);

    if (desc && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", desc);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(swPos, ImVec2(swPos.x + swSize, swPos.y + swSize), IM_COL32(255, 255, 255, 25), 8.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + swSize + 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.77f, 0.83f, 1.0f));
    float fullW = ImGui::CalcTextSize(label).x;
    if (fullW <= colW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (colW - fullW) * 0.5f);
        ImGui::TextUnformatted(label);
    } else {
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + colW);
        ImGui::TextWrapped("%s", label);
        ImGui::PopTextWrapPos();
    }
    ImGui::PopStyleColor();
    ImGui::PopID();

    return changed;
}

struct ColorEntry { const char* label; float* col; const char* desc; };

// Etiqueta de grupo chica (sin la barra/gradiente pesada de SectionTitle --
// acá adentro de una sola subcategoría ya scrolleable, varios de esos
// encabezados grandes uno tras otro eran justamente parte del problema) +
// una grilla de ColorChip que envuelve sola según el ancho disponible.
static bool ColorChipGroup(const char* groupLabel, const ColorEntry* entries, int count,
                            ImGuiColorEditFlags flags) {
    const float colW = 76.0f, rowH = 62.0f, gap = 10.0f;

    // PushID(groupLabel) acá es lo que evita que dos grupos distintos con
    // el mismo indice de chip (p.ej. "Superficie 0" en Fondos y "Acento
    // oscuro" en Acento, ambos indice 2) terminen generando el mismo
    // ImGuiID "##chip2" -- sin esto ImGui tira "Programmer error: N visible
    // items with conflicting ID" apenas hay mas de un ColorChipGroup en
    // pantalla con chips en la misma posición.
    ImGui::PushID(groupLabel);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.55f, 0.62f, 1.0f));
    ImGui::SetWindowFontScale(0.86f);
    ImGui::TextUnformatted(groupLabel);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    const int   perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (colW + gap)));
    const int   rows   = (count + perRow - 1) / perRow;
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    bool changed = false;
    for (int i = 0; i < count; i++) {
        int col = i % perRow, row = i / perRow;
        ImVec2 cellOrigin(origin.x + col * (colW + gap), origin.y + row * (rowH + gap));
        char uid[16];
        snprintf(uid, sizeof(uid), "##chip%d", i);
        changed |= ColorChip(uid, entries[i].label, entries[i].col, flags, cellOrigin, colW, entries[i].desc);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rows * (rowH + gap)));
    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    ImGui::PopID();
    return changed;
}

void SettingsPanel::RenderCategoryTheme() {
    auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;

    // Sin intro de texto acá: duplicaba la descripción de categoría que ya
    // dibuja SettingsPanel::RenderContent() un nivel arriba ("Colores,
    // fuentes y efectos visuales"), pedido explícito de sacar títulos /
    // subtítulos redundantes.
    if (SectionTitle("Temas predeterminados", "Temas")) {
        struct PresetEntry { const char* label; ThemePreset preset; };
        static const PresetEntry presets[] = {
            { "Oscuro",              ThemePreset::Dark        },
            { "Claro",               ThemePreset::Light       },
            { "Naranja y Negro",     ThemePreset::OrangeBlack },
            { "Jazz",                ThemePreset::Jazz        },
            { "Ko-fi",               ThemePreset::Kofi        },
            { "Deadlock",            ThemePreset::Deadlock    },
            { "Galaxia",             ThemePreset::Galaxy      },
            { "Mek (Catppuccin)",    ThemePreset::Mek         },
            { "Cyberpunk Pro",       ThemePreset::Cyberpunk   },
            { "Emerald Studio Pro",  ThemePreset::Emerald     },
            { "Crimson Velvet Pro",  ThemePreset::Crimson     },
            { "Midnight Blue Pro",   ThemePreset::Midnight    },
            { "Amethyst Violet Pro", ThemePreset::Amethyst    },
            { "Titanium Silver Pro", ThemePreset::Titanium    },
        };

        const float colW = kSwatchCardW + 24.0f, rowH = kSwatchCardH + 40.0f, gap = 16.0f;
        const int   count  = (int)(sizeof(presets) / sizeof(presets[0]));
        const int   perRow = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (colW + gap)));
        const int   rows   = (count + perRow - 1) / perRow;
        const ImVec2 gridOrigin = ImGui::GetCursorScreenPos();

        for (int i = 0; i < count; i++) {
            int col = i % perRow, row = i / perRow;
            ImVec2 cellOrigin(gridOrigin.x + col * (colW + gap), gridOrigin.y + row * (rowH + gap));
            if (PresetSwatch(presets[i].label, presets[i].preset, theme.preset, cellOrigin, colW)) {
                ProyecThor::Settings::SettingsManager::Get().ApplyPreset(presets[i].preset);
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(gridOrigin.x, gridOrigin.y + rows * (rowH + gap)));
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }

    // ── Entorno de trabajo ───────────────────────────────────────────────────
    // Ordenamiento de los 4 paneles dockeados (Biblioteca/Home/Vista en Vivo/
    // Diseño) -- ver UIManager::BuildWorkspaceLayout* para el DockBuilder
    // real de cada uno. Cambiar la seleccion reconstruye el layout solo
    // (UIManager lo detecta comparando contra el ultimo valor aplicado, ver
    // m_LastWorkspacePreset), no hace falta reiniciar ni pedirlo aparte.
    if (SectionTitle("Entorno de trabajo", "Entorno de trabajo")) {
        ImGui::TextDisabled("Elige como se acomodan Biblioteca, Home, Vista en Vivo y Diseño en pantalla.");
        ImGui::Spacing();

        auto& workspace = ProyecThor::Settings::SettingsManager::Get().GetSettings().workspace;

        struct WsEntry { const char* label; WorkspaceLayoutPreset preset; WorkspaceDiagramFn diagram; };
        static const WsEntry entries[] = {
            { "Clásico",     WorkspaceLayoutPreset::Classic,   DrawWorkspaceDiagramClassic   },
            { "Simple",      WorkspaceLayoutPreset::Simple,    DrawWorkspaceDiagramSimple    },
            { "Biblioteca",  WorkspaceLayoutPreset::Library,   DrawWorkspaceDiagramLibrary   },
            { "Producción",  WorkspaceLayoutPreset::Video,     DrawWorkspaceDiagramVideo     },
        };
        const int entryCount = (int)(sizeof(entries) / sizeof(entries[0]));

        // Se ajusta solo (en vez de SameLine() sin condicion) porque ya son
        // 8 tarjetas -- sin esto, en una ventana angosta las ultimas
        // quedarian literalmente afuera de la pantalla en vez de bajar de
        // renglon (mismo criterio que el wrapping del demo de ImGui).
        const float wsCardW = 150.0f;
        const float wsVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        for (int i = 0; i < entryCount; i++) {
            if (WorkspacePresetCard(entries[i].label, entries[i].preset, workspace.layoutPreset, entries[i].diagram)) {
                workspace.layoutPreset = entries[i].preset;
                ProyecThor::Settings::SettingsManager::Get().Save();
            }
            float nextX2 = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + wsCardW;
            if (i + 1 < entryCount && nextX2 < wsVisibleX2)
                ImGui::SameLine();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.9f, 1.0f), "Entorno activo: %s",
            ProyecThor::Settings::WorkspaceLayoutPresetName(workspace.layoutPreset));
    }

    // Compartidas por todos los bloques de "Colores"/"Diseño" de abajo --
    // declaradas afuera de cualquier if(SectionTitle) para que `changed`
    // acumule sin importar cual bloque este visible este frame, y el
    // chequeo final (ver mas abajo) sea incondicional.
    bool changed = false;
    static ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_AlphaPreviewHalf |
        ImGuiColorEditFlags_NoInputs;

    // Antes esto era 9 SectionTitle apilados (Personalizar colores, Fondos y
    // superficies, Acento, Bordes, Texto, Estados + 4 paletas de identidad
    // de sidebar) todos compartiendo navGroup="Colores" -- es decir, TODOS
    // renderizaban en la misma pagina, uno debajo del otro, obligando a
    // scrollear muchisimo para llegar a los ultimos. Ahora es un solo
    // SectionTitle con 2 pestañas internas (paleta del tema vs. paletas de
    // identidad de sidebar, que son datos completamente independientes) y
    // grillas cortas de chips en vez de una fila por color.
    static int s_ColorsTab = 0; // 0 = Interfaz, 1 = Categorías de sidebar

    if (SectionTitle("Personalizar colores", "Colores")) {
        ImVec4 accentV (theme.accent[0],  theme.accent[1],  theme.accent[2],  1.0f);
        ImVec4 surf2V  (theme.surface2[0], theme.surface2[1], theme.surface2[2], theme.surface2[3]);
        ImVec4 textDimV(theme.textDim[0], theme.textDim[1], theme.textDim[2], theme.textDim[3]);

        const char* tabs[2] = { "Interfaz", "Categorías de sidebar" };
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        for (int t = 0; t < 2; t++) {
            bool active = (s_ColorsTab == t);
            ImGui::PushStyleColor(ImGuiCol_Button,
                active ? ImVec4(accentV.x, accentV.y, accentV.z, 0.85f) : ImVec4(surf2V.x, surf2V.y, surf2V.z, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                active ? ImVec4(accentV.x, accentV.y, accentV.z, 0.95f) : ImVec4(surf2V.x, surf2V.y, surf2V.z, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accentV.x, accentV.y, accentV.z, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : textDimV);
            if (ImGui::Button(tabs[t], ImVec2(0.0f, 30.0f))) s_ColorsTab = t;
            ImGui::PopStyleColor(4);
            if (t == 0) ImGui::SameLine(0.0f, 8.0f);
        }
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        if (s_ColorsTab == 0) {
            ColorEntry fondos[] = {
                { "Fondo principal", theme.base,     "Color de ventanas principales." },
                { "Superficie 0",    theme.surface0, nullptr },
                { "Superficie 1",    theme.surface1, nullptr },
                { "Superficie 2",    theme.surface2, nullptr },
                { "Superficie 3",    theme.surface3, nullptr },
            };
            changed |= ColorChipGroup("FONDOS Y SUPERFICIES", fondos, 5, flags);

            ColorEntry acento[] = {
                { "Acento",        theme.accent,      nullptr },
                { "Acento claro",  theme.accentLight, nullptr },
                { "Acento oscuro", theme.accentDim,   nullptr },
                { "Acento tenue",  theme.accentFaint, nullptr },
            };
            changed |= ColorChipGroup("ACENTO", acento, 4, flags);

            ColorEntry bordesTexto[] = {
                { "Borde",            theme.border,      nullptr },
                { "Borde tenue",      theme.borderFaint, nullptr },
                { "Texto principal",  theme.textPrimary, nullptr },
                { "Texto secundario", theme.textDim,     nullptr },
                { "Texto inactivo",   theme.textFaint,   nullptr },
            };
            changed |= ColorChipGroup("BORDES Y TEXTO", bordesTexto, 5, flags);

            ColorEntry estados[] = {
                { "Error", theme.danger,  nullptr },
                { "Éxito", theme.success, nullptr },
            };
            changed |= ColorChipGroup("ESTADOS", estados, 2, flags);
        } else {
            auto& sidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;
            static const char* kCatLabels[6] = { "Letra", "Video", "Imagen", "Biblia", "Documentos", "Audio" };
            ColorEntry libEntries[6];
            for (int i = 0; i < 6; i++) libEntries[i] = { kCatLabels[i], sidebar.categoryColor[i], nullptr };
            if (ColorChipGroup("BIBLIOTECA", libEntries, 6, flags))
                ProyecThor::Settings::SettingsManager::Get().Save();

            auto& homeSidebar = ProyecThor::Settings::SettingsManager::Get().GetSettings().homeSidebar;
            static const char* kHomeCatLabels[6] = {
                "Home", "Contadores", "Anuncios", "Notas Rápidas", "Captura", "Transmisión en Red"
            };
            ColorEntry homeEntries[6];
            for (int i = 0; i < 6; i++) homeEntries[i] = { kHomeCatLabels[i], homeSidebar.categoryColor[i], nullptr };
            if (ColorChipGroup("HOME", homeEntries, 6, flags))
                ProyecThor::Settings::SettingsManager::Get().Save();

            auto& controlHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().controlHub;
            static const char* kControlCatLabels[2] = { "Control", "Stage Display" };
            ColorEntry controlEntries[2];
            for (int i = 0; i < 2; i++) controlEntries[i] = { kControlCatLabels[i], controlHub.categoryColor[i], nullptr };
            if (ColorChipGroup("CONTROL", controlEntries, 2, flags))
                ProyecThor::Settings::SettingsManager::Get().Save();

            auto& stylesHub = ProyecThor::Settings::SettingsManager::Get().GetSettings().stylesHub;
            static const char* kStylesCatLabels[3] = { "Fondos", "Estilos", "Transiciones" };
            ColorEntry stylesEntries[3];
            for (int i = 0; i < 3; i++) stylesEntries[i] = { kStylesCatLabels[i], stylesHub.categoryColor[i], nullptr };
            if (ColorChipGroup("DISEÑO", stylesEntries, 3, flags))
                ProyecThor::Settings::SettingsManager::Get().Save();
        }
    }

    if (SectionTitle("Forma", "Diseño")) {
        changed |= ImGui::SliderFloat("Redondeo de ventanas", &theme.windowRounding, 0.0f, 24.0f, "%.0f");
        changed |= ImGui::SliderFloat("Redondeo de controles", &theme.frameRounding, 0.0f, 16.0f, "%.0f");
        changed |= ImGui::SliderFloat("Grosor de scrollbar",   &theme.scrollbarSize, 4.0f, 16.0f, "%.0f");
    }

    if (changed) {
        theme.preset = ThemePreset::Custom;
        ProyecThor::Settings::SettingsManager::Get().ApplyTheme(); // preview en vivo
    }

    // ── Fuente de la interfaz ────────────────────────────────────────────────
    // Mismo selector que "Edición de estilo" (ver TabTypography::
    // RenderFontSelector): combo con las fuentes que ya están en
    // assets/fonts, + un botón para importar una nueva. Nada de elegir un
    // .ttf suelto del disco cada vez -- se elige de la misma lista/carpeta
    // que usa el resto de la app. El cambio se valida (IsValidFontFile) y
    // se aplica la próxima vez que se abra ProyecThor -- ver main.cpp.
    if (SectionTitle("Fuente de la interfaz", "Fuentes")) {
        ImGui::TextDisabled("Cambia la tipografía de toda la app.");
        ImGui::Spacing();

        std::vector<std::string> fontList;
        ProyecThor::Core::PresentationCore::Get().SyncFontListFromDisk(fontList);

        std::string currentFontName = theme.customFontPath.empty()
            ? "Predeterminada"
            : std::filesystem::path(theme.customFontPath).stem().string();

        const float importBtnW = 90.0f;
        const float comboGap    = 6.0f;
        float comboW = ImGui::GetContentRegionAvail().x - importBtnW - comboGap;

        ImGui::SetNextItemWidth(comboW);
        if (ImGui::BeginCombo("##uiFont", currentFontName.c_str())) {
            for (const auto& name : fontList) {
                bool sel = (name == currentFontName);
                if (ImGui::Selectable(name.c_str(), sel)) {
                    theme.customFontPath = ResolveFontPathByName(name);
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine(0.0f, comboGap);
        if (ImGui::Button("+ Fuente", ImVec2(importBtnW, 0.0f))) {
            std::string picked = PickFontFileDialog();
            if (!picked.empty()) {
                try {
                    std::filesystem::path fontsDir = std::filesystem::path(ProyecThor::GetAssetsPath()) / "fonts";
                    std::filesystem::create_directories(fontsDir);

                    std::filesystem::path src(picked);
                    std::filesystem::path dst = fontsDir / src.filename();
                    std::filesystem::copy(src, dst, std::filesystem::copy_options::overwrite_existing);

                    theme.customFontPath = dst.string();
                    ProyecThor::Settings::SettingsManager::Get().Save();
                    m_ShowFontRestartPrompt = true;
                } catch (const std::exception&) {
                    // Import fallido (permisos, disco, etc.): se deja la
                    // selección de fuente tal como estaba.
                }
            }
        }

        if (!theme.customFontPath.empty() && !IsValidFontFile(theme.customFontPath)) {
            ImGui::TextColored(ImVec4(0.93f, 0.35f, 0.35f, 1.0f),
                "\"%s\" no se pudo leer -- se usará la predeterminada.", currentFontName.c_str());
        }
    }

    // La fuente de la interfaz cambia el atlas de ImGui completo (y el de
    // la pantalla de carga) -- eso no se puede "reemplazar en caliente" de
    // forma segura mientras la app esta corriendo con VLC/GL en varias
    // ventanas a la vez, asi que en vez de aplicarla en silencio recien en
    // el proximo arranque, se ofrece reiniciar ya mismo. Fuera del
    // if(SectionTitle) a proposito: el modal tiene que seguir pudiendo
    // dibujarse aunque el usuario cambie de subcategoria mientras esta
    // abierto.
    if (m_ShowFontRestartPrompt) {
        ImGui::OpenPopup("Reiniciar para aplicar la fuente");
        m_ShowFontRestartPrompt = false;
    }
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Reiniciar para aplicar la fuente", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("La nueva fuente de la interfaz se aplica reiniciando ProyecThor. ¿Reiniciar ahora?");
        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        const float btnW = 150.0f;
        if (ImGui::Button("Reiniciar ahora", ImVec2(btnW, 34.0f))) {
            ProyecThor::Settings::SettingsManager::Get().RequestRestart();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, 10.0f);
        if (ImGui::Button("Más tarde", ImVec2(btnW, 34.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Las 4 paletas de identidad de sidebar (Biblioteca/Home/Control/Diseño)
    // ahora viven en la pestaña "Categorías de sidebar" de "Personalizar
    // colores" de más arriba (ver ColorChipGroup), no como bloques sueltos
    // acá abajo -- eran justamente lo que mas alargaba el scroll de esta
    // subcategoría.
}

} // namespace ProyecThor::UI::Settings