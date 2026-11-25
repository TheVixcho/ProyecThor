#include "HelpPanel.h"
#include "SettingsManager.h"
#include "core/PresentationCore.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"
#include "ui/framework/PatchNotesRenderer.h"
#include "external/tools/OpenURL.h"
#include "qrcodegen.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <vector>

namespace ProyecThor::UI {

static inline bool IsEnglishActive() {
    return ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language == ProyecThor::Settings::Language::English;
}

static inline bool IsPortugueseActive() {
    return ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language == ProyecThor::Settings::Language::Portuguese;
}

static inline const char* Tr(const char* es, const char* en, const char* pt) {
    if (IsEnglishActive()) return en;
    if (IsPortugueseActive()) return pt;
    return es;
}

static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

static inline void DrawFloatingIslandShadow(ImDrawList* dl, ImVec2 pos, ImVec2 size, float rounding) {
    const int   layers    = 6;
    const float maxOffset = 20.0f;
    for (int i = layers; i >= 1; --i) {
        float  t      = (float)i / (float)layers;
        float  offset = maxOffset * t;
        int    alpha  = (int)(44.0f * (1.0f - t * 0.5f));
        ImVec2 p0(pos.x - offset * 0.25f,         pos.y + offset * 0.25f);
        ImVec2 p1(pos.x + size.x + offset * 0.25f, pos.y + size.y + offset * 0.60f);
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, alpha), rounding + offset * 0.20f);
    }
}

HelpPanel::HelpPanel()
    : m_SelectedCategory(0)
    , m_WasOpenLastFrame(false)
    , m_CopiedTimer(0.0f)
    , m_CopiedMessage("")
    , m_ActiveQrCard(-1)
{
}

void HelpPanel::CopyToClipboard(const char* text, const char* label) {
    if (text) {
        ImGui::SetClipboardText(text);
        m_CopiedTimer = 3.0f;
        m_CopiedMessage = label ? label : text;
    }
}

void HelpPanel::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        m_WasOpenLastFrame = false;
        m_ActiveQrCard = -1;
        return;
    }

    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float dt = ImGui::GetIO().DeltaTime;
    if (m_CopiedTimer > 0.0f) {
        m_CopiedTimer -= dt;
        if (m_CopiedTimer <= 0.0f) {
            m_CopiedMessage.clear();
        }
    }

    m_WasOpenLastFrame = true;

    // Pantalla completa sobre el viewport principal (estilo idéntico a Configuraciones)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ThemeCol(theme.base, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ThemeCol(theme.surface0, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ThemeCol(theme.surface1, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0, 0, 0, 0));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool open = ImGui::Begin("##HelpPanelFullscreen", isOpen,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking);

    bool ownPopupOpen = ImGui::GetCurrentContext()->OpenPopupStack.Size > 0 &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
         ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup));
    if (!ownPopupOpen)
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!open) {
        ImGui::End();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ownPopupOpen) {
        *isOpen = false;
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* winDl = ImGui::GetWindowDrawList();

    // ── Barra Superior de Cabecera (Header Bar) ─────────────────────────────
    const float topBarH = 62.0f;
    ImVec2 topBarMin = ImGui::GetCursorScreenPos();
    ImVec2 topBarMax = ImVec2(topBarMin.x + avail.x, topBarMin.y + topBarH);

    winDl->AddRectFilled(topBarMin, topBarMax, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.98f)));
    winDl->AddLine(ImVec2(topBarMin.x, topBarMax.y), topBarMax,
                   ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.65f)), 1.2f);

    // 1. Icono + Título "Ayuda y Acerca de"
    {
        float iconSz = 22.0f;
        ImVec2 iconPos(topBarMin.x + 24.0f, topBarMin.y + (topBarH - iconSz) * 0.5f);
        ImU32 accentCol = IM_COL32(45, 212, 191, 255); // Turquesa Ayuda
        AppIcons::DrawIcon_Help(winDl, iconPos, iconSz, accentCol);

        float textX = iconPos.x + iconSz + 12.0f;
        float textY = topBarMin.y + (topBarH - ImGui::GetFontSize() * 1.30f) * 0.5f - 1.0f;
        ImGui::SetCursorScreenPos(ImVec2(textX, textY));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::SetWindowFontScale(1.30f);
        ImGui::TextUnformatted(Tr("Ayuda y Comunidad", "Help & Community", "Ajuda e Comunidade"));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // Badge de versión
        float badgeX = textX + ImGui::CalcTextSize(Tr("Ayuda y Comunidad", "Help & Community", "Ajuda e Comunidade")).x * 1.30f + 14.0f;
        float badgeY = topBarMin.y + (topBarH - 22.0f) * 0.5f;
        ImVec2 bMin(badgeX, badgeY), bMax(badgeX + 60.0f, badgeY + 22.0f);
        winDl->AddRectFilled(bMin, bMax, IM_COL32(45, 212, 191, 40), 11.0f);
        winDl->AddRect(bMin, bMax, IM_COL32(45, 212, 191, 130), 11.0f, 0, 1.0f);
        const char* verStr = "v1.2.0";
        ImVec2 vSz = ImGui::CalcTextSize(verStr);
        winDl->AddText(ImVec2(badgeX + (60.0f - vSz.x) * 0.5f, badgeY + (22.0f - vSz.y) * 0.5f),
                       IM_COL32(45, 212, 191, 255), verStr);
    }

    // Notificación flotante de enlace copiado
    if (m_CopiedTimer > 0.0f) {
        float toastW = 280.0f;
        float toastH = 34.0f;
        float toastX = topBarMin.x + (avail.x - toastW) * 0.5f;
        float toastY = topBarMin.y + (topBarH - toastH) * 0.5f;
        ImVec2 tMin(toastX, toastY), tMax(toastX + toastW, toastY + toastH);
        winDl->AddRectFilled(tMin, tMax, IM_COL32(16, 185, 129, 230), 17.0f);
        winDl->AddRect(tMin, tMax, IM_COL32(255, 255, 255, 160), 17.0f, 0, 1.2f);
        const char* toastMsg = Tr("Enlace copiado al portapapeles", "Link copied to clipboard", "Ligação copiada para a área de transferência");
        ImVec2 mSz = ImGui::CalcTextSize(toastMsg);
        winDl->AddText(ImVec2(toastX + (toastW - mSz.x) * 0.5f, toastY + (toastH - mSz.y) * 0.5f),
                       IM_COL32(255, 255, 255, 255), toastMsg);
    }

    // 2. Botón "✕ Cerrar" a la derecha
    {
        float closeBtnW = 105.0f;
        float closeBtnH = 34.0f;
        float closeBtnX = topBarMin.x + avail.x - closeBtnW - 24.0f;
        float closeBtnY = topBarMin.y + (topBarH - closeBtnH) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(closeBtnX, closeBtnY));
        if (ProyecThor::UI::DS::GlassButton(Tr("✕ Cerrar", "✕ Close", "✕ Fechar"), ImVec2(closeBtnW, closeBtnH))) {
            *isOpen = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", Tr("Cerrar ventana (Esc)", "Close window (Esc)", "Fechar janela (Esc)"));
        }
    }

    // ── Cuerpo: Islas flotantes (Sidebar + Contenido) ───────────────────────
    const float bodyY = topBarMin.y + topBarH;
    const float bodyH = avail.y - topBarH;
    const float margin  = 16.0f;
    const float gap     = 14.0f;
    const float sideW   = 270.0f;
    const float islandH  = bodyH - margin * 2.0f;
    const float contentW = avail.x - margin * 2.0f - sideW - gap;

    ImVec2 sideOrigin(topBarMin.x + margin, bodyY + margin);
    ImVec2 contentOrigin(sideOrigin.x + sideW + gap, sideOrigin.y);

    DrawFloatingIslandShadow(winDl, sideOrigin,    ImVec2(sideW,    islandH), theme.windowRounding);
    DrawFloatingIslandShadow(winDl, contentOrigin, ImVec2(contentW, islandH), theme.windowRounding);

    // Isla derecha: Contenido
    ImGui::SetCursorScreenPos(contentOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::BeginChild("##help_content_island", ImVec2(contentW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

    ImVec2 islandAvail = ImGui::GetContentRegionAvail();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(36.0f, 32.0f));
    ImGui::BeginChild("##help_content_scroll", islandAvail,
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PopStyleVar();

    RenderContent();

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // Isla izquierda: Navegación lateral
    ImGui::SetCursorScreenPos(sideOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 25.0f));
    ImGui::BeginChild("##help_sidebar", ImVec2(sideW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    RenderSidebar();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void HelpPanel::RenderSidebar() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::TextUnformatted("ProyecThor");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(25.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::Text("%s", Tr("Centro de Ayuda y Soporte", "Help & Support Center", "Centro de Ajuda e Suporte"));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    struct NavItem {
        const char* label;
        const char* desc;
        ImU32       color;
    };

    const NavItem items[] = {
        { Tr("Comunidad y Redes", "Community & Social", "Comunidade e Redes"),
          Tr("Discord, Instagram, WhatsApp y web", "Discord, Instagram, WhatsApp & web", "Discord, Instagram, WhatsApp e web"),
          IM_COL32(45, 212, 191, 255) },

        { Tr("Acerca de y Créditos", "About & Credits", "Sobre e Créditos"),
          Tr("Creador, biblias, comunidad y licencia", "Creator, bibles, community & license", "Criador, bíblias, comunidade e licença"),
          IM_COL32(245, 180, 50, 255) },

        { Tr("Notas de Versión", "Release Notes", "Notas de Versão"),
          Tr("Novedades y cambios de la v1.2.0", "What's new in v1.2.0", "Novidades e alterações da v1.2.0"),
          IM_COL32(56, 189, 248, 255) },

        { Tr("Atajos de Teclado", "Keyboard Shortcuts", "Teclas de Atalho"),
          Tr("Guía rápida de comandos y teclas", "Quick guide for keys & controls", "Guia rápido de teclas e comandos"),
          IM_COL32(167, 139, 250, 255) },

        { Tr("Donaciones", "Donations", "Doações"),
          Tr("Apoya el desarrollo continuo del proyecto", "Support ongoing project development", "Apoie o desenvolvimento contínuo do projeto"),
          IM_COL32(244, 114, 182, 255) }
    };

    const float itemH = 48.0f;
    const float itemW = ImGui::GetContentRegionAvail().x - 20.0f;

    for (int i = 0; i < 5; i++) {
        bool selected = (m_SelectedCategory == i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f;

        ImGui::SetCursorPosX(10.0f);
        std::string btnId = "##helpNav_" + std::to_string(i);

        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0, 0, 0, 0));

        if (ImGui::Selectable(btnId.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH))) {
            m_SelectedCategory = i;
            m_ActiveQrCard = -1;
        }
        bool hovered = ImGui::IsItemHovered();

        // Fondo del ítem
        if (selected) {
            dl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.70f)), 8.0f);
            dl->AddRect(p, ImVec2(p.x + itemW, p.y + itemH),
                        items[i].color, 8.0f, 0, 1.2f);
            // Barra indicadora a la izquierda
            dl->AddRectFilled(ImVec2(p.x + 2.0f, p.y + 6.0f), ImVec2(p.x + 6.0f, p.y + itemH - 6.0f),
                              items[i].color, 2.0f);
        } else if (hovered) {
            dl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.50f)), 8.0f);
        }

        // Chip / Icono circular
        ImVec2 badgeC(p.x + 24.0f, p.y + itemH * 0.5f);
        ImVec4 iconColV = ImGui::ColorConvertU32ToFloat4(items[i].color);
        iconColV.w = selected ? 0.30f : (hovered ? 0.18f : 0.12f);
        dl->AddCircleFilled(badgeC, 12.0f, ImGui::ColorConvertFloat4ToU32(iconColV), 16);
        dl->AddCircle(badgeC, 12.0f, items[i].color, 16, selected ? 1.4f : 0.8f);

        // Texto de etiqueta y subtítulo
        float labelX = p.x + 44.0f;
        float labelY = p.y + 8.0f;
        ImVec4 labelCol = selected ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textDim, hovered ? 1.0f : 0.85f);
        dl->AddText(ImVec2(labelX, labelY), ImGui::ColorConvertFloat4ToU32(labelCol), items[i].label);

        float descY = labelY + ImGui::GetTextLineHeight() + 2.0f;
        ImVec4 descCol = selected ? ThemeCol(theme.textDim) : ThemeCol(theme.textFaint);
        dl->AddText(ImVec2(labelX, descY), ImGui::ColorConvertFloat4ToU32(descCol), items[i].desc);

        ImGui::PopStyleColor(3);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }
}

void HelpPanel::RenderContent() {
    switch (m_SelectedCategory) {
        case 0: RenderCategoryCommunity();  break;
        case 1: RenderCategoryCredits();    break;
        case 2: RenderCategoryPatchNotes();  break;
        case 3: RenderCategoryShortcuts();   break;
        case 4: RenderCategoryDonations();   break;
        default: RenderCategoryCommunity(); break;
    }
}

void HelpPanel::RenderQrPreview(const char* url, const char* label) {
    if (!url || !url[0]) return;

    auto qr = qrcodegen::QrCode::encodeText(url, qrcodegen::QrCode::Ecc::MEDIUM);
    int qrSize = qr.getSize();
    float cellSize = 4.0f;
    float margin = cellSize * 2.0f;
    float totalSize = (qrSize * cellSize) + (margin * 2.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Fondo blanco nítido para escaneo perfecto con la cámara del móvil
    dl->AddRectFilled(pos, ImVec2(pos.x + totalSize, pos.y + totalSize), IM_COL32(255, 255, 255, 255), 6.0f);
    dl->AddRect(pos, ImVec2(pos.x + totalSize, pos.y + totalSize), IM_COL32(200, 200, 200, 255), 6.0f, 0, 1.0f);

    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (qr.getModule(x, y)) {
                ImVec2 minP(pos.x + margin + (x * cellSize), pos.y + margin + (y * cellSize));
                ImVec2 maxP(pos.x + margin + ((x + 1) * cellSize), pos.y + margin + ((y + 1) * cellSize));
                dl->AddRectFilled(minP, maxP, IM_COL32(15, 23, 42, 255));
            }
        }
    }

    ImGui::Dummy(ImVec2(totalSize, totalSize));
    ImGui::TextDisabled("%s", label ? label : Tr("Escanea con la cámara de tu celular", "Scan with your phone's camera", "Digitalize com a câmara do telemóvel"));
}

void HelpPanel::RenderCategoryCommunity() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(Tr("Comunidad y Canales Oficiales", "Official Community & Channels", "Comunidade e Canais Oficiais"));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "Conéctate con otros directores de medios, iglesias y el desarrollador. Obtén soporte técnico, comparte fondos y plantillas, o sigue los nuevos lanzamientos.",
        "Connect with other media directors, churches, and the developer. Get technical support, share backgrounds and templates, or follow new releases.",
        "Ligue-se a outros diretores de multimédia, igrejas e ao desenvolvedor. Obtenha suporte técnico, partilhe recursos ou acompanhe as atualizações."
    ));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    auto RenderSocialCard = [&](int cardIndex, const char* title, const char* desc, const char* url,
                                ImU32 accentCol, const char* btnText) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardW = ImGui::GetContentRegionAvail().x;
        float cardH = (m_ActiveQrCard == cardIndex) ? 280.0f : 110.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                          ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.55f)), 10.0f);
        dl->AddRect(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                    ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.45f)), 10.0f, 0, 1.0f);
        // Barra lateral de color de la marca
        dl->AddRectFilled(ImVec2(p0.x, p0.y + 6.0f), ImVec2(p0.x + 5.0f, p0.y + cardH - 6.0f), accentCol, 2.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 16.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, accentCol);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 38.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
        ImGui::TextWrapped("%s", desc);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 68.0f));
        if (ProyecThor::UI::DS::GlassButton(btnText, ImVec2(160.0f, 30.0f))) {
            ProyecThor::External::OpenURL(url);
        }
        ImGui::SameLine(0, 10.0f);

        std::string copyLabel = Tr("Copiar enlace##", "Copy link##", "Copiar ligação##") + std::string(title);
        if (ProyecThor::UI::DS::GlassButton(copyLabel.c_str(), ImVec2(120.0f, 30.0f))) {
            CopyToClipboard(url, title);
        }
        ImGui::SameLine(0, 10.0f);

        std::string qrLabel = (m_ActiveQrCard == cardIndex)
            ? (Tr("Ocultar QR##", "Hide QR##", "Ocultar QR##") + std::string(title))
            : (Tr("Escanear QR##", "Scan QR##", "Digitalizar QR##") + std::string(title));
        if (ProyecThor::UI::DS::GlassButton(qrLabel.c_str(), ImVec2(125.0f, 30.0f))) {
            m_ActiveQrCard = (m_ActiveQrCard == cardIndex) ? -1 : cardIndex;
        }

        if (m_ActiveQrCard == cardIndex) {
            ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 110.0f));
            RenderQrPreview(url, title);
        }

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 14.0f));
    };

    // 1. Discord Oficial
    RenderSocialCard(
        0,
        "Discord Oficial - ProyecThor Community",
        Tr("Únete a nuestro servidor de Discord para chatear con la comunidad, solicitar nuevas funciones, compartir plantillas y obtener soporte directo.",
           "Join our Discord server to chat with the community, request new features, share templates, and get direct support.",
           "Junte-se ao nosso servidor Discord para conversar com a comunidade, pedir recursos e obter suporte."),
        "https://discord.gg/QJ6wGHQjs6",
        IM_COL32(88, 101, 242, 255), // Blurple Discord
        Tr("Unirse a Discord", "Join Discord", "Entrar no Discord")
    );

    // 2. Instagram
    RenderSocialCard(
        1,
        "Instagram Oficial (@theproyecthor)",
        Tr("Síguenos en Instagram para ver anticipos de versiones futuras, fotos de iglesias usando ProyecThor y anuncios de la comunidad.",
           "Follow us on Instagram to see sneak peeks of future versions, photos from churches using ProyecThor, and community announcements.",
           "Siga-nos no Instagram para ver novidades de versões futuras, fotos de igrejas e avisos."),
        "https://www.instagram.com/theproyecthor/",
        IM_COL32(225, 48, 108, 255), // Rosa Instagram
        Tr("Ver Instagram", "View Instagram", "Ver Instagram")
    );

    // 3. Canal de WhatsApp
    RenderSocialCard(
        2,
        Tr("Canal de WhatsApp Oficial", "Official WhatsApp Channel", "Canal de WhatsApp Oficial"),
        Tr("Avisos directos en tu teléfono sobre lanzamientos de nuevas versiones, parches de seguridad y estado de los servicios.",
           "Direct notifications on your phone about new version releases, security patches, and service status.",
           "Avisos diretos no seu telemóvel sobre novos lançamentos, atualizações e estado dos serviços."),
        "https://whatsapp.com/channel/0029Vb7e9tj3WHTdNivIxR19",
        IM_COL32(37, 211, 102, 255), // Verde WhatsApp
        Tr("Abrir Canal", "Open Channel", "Abrir Canal")
    );

    // 4. Sitio Web Oficial
    RenderSocialCard(
        3,
        Tr("Sitio Web Oficial de ProyecThor", "Official ProyecThor Website", "Site Oficial do ProyecThor"),
        Tr("Portal central del proyecto con manuales, instaladores para Windows y Linux, y documentación completa.",
           "Central project portal with user manuals, installers for Windows and Linux, and comprehensive documentation.",
           "Portal central do projeto com manuais, instaladores para Windows e Linux e documentação."),
        "https://github.com/TheVixcho/ProyecThor",
        IM_COL32(56, 189, 248, 255), // Azul cielo
        Tr("Visitar Web", "Visit Website", "Visitar Site")
    );

    // 5. Reporte de Errores en GitHub
    RenderSocialCard(
        4,
        Tr("Reporte de Errores y Sugerencias (GitHub)", "Bug Reports & Feature Requests (GitHub)", "Reportar Erros e Sugestões (GitHub)"),
        Tr("¿Encontraste un fallo o tienes una propuesta técnica? Abre un issue en el repositorio oficial de GitHub de ProyecThor.",
           "Did you find a bug or have a technical suggestion? Open an issue on ProyecThor's official GitHub repository.",
           "Encontrou um erro ou tem uma sugestão técnica? Crie um issue no repositório oficial do GitHub."),
        "https://github.com/TheVixcho/ProyecThor/issues",
        IM_COL32(168, 85, 247, 255), // Púrpura GitHub
        Tr("Abrir GitHub Issues", "Open GitHub Issues", "Abrir GitHub Issues")
    );
}

