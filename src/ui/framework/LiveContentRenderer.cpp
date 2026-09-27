#include "LiveContentRenderer.h"
#include "core/PresentationCore.h"
#include "media/player/VLCBasePlayer.h"
#include "core/settings/SettingsManager.h"
#include "core/settings/StageLayoutTemplates.h"
#include "ui/panels/capture/CapturePanel.h"
#include "ui/panels/lab/LabPanel.h"
#include "ui/panels/monitor/MonitorTheme.h"
#include "ui/panels/overlay/OverlayLayerRender.h"
#include "ui/framework/TextEffectsRenderer.h"
#include "ui/panels/TransitionPanel.h"
#include "ui/views/Audio.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <ctime>
#include <string>

namespace ProyecThor::UI {

namespace {
    namespace MT = MonitorTheme;

    inline void DrawScaledImage(ImDrawList* dl, ImTextureID tex, ImVec2 pMin, ImVec2 pMax, float scale, float alpha)
    {
        if (!tex || alpha <= 0.001f) return;
        float w = pMax.x - pMin.x;
        float h = pMax.y - pMin.y;
        ImVec2 center((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
        float sw = w * scale * 0.5f;
        float sh = h * scale * 0.5f;
        ImU32 tint = IM_COL32(255, 255, 255, (int)(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
        dl->AddImage(tex, ImVec2(center.x - sw, center.y - sh), ImVec2(center.x + sw, center.y + sh),
                     ImVec2(0, 0), ImVec2(1, 1), tint);
    }

    inline void DrawTexturedCircle(ImDrawList* dl, ImTextureID tex, ImVec2 center, float radius,
                                  ImVec2 rectMin, ImVec2 rectMax, ImU32 tint = 0xFFFFFFFF, int segments = 64)
    {
        if (radius <= 0.001f || !tex) return;
        float rectW = rectMax.x - rectMin.x;
        float rectH = rectMax.y - rectMin.y;
        if (rectW <= 0.0f || rectH <= 0.0f) return;

        dl->PushTextureID(tex);
        dl->PrimReserve(segments * 3, segments + 1);
        ImDrawIdx centerIdx = (ImDrawIdx)dl->_VtxCurrentIdx;

        ImVec2 centerUV = ImVec2((center.x - rectMin.x) / rectW, (center.y - rectMin.y) / rectH);
        dl->PrimWriteVtx(center, centerUV, tint);

        for (int i = 0; i < segments; i++) {
            float angle = (6.28318530718f * (float)i) / (float)segments;
            float vx = center.x + cosf(angle) * radius;
            float vy = center.y + sinf(angle) * radius;
            float u = (vx - rectMin.x) / rectW;
            float v = (vy - rectMin.y) / rectH;
            dl->PrimWriteVtx(ImVec2(vx, vy), ImVec2(u, v), tint);
        }

        for (int i = 0; i < segments; i++) {
            dl->PrimWriteIdx(centerIdx);
            dl->PrimWriteIdx((ImDrawIdx)(centerIdx + 1 + i));
            dl->PrimWriteIdx((ImDrawIdx)(centerIdx + 1 + ((i + 1) % segments)));
        }
        dl->PopTextureID();
    }
}

void RenderBackgroundWithTransition(ImDrawList* dl,
                                    void* activeTex, void* standbyTex,
                                    ImVec2 pMin, ImVec2 pMax,
                                    int transitionTypeInt, float progress,
                                    bool isTransitionActive)
{
    if (!activeTex && !standbyTex) return;

    if (!activeTex && standbyTex) {
        dl->AddImage((ImTextureID)(intptr_t)standbyTex, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
        return;
    }

    if (!isTransitionActive || !standbyTex) {
        if (activeTex) {
            dl->AddImage((ImTextureID)(intptr_t)activeTex, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
        }
        return;
    }

    TransitionType type = static_cast<TransitionType>(transitionTypeInt);
    float W = pMax.x - pMin.x;
    float H = pMax.y - pMin.y;
    float p = std::clamp(progress, 0.0f, 1.0f);

    ImTextureID texOut = (ImTextureID)(intptr_t)activeTex;
    ImTextureID texIn  = (ImTextureID)(intptr_t)standbyTex;

    switch (type)
    {
        case TransitionType::Iris:
        {
            ImVec2 center((pMin.x + pMax.x) * 0.5f, (pMin.y + pMax.y) * 0.5f);
            float maxRadius = sqrtf((W * 0.5f) * (W * 0.5f) + (H * 0.5f) * (H * 0.5f)) + 4.0f;
            float r = p * maxRadius;

            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));

            if (r > 1.0f)
            {
                if (r >= maxRadius - 2.0f) {
                    dl->AddImage(texIn, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
                } else {
                    DrawTexturedCircle(dl, texIn, center, r, pMin, pMax, 0xFFFFFFFF, 64);

                    float ringAlpha = std::clamp((1.0f - p * 0.4f), 0.0f, 1.0f);
                    ImU32 ringCol = IM_COL32(255, 230, 160, (int)(ringAlpha * 220.0f));
                    dl->AddCircle(center, r, ringCol, 64, 2.5f);

                    ImU32 glowCol = IM_COL32(255, 255, 255, (int)(ringAlpha * 100.0f));
                    dl->AddCircle(center, r + 1.5f, glowCol, 64, 1.5f);
                }
            }
            break;
        }

        case TransitionType::None:
        {
            if (p >= 0.5f)
                dl->AddImage(texIn, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            else
                dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            break;
        }

        case TransitionType::Fade:
        {
            float alphaOut = 1.0f - p;
            float alphaIn  = p;
            ImU32 tintOut = IM_COL32(255, 255, 255, (int)(alphaOut * 255.0f));
            ImU32 tintIn  = IM_COL32(255, 255, 255, (int)(alphaIn * 255.0f));
            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1), tintOut);
            dl->AddImage(texIn, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1), tintIn);
            break;
        }

        case TransitionType::SlideLeft:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, ImVec2(pMin.x - p * W, pMin.y), ImVec2(pMax.x - p * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x + (1.0f - p) * W, pMin.y), ImVec2(pMax.x + (1.0f - p) * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::SlideRight:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, ImVec2(pMin.x + p * W, pMin.y), ImVec2(pMax.x + p * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x - (1.0f - p) * W, pMin.y), ImVec2(pMax.x - (1.0f - p) * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::SlideUp:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, ImVec2(pMin.x, pMin.y - p * H), ImVec2(pMax.x, pMax.y - p * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x, pMin.y + (1.0f - p) * H), ImVec2(pMax.x, pMax.y + (1.0f - p) * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::SlideDown:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, ImVec2(pMin.x, pMin.y + p * H), ImVec2(pMax.x, pMax.y + p * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x, pMin.y - (1.0f - p) * H), ImVec2(pMax.x, pMax.y - (1.0f - p) * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::CoverLeft:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x + (1.0f - p) * W, pMin.y), ImVec2(pMax.x + (1.0f - p) * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::CoverRight:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x - (1.0f - p) * W, pMin.y), ImVec2(pMax.x - (1.0f - p) * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::CoverUp:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x, pMin.y + (1.0f - p) * H), ImVec2(pMax.x, pMax.y + (1.0f - p) * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::CoverDown:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texOut, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texIn,  ImVec2(pMin.x, pMin.y - (1.0f - p) * H), ImVec2(pMax.x, pMax.y - (1.0f - p) * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::UncoverLeft:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texIn,  pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texOut, ImVec2(pMin.x - p * W, pMin.y), ImVec2(pMax.x - p * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::UncoverRight:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texIn,  pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texOut, ImVec2(pMin.x + p * W, pMin.y), ImVec2(pMax.x + p * W, pMax.y), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::UncoverUp:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texIn,  pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texOut, ImVec2(pMin.x, pMin.y - p * H), ImVec2(pMax.x, pMax.y - p * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::UncoverDown:
        {
            dl->PushClipRect(pMin, pMax, true);
            dl->AddImage(texIn,  pMin, pMax, ImVec2(0, 0), ImVec2(1, 1));
            dl->AddImage(texOut, ImVec2(pMin.x, pMin.y + p * H), ImVec2(pMax.x, pMax.y + p * H), ImVec2(0, 0), ImVec2(1, 1));
            dl->PopClipRect();
            break;
        }

        case TransitionType::ZoomIn:
        {
            dl->PushClipRect(pMin, pMax, true);
            DrawScaledImage(dl, texOut, pMin, pMax, 1.0f + p * 0.4f, 1.0f - p);
            DrawScaledImage(dl, texIn,  pMin, pMax, 0.6f + p * 0.4f, p);
            dl->PopClipRect();
            break;
        }

        case TransitionType::ZoomOut:
        {
            dl->PushClipRect(pMin, pMax, true);
            DrawScaledImage(dl, texOut, pMin, pMax, 1.0f - p * 0.4f, 1.0f - p);
            DrawScaledImage(dl, texIn,  pMin, pMax, 1.4f - p * 0.4f, p);
            dl->PopClipRect();
            break;
        }
    }
}

void DrawPublicContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH, bool suppressLyrics, bool drawBorder)
{
    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    // ── Fondo de video / Estado Inactivo ──────────────────────────────────
    if (!state.isProjecting)
    {
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(MT::k_Bg3));

        const char* msg     = "Sin proyección activa";
        ImVec2      msgSize = ImGui::CalcTextSize(msg);
        dl->AddText(
            ImVec2(p0.x + (drawW - msgSize.x) * 0.5f,
                   p0.y + (drawH - msgSize.y) * 0.5f),
            ImGui::GetColorU32(MT::k_TextDim),
            msg);

        if (drawBorder)
            dl->AddRect(p0, p1, ImGui::GetColorU32(MT::k_BorderSubtle), 0.0f, 0, 1.0f);
        return;
    }

    if (core.ShouldShowLoadingScreen())
    {
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
        void* logoTex = core.GetLoadingLogoTexture();
        int   logoW   = core.GetLoadingLogoWidth();
        int   logoH   = core.GetLoadingLogoHeight();
        if (logoTex && logoW > 0 && logoH > 0)
        {
            float destX = p0.x, destY = p0.y;
            float destW = drawW, destH = drawH;
            float logoRatio   = (float)logoW / (float)logoH;
            float screenRatio = destW / destH;

            if (logoRatio > screenRatio + 0.001f) {
                destH = destW / logoRatio;
                destY = p0.y + (drawH - destH) * 0.5f;
            } else if (logoRatio < screenRatio - 0.001f) {
                destW = destH * logoRatio;
                destX = p0.x + (drawW - destW) * 0.5f;
            }

            dl->AddImage(logoTex, ImVec2(destX, destY), ImVec2(destX + destW, destY + destH),
                         ImVec2(0, 0), ImVec2(1, 1));
        }
        if (drawBorder)
            dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);
        return;
    }

    // Si está proyectando
    if (state.bgType == Core::PresentationState::BackgroundType::Video)
    {
        void* texID = core.GetProcessedBackgroundTexture((int)drawW, (int)drawH);
        if (!texID) texID = core.GetPreviewBackgroundTexture((int)drawW, (int)drawH);
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
        if (texID)
        {
            ImVec2 vp0 = p0, vp1 = p1;
            int vw = 0, vh = 0;
            core.GetBackgroundVideoSize(vw, vh);

            if (vw > 0 && vh > 0 && !core.GetStretchToFill())
            {
                float videoRatio  = (float)vw / (float)vh;
                float screenRatio = drawW / drawH;

                if (videoRatio > screenRatio + 0.001f)
                {
                    float fitH = drawW / videoRatio;
                    float offY = (drawH - fitH) * 0.5f;
                    vp0 = { p0.x, p0.y + offY };
                    vp1 = { p1.x, p0.y + offY + fitH };
                }
                else if (videoRatio < screenRatio - 0.001f)
                {
                    float fitW = drawH * videoRatio;
                    float offX = (drawW - fitW) * 0.5f;
                    vp0 = { p0.x + offX, p0.y };
                    vp1 = { p0.x + offX + fitW, p1.y };
                }
            }

            bool hasBars = ((vp1.x - vp0.x) < drawW - 0.5f) ||
                           ((vp1.y - vp0.y) < drawH - 0.5f);
            if (hasBars && core.GetFillBlurEnabled()) {
                void* fillTex = core.GetBackgroundFillTexture((int)drawW, (int)drawH);
                if (fillTex) {
                    float b = std::clamp(core.GetFillBlurBrightness(), 0.0f, 1.0f);
                    ImU32 fillTint = IM_COL32((int)(b * 255.0f), (int)(b * 255.0f), (int)(b * 255.0f), 255);
                    dl->AddImage(fillTex, p0, p1, ImVec2(0, 0), ImVec2(1, 1), fillTint);
                }
            }

            void* standbyTex = nullptr;
            bool isTransActive = false;
            float progress = 0.0f;
            int transType = core.GetBackgroundTransitionType();

            if (core.IsBackgroundSwapPending() && core.IsBackgroundStandbyReady())
            {
                standbyTex = core.GetStandbyBackgroundTexture();
                if (!standbyTex) standbyTex = core.GetPreviewStandbyBackgroundTexture((int)drawW, (int)drawH);
                progress = std::clamp(core.GetBackgroundBlendProgress(), 0.0f, 1.0f);
                isTransActive = (standbyTex != nullptr);
            }

            RenderBackgroundWithTransition(dl, texID, standbyTex, vp0, vp1, transType, progress, isTransActive);
        }
    }
    else if (state.bgType == Core::PresentationState::BackgroundType::SolidColor)
    {
        ImU32 finalCol = IM_COL32(
            (int)(state.bgColor[0] * 255.0f),
            (int)(state.bgColor[1] * 255.0f),
            (int)(state.bgColor[2] * 255.0f),
            255);
        dl->AddRectFilled(p0, p1, finalCol);
    }
    else if (state.bgType == Core::PresentationState::BackgroundType::Audio)
    {
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
        if (auto* audioPanel = core.GetAudioPanelRef())
            audioPanel->RenderLiveBackground(p0.x, p0.y, drawW, drawH);
    }
    else {
         // Fondo base si proyecta algo que no es video (como imágenes o color sólido)
         dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 255));
    }

    // ── Texto proyectado (Letras, y opcionalmente el Indice) ────────────────
    // Los margenes/tamano de texto estan definidos en unidades de
    // referencia sobre un lienzo de 1920px (ver DrawTextBlock en
    // UIManager.cpp, que es lo que realmente se dibuja en la pantalla al
    // publico: usa screenScale = anchoRealDelMonitor / 1920). Como drawW ya
    // representa el ancho COMPLETO del monitor real dentro del panel, la
    // conversion correcta de "unidades de 1920" a "pixeles de preview" es
    // simplemente drawW/1920.
    float scale = drawW / 1920.0f;
    bool  isSong = (core.PeekSelection().type == Core::ItemType::Song);

    auto DrawBox = [&](const std::string& text, const Core::TextBoxStyle& box, bool isLyricsBox)
    {
        if (text.empty()) return;

        float boxW = std::max(10.0f, box.sizeW * drawW);
        float boxH = std::max(10.0f, box.sizeH * drawH);
        float boxX = p0.x + box.posX * drawW - boxW * 0.5f;
        float boxY = p0.y + box.posY * drawH - boxH * 0.5f;

        if (box.bgMediaEnabled && !box.bgMediaPath.empty()) {
            unsigned int bgTex = core.GetBoxBgTexture(isLyricsBox, box.bgMediaPath);
            if (bgTex != 0) {
                ImU32 tint = IM_COL32(255, 255, 255,
                    (int)(std::clamp(box.bgMediaOpacity, 0.0f, 1.0f) * 255.0f));
                dl->AddImage((ImTextureID)(intptr_t)bgTex,
                    ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH),
                    ImVec2(0, 0), ImVec2(1, 1), tint);
            }
        }

        float fontSize = box.textSize * scale;

        ImFont* font = core.GetImGuiFont(box.fontName, fontSize);
        if (!font) font = ImGui::GetFont();

        if (box.autoScale)
        {
            while (fontSize > 4.0f)
            {
                ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, text.c_str());
                if (ts.y <= boxH) break;
                fontSize -= 1.0f;
            }
        }

        ImVec2 textBlock = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, text.c_str());

        float textX = boxX;
        if (box.hAlign == 1)
            textX += (boxW - textBlock.x) * 0.5f;
        else if (box.hAlign == 2)
            textX += (boxW - textBlock.x);

        float textY = boxY;
        if (box.vAlign == 1)
            textY += (boxH - textBlock.y) * 0.5f;
        else if (box.vAlign == 2)
            textY += (boxH - textBlock.y);

        dl->PushClipRect(p0, p1, true);

        ImU32 textCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(box.color[0], box.color[1], box.color[2], box.color[3]));

        if (isSong && box.hAlign == 1)
        {
            float lineH = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, "A").y;

            float startY = boxY;
            if (box.vAlign == 1)
                startY += (boxH - textBlock.y) * 0.5f;
            else if (box.vAlign == 2)
                startY += (boxH - textBlock.y);

            float  curY     = startY;
            size_t startPos = 0;
            size_t endPos   = text.find('\n');

            while (startPos != std::string::npos)
            {
                std::string line = text.substr(startPos, endPos - startPos);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                if (!line.empty())
                {
                    ImVec2 lSize =
                        font->CalcTextSizeA(fontSize, FLT_MAX, boxW, line.c_str());
                    float lx = boxX + (boxW - lSize.x) * 0.5f;

                    DrawStyledText(dl, font, fontSize, ImVec2(lx, curY), textCol,
                                   line.c_str(), 0.0f, scale, box.effects);
                }

                curY += lineH;
                if (endPos == std::string::npos) break;
                startPos = endPos + 1;
                endPos   = text.find('\n', startPos);
            }
        }
        else
        {
            DrawStyledText(dl, font, fontSize, ImVec2(textX, textY), textCol,
                           text.c_str(), boxW, scale, box.effects);
        }

        dl->PopClipRect();
    };

