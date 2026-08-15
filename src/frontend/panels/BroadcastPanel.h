#pragma once
#include "capture/CapturePanel.h"
#include "backend/core/StreamEncoder.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ProyecThor::UI {

// Que muestra una entrada de la lista de capas de Transmisión (ver
// BroadcastPanel::RenderLayerSection) -- pedido explicito: "manejar las
// capas de la transmision... poner cosas como overlays, capture, etc.",
// incluida la salida en vivo (Público) como una fuente mas.
enum class StreamLayerKind { Capture, Overlay, LiveOutput };

struct StreamLayerEntry {
    StreamLayerKind kind = StreamLayerKind::Capture;
    std::string     name;            // etiqueta mostrada en la lista
    std::string     overlayPngPath;  // solo kind==Overlay
};

// ── BroadcastPanel ───────────────────────────────────────────────────────────
// Transmision en vivo por RTMP, estilo OBS. Vive dentro de Ajustes >
// Conexiones > Streaming Y en el espacio de trabajo "Transmisión" (ver
// StreamingWorkspacePanel) -- misma instancia, no es un IPanel propio.
//  - Capture: la MISMA fuente de captura que ya usa el resto de ProyecThor
//    (CapturePanel — camara/ventana/monitor).
//  - Layer: preview + LISTA de capas disponibles (Captura/Overlay/Vista en
//    vivo) -- el operador elige cual esta activa (ver m_ActiveLayer). Solo
//    UNA capa activa a la vez todavia (no hay composicion simultanea con
//    alpha blending real, eso queda para mas adelante); la lista sirve para
//    tener varias fuentes preparadas y cambiar entre ellas rapido, como las
//    "Sources" de OBS pero sin mezclar.
//  - Iniciar: servidor + clave de stream, bitrate/fps, y el boton de
//    arrancar/detener la transmision real (StreamEncoder, subproceso ffmpeg).
class BroadcastPanel {
public:
    ~BroadcastPanel();

    // Llamar UNA VEZ POR FRAME sin importar que pestaña de Yggdrasil este
    // activa: si hay una transmision en curso, sigue empujando frames
    // aunque el operador este mirando OSC/Red/Chat -- mismo criterio que
    // StreamingPanel::Update()/TeamChatPanel::Update().
    void Update();

    void RenderCaptureSection();
    void RenderLayerSection();
    void RenderStartSection();

private:
    struct CachedTex { unsigned int tex = 0; int w = 0; int h = 0; };

    // Textura + tamaño de lo que la capa activa (o Captura, si ninguna esta
    // elegida en la lista) debe mostrar AHORA MISMO -- usado tanto por el
    // preview de RenderLayerSection() como por el encode real de Update()/
    // RenderStartSection(), asi los dos SIEMPRE ven exactamente lo mismo.
    // outTex queda en nullptr si no hay nada que mostrar todavia.
    void ResolveActiveSource(void*& outTex, int& outW, int& outH);
    void ResolveOverlayTexture(const std::string& path, unsigned int& outTex, int& outW, int& outH);

    CapturePanel m_Capture;      // instancia propia, independiente de la de Diseño/Captura
    bool         m_ShowInLayer = false;

    std::vector<StreamLayerEntry> m_Layers;
    int                             m_ActiveLayer = -1; // -1 = Captura directa (comportamiento de siempre)
    std::unordered_map<std::string, CachedTex> m_OverlayTexCache;

    Core::StreamEncoder        m_Encoder;
    std::string                m_StatusMessage;
    bool                       m_StatusIsError = false;
    std::vector<unsigned char> m_ReadbackBuffer; // reusado entre frames, evita reallocs
};

} // namespace ProyecThor::UI