void HelpPanel::RenderCategoryCredits() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(Tr("Acerca de ProyecThor y Créditos", "About ProyecThor & Credits", "Sobre o ProyecThor e Créditos"));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "ProyecThor es software moderno y potente diseñado para la alabanza, adoración y proyección multimedia en iglesias y eventos.",
        "ProyecThor is modern, powerful software designed for praise, worship, and multimedia projection in churches and events.",
        "O ProyecThor é um software moderno e potente concebido para louvor, adoração e projeção multimédia em igrejas e eventos."
    ));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // 1. Tarjeta: Creador del Proyecto (TheVixcho)
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardW = ImGui::GetContentRegionAvail().x;
        float cardH = 120.0f;

        dl->AddRectFilled(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                          ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.65f)), 10.0f);
        dl->AddRect(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                    IM_COL32(245, 180, 50, 160), 10.0f, 0, 1.2f);
        dl->AddRectFilled(ImVec2(p0.x, p0.y + 6.0f), ImVec2(p0.x + 5.0f, p0.y + cardH - 6.0f),
                          IM_COL32(245, 180, 50, 255), 2.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(245, 180, 50, 255));
        ImGui::TextUnformatted(Tr("Creador y Desarrollador", "Creator & Lead Developer", "Criador e Desenvolvedor"));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 34.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::SetWindowFontScale(1.20f);
        ImGui::TextUnformatted("TheVixcho");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 60.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
        ImGui::TextWrapped("%s", Tr(
            "Ideado, creado y desarrollado por TheVixcho para entregar una herramienta de calidad profesional a las iglesias.",
            "Conceived, created, and developed by TheVixcho to provide professional-grade tools for churches.",
            "Idealizado, criado e desenvolvido por TheVixcho para oferecer uma ferramenta profissional para as igrejas."
        ));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 84.0f));
        if (ProyecThor::UI::DS::GlassButton("GitHub: @TheVixcho", ImVec2(150.0f, 26.0f))) {
            ProyecThor::External::OpenURL("https://github.com/TheVixcho");
        }

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 16.0f));
    }

    // 2. Tarjeta: Reconocimiento y Fuentes Bíblicas (Beblia / Nameless, Proud Slave of Christ)
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardW = ImGui::GetContentRegionAvail().x;
        float cardH = 155.0f;

        dl->AddRectFilled(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                          ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.65f)), 10.0f);
        dl->AddRect(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                    IM_COL32(56, 189, 248, 160), 10.0f, 0, 1.2f);
        dl->AddRectFilled(ImVec2(p0.x, p0.y + 6.0f), ImVec2(p0.x + 5.0f, p0.y + cardH - 6.0f),
                          IM_COL32(56, 189, 248, 255), 2.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(56, 189, 248, 255));
        ImGui::TextUnformatted(Tr("Agradecimiento Especial: Fuentes Bíblicas (XML)", "Special Acknowledgment: Bible Sources (XML)", "Agradecimento Especial: Fontes Bíblicas (XML)"));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::TextWrapped("%s", Tr(
            "Las biblias fueron obtenidas en este repositorio:\nhttps://github.com/Beblia/Holy-Bible-XML-Format\ngracias a Nameless, Proud Slave of Christ.",
            "The bibles were obtained from this repository:\nhttps://github.com/Beblia/Holy-Bible-XML-Format\nthanks to Nameless, Proud Slave of Christ.",
            "As bíblias foram obtidas neste repositório:\nhttps://github.com/Beblia/Holy-Bible-XML-Format\ngraças a Nameless, Proud Slave of Christ."
        ));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 104.0f));
        if (ProyecThor::UI::DS::GlassButton(Tr("Abrir Repositorio Beblia", "Open Beblia Repository", "Abrir Repositório Beblia"), ImVec2(200.0f, 30.0f))) {
            ProyecThor::External::OpenURL("https://github.com/Beblia/Holy-Bible-XML-Format");
        }
        ImGui::SameLine(0, 10.0f);
        if (ProyecThor::UI::DS::GlassButton(Tr("Copiar enlace del repositorio", "Copy repository link", "Copiar ligação do repositório"), ImVec2(220.0f, 30.0f))) {
            CopyToClipboard("https://github.com/Beblia/Holy-Bible-XML-Format", "Repositorio Beblia");
        }

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 16.0f));
    }

    // 3. Tarjeta: Colaboradores y Comunidad
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardW = ImGui::GetContentRegionAvail().x;
        float cardH = 95.0f;

        dl->AddRectFilled(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                          ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.55f)), 10.0f);
        dl->AddRect(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                    ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.45f)), 10.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(167, 139, 250, 255));
        ImGui::TextUnformatted(Tr("Colaboradores y Agradecimientos", "Contributors & Acknowledgments", "Colaboradores e Agradecimentos"));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::TextUnformatted("Victor Farias, Fabiola Fernandez y toda la comunidad de ProyecThor.");
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 58.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
        ImGui::TextWrapped("%s", Tr(
            "Agradecimiento especial a todas las iglesias y líderes técnicos que prueban y retroalimentan la plataforma día a día.",
            "Special thanks to all churches and technical directors testing and providing feedback every day.",
            "Agradecimento especial a todas as igrejas e técnicos que testam e apoiam a plataforma diariamente."
        ));
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH + 16.0f));
    }
}

