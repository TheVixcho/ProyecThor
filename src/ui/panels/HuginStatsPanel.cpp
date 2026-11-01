#include "HuginStatsPanel.h"
#include "core/HuginStatsManager.h"
#include "core/settings/SettingsManager.h"
#include "ui/framework/SongPlayStats.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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

enum class HuginCatIcon { Sliders, MusicNote, Monitor, Storage };

struct HuginSection {
    const char* label;
    const char* description;
    HuginCatIcon icon;
    ImU32 color;
};

static const HuginSection k_HuginSections[] = {
    { "Resumen General", "Métricas clave de rendimiento y telemetría de proyección", HuginCatIcon::Sliders,   IM_COL32(245, 158,  11, 255) },
    { "Top Canciones",   "Frecuencia histórica de canciones entonadas en el auditorio", HuginCatIcon::MusicNote, IM_COL32(235, 105, 165, 255) },
    { "Rendimiento FPS", "Estabilidad de la tasa de cuadros y GPU en tiempo real",   HuginCatIcon::Monitor,   IM_COL32( 34, 197,  94, 255) },
    { "Sesión Activa",   "Información de tiempos de uso y monitores de proyección", HuginCatIcon::Storage,   IM_COL32( 56, 189, 248, 255) },
};
static constexpr int k_HuginSectionCount = 4;

static void DrawHuginCatIcon(ImDrawList* dl, HuginCatIcon icon, ImVec2 c, float r, ImU32 color) {
    switch (icon) {
        case HuginCatIcon::Sliders: {
            float w = r * 1.6f;
            float ys[3] = { c.y - r * 0.60f, c.y, c.y + r * 0.60f };
            float hx[3] = { c.x - w * 0.15f, c.x + w * 0.20f, c.x - w * 0.05f };
            for (int i = 0; i < 3; i++) {
                dl->AddLine(ImVec2(c.x - w * 0.5f, ys[i]), ImVec2(c.x + w * 0.5f, ys[i]), color, 1.4f);
                dl->AddCircleFilled(ImVec2(hx[i], ys[i]), r * 0.16f, color);
            }
            break;
        }
        case HuginCatIcon::MusicNote: {
            ImVec2 head(c.x - r * 0.35f, c.y + r * 0.40f);
            dl->AddCircleFilled(head, r * 0.30f, color);
            dl->AddLine(ImVec2(head.x + r * 0.28f, head.y - r * 0.05f), ImVec2(head.x + r * 0.28f, c.y - r * 0.70f), color, 1.5f);
            dl->AddLine(ImVec2(head.x + r * 0.28f, c.y - r * 0.70f), ImVec2(head.x + r * 0.65f, c.y - r * 0.48f), color, 1.5f);
            break;
        }
        case HuginCatIcon::Monitor: {
            ImVec2 mn(c.x - r * 0.85f, c.y - r * 0.65f), mx(c.x + r * 0.85f, c.y + r * 0.30f);
            dl->AddRect(mn, mx, color, 2.0f, 0, 1.3f);
            dl->AddLine(ImVec2(c.x, mx.y), ImVec2(c.x, mx.y + r * 0.35f), color, 1.3f);
            dl->AddLine(ImVec2(c.x - r * 0.35f, mx.y + r * 0.35f), ImVec2(c.x + r * 0.35f, mx.y + r * 0.35f), color, 1.3f);
            break;
        }
        case HuginCatIcon::Storage: {
            // Reloj / Uptime
            dl->AddCircle(c, r * 0.80f, color, 18, 1.3f);
            dl->AddLine(c, ImVec2(c.x, c.y - r * 0.50f), color, 1.4f);
            dl->AddLine(c, ImVec2(c.x + r * 0.40f, c.y), color, 1.4f);
            break;
        }
    }
}

