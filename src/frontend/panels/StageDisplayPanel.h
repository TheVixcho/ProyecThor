#pragma once
#include "backend/core/FrameEncodeWorker.h"
#include <string>

namespace ProyecThor::UI {

    // Configura y controla el Monitor de Control (Stage): activarlo/apagarlo,
    // elegir a que pantalla fisica (o LAN) se sirve, y que se muestra en el
    // (reloj, texto en vivo, proxima linea, organizados en una grilla de
    // celdas). El render real de la grilla ocurre en UIManager.cpp (bloque
    // "StageLive"); este panel solo edita la configuración persistida en
    // SettingsManager y controla el arranque/parada real via PresentationCore.
    // Vive como categoria dentro de Ajustes (ver SettingsPanel::
    // RenderCategoryStage) — el hub de Control se elimino porque su
    // configuracion ya estaba duplicada en Ajustes > Proyeccion.
    class StageDisplayPanel {
    public:
        StageDisplayPanel()  = default;
        ~StageDisplayPanel() = default;

        void        RenderContent();
        std::string GetName() const { return "Stage Display"; }

    private:
        void RenderActivationCard();
        void RenderTemplateSelector();
        void RenderCellPreview();
        void RenderCellAssignments();

        void ToggleStageDisplay(bool active);
        void CycleStageMonitor(int direction);
        void CaptureAndPushLANFrame(int w, int h, int quality);

        // Pantalla/LAN/puerto elegidos: viven en SettingsManager (Settings::
        // StageDisplaySettings) en vez de miembros efimeros aca, para que
        // ViewPanel pueda prender/apagar Stage (ver el punto "Stage" en
        // RenderLiveTransport) sin necesitar una instancia de esta clase.
        double     m_LANLastCaptureTime  = 0.0;

        // Encode JPEG en hilo dedicado — ver FrameEncodeWorker.h. Evita que
        // el encode bloquee el hilo de render/UI (el mismo que dibuja el
        // proyector) cada ~125ms mientras el monitor de control se sirve LAN.
        Core::FrameEncodeWorker m_EncodeWorker;
    };

} // namespace ProyecThor::UI
