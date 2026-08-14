#include "LibrarySidebar.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/ui/DesignSystem.h"
#include "backend/settings/SettingsManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cmath>
#include <algorithm>

namespace DS = ProyecThor::UI::DS;

// El enum vive en LibraryPanel.h; aqui lo reproducimos como constantes locales
// para no crear una dependencia circular con el header del panel.
// El orden debe coincidir con LibraryCategory.
static constexpr int kCat_Songs      = 0;
static constexpr int kCat_Bibles     = 3;
static constexpr int kCat_Documents  = 4;
static constexpr int kCat_Multimedia = 6;

// Mismo motivo — espeja UI::LibrarySideMode (LibraryPanel.h). Red/Mobile se
// mudaron a Ajustes > Conexiones y Reloj al toolbar inline de ViewPanel, por
// eso los indices 1/2/4 no aparecen aca.
static constexpr int kSideMode_Categories = 0;
static constexpr int kSideMode_Render     = 3;
static constexpr int kSideMode_Overlay    = 4;

namespace ProyecThor::Library {

// Progreso animado (0..1) de "mostrar título" — misma idea que IconRail.cpp,
// para que este sidebar (implementacion propia, no comparte RenderIconRail)
// se comporte igual que los otros 3 rails ante Vista > Titulos en barras.
static float RailLabelProgress()
{
    bool wantLabels = ProyecThor::Settings::SettingsManager::Get().GetSettings().general.showRailLabels;
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##librarySidebarLabelT");
    float* cur = storage->GetFloatRef(id, wantLabels ? 1.0f : 0.0f);
    float target = wantLabels ? 1.0f : 0.0f;
    *cur += (target - *cur) * std::min(1.0f, ImGui::GetIO().DeltaTime * 10.0f);
    return *cur;
}

using DrawFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

// Dibuja un boton del sidebar (icono + label opcional + barra de acento +
// hover) en la posicion actual del cursor, consumiendo btnH de alto —
// mismo look para las 6 categorias de contenido y para el grupo Red/Reloj
// de abajo (ver comentario de RenderCategoryButtons). Devuelve true si se
// clickeo. No muta ctx: el llamador decide que hacer con el click.
static bool RenderSidebarButton(ImDrawList* dl, ImGuiStorage* storage,
                                float sidebarW, float btnH, float iconSz, float lt,
                                const char* label, DrawFn drawIcon,
                                bool active, const float cc[4])
{
    const ImU32 accentBar = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));
    constexpr float rounding = 5.0f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 bMin   = cursor;
    ImVec2 bMax   = { cursor.x + sidebarW, cursor.y + btnH };

    ImGuiID hovId = ImGui::GetID(label);
    float*  pT    = storage->GetFloatRef(hovId ^ 0xABCD1234u, 0.0f);
    bool hovered  = ImGui::IsMouseHoveringRect(bMin, bMax, false);
    *pT = Lerp(*pT, hovered ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float t = *pT;

    // ── Fondo ─────────────────────────────────────────────────────────
    if (active) {
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
        ac.w = 0.12f;
        dl->AddRectFilled(bMin, bMax,
                          ImGui::ColorConvertFloat4ToU32(ac), rounding);
    } else if (t > 0.01f) {
        dl->AddRectFilled(bMin, bMax,
                          IM_COL32(255, 255, 255, (int)(t * 14.f)), rounding);
    }

    // ── Barra lateral izquierda ────────────────────────────────────────
    {
        float barH     = btnH * 0.60f * (active ? 1.0f : t);
        float barY0    = cursor.y + (btnH - barH) * 0.5f;
        float barAlpha = active ? 1.0f : t * 0.55f;
        ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accentBar);
        ac.w           = barAlpha;
        dl->AddRectFilled(
            { bMin.x,        barY0 },
            { bMin.x + 3.0f, barY0 + barH },
            ImGui::ColorConvertFloat4ToU32(ac), 2.0f);
    }

    ImGui::SetCursorScreenPos(bMin);
    const std::string btnId = std::string("##cat_") + label;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), { sidebarW, btnH });

    // ── Icono + label ──────────────────────────────────────────────────
    // Base theme-aware (DS::TextSecondary..TextPrimary) en vez de gris/
    // blanco fijo -- pedido explicito: al seleccionar (active=true) esto
    // quedaba en blanco puro, invisible contra un rail con fondo claro (ver
    // DS::GlassFillTop arriba, ya theme-aware).
    {
        ImVec4 textPriV = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
        ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
        float  brightT  = active ? 1.0f : t;
        ImVec4 icF = LerpColor(textDimV, textPriV, brightT);
        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
            icF = LerpColor(icF, ac, 0.35f);
            icF.w = 1.0f;
        }

        ImVec2 lblDim       = ImGui::CalcTextSize(label);
        float  totalContent = iconSz + lt * (5.0f + lblDim.y);
        float  startY       = cursor.y + (btnH - totalContent) * 0.5f;
        float  iconX        = cursor.x + (sidebarW - iconSz) * 0.5f;

        drawIcon(dl, { iconX, startY }, iconSz,
                ImGui::ColorConvertFloat4ToU32(icF));

        if (lt > 0.01f) {
            ImVec4 lblF = LerpColor(textDimV, textPriV, brightT);
            lblF.w = lt;
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
                lblF = LerpColor(lblF, ac, 0.25f);
                lblF.w = lt;
            }

            float lblX = cursor.x + (sidebarW - lblDim.x) * 0.5f;
            float lblY = startY + iconSz + 5.0f;
            dl->AddText({ lblX, lblY },
                        ImGui::ColorConvertFloat4ToU32(lblF), label);
        }
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", label);

    return clicked;
}