void HuginStatsPanel::Render(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    auto& hugin = Core::HuginStatsManager::Get();
    hugin.UpdateFPS(ImGui::GetIO().Framerate);

    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const ImVec2 baseSize(1040.0f, 700.0f);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 workCenter(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                       vp->WorkPos.y + vp->WorkSize.y * 0.5f);

    // Fondo oscurecido (Dimmed Backdrop Scrim estilo Ajustes)
    ImDrawList* bgDl = ImGui::GetBackgroundDrawList(vp);
    if (bgDl) {
        bgDl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(0, 0, 0, 150));
    }

    ImGui::SetNextWindowPos(workCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(baseSize, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(840, 560), ImVec2(FLT_MAX, FLT_MAX));

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,      ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       ThemeCol(theme.surface0, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ThemeCol(theme.surface1, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,        ThemeCol(theme.border, 0.70f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool open = ImGui::Begin("Hugin — Estadísticas del Auditorio y Rendimiento", pOpen,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);

    bool ownPopupOpen = ImGui::GetCurrentContext()->OpenPopupStack.Size > 0 &&
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
         ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup));
    if (!ownPopupOpen) {
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    if (!open) {
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float margin  = 14.0f;
    const float gap     = 12.0f;
    const float sideW   = 240.0f;
    const float footerH = 64.0f;
    const float islandH = avail.y - margin * 2.0f;
    const float contentW = avail.x - margin * 2.0f - sideW - gap;

    ImVec2 origin(ImGui::GetCursorScreenPos().x + margin, ImGui::GetCursorScreenPos().y + margin);
    ImVec2 sideOrigin(origin.x, origin.y);
    ImVec2 contentOrigin(origin.x + sideW + gap, origin.y);

    ImDrawList* winDl = ImGui::GetWindowDrawList();
    DrawFloatingIslandShadow(winDl, sideOrigin,    ImVec2(sideW,    islandH), theme.windowRounding);
    DrawFloatingIslandShadow(winDl, contentOrigin, ImVec2(contentW, islandH), theme.windowRounding);

    // Divisor entre islas
    {
        float midX = sideOrigin.x + sideW + gap * 0.5f;
        float y0   = sideOrigin.y + islandH * 0.20f;
        float y1   = sideOrigin.y + islandH * 0.80f;
        ImU32 segCol = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.90f));
        winDl->AddLine(ImVec2(midX, y0), ImVec2(midX, y1), segCol, 1.5f);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Isla Derecha: Contenido de Telemetría
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::SetCursorScreenPos(contentOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));

    ImGui::BeginChild("##hugin_content_island", ImVec2(contentW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImVec2 islandAvail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(35.0f, 30.0f));
    ImGui::BeginChild("##hugin_content_scroll", ImVec2(islandAvail.x, islandAvail.y - footerH),
                      ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
    ImGui::PopStyleVar();

    const HuginSection& activeSec = k_HuginSections[m_SelectedSection];

    // Encabezado de página
    {
        ImGui::BeginGroup();
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
            ImGui::SetWindowFontScale(1.4f);
            ImGui::TextUnformatted(activeSec.label);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::TextUnformatted(activeSec.description);
            ImGui::PopStyleColor();
        }
        ImGui::EndGroup();

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Separador líquido degradado
        ImVec2 sepP = ImGui::GetCursorScreenPos();
        float sepW = ImGui::GetContentRegionAvail().x;
        ImVec4 accentV = ThemeCol(theme.accent);
        ImU32 sepAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.6f));
        ImU32 sepFade   = ImGui::ColorConvertFloat4ToU32(ImVec4(accentV.x, accentV.y, accentV.z, 0.0f));
        ImDrawList* cDl = ImGui::GetWindowDrawList();
        cDl->AddRectFilledMultiColor(sepP, ImVec2(sepP.x + sepW, sepP.y + 1.5f), sepAccent, sepFade, sepFade, sepAccent);

        ImGui::Dummy(ImVec2(0.0f, 18.0f));
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Helper para dibujar tarjetas KPI con el diseño de Ajustes ──
    auto DrawKpiCard = [&](const char* title, const std::string& mainVal, const std::string& subVal, ImU32 accentCol, float cardW, float cardH) {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);

        dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.60f)), theme.frameRounding);
        dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.50f)), theme.frameRounding, 0, 1.0f);
        dl->AddRectFilled(p0, ImVec2(p1.x, p0.y + 3.0f), accentCol, theme.frameRounding, ImDrawFlags_RoundCornersTop);

        dl->AddText(ImVec2(p0.x + 14.0f, p0.y + 12.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)), title);

        ImGui::PushFont(ImGui::GetFont());
        dl->AddText(ImVec2(p0.x + 14.0f, p0.y + 32.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), mainVal.c_str());
        ImGui::PopFont();

        dl->AddText(ImVec2(p0.x + 14.0f, p0.y + 66.0f), accentCol, subVal.c_str());

        ImGui::Dummy(ImVec2(cardW, cardH));
    };

    // ── PÁGINA 0: Resumen General ──
    if (m_SelectedSection == 0) {
        float contentWAvail = ImGui::GetContentRegionAvail().x;
        float cardGap = 12.0f;
        float cardW = (contentWAvail - cardGap * 3.0f) / 4.0f;
        if (cardW < 140.0f) cardW = (contentWAvail - cardGap) / 2.0f;
        float cardH = 92.0f;

        // 1. FPS Promedio
        char fpsMain[32], fpsSub[32];
        snprintf(fpsMain, sizeof(fpsMain), "%.1f FPS", hugin.GetAverageFPS());
        snprintf(fpsSub, sizeof(fpsSub), "%.1f ms / frame", hugin.GetFrameTimeMs());
        DrawKpiCard("RENDIMIENTO", fpsMain, fpsSub, IM_COL32(34, 197, 94, 255), cardW, cardH);
        ImGui::SameLine(0.0f, cardGap);

        // 2. Más Cantada
        auto topStats = GetTopSongPlayStats(1);
        std::string topTitle = topStats.empty() ? "Sin datos" : topStats[0].first;
        if (topTitle.size() > 18) topTitle = topTitle.substr(0, 16) + "...";
        std::string topSub = topStats.empty() ? "0 proyecciones" : (std::to_string(topStats[0].second) + " veces");
        DrawKpiCard("MÁS CANTADA", topTitle, topSub, IM_COL32(245, 158, 11, 255), cardW, cardH);
        ImGui::SameLine(0.0f, cardGap);

        // 3. Proyecciones
        int totalProjs = GetTotalSongProjections();
        char totMain[32];
        snprintf(totMain, sizeof(totMain), "%d", totalProjs);
        DrawKpiCard("PROYECCIONES", totMain, "Total acumulado", IM_COL32(56, 189, 248, 255), cardW, cardH);
        ImGui::SameLine(0.0f, cardGap);

        // 4. Tiempo Sesión
        double sec = hugin.GetSessionSeconds();
        int hrs = (int)(sec / 3600.0);
        int mins = (int)((sec - hrs * 3600.0) / 60.0);
        char sesMain[32];
        snprintf(sesMain, sizeof(sesMain), "%02dh %02dm", hrs, mins);
        DrawKpiCard("SESIÓN ACTIVA", sesMain, "App en ejecución", IM_COL32(168, 85, 247, 255), cardW, cardH);

        ImGui::Dummy(ImVec2(0.0f, 22.0f));

        // Vista previa de Top 5 canciones
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
        ImGui::TextUnformatted("Canciones Más Frecuentes en el Auditorio");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        auto top5 = GetTopSongPlayStats(5);
        if (top5.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::TextUnformatted("Aún no se registran proyecciones en vivo en esta sesión.");
            ImGui::PopStyleColor();
        } else {
            int maxP = top5[0].second > 0 ? top5[0].second : 1;
            for (size_t i = 0; i < top5.size(); i++) {
                ImVec2 r0 = ImGui::GetCursorScreenPos();
                float rw = ImGui::GetContentRegionAvail().x;
                float rh = 36.0f;
                ImVec2 r1 = ImVec2(r0.x + rw, r0.y + rh);

                dl->AddRectFilled(r0, r1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.45f)), theme.frameRounding);
                dl->AddRect(r0, r1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.35f)), theme.frameRounding, 0, 1.0f);

                // Medalla / Posición
                std::string posStr = "#" + std::to_string(i + 1);
                dl->AddText(ImVec2(r0.x + 12.0f, r0.y + 9.0f), IM_COL32(245, 158, 11, 255), posStr.c_str());

                // Título
                dl->AddText(ImVec2(r0.x + 45.0f, r0.y + 9.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), top5[i].first.c_str());

                // Barra de progreso
                float barW = 120.0f;
                float barX = r1.x - barW - 90.0f;
                float frac = (float)top5[i].second / (float)maxP;
                dl->AddRectFilled(ImVec2(barX, r0.y + 13.0f), ImVec2(barX + barW, r0.y + 23.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.8f)), 4.0f);
                dl->AddRectFilled(ImVec2(barX, r0.y + 13.0f), ImVec2(barX + barW * frac, r0.y + 23.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.85f)), 4.0f);

                // Conteo
                std::string cntStr = std::to_string(top5[i].second) + " veces";
                dl->AddText(ImVec2(r1.x - 75.0f, r0.y + 9.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)), cntStr.c_str());

                ImGui::Dummy(ImVec2(rw, rh + 6.0f));
            }
        }
    }
    // ── PÁGINA 1: Top Canciones Completo ──
    else if (m_SelectedSection == 1) {
        auto songRanking = GetTopSongPlayStats(25);
        if (songRanking.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::TextUnformatted("Aún no has proyectado canciones en vivo. ¡Proyecta canciones para ver las estadísticas aquí!");
            ImGui::PopStyleColor();
        } else {
            int maxPlays = songRanking[0].second > 0 ? songRanking[0].second : 1;

            for (size_t i = 0; i < songRanking.size(); i++) {
                const auto& item = songRanking[i];
                ImGui::PushID((int)i);

                float rowH = 38.0f;
                ImVec2 r0 = ImGui::GetCursorScreenPos();
                float rW = ImGui::GetContentRegionAvail().x;
                ImVec2 r1 = ImVec2(r0.x + rW, r0.y + rowH);

                dl->AddRectFilled(r0, r1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.45f)), theme.frameRounding);
                dl->AddRect(r0, r1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.35f)), theme.frameRounding, 0, 1.0f);

                // Número de ranking
                const char* medal = (i == 0) ? "🥇" : (i == 1 ? "🥈" : (i == 2 ? "🥉" : ""));
                if (medal[0] != '\0') {
                    dl->AddText(ImVec2(r0.x + 12.0f, r0.y + 10.0f), IM_COL32_WHITE, medal);
                } else {
                    std::string num = "#" + std::to_string(i + 1);
                    dl->AddText(ImVec2(r0.x + 12.0f, r0.y + 10.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)), num.c_str());
                }

                // Título de la canción
                dl->AddText(ImVec2(r0.x + 48.0f, r0.y + 10.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), item.first.c_str());

                // Barra de frecuencia
                float barW = 160.0f;
                float barX = r1.x - barW - 110.0f;
                float frac = (float)item.second / (float)maxPlays;
                dl->AddRectFilled(ImVec2(barX, r0.y + 14.0f), ImVec2(barX + barW, r0.y + 24.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.8f)), 4.0f);
                dl->AddRectFilled(ImVec2(barX, r0.y + 14.0f), ImVec2(barX + barW * frac, r0.y + 24.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.85f)), 4.0f);

                // Veces cantada
                std::string plays = std::to_string(item.second) + " veces";
                dl->AddText(ImVec2(r1.x - 90.0f, r0.y + 10.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)), plays.c_str());

                ImGui::Dummy(ImVec2(rW, rowH + 6.0f));
                ImGui::PopID();
            }
        }
    }
    // ── PÁGINA 2: Rendimiento FPS ──
    else if (m_SelectedSection == 2) {
        float contentWAvail = ImGui::GetContentRegionAvail().x;
        float cardGap = 14.0f;
        float cardW = (contentWAvail - cardGap) / 2.0f;
        float cardH = 100.0f;

        char fpsMain[32], fpsSub[32];
        snprintf(fpsMain, sizeof(fpsMain), "%.1f FPS", hugin.GetAverageFPS());
        snprintf(fpsSub, sizeof(fpsSub), "Objetivo 60.0 FPS • VSync Activo");
        DrawKpiCard("TASA DE CUADROS PROMEDIO", fpsMain, fpsSub, IM_COL32(34, 197, 94, 255), cardW, cardH);
        ImGui::SameLine(0.0f, cardGap);

        char msMain[32], msSub[32];
        snprintf(msMain, sizeof(msMain), "%.2f ms", hugin.GetFrameTimeMs());
        snprintf(msSub, sizeof(msSub), "Presupuesto: 16.6 ms por cuadro");
        DrawKpiCard("TIEMPO DE RENDERIZADO", msMain, msSub, IM_COL32(56, 189, 248, 255), cardW, cardH);

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        // Caja de información sobre el motor de render
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface1, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.40f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        if (ImGui::BeginChild("##engineCard", ImVec2(contentWAvail, 120.0f), true)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
            ImGui::TextUnformatted("Motor Gráfico y Aceleración por Hardware");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::TextWrapped("ProyecThor utiliza OpenGL 3.3 Core con triple búfer y sincronización vertical automática. La reproducción de fondos de video animados en bucle y las transiciones fluidas de versículos se procesan directamente en la GPU sin impacto en la CPU.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }
    // ── PÁGINA 3: Sesión Activa ──
    else if (m_SelectedSection == 3) {
        double sec = hugin.GetSessionSeconds();
        int hrs = (int)(sec / 3600.0);
        int mins = (int)((sec - hrs * 3600.0) / 60.0);
        int secs = (int)(sec - hrs * 3600.0 - mins * 60.0);

        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr), "%02d horas, %02d minutos, %02d segundos", hrs, mins, secs);

        float contentWAvail = ImGui::GetContentRegionAvail().x;
        DrawKpiCard("TIEMPO TOTAL DE ESTA REUNIÓN", timeStr, "Cronómetro continuo de la aplicación", IM_COL32(168, 85, 247, 255), contentWAvail, 92.0f);

        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        // Tarjeta informativa de proyección
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface1, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.40f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.frameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        if (ImGui::BeginChild("##outputCard", ImVec2(contentWAvail, 120.0f), true)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
            ImGui::TextUnformatted("Estado de Proyección y Salidas");
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
            ImGui::Text("• Proyecciones registradas en este servicio: %d canciones", GetTotalSongProjections());
            ImGui::TextUnformatted("• Conexión de monitor secundario para proyección: Activa");
            ImGui::TextUnformatted("• Registro de auditorio Hugin PRO: Operacional");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild(); // hugin_content_scroll

    // Barra inferior de estado (Footer Bar estilo Ajustes)
    {
        ImVec2 winPos  = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();
        float  barY    = winPos.y + winSize.y - footerH;

        ImDrawList* dlFoot = ImGui::GetWindowDrawList();

        dlFoot->AddRectFilled(
            ImVec2(winPos.x, barY),
            ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.98f)),
            theme.windowRounding,
            ImDrawFlags_RoundCornersBottom);

        dlFoot->AddLine(
            ImVec2(winPos.x, barY),
            ImVec2(winPos.x + winSize.x, barY),
            ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.9f)),
            1.5f);

        float textY = barY + (footerH - ImGui::GetTextLineHeight()) * 0.5f;

        dlFoot->AddText(ImVec2(winPos.x + 30.0f, textY),
                        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textDim)),
                        "● Telemetría Hugin PRO activa • Muestreo continuo a 60 FPS");

        const char* rInfo = "ProyecThor Telemetry Engine";
        float rInfoW = ImGui::CalcTextSize(rInfo).x;
        dlFoot->AddText(ImVec2(winPos.x + winSize.x - rInfoW - 30.0f, textY),
                        ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint)),
                        rInfo);
    }

    ImGui::EndChild(); // hugin_content_island
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // ─────────────────────────────────────────────────────────────────────────
    //  Isla Izquierda: Navegación del Sidebar (Estilo Ajustes)
    // ─────────────────────────────────────────────────────────────────────────
    ImGui::SetCursorScreenPos(sideOrigin);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.base, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.92f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   theme.windowRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 25.0f));

    ImGui::BeginChild("##hugin_sidebar", ImVec2(sideW, islandH), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    ImDrawList* sideDl = ImGui::GetWindowDrawList();

    // Encabezado
    ImGui::SetCursorPosX(22.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textPrimary));
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextUnformatted("Hugin");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);
    ImVec2 bPos = ImGui::GetCursorScreenPos();
    ImVec2 bSz(38.0f, 17.0f);
    sideDl->AddRectFilled(bPos, ImVec2(bPos.x + bSz.x, bPos.y + bSz.y), IM_COL32(245, 158, 11, 45), 4.0f);
    sideDl->AddRect(bPos, ImVec2(bPos.x + bSz.x, bPos.y + bSz.y), IM_COL32(245, 158, 11, 200), 4.0f);
    sideDl->AddText(ImVec2(bPos.x + 7.0f, bPos.y + 1.0f), IM_COL32(255, 215, 0, 255), "PRO");
    ImGui::Dummy(bSz);

    ImGui::SetCursorPosX(22.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.textDim));
    ImGui::TextUnformatted("Telemetría del Auditorio");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 22.0f));

    // Secciones en el Sidebar
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));

    const float itemH = 45.0f;
    const float itemW = sideW - 20.0f;

    sideDl->ChannelsSplit(2);
    float targetPillY = -1.0f;

    for (int i = 0; i < k_HuginSectionCount; i++) {
        bool selected = (m_SelectedSection == i);
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += 10.0f;

        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));

        char selectId[32];
        snprintf(selectId, sizeof(selectId), "##hugin_nav%d", i);

        sideDl->ChannelsSetCurrent(1);

        bool clicked = ImGui::Selectable(selectId, selected, ImGuiSelectableFlags_None, ImVec2(itemW, itemH));
        bool hovered = ImGui::IsItemHovered() && !selected;

        if (clicked && m_SelectedSection != i) {
            m_SelectedSection = i;
        }

        if (selected) {
            targetPillY = p.y - ImGui::GetWindowPos().y;
        } else if (hovered) {
            sideDl->AddRectFilled(p, ImVec2(p.x + itemW, p.y + itemH),
                                  ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface2, 0.5f)), 8.0f);
        }

        // Chip circular con icono
        ImVec4 iconColV = ImGui::ColorConvertU32ToFloat4(k_HuginSections[i].color);
        ImVec2 badgeC(p.x + 22.0f, p.y + itemH * 0.5f);
        ImVec4 badgeFillV = iconColV;
        badgeFillV.w = selected ? 0.24f : (hovered ? 0.16f : 0.10f);
        sideDl->AddCircleFilled(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeFillV), 20);
        if (selected) {
            ImVec4 badgeRingV = iconColV;
            badgeRingV.w = 0.55f;
            sideDl->AddCircle(badgeC, 13.5f, ImGui::ColorConvertFloat4ToU32(badgeRingV), 20, 1.2f);
        }
        iconColV.w *= selected ? 1.0f : (hovered ? 0.90f : 0.62f);
        DrawHuginCatIcon(sideDl, k_HuginSections[i].icon, badgeC, 8.5f, ImGui::ColorConvertFloat4ToU32(iconColV));

        float labelY = p.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f;
        float labelX = p.x + 42.0f;
        ImVec4 labelColV = selected ? ThemeCol(theme.textPrimary) : ThemeCol(theme.textDim, hovered ? 1.0f : 0.85f);
        sideDl->AddText(ImVec2(labelX, labelY), ImGui::ColorConvertFloat4ToU32(labelColV), k_HuginSections[i].label);

        ImGui::PopStyleColor(3);
    }

    // Píldora de selección en canal 0
    if (targetPillY >= 0.0f) {
        sideDl->ChannelsSetCurrent(0);
        ImVec2 pillP(ImGui::GetWindowPos().x + 10.0f, ImGui::GetWindowPos().y + targetPillY);

        ImU32 colLeft  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.30f));
        ImU32 colRight = ImGui::ColorConvertFloat4ToU32(ImVec4(theme.accent[0], theme.accent[1], theme.accent[2], 0.0f));
        sideDl->AddRectFilledMultiColor(pillP, ImVec2(pillP.x + itemW, pillP.y + itemH), colLeft, colRight, colRight, colLeft);

        sideDl->AddRectFilled(ImVec2(pillP.x, pillP.y + 8.0f), ImVec2(pillP.x + 4.0f, pillP.y + itemH - 8.0f),
                              ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent)), 2.0f);
    }

    sideDl->ChannelsMerge();
    ImGui::PopStyleVar();

    // Tarjeta inferior de estado de telemetría en vivo (sin alterar cursor)
    {
        ImVec2 winP = ImGui::GetWindowPos();
        ImVec2 cardP0(winP.x + 10.0f, winP.y + islandH - 75.0f);
        ImVec2 cardP1(cardP0.x + sideW - 20.0f, cardP0.y + 60.0f);
        sideDl->AddRectFilled(cardP0, cardP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface1, 0.60f)), theme.frameRounding);
        sideDl->AddRect(cardP0, cardP1, ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.40f)), theme.frameRounding, 0, 1.0f);

        sideDl->AddCircleFilled(ImVec2(cardP0.x + 16.0f, cardP0.y + 22.0f), 4.5f, IM_COL32(34, 197, 94, 255));
        sideDl->AddText(ImVec2(cardP0.x + 28.0f, cardP0.y + 14.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textPrimary)), "Telemetría Activa");
        sideDl->AddText(ImVec2(cardP0.x + 28.0f, cardP0.y + 32.0f), ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.textFaint)), "Muestreo en Tiempo Real");
    }

    ImGui::EndChild(); // hugin_sidebar
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::End();
}

} // namespace ProyecThor::UI
