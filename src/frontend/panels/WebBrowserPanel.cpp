#include "WebBrowserPanel.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/ui/LoadingSpinner.h"
#include "backend/core/PresentationCore.h"
#include "external/tools/OpenURL.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>

namespace ProyecThor::UI {

void WebBrowserPanel::Go()
{
    std::string url = m_UrlBuf;
    if (url.empty()) return;
    // Sin esquema, la mayoria de los sitios igual resuelven con https --
    // evita que el operador tenga que acordarse de escribir "https://".
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
        url = "https://" + url;

    m_Navigated = true;
    if (m_WebView.IsAvailable())
        m_WebView.NavigateTo(url);
    else
        ProyecThor::External::OpenURL(url);
}

void WebBrowserPanel::StartSendToPublic()
{
    m_SendingToPublic = true;
    // Por si el proyector todavia no estaba activo -- UpdateSendToPublicBounds
    // reintenta cada frame hasta que la ventana nativa exista.
    Core::PresentationCore::Get().SetProjecting(true);
}

void WebBrowserPanel::StopSendToPublic()
{
    m_SendingToPublic = false;
    m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

void WebBrowserPanel::UpdateSendToPublicBounds()
{
    void* hwnd = Core::PresentationCore::Get().GetProjectorNativeWindow();
    if (!hwnd) return; // ventana del proyector todavia no existe este frame, se reintenta el que viene

    m_WebView.Reparent(hwnd);

    auto state = Core::PresentationCore::Get().GetState();
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (!monitors || monitorCount == 0) return;
    int idx = std::clamp(state.targetMonitorIndex, 0, monitorCount - 1);

    int mx = 0, my = 0;
    glfwGetMonitorPos(monitors[idx], &mx, &my);
    const GLFWvidmode* mode = glfwGetVideoMode(monitors[idx]);
    if (!mode) return;

    m_WebView.UpdateBounds(mx, my, mode->width, mode->height, true);
}

void WebBrowserPanel::Render()
{
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 190.0f);
    bool enterPressed = ImGui::InputText("##webUrl", m_UrlBuf, sizeof(m_UrlBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    bool goClicked = DS::GlassButton("Ir", ImVec2(50.0f, 0.0f));

    ImGui::SameLine();
    bool canSend = m_Navigated && m_WebView.IsAvailable();
    ImGui::BeginDisabled(!canSend);
    if (m_SendingToPublic) {
        if (DS::GlassButton("Dejar de enviar", ImVec2(130.0f, 0.0f), DS::DangerColor))
            StopSendToPublic();
    } else {
        if (DS::GlassButton("Enviar a Público", ImVec2(130.0f, 0.0f), DS::SuccessColor))
            StartSendToPublic();
    }
    ImGui::EndDisabled();

    if (enterPressed || goClicked) Go();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (!m_Navigated) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
            "Escribi una URL y toca Ir -- podes navegar como en cualquier navegador.");
        return;
    }

    if (m_SendingToPublic) {
        UpdateSendToPublicBounds();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::SuccessColor),
            "Mostrando esta pagina en la salida real al público.");
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
            "El navegador se movio a esa pantalla -- ya no se previsualiza aca mientras dure.");
        return;
    }

    if (!m_WebView.IsAvailable()) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Se abrio en tu navegador externo (esta ventana no puede embeberlo aca).");
        if (DS::GlassButton("Volver a abrir", ImVec2(160.0f, 30.0f)))
            ProyecThor::External::OpenURL(m_UrlBuf);
        return;
    }

    if (m_WebView.HasError()) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::DangerColor), "%s", m_WebView.GetLastError().c_str());
        return;
    }

    ImGui::BeginChild("##webArea", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool ready = m_WebView.IsReady();
    if (!ready) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 center = { avail.x * 0.5f, avail.y * 0.42f };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        DrawLoadingSpinner(dl, { origin.x + center.x, origin.y + center.y }, 16.0f);

        const char* msg = "Cargando la pagina...";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos({ center.x - ts.x * 0.5f, center.y + 26.0f });
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "%s", msg);
    }
    ImVec2 areaPos  = ImGui::GetWindowPos();
    ImVec2 areaSize = ImGui::GetWindowSize();
    ImGui::EndChild();

    if (ready)
        m_WebView.UpdateBounds(static_cast<int>(areaPos.x), static_cast<int>(areaPos.y),
                                static_cast<int>(areaSize.x), static_cast<int>(areaSize.y), true);
    else
        m_WebView.UpdateBounds(0, 0, 0, 0, false);
}

} // namespace ProyecThor::UI