void HelpPanel::RenderCategoryPatchNotes() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(Tr("Historial de Versiones y Novedades", "Version History & Release Notes", "Histórico de Versões e Novidades"));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "Consulta todas las mejoras, nuevas características y correcciones integradas en cada versión de ProyecThor.",
        "Browse all improvements, new features, and fixes included in every ProyecThor release.",
        "Consulte todas as melhorias, novos recursos e correções incluídas em cada versão do ProyecThor."
    ));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    PatchNotesStyleConfig cfg;
    cfg.accent        = ThemeCol(theme.accent);
    cfg.accentLight   = ThemeCol(theme.accentLight);
    cfg.textPrimary   = ThemeCol(theme.textPrimary);
    cfg.textSecondary = ThemeCol(theme.textDim);
    cfg.cardBg        = ThemeCol(theme.surface1, 0.65f);
    cfg.cardBorder    = ThemeCol(theme.border, 0.45f);
    cfg.windowBg      = ThemeCol(theme.base);
    cfg.border        = ThemeCol(theme.border);
    cfg.scale         = 1.0f;

    static PatchNotesViewState s_HelpNotesState;
    RenderPatchNotesViewContent(s_HelpNotesState, cfg, 0.0f);
}

void HelpPanel::RenderCategoryShortcuts() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(Tr("Atajos de Teclado y Control Rápido", "Keyboard Shortcuts & Quick Control", "Teclas de Atalho e Controlo Rápido"));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "Acelera tu flujo de trabajo en vivo durante cultos y presentaciones con estos atajos de teclado.",
        "Speed up your live workflow during services and presentations with these shortcuts.",
        "Acelere o seu fluxo de trabalho ao vivo durante os cultos com estes atalhos."
    ));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    struct ShortcutItem {
        const char* key;
        const char* desc;
    };

    auto RenderShortcutSection = [&](const char* title, const std::vector<ShortcutItem>& list, ImU32 col) {
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        for (const auto& item : list) {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = 34.0f;

            dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.40f)), 6.0f);

            // Tecla en píldora destacada
            ImVec2 kSz = ImGui::CalcTextSize(item.key);
            float pillW = std::max(60.0f, kSz.x + 18.0f);
            ImVec2 kMin(p0.x + 10.0f, p0.y + 6.0f);
            ImVec2 kMax(kMin.x + pillW, p0.y + h - 6.0f);
            dl->AddRectFilled(kMin, kMax, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.90f)), 5.0f);
            dl->AddRect(kMin, kMax, col, 5.0f, 0, 1.0f);
            dl->AddText(ImVec2(kMin.x + (pillW - kSz.x) * 0.5f, kMin.y + ((h - 12.0f) - kSz.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), item.key);

            // Descripción
            dl->AddText(ImVec2(kMax.x + 16.0f, p0.y + (h - ImGui::GetTextLineHeight()) * 0.5f),
                        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), item.desc);

            ImGui::Dummy(ImVec2(0.0f, h + 4.0f));
        }
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
    };

    RenderShortcutSection(
        Tr("Proyección y Navegación en Vivo", "Live Projection & Navigation", "Projeção e Navegação ao Vivo"),
        {
            { "Espacio / Flecha Derecha", Tr("Avanzar a la siguiente diapositiva o versículo", "Next slide or verse", "Avançar para o próximo diapositivo ou versículo") },
            { "Flecha Izquierda", Tr("Retroceder a la diapositiva o versículo anterior", "Previous slide or verse", "Voltar ao diapositivo ou versículo anterior") },
            { "F5", Tr("Iniciar o alternar proyección de audiencia en vivo", "Toggle audience live projection", "Alternar projeção de público ao vivo") },
            { "F11", Tr("Alternar pantalla completa de la ventana principal", "Toggle fullscreen on main window", "Alternar ecrã inteiro na janela principal") },
            { "Esc", Tr("Cerrar ventana emergente o cancelar salida", "Close popup or cancel output", "Fechar janela ou cancelar saída") },
            { "Retroceso / Del", Tr("Ocultar o limpiar texto proyectado en vivo", "Clear or blank live projected text", "Limpar texto projetado ao vivo") }
        },
        IM_COL32(45, 212, 191, 255)
    );

    RenderShortcutSection(
        Tr("Herramientas y Ventanas", "Tools & Windows", "Ferramentas e Janelas"),
        {
            { "F1", Tr("Abrir o cerrar esta ventana de Ayuda y Comunidad", "Open or close this Help & Community window", "Abrir ou fechar esta janela de Ajuda") },
            { "Ctrl + P", Tr("Abrir panel de Configuración y Preferencias", "Open Settings and Preferences panel", "Abrir painel de Definições") },
            { "Ctrl + F", Tr("Enfocar búsqueda rápida en biblioteca o biblia", "Focus quick search in library or bible", "Focar pesquisa rápida na biblioteca ou bíblia") },
            { "Ctrl + Alt + N", Tr("Ver historial completo de notas de versión", "View full release notes changelog", "Ver notas de versão completas") }
        },
        IM_COL32(167, 139, 250, 255)
    );
}

