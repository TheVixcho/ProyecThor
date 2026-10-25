#include "LibrarySidebar.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/framework/UIStrings.h"
#include "ui/framework/AppIcons.h"
#include "ui/panels/home/HomeIcons.h"
#include "ui/framework/DesignSystem.h"
#include "core/settings/SettingsManager.h"

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

// Mismo motivo — espeja UI::LibrarySideMode (LibraryPanel.h).
static constexpr int kSideMode_Categories = 0;
static constexpr int kSideMode_Clock      = 2;
static constexpr int kSideMode_Render     = 3;
static constexpr int kSideMode_Overlay    = 4;
static constexpr int kSideMode_Web        = 5;
static constexpr int kSideMode_Model3D    = 6;
static constexpr int kSideMode_Lab        = 7;
static constexpr int kSideMode_Picker     = 8;
static constexpr int kSideMode_Plugins    = 9;

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

    ImGui::SetCursorScreenPos(bMin);
    const std::string btnId = std::string("##cat_") + label;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), { sidebarW, btnH });
    bool hovered = ImGui::IsItemHovered();
    float t = hovered ? 1.0f : 0.0f;

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

// Dibuja un boton de categoria para toolbar horizontal inferior.
static bool RenderSidebarButtonHorizontal(ImDrawList* dl, ImGuiStorage* storage,
                                          float btnW, float btnH, float iconSz, float lt,
                                          const char* label, DrawFn drawIcon,
                                          bool active, const float cc[4])
{
    const ImU32 accentBar = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[0], cc[1], cc[2], cc[3]));
    constexpr float rounding = DS::RadiusSmall;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 bMin   = { cursor.x, cursor.y + 2.0f };
    ImVec2 bMax   = { cursor.x + btnW, cursor.y + btnH - 2.0f };

    ImGui::SetCursorScreenPos(cursor);
    const std::string btnId = std::string("##cat_h_") + label;
    bool clicked = ImGui::InvisibleButton(btnId.c_str(), { btnW, btnH });
    bool hovered = ImGui::IsItemHovered();
    float t = hovered ? 1.0f : 0.0f;

    // ── Fondo ─────────────────────────────────────────────────────────
    if (active) {
        ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
        ac.w = 0.14f;
        dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding);
        ac.w = 0.28f;
        dl->AddRect(bMin, bMax, ImGui::ColorConvertFloat4ToU32(ac), rounding, 0, 1.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, (int)(t * 16.f)), rounding);
    }

    // ── Indicador superior (conecta el botón con el contenido del panel) ──
    {
        float barW     = (btnW - 10.0f) * (active ? 1.0f : t);
        float barX0    = cursor.x + (btnW - barW) * 0.5f;
        float barAlpha = active ? 1.0f : t * 0.55f;
        ImVec4 ac      = ImGui::ColorConvertU32ToFloat4(accentBar);
        ac.w           = barAlpha;
        dl->AddRectFilled(
            { barX0, bMin.y },
            { barX0 + barW, bMin.y + 2.5f },
            ImGui::ColorConvertFloat4ToU32(ac), 1.0f);
    }

    // ── Icono + label ──────────────────────────────────────────────────
    dl->PushClipRect(bMin, bMax, true);
    {
        ImVec4 textPriV = ImGui::ColorConvertU32ToFloat4(DS::TextPrimary);
        ImVec4 textDimV = ImGui::ColorConvertU32ToFloat4(DS::TextSecondary);
        float  brightT  = active ? 1.0f : t;
        ImVec4 icF      = LerpColor(textDimV, textPriV, brightT);
        if (active) {
            ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
            icF = LerpColor(icF, ac, 0.35f);
            icF.w = 1.0f;
        }

        if (lt > 0.05f) {
            ImVec2 lblDim       = ImGui::CalcTextSize(label);
            float  totalContent = iconSz + lt * (3.0f + lblDim.y);
            float  startY       = cursor.y + (btnH - totalContent) * 0.5f + 1.0f;
            float  iconX        = cursor.x + (btnW - iconSz) * 0.5f;

            drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));

            ImVec4 lblF = LerpColor(textDimV, textPriV, brightT);
            lblF.w      = lt;
            if (active) {
                ImVec4 ac = ImGui::ColorConvertU32ToFloat4(accentBar);
                lblF = LerpColor(lblF, ac, 0.25f);
                lblF.w = lt;
            }

            float lblX = cursor.x + (btnW - lblDim.x) * 0.5f;
            float lblY = startY + iconSz + 2.0f;
            dl->AddText({ lblX, lblY }, ImGui::ColorConvertFloat4ToU32(lblF), label);
        } else {
            float startY = cursor.y + (btnH - iconSz) * 0.5f;
            float iconX  = cursor.x + (btnW - iconSz) * 0.5f;
            drawIcon(dl, { iconX, startY }, iconSz, ImGui::ColorConvertFloat4ToU32(icF));
        }
    }
    dl->PopClipRect();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("%s", label);

    return clicked;
}

