#include "PerformancePanel.h"
#include "SettingsManager.h"
#include "core/PerformanceGovernor.h"
#include "core/SystemStats.h"
#include "core/MemoryOptimizer.h"
#include "ui/framework/DesignSystem.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI {

namespace {

bool MiniStudioBtn(const char* id, const char* label, ImVec2 sz, bool active, ImU32 accent = 0) {
    sz.x = std::max(24.0f, sz.x);
    sz.y = std::max(20.0f, sz.y);

    ImGui::PushID(id);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##b", sz);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 p1 = {p0.x + sz.x, p0.y + sz.y};

    const ImU32 ac = accent ? accent : DS::AccentLight;
    ImU32 bg = DS::BtnDefaultFill;
    if (active) {
        ImVec4 a = ImGui::ColorConvertU32ToFloat4(ac);
        ImVec4 b = ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill);
        bg = ImGui::ColorConvertFloat4ToU32({b.x + (a.x - b.x) * 0.20f, b.y + (a.y - b.y) * 0.20f, b.z + (a.z - b.z) * 0.20f, 1.0f});
    } else if (hovered) {
        bg = DS::BtnHoverFill;
    }
    const ImU32 bord = active ? ac : (hovered ? IM_COL32(255, 255, 255, 46) : IM_COL32(255, 255, 255, 20));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
    dl->AddRect(p0, p1, bord, DS::RadiusMedium, 0, active ? 1.3f : 1.0f);

    const ImVec2 ts = ImGui::CalcTextSize(label);
    dl->PushClipRect({ p0.x + 3.0f, p0.y + 2.0f }, { p1.x - 3.0f, p1.y - 2.0f }, true);
    dl->AddText({p0.x + (sz.x - ts.x) * 0.5f, p0.y + (sz.y - ts.y) * 0.5f},
                active ? DS::TextPrimary : (hovered ? DS::TextPrimary : DS::TextSecondary), label);
    dl->PopClipRect();
    ImGui::PopID();
    return clicked;
}

} // namespace

void PerformancePanel::Render(bool* isOpen)
{
    ImGui::SetNextWindowSize(ImVec2(356.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Ahorro de energía###PerfPanel", isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    auto& mgr  = ProyecThor::Settings::SettingsManager::Get();
    auto& ps   = mgr.GetSettings().performance;
    auto& gov  = Core::PerformanceGovernor::Get();
    auto& sys  = Core::SystemStats::Get();

    static float s_Feedback = 0.0f;
    if (s_Feedback > 0.0f) s_Feedback = std::max(0.0f, s_Feedback - ImGui::GetIO().DeltaTime);

    const float w   = std::max(336.0f, ImGui::GetContentRegionAvail().x);
    const float gap = 6.0f;
    const float bw  = (w - gap * 2.0f) / 3.0f;

    const bool isMin = ps.lowMemoryMode;
    const bool isEco = ps.performanceMode && !ps.lowMemoryMode;
    const bool isStd = !ps.performanceMode && !ps.lowMemoryMode;

    if (MiniStudioBtn("##std", "Estándar", {bw - 12.0f, 30.0f}, isStd)) {
        ps.performanceMode     = false;
        ps.lowMemoryMode       = false;
        ps.disableGlassBlur    = false;
        ps.pauseBackgroundScan = false;
        ps.targetFPS           = 60;
        mgr.SaveSettings();
    }
    ImGui::SameLine(0.0f, gap);
    if (MiniStudioBtn("##eco", "Ahorro de energía", {bw + 24.0f, 30.0f}, isEco, IM_COL32(72, 208, 148, 255))) {
        ps.performanceMode     = true;
        ps.lowMemoryMode       = false;
        ps.disableGlassBlur    = true;
        ps.pauseBackgroundScan = true;
        ps.targetFPS           = 60;
        mgr.SaveSettings();
        Core::MemoryOptimizer::RequestPurge(false, false);
    }
    ImGui::SameLine(0.0f, gap);
    if (MiniStudioBtn("##min", "RAM Mínima", {bw - 12.0f, 30.0f}, isMin, IM_COL32(88, 178, 236, 255))) {
        ps.performanceMode     = true;
        ps.lowMemoryMode       = true;
        ps.disableGlassBlur    = true;
        ps.pauseBackgroundScan = true;
        ps.targetFPS           = 30;
        mgr.SaveSettings();
        Core::MemoryOptimizer::RequestPurge(true, true);
        s_Feedback = 2.0f;
    }

    ImGui::Spacing();

    char statsLine[96];
    const double fpsVal = gov.Fps() > 0.0 ? gov.Fps() : ImGui::GetIO().Framerate;
    std::snprintf(statsLine, sizeof(statsLine), "%.0f FPS   |   %.0f%% CPU   |   %.0f MB",
                  fpsVal, sys.CpuPercent(), Core::MemoryOptimizer::GetProcessRamMB());
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    ImGui::TextUnformatted(statsLine);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (MiniStudioBtn("##purge", s_Feedback > 0.0f ? "RAM Liberada" : "Liberar RAM",
                      {w, 30.0f}, s_Feedback > 0.0f, IM_COL32(72, 208, 148, 255)))
    {
        Core::MemoryOptimizer::RequestPurge(true, true);
        s_Feedback = 2.0f;
    }

    ImGui::End();
}

} // namespace ProyecThor::UI
