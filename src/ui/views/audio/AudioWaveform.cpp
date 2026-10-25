
#include "AudioWaveform.h"
#include <cmath>
#include <algorithm>

namespace ProyecThor::Audio {

static void HsvToRgbW(float h, float s, float v, float& r, float& g, float& b)
{
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
}

void WaveformRenderer::Draw(ImDrawList*               dl,
                             ImVec2                    origin,
                             ImVec2                    size,
                             const std::vector<float>& bars,
                             float                     hue,
                             float                     time,
                             bool                      playing)
{
    if (bars.empty() || size.x <= 0 || size.y <= 0) return;

    const int   N      = static_cast<int>(bars.size());
    const float gap    = 2.0f;
    const float barW   = std::max(2.0f, (size.x - gap * (N - 1)) / N);
    const float maxH   = size.y;

    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    for (int i = 0; i < N; i++)
    {
        float amp       = std::max(0.02f, bars[i]);
        float barHeight = amp * maxH;

        float bx  = origin.x + i * (barW + gap);
        float by0 = origin.y + (maxH - barHeight) * 0.5f;
        float by1 = by0 + barHeight;

        float brightness = playing ? (0.45f + amp * 0.55f) : 0.30f;
        float saturation = playing ? 0.70f : 0.40f;
        float r, g, b;
        HsvToRgbW(hue, saturation, brightness, r, g, b);
        float alpha = playing ? (0.65f + amp * 0.35f) : 0.35f;

        ImU32 col = IM_COL32(static_cast<int>(r * 255),
                              static_cast<int>(g * 255),
                              static_cast<int>(b * 255),
                              static_cast<int>(alpha * 255));

        dl->AddRectFilled(ImVec2(bx, by0), ImVec2(bx + barW, by1), col, 1.5f);
    }

    dl->PopClipRect();
}

void WaveformRenderer::DrawProjector(ImDrawList*               dl,
                                      ImVec2                    origin,
                                      ImVec2                    size,
                                      const std::vector<float>& bars,
                                      float                     hue,
                                      float                     time,
                                      bool                      playing)
{
    if (bars.empty() || size.x <= 0 || size.y <= 0) return;

    const int   N    = static_cast<int>(bars.size());
    const float gap  = 4.0f;
    const float barW = std::max(3.0f, (size.x - gap * (N - 1)) / N);
    const float maxH = size.y;

    for (int i = 0; i < N; i++)
    {
        float amp = std::max(0.03f, bars[i]);

        float pulse    = 1.0f + 0.08f * std::sin(time * 3.0f + i * 0.4f);
        float barHeight = std::min(amp * maxH * pulse, maxH);

        float bx  = origin.x + i * (barW + gap);
        float by0 = origin.y + (maxH - barHeight) * 0.5f;
        float by1 = by0 + barHeight;

        float brightness = 0.55f + amp * 0.45f;
        float r, g, b;
        HsvToRgbW(hue, 0.75f, brightness, r, g, b);

        ImU32 colTop = IM_COL32(static_cast<int>(r * 255),
                                  static_cast<int>(g * 255),
                                  static_cast<int>(b * 255), 230);
        ImU32 colBot = IM_COL32(static_cast<int>(r * 160),
                                  static_cast<int>(g * 160),
                                  static_cast<int>(b * 160), 140);

        dl->AddRectFilledMultiColor(
            ImVec2(bx, by0), ImVec2(bx + barW, by1),
            colTop, colTop, colBot, colBot);

        float tipH = std::min(4.0f, barHeight * 0.08f);
        if (tipH > 1.0f) {
            dl->AddRectFilled(
                ImVec2(bx, by0),
                ImVec2(bx + barW, by0 + tipH),
                IM_COL32(255, 255, 255, 180), 1.0f);
        }
    }
}

}