static void RenderCategoryButtonsVertical(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    // El color de identidad de cada categoria (accentBar) es configurable
    // desde Ajustes > Apariencia (SettingsManager: librarySidebar.categoryColor,
    // en el mismo orden que este array). El resto del look de cada boton
    // (fondo activo, barra lateral, tinte de icono/label) se deriva de ese
    // unico color mas abajo, no hace falta guardar variantes aparte.
    struct RailItemDef {
        int         id;
        bool        isCategory;
        DrawFn      drawIcon;
        const char* label;
        int         colorIndex;
    };

    const RailItemDef k_MainItems[] = {
        { kCat_Multimedia,   true,  DrawIcon_Multimedia,                   str.libRailMultimedia, 1 },
        { kSideMode_Overlay,  false, ProyecThor::UI::AppIcons::DrawIcon_Overlay, "Overlays",            9 },
        { kCat_Songs,        true,  DrawIcon_Music,                        str.libRailSongs,      0 },
        { kCat_Bibles,       true,  DrawIcon_Cross,                        str.libCatBible,       3 },
        { kCat_Documents,    true,  DrawIcon_Document,                     str.libRailDocs,       4 },
        { kSideMode_Clock,    false, ProyecThor::UI::HomeIcons::DrawIcon_Clock, "Reloj",               7 },
    };

    const auto& sidebarSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;

    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const float  sidebarW = ImGui::GetContentRegionAvail().x;
    const float  winH     = ImGui::GetWindowHeight();
    const ImVec2 winPos   = ImGui::GetWindowPos();

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

    for (int idx = 0; idx < (int)(sizeof(k_MainItems) / sizeof(k_MainItems[0])); idx++)
    {
        const auto& it = k_MainItems[idx];

        const bool active = it.isCategory
            ? ((ctx.sideModeInt == kSideMode_Categories) && (ctx.currentCategoryInt == it.id))
            : (ctx.sideModeInt == it.id);

        bool clicked = RenderSidebarButton(dl, storage, sidebarW, btnH, iconSz, lt,
                                           it.label, it.drawIcon, active,
                                           sidebarSettings.categoryColor[it.colorIndex]);
        if (clicked) {
            if (it.isCategory) {
                ctx.currentCategoryInt = it.id;
                ctx.sideModeInt        = kSideMode_Categories;
                ctx.selectedIndex      = -1;
                ctx.refreshList();
            } else {
                ctx.sideModeInt        = it.id;
            }
        }
    }

    // ── Grupo secundario (Render / Web / 3D / Lab / Paneles) ───────────────
    ImGui::Dummy({ sidebarW, 8.0f });

    struct SideDef { const char* label; DrawFn drawIcon; int mode; };
    static const SideDef k_SideItems[] = {
        { "Render",   ProyecThor::UI::AppIcons::DrawIcon_Swap,       kSideMode_Render    },
        { "Web",      ProyecThor::UI::AppIcons::DrawIcon_Globe,      kSideMode_Web       },
        { "3D",       ProyecThor::UI::AppIcons::DrawIcon_Cube3D,     kSideMode_Model3D   },
        { "Lab",      ProyecThor::UI::AppIcons::DrawIcon_Formula,    kSideMode_Lab       },
        { "Plugins",  ProyecThor::UI::AppIcons::DrawIcon_Plugin,     kSideMode_Plugins   },
        { "Paneles",  ProyecThor::UI::AppIcons::DrawIcon_Grid,       kSideMode_Picker    },
    };

    for (const auto& sd : k_SideItems)
    {
        if (sd.mode == kSideMode_Render && !sidebarSettings.showRender) continue;
        if (sd.mode == kSideMode_Web && !sidebarSettings.showWeb) continue;
        if (sd.mode == kSideMode_Model3D && !sidebarSettings.showModel3D) continue;
        if (sd.mode == kSideMode_Lab && !sidebarSettings.showLab) continue;
        if (sd.mode == kSideMode_Plugins && !sidebarSettings.showPlugins) continue;

        const bool active = (ctx.sideModeInt == sd.mode);
        int colorIdx = (sd.mode == kSideMode_Render) ? 8 : 9;

        bool clicked = RenderSidebarButton(dl, storage, sidebarW, btnH, iconSz, lt,
                                           sd.label, sd.drawIcon, active,
                                           sidebarSettings.categoryColor[colorIdx]);
        if (clicked) {
            if (sd.mode == kSideMode_Picker) {
                if (ctx.openPanelPicker) {
                    ctx.openPanelPicker();
                }
            } else {
                ctx.sideModeInt = sd.mode;
            }
        }
    }

    ImGui::PopStyleVar();
}