void RenderCategoryButtons(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    // El color de identidad de cada categoria (accentBar) es configurable
    // desde Ajustes > Apariencia (SettingsManager: librarySidebar.categoryColor,
    // en el mismo orden que este array). El resto del look de cada boton
    // (fondo activo, barra lateral, tinte de icono/label) se deriva de ese
    // unico color mas abajo, no hace falta guardar variantes aparte.
    struct CatDef {
        int         catInt;
        DrawFn      drawIcon;
        const char* label;
    };

    // Sin "static": el label depende del idioma activo (ver GetUIStrings),
    // que solo cambia con un reinicio de la app, pero recalcularlo por
    // frame es gratis y evita que un CatDef "static" quede con el idioma
    // del primer frame para siempre.
    const CatDef k_Cats[] = {
        { kCat_Songs,      DrawIcon_Music,      str.libRailSongs      },
        { kCat_Multimedia, DrawIcon_Multimedia, str.libRailMultimedia },
        { kCat_Bibles,     DrawIcon_Cross,      str.libCatBible       },
        { kCat_Documents,  DrawIcon_Document,   str.libRailDocs       },
    };

    const auto& sidebarSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;

    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const float  sidebarW = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();

    // FIX: antes un negro-azulado fijo (IM_COL32(11,11,20,255)) sin relacion
    // con el tema elegido en Ajustes > Apariencia -- desentonaba contra el
    // resto del panel (glass, sincronizado con el tema via DS::SyncFromTheme).
    dl->AddRectFilled(winPos, { winPos.x + sidebarW, winPos.y + winH },
                      DS::GlassFillTop);

    ImGui::Dummy({ sidebarW, 4.0f });

    constexpr float btnGapY  = 1.0f;
    const float     btnH     = 46.0f;
    const float     iconSz   = std::floor(btnH * 0.38f);
    (void)winH;

    const float lt = RailLabelProgress();

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, btnGapY));

    for (int catIdx = 0; catIdx < (int)(sizeof(k_Cats) / sizeof(k_Cats[0])); catIdx++)
    {
        const auto& cd     = k_Cats[catIdx];
        const bool  active = (ctx.sideModeInt == kSideMode_Categories) && (ctx.currentCategoryInt == cd.catInt);

        bool clicked = RenderSidebarButton(dl, storage, sidebarW, btnH, iconSz, lt,
                                           cd.label, cd.drawIcon, active,
                                           sidebarSettings.categoryColor[catIdx]);
        if (clicked) {
            ctx.currentCategoryInt = cd.catInt;
            ctx.sideModeInt        = kSideMode_Categories;
            ctx.selectedIndex      = -1;
            ctx.refreshList();
        }
    }

    // ── Grupo aparte "Render"/"Overlay" ─────────────────────────────────────
    // Mudados desde ViewToolsPanel/LibraryManagerPanel — el operador los
    // pedia junto a la biblioteca de contenido, no mezclados con las
    // categorias de arriba. Antes llevaban una linea separadora de 1px acá
    // -- pedido explicito de sacarla (se veia como un corte feo en el
    // rail); el espacio extra de por si ya lee como "grupo aparte" sin
    // necesidad de la linea. No tocan ctx.currentCategoryInt/LibraryCategory:
    // usan su propio modo (ctx.sideModeInt, ver UI::LibrarySideMode en
    // LibraryPanel.h). Red y Mobile vivian aca tambien; se mudaron a
    // Ajustes > Conexiones (ver CategoryConnections.cpp), junto con
    // Streaming (RTMP) y OSC. Reloj tambien vivia aca; se saco por quedar
    // duplicado con el toolbar inline de ViewPanel (ver InlineTool::Clock).
    ImGui::Dummy({ sidebarW, 8.0f });

    struct SideDef { const char* label; DrawFn drawIcon; int mode; };
    static const SideDef k_SideItems[] = {
        { "Render",   ProyecThor::UI::AppIcons::DrawIcon_Swap,       kSideMode_Render    },
        { "Overlay",  ProyecThor::UI::AppIcons::DrawIcon_Overlay,    kSideMode_Overlay   },
    };

    for (const auto& sd : k_SideItems)
    {
        const bool active = (ctx.sideModeInt == sd.mode);
        // Colores en los indices 8/9 de librarySidebar.categoryColor — ver
        // SettingsManager.h (7, Reloj, quedo sin uso aca).
        int colorIdx = (sd.mode == kSideMode_Render) ? 8 : 9;

        bool clicked = RenderSidebarButton(dl, storage, sidebarW, btnH, iconSz, lt,
                                           sd.label, sd.drawIcon, active,
                                           sidebarSettings.categoryColor[colorIdx]);
        if (clicked) ctx.sideModeInt = sd.mode;
    }

    ImGui::PopStyleVar();
}

} // namespace ProyecThor::Library