    if (!suppressLyrics && state.showText && !state.currentText.empty())
        DrawBox(state.currentText, state.lyricsBox, true);

    if (state.indexEnabled && !state.currentRef.empty())
        DrawBox(state.currentRef, state.indexBox, false);

    // ── Overlay (PNG transparente) ──────────────────────────────────────────
    // Capa APARTE de fondo/texto (ver PresentationCore::SetOverlayMedia) --
    // se dibuja encima de los dos, dejando ver lo que haya debajo gracias al
    // alpha real del PNG. Mismo orden que en la salida real (UIManager.cpp).
    if (void* overlayTex = core.GetOverlayTexture())
        dl->AddImage(overlayTex, p0, p1);

    // ── Capa 3D (Modelos y Recursos 3D en vivo) ─────────────────────────────
    if (core.IsLive3DModelActive()) {
        if (void* model3dTex = core.GetLive3DModelTexture())
            dl->AddImage(model3dTex, p0, p1);
    }

    // ── Capa de Laboratorio Matemático (Gráficas GeoGebra en vivo) ─────────
    if (core.IsLiveLabActive()) {
        if (LabPanel* lab = core.GetLabPanelRef())
            lab->RenderLiveProjection(dl, p0, p1, drawW, drawH);
    }

    // ── Reloj/contador en vivo sobre el overlay ─────────────────────────────
    // Cuadro-flag definido en el overlay activo (ver OverlayCanvasEditor,
    // capa Clock) -- se dibuja en vivo aca, nunca esta horneado en el PNG
    // del overlay (ver PresentationCore::SetOverlayClockLayer/
    // SetLiveOverlayClockText, publicado desde OClock::SyncTransmission()).
    if (core.HasOverlayClockLayer()) {
        std::string clockTxt = core.GetLiveOverlayClockText();
        if (!clockTxt.empty()) {
            OverlayLayer cl = core.GetOverlayClockLayer();
            ImFont* clockFont = core.GetImGuiFont(cl.fontName, cl.fontSize);
            if (!clockFont) clockFont = ImGui::GetFont();

            float clockScale = drawW / (float)std::max(1, core.GetOverlayClockCanvasW());
            float clockDispSize = std::max(4.0f, cl.fontSize * clockScale);
            ImVec2 clockBlockSz = clockFont->CalcTextSizeA(clockDispSize, FLT_MAX, FLT_MAX, clockTxt.c_str());
            ImVec2 clockCenter = ImVec2(p0.x + cl.posX * drawW, p0.y + cl.posY * drawH);
            ImVec2 clockTL = ImVec2(clockCenter.x - clockBlockSz.x * 0.5f, clockCenter.y - clockBlockSz.y * 0.5f);

            float clockColorOverride[4];
            bool hasOverride = core.HasLiveOverlayClockColorOverride();
            if (hasOverride) core.GetLiveOverlayClockColorOverride(clockColorOverride);

            DrawOverlayLayerStyledText(dl, clockFont, clockDispSize, clockTL, clockBlockSz, cl,
                                       clockTxt.c_str(), clockScale, hasOverride ? clockColorOverride : nullptr);
        }
    }

