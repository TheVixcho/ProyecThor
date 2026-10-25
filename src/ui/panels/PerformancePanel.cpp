#include "PerformancePanel.h"
#include "core/PerformanceGovernor.h"
#include "core/SystemStats.h"
#include <imgui.h>

namespace ProyecThor::UI {

void PerformancePanel::Render(bool* isOpen)
{
    ImGui::SetNextWindowSize(ImVec2(260, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Rendimiento", isOpen))
    {
        ImGui::End();
        return;
    }

    auto& perf = Core::PerformanceGovernor::Get();
    auto& sys  = Core::SystemStats::Get();

    ImGui::Text("FPS: %.0f", perf.Fps());
    ImGui::Separator();
    ImGui::Text("CPU: %.0f%%", sys.CpuPercent());
    ImGui::Text("RAM: %.0f / %.0f MB", sys.RamUsedMB(), sys.RamTotalMB());
    ImGui::Separator();
    ImGui::TextWrapped("GPU: %s", sys.GpuName().c_str());

    ImGui::End();
}

} // namespace ProyecThor::UI
