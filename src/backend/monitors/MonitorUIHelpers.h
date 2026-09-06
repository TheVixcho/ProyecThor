#pragma once
#include <imgui.h>
#include <string>
#include <cstdint>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"

namespace ProyecThor::UI::Components {

    struct TransportConfig {
        float  availW, transportH, gap;
        ImVec4 btnBase, btnHov, btnAct;
        ImVec4 playBase, playHov, playAct, playText, accentText;
        bool   showHome, isPlaying;
        const char *playId, *pauseId, *homeId, *skipBkId, *skipFwId, *stopId;
    };

    struct VolumeConfig {
        float  availW, volumeH, pad;
        const char* muteLabel;
        float* volume;
        bool*  muted;
        ImVec4 sliderBg, grab, grabAct;
        ImVec4 btnBase, btnHov, btnAct;
        const char* sliderId;
    };

    std::string FormatTime(int64_t ms);
    void DrawStatusDot(ImDrawList* dl, ImVec2 center, float r, ImVec4 col, bool active);
    void DrawAccentLine(float width, ImVec4 color, float thickness = 1.5f);
    bool BMButton(const char* label, ImVec2 size, ImVec4 base, ImVec4 hov, ImVec4 act, ImVec4 textCol = { -1, -1, -1, -1 }, float rounding = 6.0f);
    bool BMSlider(const char* id, float* val, float lo, float hi, const char* fmt, ImVec4 frameBg, ImVec4 grab, ImVec4 grabAct, float width = -1.0f);
    void DrawTimeRow(float innerW, float padLeft, int64_t curMs, int64_t lenMs);
    // showBadge=false saca el recuadro con badgeLabel (ej. "PVW") -- pedido
    // explicito para el Preview a pantalla completa, donde ya queda claro
    // por el propio toolbar que es preview y el badge sobra.
    // fsr: si no es null Y esta habilitado (ver PostProcessorFSR::
    // IsEnabled), se lo reescala con FSR (EASU+RCAS) al tamaño final de
    // pantalla ANTES de dibujarlo -- solo tiene sentido cuando el video
    // fuente es mas chico que el area de destino (ej. Preview en pantalla
    // completa). nullptr (default) = blit directo, sin cambios de
    // comportamiento en los call sites existentes.
    void DrawVideoFrame(Core::VLCBasePlayer* player, float w, float h, const char* placeholder,
                        const char* badgeLabel, ImVec4 badgeAccent, bool pulseBorder = false,
                        bool showBadge = true, ProyecThor::Shaders::PostProcessorFSR* fsr = nullptr);
    int  RenderTransportRow(const TransportConfig& cfg);
    bool RenderVolumeRow(const VolumeConfig& cfg);

} // namespace ProyecThor::UI::Components