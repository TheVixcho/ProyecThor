#include "LiveContentRenderer.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/settings/SettingsManager.h"
#include "backend/settings/StageLayoutTemplates.h"
#include "frontend/panels/capture/CapturePanel.h"
#include "frontend/panels/lab/LabPanel.h"
#include "frontend/panels/monitor/MonitorTheme.h"
#include "frontend/panels/overlay/OverlayLayerRender.h"
#include "frontend/ui/TextEffectsRenderer.h"
#include "frontend/panels/TransitionPanel.h"
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

void DrawPublicContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH)
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

        dl->AddRect(p0, p1, ImGui::GetColorU32(MT::k_BorderSubtle), 0.0f, 0, 1.0f);
        return;
    }

    // Si está proyectando
    if (state.bgType == Core::PresentationState::BackgroundType::Video)
    {
        void* texID = core.GetPreviewBackgroundTexture((int)drawW, (int)drawH);
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

            void* standbyTex = nullptr;
            bool isTransActive = false;
            float progress = 0.0f;
            int transType = core.GetBackgroundTransitionType();

            if (core.IsBackgroundSwapPending() && core.IsBackgroundStandbyReady())
            {
                standbyTex = core.GetPreviewStandbyBackgroundTexture((int)drawW, (int)drawH);
                if (!standbyTex) standbyTex = core.GetStandbyBackgroundTexture();
                progress = std::clamp(core.GetBackgroundBlendProgress(), 0.0f, 1.0f);
                isTransActive = (standbyTex != nullptr);
            }

            RenderBackgroundWithTransition(dl, texID, standbyTex, vp0, vp1, transType, progress, isTransActive);
        }
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

    if (state.showText && !state.currentText.empty())
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
    dl->AddRect(p0, p1, IM_COL32(50, 55, 80, 180), 0.0f, 0, 1.0f);
}

void DrawStageContent(ImDrawList* dl, ImVec2 p0, ImVec2 p1)
{
    auto& stageSettings =
        ProyecThor::Settings::SettingsManager::Get().GetSettings().stageDisplay;

    if (stageSettings.mirrorPublicOutput)
    {
        DrawPublicContent(dl, p0, p1, p1.x - p0.x, p1.y - p0.y);
        return;
    }

    dl->AddRectFilled(p0, p1, IM_COL32(8, 8, 10, 255));

    auto& core  = ProyecThor::Core::PresentationCore::Get();
    auto  state = core.GetState();

    int tmplIdx = std::clamp(stageSettings.layoutTemplateIndex, 0,
                              ProyecThor::Settings::kStageLayoutTemplateCount - 1);
    const auto& stageTmpl = ProyecThor::Settings::kStageLayoutTemplates[tmplIdx];

    float w = p1.x - p0.x;
    float h = p1.y - p0.y;

    for (int i = 0; i < stageTmpl.cellCount; i++)
    {
        const float* r = stageTmpl.rect[i];
        float cx0 = p0.x + r[0] * w;
        float cy0 = p0.y + r[1] * h;
        float cw  = r[2] * w;
        float ch  = r[3] * h;
        const float pad = 12.0f;

        dl->AddRect(
            ImVec2(cx0 + pad, cy0 + pad), ImVec2(cx0 + cw - pad, cy0 + ch - pad),
            IM_COL32(255, 255, 255, 25), 8.0f);

        auto widget = static_cast<ProyecThor::Settings::StageWidgetType>(
            std::clamp(stageSettings.cellWidget[i], 0, 3));

        std::string cellText;
        ImU32 cellColor = IM_COL32(235, 235, 240, 255);
        float fontFrac  = 0.16f;
        ImFont* cellFont = ImGui::GetFont();

        switch (widget) {
            case ProyecThor::Settings::StageWidgetType::Clock: {
                std::time_t now = std::time(nullptr);
                std::tm lt{};
#ifdef _WIN32
                localtime_s(&lt, &now);
#else
                localtime_r(&now, &lt);
#endif
                char buf[16];
                std::strftime(buf, sizeof(buf), "%H:%M:%S", &lt);
                cellText = buf;
                fontFrac = 0.24f;
                break;
            }
            case ProyecThor::Settings::StageWidgetType::LiveText: {
                cellText = state.currentText;
                ImFont* activeFont = core.GetImGuiFont(core.GetActiveFontName(), ch * fontFrac);
                if (activeFont) cellFont = activeFont;
                break;
            }
            case ProyecThor::Settings::StageWidgetType::NextLine: {
                cellText = state.nextText;
                cellColor = IM_COL32(170, 175, 190, 255);
                fontFrac  = 0.12f;
                ImFont* activeFont = core.GetImGuiFont(core.GetActiveFontName(), ch * fontFrac);
                if (activeFont) cellFont = activeFont;
                break;
            }
            default:
                break;
        }

        if (!cellText.empty()) {
            float wrapW    = std::max(10.0f, cw - pad * 4.0f);
            float fontSize = std::clamp(ch * fontFrac, 14.0f, 140.0f);

            ImVec2 ts = cellFont->CalcTextSizeA(fontSize, FLT_MAX, wrapW, cellText.c_str());
            ImVec2 pos = ImVec2(cx0 + (cw - ts.x) * 0.5f, cy0 + (ch - ts.y) * 0.5f);

            dl->PushClipRect(ImVec2(cx0, cy0), ImVec2(cx0 + cw, cy0 + ch), true);
            dl->AddText(cellFont, fontSize, pos, cellColor,
                        cellText.c_str(), nullptr, wrapW);
            dl->PopClipRect();
        }
    }
}

} // namespace ProyecThor::UI
