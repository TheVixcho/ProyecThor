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
    if (!hwnd) return;

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
    const float availW = ImGui::GetContentRegionAvail().x;

    // ── Fila 1: Barra de URL + Botón de Navegación ───────────────────────
    const float goBtnW = 38.0f;
    const float urlInputW = std::max(80.0f, availW - goBtnW - 4.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(8.0f, 5.0f));

    ImGui::SetNextItemWidth(urlInputW);
    bool enterPressed = ImGui::InputTextWithHint("##webUrl", "https://ejemplo.com...", m_UrlBuf, sizeof(m_UrlBuf), ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 4.0f);
    bool goClicked = DS::GlassButton("->", ImVec2(goBtnW, 28.0f));

    if (enterPressed || goClicked) Go();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Fila 2: Botón de Acción Completa (Enviar a Público / Detener) ────
    bool canSend = m_Navigated && m_WebView.IsAvailable();
    ImGui::BeginDisabled(!canSend);
    if (m_SendingToPublic) {
        if (DS::GlassButton("Detener envío a Público", ImVec2(availW, 30.0f), DS::DangerColor))
            StopSendToPublic();
    } else {
        if (DS::GlassButton("Enviar a Público", ImVec2(availW, 30.0f), DS::SuccessColor))
            StartSendToPublic();
    }
    ImGui::EndDisabled();

    // ── Instrucción inicial cuando aún no se ha navegado ────────────────
    if (!m_Navigated) {
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
            "Escribe una URL en la barra superior y presiona '->' para navegar.");
        return;
    }

    // ── Estado de proyección a público ───────────────────────────────────
    if (m_SendingToPublic) {
        UpdateSendToPublicBounds();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::SuccessColor),
            "Mostrando en vivo en la salida de video.");
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
            "El navegador se encuentra activo en la pantalla secundaria.");
        return;
    }

    if (!m_WebView.IsAvailable()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
            "Se abrió en tu navegador externo.");
        if (DS::GlassButton("Volver a abrir", ImVec2(availW, 30.0f)))
            ProyecThor::External::OpenURL(m_UrlBuf);
        return;
    }

    if (m_WebView.HasError()) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::DangerColor), "%s", m_WebView.GetLastError().c_str());
        return;
    }

    // ── Área del Navegador Web (Ocupa todo el alto disponible) ───────────
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::BeginChild("##webArea", ImVec2(0.0f, 0.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    bool ready = m_WebView.IsReady();
    if (!ready) {
        ImVec2 childAvail = ImGui::GetContentRegionAvail();
        ImVec2 center = { childAvail.x * 0.5f, childAvail.y * 0.40f };
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        DrawLoadingSpinner(dl, { origin.x + center.x, origin.y + center.y }, 16.0f);

        const char* msg = "Cargando página...";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos({ center.x - ts.x * 0.5f, center.y + 24.0f });
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
