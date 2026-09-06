#pragma once
#include "backend/core/NetworkStreamServer.h"
#include "backend/core/FrameEncodeWorker.h"
#include <string>
#include <vector>
#include <mutex>
#include <imgui.h>

namespace ProyecThor::UI {

// Ya no es un IPanel independiente: ahora vive como contenido de una de las
// secciones del sidebar de HomePanel ("Transmisión en Red"). Ver HomePanel.cpp.
class StreamingPanel {
public:
    StreamingPanel()  = default;
    ~StreamingPanel() = default;

    // Debe llamarse UNA VEZ POR FRAME sin importar que seccion de Home este
    // activa: mantiene vivo el envio de frames/QR al stream LAN aunque el
    // operador este mirando otra pestaña (ver captura de estado en el plan).
    void Update();

    // Dibuja los controles (solo cuando la sección "Transmisión en Red" esta
    // activa). Ya no abre su propia ventana — HomePanel es dueño de esa.
    void RenderContent();

    std::string GetName() const { return "Transmisión en Red"; }

private:
    void RenderServerControl();
    void RenderLayerSelector();
    void RenderQualitySelector();
    void RenderURLSection();

    void CaptureAndPushFrame(int w, int h, int quality);

    void RebuildQRTexture(const std::string& url);
    void DrawQR(ImDrawList* dl, ImVec2 origin, float size);

    int  m_Port        = 8080;

    // Config local — se sincroniza con el servidor al cambiar
    Core::StreamConfig m_Config;
    bool               m_ConfigDirty = false;

    // Throttle de captura: ImGui::GetTime() de la ultima vez que se
    // capturo/comprimio un frame. Usado en Render() para no capturar mas
    // rapido de lo que cada modo de transmision realmente necesita.
    double m_LastCaptureTime = 0.0;

    // Encode JPEG en hilo dedicado — ver FrameEncodeWorker.h. Evita que el
    // encode bloquee el hilo de render/UI (el mismo que dibuja el proyector).
    Core::FrameEncodeWorker m_EncodeWorker;

    // QR
    std::string           m_QRCachedURL;
    std::vector<uint8_t>  m_QRModules;
    int                   m_QRSize = 0;
};

} // namespace ProyecThor::UI