void HelpPanel::RenderCategoryDonations() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;
    ImDrawList* dl    = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(Tr("Apoya a ProyecThor en Ko-fi", "Support ProyecThor on Ko-fi", "Apoie o ProyecThor no Ko-fi"));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "ProyecThor está enfocado en bendecir y servir a las congregaciones e iglesias en todo el mundo.",
        "ProyecThor is focused on blessing and serving congregations and churches worldwide.",
        "O ProyecThor dedica-se a abençoar e servir as congregações e igrejas em todo o mundo."
    ));
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float cardW = ImGui::GetContentRegionAvail().x;
    float cardH = (m_ActiveQrCard == 5) ? 380.0f : 160.0f;

    dl->AddRectFilled(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                      ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.65f)), 10.0f);
    dl->AddRect(p0, ImVec2(p0.x + cardW, p0.y + cardH),
                IM_COL32(244, 114, 182, 160), 10.0f, 0, 1.2f);
    dl->AddRectFilled(ImVec2(p0.x, p0.y + 6.0f), ImVec2(p0.x + 5.0f, p0.y + cardH - 6.0f),
                      IM_COL32(244, 114, 182, 255), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 16.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(244, 114, 182, 255));
    ImGui::TextUnformatted(Tr("Donación Voluntaria para el Proyecto", "Voluntary Donation for the Project", "Doação Voluntária para o Projeto"));
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 40.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextWrapped("%s", Tr(
        "Si ProyecThor bendice tu ministerio o congregación y deseas aportar voluntariamente para ayudar a costear dominios, certificados y desarrollo activo, puedes invitarnos a un café en Ko-fi.",
        "If ProyecThor blesses your ministry or congregation and you'd like to voluntarily help fund domains, certificates, and development, you can buy us a coffee on Ko-fi.",
        "Se o ProyecThor abençoa o seu ministério e deseja contribuir para ajudar a manter o projeto, pode pagar-nos um café no Ko-fi."
    ));
    ImGui::PopStyleColor();

    const char* kofiUrl = "https://ko-fi.com/vixcho";

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 104.0f));
    if (ProyecThor::UI::DS::GlassButton(Tr("Donar en Ko-fi (ko-fi.com/vixcho)", "Donate on Ko-fi (ko-fi.com/vixcho)", "Doar no Ko-fi (ko-fi.com/vixcho)"), ImVec2(240.0f, 32.0f))) {
        ProyecThor::External::OpenURL(kofiUrl);
    }
    ImGui::SameLine(0, 10.0f);
    if (ProyecThor::UI::DS::GlassButton(Tr("Copiar enlace##kofi", "Copy link##kofi", "Copiar ligação##kofi"), ImVec2(120.0f, 32.0f))) {
        CopyToClipboard(kofiUrl, "Ko-fi");
    }
    ImGui::SameLine(0, 10.0f);
    std::string qrKofiLabel = (m_ActiveQrCard == 5)
        ? Tr("Ocultar QR##kofi", "Hide QR##kofi", "Ocultar QR##kofi")
        : Tr("Escanear QR con Celular##kofi", "Scan QR with Phone##kofi", "Digitalizar QR com Telemóvel##kofi");
    if (ProyecThor::UI::DS::GlassButton(qrKofiLabel.c_str(), ImVec2(210.0f, 32.0f))) {
        m_ActiveQrCard = (m_ActiveQrCard == 5) ? -1 : 5;
    }

    if (m_ActiveQrCard == 5) {
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 20.0f, p0.y + 155.0f));
        RenderQrPreview(kofiUrl, "https://ko-fi.com/vixcho");
    }
}

} // namespace ProyecThor::UI
