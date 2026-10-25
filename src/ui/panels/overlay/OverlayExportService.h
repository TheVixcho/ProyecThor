#pragma once
#include <imgui.h>
#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayExportService — rasteriza un ImDrawList a un PNG CON transparencia
//  (a diferencia de un fondo/tema, un Overlay se proyecta ENCIMA de lo que
//  ya este en pantalla, por eso el canal alpha se preserva tal cual en vez de
//  forzarse a opaco), escalado a la resolucion de exportacion via
//  ImDrawData::FramebufferScale — el mismo mecanismo que usa ImGui para
//  pantallas HiDPI/Retina.
//
//  El draw list a exportar lo arma el LLAMADOR (ver OverlayCanvasEditor::
//  DrawLayersForExport) dibujando SOLO el contenido real de las capas, en un
//  ImDrawList propio e independiente del que se usa para el canvas en vivo
//  (que además tiene el cuadriculado "sin fondo" y el chrome de edicion) --
//  asi no hace falta ningun recorte/skip de comandos: lo que se pide
//  exportar es exactamente lo que se exporta, sin ambiguedad.
//
//  Uso: RequestCapture(...) se llama mientras se construye la UI (ej. al
//  apretar "Guardar"); ProcessPending() debe llamarse una vez por frame desde
//  main.cpp, justo despues de ImGui::Render() y antes de
//  ImGui_ImplOpenGL3_RenderDrawData(), para que el ImDrawList siga siendo
//  valido (comparte ImDrawListSharedData con el frame actual).
// ─────────────────────────────────────────────────────────────────────────────
class OverlayExportService {
public:
    static OverlayExportService& Get();

    // exportDrawList: ImDrawList armado por el llamador este mismo frame
    // (ver DrawLayersForExport), en coordenadas de PANTALLA dentro del
    // rectangulo [canvasScreenPos, canvasScreenPos+canvasScreenSize]. Se
    // mantiene vivo (shared_ptr) hasta que ProcessPending() lo consuma.
    void RequestCapture(std::shared_ptr<ImDrawList> exportDrawList,
                        ImVec2 canvasScreenPos, ImVec2 canvasScreenSize,
                        const std::string& outPngPath,
                        int exportW, int exportH,
                        std::function<void(bool)> onDone);

    void ProcessPending();

private:
    OverlayExportService() = default;

    struct PendingCapture {
        std::shared_ptr<ImDrawList> exportDrawList;
        ImVec2                    screenPos;
        ImVec2                    screenSize;
        std::string               outPngPath;
        int                       exportW, exportH;
        std::function<void(bool)> onDone;
    };

    std::vector<PendingCapture> m_Pending;

    bool CaptureOne(const PendingCapture& req);
};

} // namespace ProyecThor::UI