static void RenderCategoryButtonsHorizontal(LibraryContext& ctx)
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    struct RailItemDef {
        int         id;
        bool        isCategory;
        DrawFn      drawIcon;
        const char* label;
        int         colorIndex;
    };

    const RailItemDef k_MainItems[] = {
        { kCat_Multimedia,   true,  DrawIcon_Multimedia,                   str.libRailMultimedia, 1 },
        { kSideMode_Overlay,  false, ProyecThor::UI::AppIcons::DrawIcon_Overlay, "Overlays",            9 },
        { kCat_Songs,        true,  DrawIcon_Music,                        str.libRailSongs,      0 },
        { kCat_Bibles,       true,  DrawIcon_Cross,                        str.libCatBible,       3 },
        { kCat_Documents,    true,  DrawIcon_Document,                     str.libRailDocs,       4 },
        { kSideMode_Clock,    false, ProyecThor::UI::HomeIcons::DrawIcon_Clock, "Reloj",               7 },
    };

    const auto& sidebarSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar;

    struct SideDef { const char* label; DrawFn drawIcon; int mode; };
    static const SideDef k_SideItems[] = {
        { "Render",   ProyecThor::UI::AppIcons::DrawIcon_Swap,       kSideMode_Render    },
        { "Web",      ProyecThor::UI::AppIcons::DrawIcon_Globe,      kSideMode_Web       },
        { "3D",       ProyecThor::UI::AppIcons::DrawIcon_Cube3D,     kSideMode_Model3D   },
        { "Lab",      ProyecThor::UI::AppIcons::DrawIcon_Formula,    kSideMode_Lab       },
        { "Plugins",  ProyecThor::UI::AppIcons::DrawIcon_Plugin,     kSideMode_Plugins   },
        { "Paneles",  ProyecThor::UI::AppIcons::DrawIcon_Grid,       kSideMode_Picker    },
    };

    int sideCount = 0;
    for (const auto& sd : k_SideItems)
    {
        if (sd.mode == kSideMode_Render && !sidebarSettings.showRender) continue;
        if (sd.mode == kSideMode_Web && !sidebarSettings.showWeb) continue;
        if (sd.mode == kSideMode_Model3D && !sidebarSettings.showModel3D) continue;
        if (sd.mode == kSideMode_Lab && !sidebarSettings.showLab) continue;
        if (sd.mode == kSideMode_Plugins && !sidebarSettings.showPlugins) continue;
        sideCount++;
    }

    constexpr int mainCount    = (int)(sizeof(k_MainItems) / sizeof(k_MainItems[0]));
    const int     totalButtons = mainCount + sideCount;

    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    const float  toolbarW = ImGui::GetContentRegionAvail().x;
    const float  toolbarH = ImGui::GetContentRegionAvail().y;
    const ImVec2 winPos   = ImGui::GetWindowPos();

    dl->AddRectFilled(winPos, { winPos.x + toolbarW, winPos.y + toolbarH },
                      DS::GlassFillTop);

    constexpr float btnGapX      = 3.0f;
    const float     sepWidth     = (sideCount > 0) ? 12.0f : 0.0f;
    const float     padLeft      = 6.0f;
    const float     totalGaps    = (totalButtons > 1) ? (totalButtons - 1) * btnGapX : 0.0f;
    const float     availForBtns = std::max(20.0f, toolbarW - padLeft * 2.0f - sepWidth - totalGaps);
    const float     rawBtnW      = (totalButtons > 0) ? (availForBtns / totalButtons) : 60.0f;

    const float lt = RailLabelProgress();

    float btnW;
    float effectiveLt;

    if (lt > 0.01f) {
        if (rawBtnW >= 56.0f) {
            btnW        = std::clamp(rawBtnW, 56.0f, 88.0f);
            effectiveLt = lt;
        } else if (rawBtnW >= 40.0f) {
            btnW        = rawBtnW;
            effectiveLt = lt * std::clamp((rawBtnW - 40.0f) / 16.0f, 0.0f, 1.0f);
        } else {
            btnW        = std::clamp(rawBtnW, 28.0f, 40.0f);
            effectiveLt = 0.0f;
        }
    } else {
        btnW        = std::clamp(rawBtnW, 28.0f, 44.0f);
        effectiveLt = 0.0f;
    }

    const float iconSz = (effectiveLt > 0.05f)
        ? std::clamp(std::floor(toolbarH * 0.34f), 14.0f, 17.0f)
        : std::clamp(std::floor(toolbarH * 0.44f), 16.0f, 21.0f);

    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(btnGapX, 0.f));

    ImGui::Dummy({ padLeft, toolbarH });
    ImGui::SameLine(0.0f, 0.0f);

    for (int idx = 0; idx < mainCount; idx++)
    {
        const auto& it = k_MainItems[idx];

        const bool active = it.isCategory
            ? ((ctx.sideModeInt == kSideMode_Categories) && (ctx.currentCategoryInt == it.id))
            : (ctx.sideModeInt == it.id);

        if (idx > 0) ImGui::SameLine();

        bool clicked = RenderSidebarButtonHorizontal(dl, storage, btnW, toolbarH, iconSz, effectiveLt,
                                                     it.label, it.drawIcon, active,
                                                     sidebarSettings.categoryColor[it.colorIndex]);
        if (clicked) {
            if (it.isCategory) {
                ctx.currentCategoryInt = it.id;
                ctx.sideModeInt        = kSideMode_Categories;
                ctx.selectedIndex      = -1;
                ctx.refreshList();
            } else {
                ctx.sideModeInt        = it.id;
            }
        }
    }

    if (sideCount > 0)
    {
        ImGui::SameLine();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float sepY0 = p.y + (toolbarH - 20.0f) * 0.5f;
        dl->AddLine({ p.x + 4.0f, sepY0 }, { p.x + 4.0f, sepY0 + 20.0f },
                    DS::GlassBorder, 1.0f);
        ImGui::Dummy(ImVec2(sepWidth, toolbarH));
        ImGui::SameLine();
    }

    bool firstSide = true;
    for (const auto& sd : k_SideItems)
    {
        if (sd.mode == kSideMode_Render && !sidebarSettings.showRender) continue;
        if (sd.mode == kSideMode_Web && !sidebarSettings.showWeb) continue;
        if (sd.mode == kSideMode_Model3D && !sidebarSettings.showModel3D) continue;
        if (sd.mode == kSideMode_Lab && !sidebarSettings.showLab) continue;
        if (sd.mode == kSideMode_Plugins && !sidebarSettings.showPlugins) continue;

        if (!firstSide) ImGui::SameLine();
        firstSide = false;

        const bool active = (ctx.sideModeInt == sd.mode);
        int colorIdx = (sd.mode == kSideMode_Render) ? 8 : 9;

        bool clicked = RenderSidebarButtonHorizontal(dl, storage, btnW, toolbarH, iconSz, effectiveLt,
                                                     sd.label, sd.drawIcon, active,
                                                     sidebarSettings.categoryColor[colorIdx]);
        if (clicked) {
            if (sd.mode == kSideMode_Picker) {
                if (ctx.openPanelPicker) {
                    ctx.openPanelPicker();
                }
            } else {
                ctx.sideModeInt = sd.mode;
            }
        }
    }

    ImGui::PopStyleVar();
}

void RenderCategoryButtons(LibraryContext& ctx, bool horizontal)
{
    if (horizontal)
        RenderCategoryButtonsHorizontal(ctx);
    else
        RenderCategoryButtonsVertical(ctx);
}

} // namespace ProyecThor::Library