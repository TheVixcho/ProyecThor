#pragma once

#include "ui/framework/GlassRenderer.h"
#include <string>
#include <functional>
#include <mutex>

namespace ProyecThor::UI {

class CanvaPanel {
public:
    CanvaPanel();
    ~CanvaPanel() = default;

    // Renderiza el espacio de trabajo de Canva dentro de una sección contenedora (ej. LibraryPanel)
    void RenderContent(GlassRenderer* glass = nullptr);

    // Mantenido por compatibilidad si se requiere ventana flotante
    void Render(bool* pOpen, GlassRenderer& glass);

    void SetOnOpenMultimedia(std::function<void()> cb) { m_OnOpenMultimedia = cb; }

private:
    char m_SearchFilter[128] = "";
    char m_ManualCodeBuf[512] = "";
    std::string m_NotificationMsg;
    bool m_NotificationIsSuccess = true;
    float m_NotificationTimer = 0.0f;
    std::mutex m_NotificationMutex;
    std::function<void()> m_OnOpenMultimedia;

    void SetNotification(const std::string& msg, bool success);
};

} // namespace ProyecThor::UI