    // ── Captura (cámara/pantalla/ventana) ──────────────────────────────────
    // Antes esto NUNCA se dibujaba en el preview -- en la salida real
    // (UIManager.cpp) es un llamado aparte, directo sobre "ProjectorLive",
    // que este código compartido no replicaba. Mismo orden que ahí: encima
    // del fondo/overlay/texto. RenderOnProjector ya se auto-descarta si no
    // hay captura en vivo, así que es seguro llamarlo siempre.
    if (CapturePanel* cap = core.GetCapturePanelRef())
        cap->RenderOnProjector(dl, p0.x, p0.y, drawW, drawH);

    // ── Borde ──────────────────────────────────────────────────────────────
    if (drawBorder)
        dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);
}

bool GetLyricsScreenBounds(const Core::PresentationState& state, ImVec2 p0, float drawW, float drawH,
                           ImVec2& outBoxMin, ImVec2& outBoxMax,
                           ImVec2& outTextMin, ImVec2& outTextMax)
{
    if (!state.showText || state.currentText.empty())
        return false;

    auto& core = Core::PresentationCore::Get();
    float scale = drawW / 1920.0f;
    const auto& box = state.lyricsBox;

    float boxW = std::max(10.0f, box.sizeW * drawW);
    float boxH = std::max(10.0f, box.sizeH * drawH);
    float boxX = p0.x + box.posX * drawW - boxW * 0.5f;
    float boxY = p0.y + box.posY * drawH - boxH * 0.5f;

    outBoxMin = ImVec2(boxX, boxY);
    outBoxMax = ImVec2(boxX + boxW, boxY + boxH);

    float fontSize = box.textSize * scale;
    ImFont* font = core.GetImGuiFont(box.fontName, fontSize);
    if (!font) font = ImGui::GetFont();

    if (box.autoScale)
    {
        while (fontSize > 4.0f)
        {
            ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, state.currentText.c_str());
            if (ts.y <= boxH) break;
            fontSize -= 1.0f;
        }
    }

    ImVec2 textBlock = font->CalcTextSizeA(fontSize, FLT_MAX, boxW, state.currentText.c_str());

    float textX = boxX;
    if (box.hAlign == 1)
        textX += (boxW - textBlock.x) * 0.5f;
    else if (box.hAlign == 2)
        textX += (boxW - textBlock.x);

    float textY = boxY;
    if (box.vAlign == 1)
        textY += (boxH - textBlock.y) * 0.5f;
    else if (box.vAlign == 2)
        textY += (boxH - textBlock.y);

    outTextMin = ImVec2(textX, textY);
    outTextMax = ImVec2(textX + textBlock.x, textY + textBlock.y);
    return true;
}

void DrawStageContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1)
{
    auto& stageSettings =
        ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    float w = p1.x - p0.x;
    float h = p1.y - p0.y;
    if (w <= 0.0f || h <= 0.0f) return;

    if (stageSettings.mirrorPublicOutput)
    {
        DrawPublicContent(dl, p0, p1, w, h);
        return;
    }

    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    // Fondo oscuro sobrio sin reflejos para escenario
    dl->AddRectFilled(p0, p1, IM_COL32(10, 11, 14, 255));

    // ── 1. BARRA SUPERIOR: Reloj Digital + Cuenta Regresiva OClock + Estado ───
    float topBarH = std::clamp(h * 0.12f, 38.0f, 75.0f);
    ImVec2 topMin = p0;
    ImVec2 topMax = ImVec2(p1.x, p0.y + topBarH);
    dl->AddRectFilled(topMin, topMax, IM_COL32(18, 20, 26, 255));
    dl->AddLine(ImVec2(topMin.x, topMax.y), ImVec2(topMax.x, topMax.y),
                IM_COL32(40, 44, 56, 255), 1.5f);

    // Reloj digital actual
    std::time_t now = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &lt);

    float topFontSize = std::clamp(topBarH * 0.48f, 15.0f, 32.0f);
    ImFont* topFont   = ImGui::GetFont();
    ImVec2 timeSz     = topFont->CalcTextSizeA(topFontSize, FLT_MAX, FLT_MAX, timeBuf);
    float timeY       = topMin.y + (topBarH - timeSz.y) * 0.5f;
    dl->AddText(topFont, topFontSize, ImVec2(topMin.x + 18.0f, timeY),
                IM_COL32(230, 235, 245, 255), timeBuf);

    // Cuenta regresiva / Cronometro de OClock (si esta corriendo)
    std::string clockStr = core.GetLiveOverlayClockText();
    if (!clockStr.empty())
    {
        std::string timerLabel = "⏱ " + clockStr;
        ImVec2 timerSz = topFont->CalcTextSizeA(topFontSize, FLT_MAX, FLT_MAX, timerLabel.c_str());
        float timerX   = p0.x + (w - timerSz.x) * 0.5f;
        // Si el tiempo está terminando ("00:" o menos de un minuto), destacar en rojo/naranja
        bool isEnding = (clockStr.find("00:") == 0 || clockStr.find("0:") == 0);
        ImU32 timerCol = isEnding ? IM_COL32(255, 80, 80, 255) : IM_COL32(245, 185, 55, 255);
        dl->AddText(topFont, topFontSize, ImVec2(timerX, timeY), timerCol, timerLabel.c_str());
    }

    // Badge de estado de salida (derecha)
    const char* statusTxt = "○ EN ESPERA";
    ImU32 statusBgCol    = IM_COL32(45, 48, 58, 255);
    ImU32 statusTxtCol   = IM_COL32(180, 185, 195, 255);

    if (state.isProjecting)
    {
        statusTxt    = "● EN VIVO";
        statusBgCol  = IM_COL32(20, 95, 45, 255);
        statusTxtCol = IM_COL32(140, 255, 170, 255);
    }

    float badgeFontSz = std::clamp(topBarH * 0.36f, 13.0f, 22.0f);
    ImVec2 badgeTxtSz = topFont->CalcTextSizeA(badgeFontSz, FLT_MAX, FLT_MAX, statusTxt);
    float badgePadH   = 10.0f;
    float badgePadV   = 5.0f;
    float badgeW      = badgeTxtSz.x + badgePadH * 2.0f;
    float badgeH      = badgeTxtSz.y + badgePadV * 2.0f;
    float badgeX      = topMax.x - badgeW - 18.0f;
    float badgeY      = topMin.y + (topBarH - badgeH) * 0.5f;

    dl->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH),
                      statusBgCol, 4.0f);
    dl->AddText(topFont, badgeFontSz, ImVec2(badgeX + badgePadH, badgeY + badgePadV),
                statusTxtCol, statusTxt);

    // ── 2. BANNER DE AVISO / ALERTA DE ESCENARIO ──────────────────────────────
    std::string alertMsg = core.GetStageAlertMessage();
    if (alertMsg.empty() && (state.showQuickNote || state.showLanQuickNote)) {
        alertMsg = state.lanQuickNoteText;
    }

    float alertH = 0.0f;
    if (!alertMsg.empty())
    {
        alertH = std::clamp(h * 0.10f, 32.0f, 60.0f);
        ImVec2 aMin = ImVec2(p0.x, topMax.y);
        ImVec2 aMax = ImVec2(p1.x, topMax.y + alertH);
        dl->AddRectFilled(aMin, aMax, IM_COL32(180, 40, 40, 240));
        dl->AddLine(ImVec2(aMin.x, aMax.y), ImVec2(aMax.x, aMax.y),
                    IM_COL32(230, 80, 80, 255), 1.5f);

        std::string fullAlert = "⚠ AVISO A ESCENARIO: " + alertMsg;
        float aFontSz = std::clamp(alertH * 0.45f, 13.0f, 24.0f);
        ImVec2 aSz    = topFont->CalcTextSizeA(aFontSz, FLT_MAX, w - 40.0f, fullAlert.c_str());
        float aX      = p0.x + (w - aSz.x) * 0.5f;
        float aY      = aMin.y + (alertH - aSz.y) * 0.5f;
        dl->AddText(topFont, aFontSz, ImVec2(aX, aY), IM_COL32(255, 255, 255, 255), fullAlert.c_str());
    }

    // ── 3. CUERPO: TEXTO ACTUAL + PRÓXIMA LÍNEA (CONFIDENCE MONITOR) ─────────
    float contentY0 = topMax.y + alertH + 8.0f;
    float contentH  = p1.y - contentY0 - 8.0f;
    if (contentH <= 20.0f) return;

    // Proporción: 62% para el texto actual (en pantalla), 38% para el siguiente
    float marginX = 14.0f;
    float cardW   = w - marginX * 2.0f;
    float gapY    = 8.0f;

    bool hasNext = !state.nextText.empty();
    float currentCardH = hasNext ? (contentH - gapY) * 0.62f : contentH;
    float nextCardH    = hasNext ? (contentH - gapY) * 0.38f : 0.0f;

    // ── Card 1: Letra / Versículo en Pantalla (ACTUAL) ──
    {
        ImVec2 c0 = ImVec2(p0.x + marginX, contentY0);
        ImVec2 c1 = ImVec2(c0.x + cardW, c0.y + currentCardH);

        dl->AddRectFilled(c0, c1, IM_COL32(18, 20, 26, 255), 4.0f);
        dl->AddRect(c0, c1, IM_COL32(45, 50, 65, 255), 4.0f, 0, 1.0f);

        // Header pill "EN PANTALLA"
        const char* currentTag = "● EN PANTALLA";
        float tagFontSz = std::clamp(currentCardH * 0.08f, 11.0f, 16.0f);
        dl->AddText(topFont, tagFontSz, ImVec2(c0.x + 14.0f, c0.y + 10.0f),
                    IM_COL32(80, 160, 240, 255), currentTag);

        float textPadY = tagFontSz + 18.0f;
        float textPadX = 18.0f;
        float textAreaW = std::max(10.0f, cardW - textPadX * 2.0f);
        float textAreaH = std::max(10.0f, currentCardH - textPadY - 14.0f);

        std::string mainText = state.currentText;
        if (mainText.empty()) {
            mainText = "[Sin texto proyectado en vivo]";
        }

        // Auto-escala dinámica para máxima legibilidad
        float testSize = std::clamp(textAreaH * 0.28f, 16.0f, 96.0f);
        ImFont* mainFont = core.GetImGuiFont(core.GetActiveFontName(), testSize);
        if (!mainFont) mainFont = topFont;

        while (testSize > 14.0f)
        {
            ImVec2 ts = mainFont->CalcTextSizeA(testSize, FLT_MAX, textAreaW, mainText.c_str());
            if (ts.y <= textAreaH) break;
            testSize -= 2.0f;
        }

        ImVec2 finalTs = mainFont->CalcTextSizeA(testSize, FLT_MAX, textAreaW, mainText.c_str());
        float textX = c0.x + textPadX + (textAreaW - finalTs.x) * 0.5f;
        float textY = c0.y + textPadY + (textAreaH - finalTs.y) * 0.5f;

        ImU32 mainColor = state.currentText.empty()
            ? IM_COL32(110, 115, 130, 255)
            : IM_COL32(255, 255, 255, 255);

        dl->PushClipRect(c0, c1, true);
        dl->AddText(mainFont, testSize, ImVec2(textX, textY), mainColor,
                    mainText.c_str(), nullptr, textAreaW);
        dl->PopClipRect();
    }

    // ── Card 2: Próxima Línea (SIGUIENTE) ──
    if (hasNext && nextCardH > 15.0f)
    {
        ImVec2 n0 = ImVec2(p0.x + marginX, contentY0 + currentCardH + gapY);
        ImVec2 n1 = ImVec2(n0.x + cardW, n0.y + nextCardH);

        dl->AddRectFilled(n0, n1, IM_COL32(14, 15, 20, 255), 4.0f);
        dl->AddRect(n0, n1, IM_COL32(36, 40, 52, 255), 4.0f, 0, 1.0f);

        // Header pill "SIGUIENTE"
        const char* nextTag = "➡ SIGUIENTE";
        float tagFontSz = std::clamp(nextCardH * 0.12f, 11.0f, 16.0f);
        dl->AddText(topFont, tagFontSz, ImVec2(n0.x + 14.0f, n0.y + 8.0f),
                    IM_COL32(235, 175, 70, 255), nextTag);

        float textPadY = tagFontSz + 14.0f;
        float textPadX = 18.0f;
        float textAreaW = std::max(10.0f, cardW - textPadX * 2.0f);
        float textAreaH = std::max(10.0f, nextCardH - textPadY - 10.0f);

        float testSize = std::clamp(textAreaH * 0.35f, 14.0f, 54.0f);
        ImFont* nextFont = core.GetImGuiFont(core.GetActiveFontName(), testSize);
        if (!nextFont) nextFont = topFont;

        while (testSize > 12.0f)
        {
            ImVec2 ts = nextFont->CalcTextSizeA(testSize, FLT_MAX, textAreaW, state.nextText.c_str());
            if (ts.y <= textAreaH) break;
            testSize -= 2.0f;
        }

        ImVec2 finalTs = nextFont->CalcTextSizeA(testSize, FLT_MAX, textAreaW, state.nextText.c_str());
        float textX = n0.x + textPadX + (textAreaW - finalTs.x) * 0.5f;
        float textY = n0.y + textPadY + (textAreaH - finalTs.y) * 0.5f;

        dl->PushClipRect(n0, n1, true);
        dl->AddText(nextFont, testSize, ImVec2(textX, textY),
                    IM_COL32(229, 192, 123, 255), state.nextText.c_str(), nullptr, textAreaW);
        dl->PopClipRect();
    }
}

} // namespace ProyecThor::UI
