#pragma once
#include "../IPanel.h"
#include "backend/core/PresentationCore.h"
#include "AudioMeters.h"
#include <string>
#include <unordered_map>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;
class TeamChatPanel;

class ViewPanel : public IPanel {
public:
    explicit ViewPanel(UIManager* uiManager = nullptr) : m_UIManager(uiManager) {}
    ~ViewPanel() override = default;

    void        Render() override;
    std::string GetName() const override { return "Vista en Vivo"; }

    // Misma instancia que Herramientas (retirado) -- Chat aparece en varios
    // lugares pero es un unico servidor real. Ver cableado en main.cpp.
    void SetTeamChatPanelRef(TeamChatPanel* ref) { m_TeamChatPanelRef = ref; }

private:
    UIManager*      m_UIManager        = nullptr;
    TeamChatPanel*  m_TeamChatPanelRef = nullptr;

    void RenderContent(float panelW, float panelH);

    // Riel vertical de iconos a la derecha del video ("Limpiar <tipo>",
    // contenido en vivo) + franja horizontal debajo del transporte
    // (configuracion/vista: proporcion, ajustes, que fuente previsualizar,
    // Overlays, Chat, Pads) -- separados a proposito para no mezclar "acción
    // destructiva" con "ajuste de vista".
    void RenderQuickActionsClear(float railW);
    void RenderQuickActionsConfig(float stripH);

    // Modo compacto horizontal -- cuando el panel queda mas ancho que alto
    // de lo normal (ej. franja superior completa en Ajustes > Apariencia >
    // Entorno de trabajo > Transmisión), apilar video/transporte/config
    // verticalmente como en el modo de siempre dejaria un video minusculo.
    // En vez de eso, toda la toolbar (transporte + config + limpiar) pasa a
    // una columna a la IZQUIERDA con su propio scroll, y el video se queda
    // con TODA la altura disponible de la franja. Ver Render().
    void RenderCompactWide(ImVec2 avail, bool showQuickActions);

    // Herramienta inline activa (ver RenderInlineTool) -- en vez de abrir un
    // popup flotante separado, Overlays/Chat/Pads se muestran EN EL MISMO
    // panel, ocupando el espacio libre entre el transporte y la franja de
    // config de abajo (pedido explicito: "que muestren el contenido abajo,
    // no como panel aparte sino como si fuera parte del mismo panel").
    // Click de nuevo en el mismo boton = cerrar (volver a None).
    enum class InlineTool { None, Overlays, Chat, Pads, Clock };
    InlineTool m_ActiveTool = InlineTool::None;

    // Alto minimo que se le reserva siempre al transporte (progreso + pads +
    // fader) aunque haya una herramienta inline abierta -- ver RenderContent
    // de cada seccion en Render().
    static constexpr float kLiveTransportMinH = 120.0f;

    void RenderInlineTool(float w, float h);
    void RenderOverlaysContent();
    void RenderChatContent();
    void RenderPadsContent();
    void RenderClockContent();
    std::unordered_map<std::string, ImTextureID> m_OverlayThumbCache;

    // Barra de streaming en red — extraída para no ensuciar RenderContent.
    // Recibe los límites del contenedor de video (p0/p1) y el estado ya leído.
    void RenderNetworkBar(
        const ImVec2&                   p0,
        const ImVec2&                   p1,
        const Core::PresentationState&  state,
        Core::PresentationCore&         core);

    // Transporte + VU meters del player "general" (bg, el que va a
    // público) — antes vivian en Monitor (MonitorLiveControls, ver
    // RenderLiveMonitor/RenderLiveControls, ya eliminados de ahi). Se
    // movieron aca porque Monitor se quedaba sin aire en pantallas chicas,
    // y este panel ya es "donde el usuario ve lo que transmite".
    void RenderLiveTransport(float w, float h);

    // Que fuente previsualiza el video de "Vista en Vivo" — no confundir con
    // los puntos de estado "Público"/"Stage" (ahora en la toolbar superior,
    // ver UIManager::RenderModeToolbarStatusActions), que prenden/apagan las
    // salidas reales. Esto solo cambia que ve el OPERADOR aca, para poder
    // llevar constancia de las 4 salidas sin pararse frente a cada pantalla
    // (ver botón "vaPreviewSource" en RenderQuickActions, ciclа entre las 4).
    // Publico/Stage siempre reflejan lo que este en vivo (sin cambios acá).
    // Transmision no tiene contenido propio para previsualizar aca (RTMP usa
    // Captura, ver BroadcastPanel) -- ese modo solo ofrece un atajo al
    // espacio de trabajo "Transmisión". Lan SI puede divergir de Publico
    // (ver Core::OutputContentMode / PresentationCore::SetLanContentMode):
    // el operador puede clavarla en "Solo reloj"/"En blanco" desde aca.
    enum class PreviewSource { Publico, Stage, Transmision, Lan };
    PreviewSource m_PreviewSource = PreviewSource::Publico;

    AudioMeters m_AudioMeters;
    bool        m_LivePlaying = false;
    bool        m_LiveMuted   = false;
    float       m_LiveVolume  = 0.8f;
};

} // namespace ProyecThor::UI