#include "OverlayLayerRender.h"
#include <algorithm>

namespace ProyecThor::UI {

void DrawOverlayLayerStyledText(ImDrawList* dl, ImFont* font, float displaySize,
                                 ImVec2 tl, ImVec2 blockSz, const OverlayLayer& layer,
                                 const char* text, float scale,
                                 const float* colorOverrideRGBA) {
    ImVec2 br = ImVec2(tl.x + blockSz.x, tl.y + blockSz.y);
    float  op = std::clamp(layer.opacity, 0.0f, 1.0f);
    if (op <= 0.001f) return; // invisible, no hace falta dibujar nada

    if (layer.bgEnabled) {
        ImU32 bgc = ImGui::ColorConvertFloat4ToU32(
            ImVec4(layer.bgColor[0], layer.bgColor[1], layer.bgColor[2], layer.bgColor[3] * op));
        float padX = layer.bgPaddingX * scale, padY = layer.bgPaddingY * scale;
        dl->AddRectFilled(ImVec2(tl.x - padX, tl.y - padY), ImVec2(br.x + padX, br.y + padY),
                          bgc, layer.bgRounding * scale);
    }
    if (layer.shadowEnabled) {
        ImU32 shc = ImGui::ColorConvertFloat4ToU32(ImVec4(
            layer.shadowColor[0], layer.shadowColor[1], layer.shadowColor[2], layer.shadowColor[3] * op));
        ImVec2 so = ImVec2(layer.shadowOffsetX * scale, layer.shadowOffsetY * scale);
        dl->AddText(font, displaySize, ImVec2(tl.x + so.x, tl.y + so.y), shc, text);
    }
    if (layer.outlineEnabled) {
        ImU32 oc = ImGui::ColorConvertFloat4ToU32(ImVec4(
            layer.outlineColor[0], layer.outlineColor[1], layer.outlineColor[2], layer.outlineColor[3] * op));
        float ow = std::max(0.5f, layer.outlineWidth * scale);
        static const ImVec2 kDirs[8] = {
            {-1,-1},{0,-1},{1,-1}, {-1,0},{1,0}, {-1,1},{0,1},{1,1}
        };
        for (const auto& d : kDirs)
            dl->AddText(font, displaySize, ImVec2(tl.x + d.x * ow, tl.y + d.y * ow), oc, text);
    }

    ImU32 col = colorOverrideRGBA
        ? ImGui::ColorConvertFloat4ToU32(ImVec4(colorOverrideRGBA[0], colorOverrideRGBA[1], colorOverrideRGBA[2], colorOverrideRGBA[3] * op))
        : ImGui::ColorConvertFloat4ToU32(ImVec4(layer.color[0], layer.color[1], layer.color[2], layer.color[3] * op));
    dl->AddText(font, displaySize, tl, col, text);
}

} // namespace ProyecThor::UI
