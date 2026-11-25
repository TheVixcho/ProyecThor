#include "DebugPanel.h"
#include "SettingsManager.h"
#include "core/AppLog.h"
#include "core/AppPaths.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <string>
#include <vector>
#include <ctime>

#if defined(_WIN32)
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace ProyecThor::UI {

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

DebugPanel::DebugPanel()
    : m_SelectedCategory(0)
    , m_WasOpenLastFrame(false)
{
}

void DebugPanel::Render(bool* isOpen) {
    if (!isOpen || !*isOpen) {
        m_WasOpenLastFrame = false;
        return;
    }

    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float dt = ImGui::GetIO().DeltaTime;
    if (m_FeedbackTimer > 0.0f) {
        m_FeedbackTimer -= dt;
        if (m_FeedbackTimer <= 0.0f) {
            m_FeedbackMsg.clear();
        }
    }

    m_WasOpenLastFrame = true;

    // Pantalla completa sobre el viewport principal
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

    bool open = ImGui::Begin("##DebugPanelFullscreen", isOpen,
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

    // Tecla Escape cierra el panel
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        *isOpen = false;
    }

    // ── Ventana modal flotante centrada ──────────────────────────────────────
    const ImVec2 vpSize  = vp->Size;
    const float  panelW  = std::min(vpSize.x - 48.0f, 1160.0f);
    const float  panelH  = std::min(vpSize.y - 48.0f, 760.0f);
    const ImVec2 center  = ImVec2(vp->Pos.x + vpSize.x * 0.5f, vp->Pos.y + vpSize.y * 0.5f);
    const ImVec2 panelPos = ImVec2(center.x - panelW * 0.5f, center.y - panelH * 0.5f);

    ImDrawList* bgDl = ImGui::GetWindowDrawList();
    DrawFloatingIslandShadow(bgDl, panelPos, ImVec2(panelW, panelH), 14.0f);

    ImGui::SetCursorScreenPos(panelPos);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.9f, 0.25f, 0.25f, 0.45f)); // Borde carmesí sutil de modo admin
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);

    if (ImGui::BeginChild("##DebugFloatingCard", ImVec2(panelW, panelH), true, ImGuiWindowFlags_NoScrollbar)) {
        // ── Barra Superior (Header) ──────────────────────────────────────────
        const float headerH = 58.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 hMin = ImGui::GetCursorScreenPos();
        ImVec2 hMax = ImVec2(hMin.x + panelW, hMin.y + headerH);

        dl->AddRectFilled(hMin, hMax, ImGui::GetColorU32(ThemeCol(theme.surface1, 0.75f)), 14.0f, ImDrawFlags_RoundCornersTop);
        dl->AddLine(ImVec2(hMin.x, hMax.y), hMax, IM_COL32(239, 68, 68, 120), 1.5f);

        // Icono y Título
        float iconSz = 22.0f;
        ImVec2 iconPos = ImVec2(hMin.x + 20.0f, hMin.y + (headerH - iconSz) * 0.5f);
        AppIcons::DrawIcon_Debug(dl, iconPos, iconSz, IM_COL32(239, 68, 68, 255));

        ImGui::SetCursorScreenPos(ImVec2(iconPos.x + iconSz + 14.0f, hMin.y + 11.0f));
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont());
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.98f), "ProyecThor Debug Console");
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(iconPos.x + iconSz + 14.0f, hMin.y + 33.0f));
        ImGui::TextColored(ImVec4(0.94f, 0.35f, 0.35f, 0.95f), "Panel de Depuración Avanzado • Exclusivo para Administradores");

        // Badge Admin a la derecha
        float badgeW = 120.0f;
        float badgeH = 26.0f;
        ImVec2 badgePos = ImVec2(hMax.x - 70.0f - badgeW, hMin.y + (headerH - badgeH) * 0.5f);
        dl->AddRectFilled(badgePos, ImVec2(badgePos.x + badgeW, badgePos.y + badgeH), IM_COL32(239, 68, 68, 40), 6.0f);
        dl->AddRect(badgePos, ImVec2(badgePos.x + badgeW, badgePos.y + badgeH), IM_COL32(239, 68, 68, 180), 6.0f, 0, 1.0f);
        dl->AddText(ImVec2(badgePos.x + 12.0f, badgePos.y + 5.0f), IM_COL32(252, 165, 165, 255), "ADMIN ACCESS");

        // Botón Cerrar (X)
        float closeBtnSz = 28.0f;
        ImVec2 closePos = ImVec2(hMax.x - 18.0f - closeBtnSz, hMin.y + (headerH - closeBtnSz) * 0.5f);
        ImGui::SetCursorScreenPos(closePos);
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::Button("X##CloseDebug", ImVec2(closeBtnSz, closeBtnSz))) {
            *isOpen = false;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(hMin.x, hMin.y + headerH));

        // ── Layout de 2 Columnas (Sidebar + Contenido) ───────────────────────
        const float sidebarW = 240.0f;
        const float contentH = panelH - headerH;

        ImGui::BeginGroup();
        RenderSidebar();
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, 0.0f);

        ImGui::BeginGroup();
        RenderContent();
        ImGui::EndGroup();
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void DebugPanel::RenderSidebar() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float sidebarW = 240.0f;
    const float sidebarH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.55f));
    ImGui::BeginChild("##DebugSidebar", ImVec2(sidebarW, sidebarH), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::Spacing();
    ImGui::Spacing();

    struct NavItem {
        int         id;
        const char* label;
        const char* sublabel;
        ImU32       accentCol;
    };

    static const NavItem items[] = {
        { 0, "Logs del Sistema",       "Consola de eventos en vivo", IM_COL32(52, 211, 153, 255)  },
        { 1, "Diagnóstico y Rutas",    "Almacenamiento y sistema",   IM_COL32(251, 191, 36, 255)  }
    };

    for (const auto& item : items) {
        bool selected = (m_SelectedCategory == item.id);
        ImGui::PushID(item.id);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float itemW = sidebarW - 16.0f;
        float itemH = 50.0f;
        ImVec2 p1 = ImVec2(p0.x + itemW, p0.y + itemH);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 8.0f, p0.y));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool hovered = ImGui::IsMouseHoveringRect(ImVec2(p0.x + 8.0f, p0.y), ImVec2(p0.x + 8.0f + itemW, p0.y + itemH));

        if (selected) {
            dl->AddRectFilled(ImVec2(p0.x + 8.0f, p0.y), ImVec2(p0.x + 8.0f + itemW, p0.y + itemH),
                              ImGui::GetColorU32(ThemeCol(theme.surface2, 0.85f)), 8.0f);
            dl->AddRectFilled(ImVec2(p0.x + 8.0f, p0.y + 8.0f), ImVec2(p0.x + 12.0f, p0.y + itemH - 8.0f),
                              item.accentCol, 2.0f);
        } else if (hovered) {
            dl->AddRectFilled(ImVec2(p0.x + 8.0f, p0.y), ImVec2(p0.x + 8.0f + itemW, p0.y + itemH),
                              ImGui::GetColorU32(ThemeCol(theme.surface1, 0.50f)), 8.0f);
        }

        ImGui::InvisibleButton("##NavBtn", ImVec2(itemW, itemH));
        if (ImGui::IsItemClicked()) {
            m_SelectedCategory = item.id;
        }

        // Texto del botón
        dl->AddText(ImVec2(p0.x + 22.0f, p0.y + 8.0f),
                    selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 220, 230, 240),
                    item.label);
        dl->AddText(ImVec2(p0.x + 22.0f, p0.y + 28.0f),
                    selected ? item.accentCol : IM_COL32(140, 150, 165, 220),
                    item.sublabel);

        ImGui::Spacing();
        ImGui::PopID();
    }

    // Pie del sidebar
    ImGui::SetCursorPosY(sidebarH - 45.0f);
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.72f, 0.75f));
    ImGui::TextWrapped("ProyecThor Open Source");
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DebugPanel::RenderContent() {
    auto&       mgr   = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const float contentW = ImGui::GetContentRegionAvail().x;
    const float contentH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 1.0f));
    ImGui::BeginChild("##DebugContentArea", ImVec2(contentW, contentH), false);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::SetCursorPos(ImVec2(24.0f, 20.0f));

    switch (m_SelectedCategory) {
        case 0: RenderCategoryLogs();        break;
        case 1: RenderCategoryDiagnostics(); break;
        default: RenderCategoryLogs();       break;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void DebugPanel::RenderCategoryLogs() {
    auto& logMgr = ProyecThor::Core::AppLog::Get();
    std::vector<ProyecThor::Core::LogEntry> entries = logMgr.GetEntries();

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont());
    ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.6f, 1.0f), "Visor de Logs de la Aplicación");
    ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.8f, 0.85f),
        "Monitoreo en tiempo real de eventos, advertencias, descargas y errores del sistema.");

    ImGui::Spacing();

    // ── Barra de Herramientas de Filtro ──────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    // Filtro por texto
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##LogSearch", "Buscar texto en logs...", m_LogSearchBuffer, sizeof(m_LogSearchBuffer));

    ImGui::SameLine();

    // Chips de Nivel
    auto levelBtn = [this](int lvl, const char* label, ImVec4 col) {
        bool active = (m_LogLevelFilter == lvl);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, col);
        else        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.2f));

        if (ImGui::Button(label)) {
            m_LogLevelFilter = (m_LogLevelFilter == lvl) ? -1 : lvl;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    levelBtn(-1, "TODOS",  ImVec4(0.4f, 0.45f, 0.55f, 0.9f));
    levelBtn(0,  "DEBUG",  ImVec4(0.2f, 0.75f, 0.95f, 0.9f));
    levelBtn(1,  "INFO",   ImVec4(0.2f, 0.85f, 0.55f, 0.9f));
    levelBtn(2,  "WARN",   ImVec4(0.95f, 0.75f, 0.20f, 0.9f));
    levelBtn(3,  "ERROR",  ImVec4(0.95f, 0.35f, 0.35f, 0.9f));

    ImGui::Checkbox("Auto-scroll", &m_LogAutoScroll);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Acciones rápidas
    if (ImGui::Button("Copiar Todo")) {
        std::string allLogs;
        for (const auto& e : entries) {
            allLogs += "[" + e.timestamp + "] [" + e.tag + "] " + e.message + "\n";
        }
        ImGui::SetClipboardText(allLogs.c_str());
        m_FeedbackMsg = "Logs copiados al portapapeles.";
        m_FeedbackTimer = 3.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button("Limpiar Consola")) {
        logMgr.Clear();
    }

    ImGui::SameLine();
    if (ImGui::Button("Abrir app.log")) {
        std::string logPath = logMgr.GetLogFilePath();
#if defined(_WIN32)
        ShellExecuteA(NULL, "open", logPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
    }

    ImGui::SameLine();
    if (ImGui::Button("Log de Prueba")) {
        ProyecThor::Core::AppLog::Inf("Test", "Prueba de evento informativo desde Debug.");
        ProyecThor::Core::AppLog::Warn("Test", "Advertencia de prueba.");
        ProyecThor::Core::AppLog::Err("Test", "Error de prueba para verificación de consola.");
    }

    ImGui::PopStyleVar();

    if (!m_FeedbackMsg.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%s", m_FeedbackMsg.c_str());
    }

    ImGui::Separator();

    // ── Ventana de Logs con Scroll ───────────────────────────────────────────
    const float logBoxH = ImGui::GetContentRegionAvail().y - 20.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.07f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    if (ImGui::BeginChild("##LogConsoleBox", ImVec2(0, logBoxH), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        std::string filterLower = m_LogSearchBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        for (const auto& entry : entries) {
            // Filtro por nivel
            if (m_LogLevelFilter != -1) {
                int eLvl = static_cast<int>(entry.level);
                if (eLvl != m_LogLevelFilter) continue;
            }

            // Filtro por texto
            if (!filterLower.empty()) {
                std::string msgLower = entry.message;
                std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
                std::string tagLower = entry.tag;
                std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);

                if (msgLower.find(filterLower) == std::string::npos &&
                    tagLower.find(filterLower) == std::string::npos) {
                    continue;
                }
            }

            // Timestamp en gris tenue
            ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.62f, 0.8f), "[%s]", entry.timestamp.c_str());
            ImGui::SameLine();

            // Etiqueta de nivel coloreada
            switch (entry.level) {
                case ProyecThor::Core::LogLevel::Debug:
                    ImGui::TextColored(ImVec4(0.3f, 0.75f, 0.95f, 1.0f), "[DEBUG]");
                    break;
                case ProyecThor::Core::LogLevel::Info:
                    ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.55f, 1.0f), "[INFO ]");
                    break;
                case ProyecThor::Core::LogLevel::Warning:
                    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.0f), "[WARN ]");
                    break;
                case ProyecThor::Core::LogLevel::Error:
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "[ERROR]");
                    break;
            }
            ImGui::SameLine();

            // Tag
            ImGui::TextColored(ImVec4(0.75f, 0.65f, 0.95f, 1.0f), "[%s]", entry.tag.c_str());
            ImGui::SameLine();

            // Mensaje
            ImGui::TextUnformatted(entry.message.c_str());
        }

        if (m_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 30.0f) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void DebugPanel::RenderCategoryDiagnostics() {
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont());
    ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.25f, 1.0f), "Diagnóstico del Sistema y Almacenamiento");
    ImGui::PopFont();
    ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.8f, 0.85f),
        "Información técnica y rutas de almacenamiento local de ProyecThor Open Source.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Tarjeta: Rutas de Carpetas y Almacenamiento ──────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

    if (ImGui::BeginChild("##StoragePathsCard", ImVec2(0, 160.0f), true)) {
        ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1.0f), "Rutas del Sistema de Archivos");

        std::string appData = ProyecThor::GetAppDataRoot();
        std::string assets  = ProyecThor::GetAssetsPath();

        ImGui::Text("AppData Root: %s", appData.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Abrir AppData##Btn")) {
#if defined(_WIN32)
            ShellExecuteA(NULL, "open", appData.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
        }

        ImGui::Text("Assets: %s", assets.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Abrir Assets##Btn")) {
#if defined(_WIN32)
            ShellExecuteA(NULL, "open", assets.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
        }

#if defined(_WIN32)
        char tempDir[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, tempDir);
        ImGui::Text("Temp Folder: %s", tempDir);
        ImGui::SameLine();
        if (ImGui::Button("Abrir Temp##Btn")) {
            ShellExecuteA(NULL, "open", tempDir, NULL, NULL, SW_SHOWNORMAL);
        }
#endif
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI
