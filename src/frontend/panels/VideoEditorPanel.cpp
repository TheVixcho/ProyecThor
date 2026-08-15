#include "VideoEditorPanel.h"
#include "UIManager.h"
#include "DesignSystem.h"
#include "biblio/LibraryIcons.h"
#include <imgui.h>

namespace ProyecThor::UI {

void VideoEditorPanel::Render()
{
    bool visible = false;
    if (m_UIManagerRef)
        visible = DS::BeginGlassPanel("Editor de Video", m_UIManagerRef->GetGlassRenderer(),
                                      nullptr, 0, ImVec2(0.0f, 0.0f));
    else
        visible = ImGui::Begin("Editor de Video");

    if (!visible) {
        if (m_UIManagerRef) DS::EndGlassPanel();
        else                ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 center = { origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f };

    const float iconSz = 56.0f;
    ProyecThor::Library::DrawIcon_Video(dl, { center.x - iconSz * 0.5f, center.y - iconSz * 1.6f }, iconSz,
                                        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.35f)));

    const char* title = "Editor de Video";
    ImVec2 titleSz = ImGui::CalcTextSize(title);
    dl->AddText({ center.x - titleSz.x * 0.5f, center.y - 4.0f },
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.85f)), title);

    const char* sub = "Proximamente";
    ImVec2 subSz = ImGui::CalcTextSize(sub);
    dl->AddText({ center.x - subSz.x * 0.5f, center.y + 20.0f },
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.45f)), sub);

    if (m_UIManagerRef) DS::EndGlassPanel();
    else                ImGui::End();
}

} // namespace ProyecThor::UI
