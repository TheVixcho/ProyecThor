#pragma once
#include "media/player/VLCBasePlayer.h"
#include <imgui.h>

namespace ProyecThor::UI {

class AudioMeters {
public:
    AudioMeters() = default;

    void Update(Core::VLCBasePlayer* player, bool isLive, bool isPlaying, bool isMuted, float currentVolume);
    void Render(float w, float h);

    // Version compacta: barras verticales, pensada para dibujarse pegada al
    // borde izquierdo del visor de video (encima del propio ImDrawList del
    // video, con coordenadas de pantalla explicitas en vez del cursor
    // ImGui normal).
    void RenderVertical(ImDrawList* dl, ImVec2 pos0, float w, float h);

private:
    float m_VU_L     = 0.0f;
    float m_VU_R     = 0.0f;
    float m_VU_PeakL = 0.0f;
    float m_VU_PeakR = 0.0f;
};

}