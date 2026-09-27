#pragma once

#include <imgui.h>
#include <vector>

namespace ProyecThor::Audio {

struct WaveformRenderer
{
    static void Draw(ImDrawList*               dl,
                     ImVec2                    origin,
                     ImVec2                    size,
                     const std::vector<float>& bars,
                     float                     hue,
                     float                     time,
                     bool                      playing);

    static void DrawProjector(ImDrawList*               dl,
                               ImVec2                    origin,
                               ImVec2                    size,
                               const std::vector<float>& bars,
                               float                     hue,
                               float                     time,
                               bool                      playing);
};

}
