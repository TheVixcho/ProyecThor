#include "MonitorUIHelpers.h"
#include "MonitorDesign.h"
#include "DesignSystem.h"
#include <imgui_internal.h>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace ProyecThor::UI::Components {

using namespace Design;

std::string FormatTime(int64_t ms)
{
    if (ms < 0) ms = 0;
    int total   = static_cast<int>(ms / 1000);
    int hours   = total / 3600;
    int minutes = (total % 3600) / 60;
    int seconds = total % 60;
    std::ostringstream oss;
    if (hours > 0)
        oss << std::setfill('0') << std::setw(2) << hours   << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
    else
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

void DrawStatusDot(ImDrawList* dl, ImVec2 center, float r, ImVec4 col, bool active)
{
    if (active)
    {
        float t     = static_cast<float>(ImGui::GetTime());
        float pulse = 0.30f + 0.18f * std::abs(std::sin(t * 2.4f));
        dl->AddCircleFilled(center, r * 2.8f,
            ImGui::ColorConvertFloat4ToU32({ col.x, col.y, col.z, pulse * 0.4f }));
        dl->AddCircleFilled(center, r * 1.8f,
            ImGui::ColorConvertFloat4ToU32({ col.x, col.y, col.z, pulse * 0.65f }));
        dl->AddCircleFilled(center, r,
            ImGui::ColorConvertFloat4ToU32(col));
        dl->AddCircleFilled({ center.x - r * 0.22f, center.y - r * 0.22f },
            r * 0.32f, IM_COL32(255, 255, 255, 180));
    }
    else
    {
        ImVec4 dimCol = { col.x * 0.12f, col.y * 0.12f, col.z * 0.12f, 1.0f };
        dl->AddCircleFilled(center, r, ImGui::ColorConvertFloat4ToU32(dimCol));
        dl->AddCircle(center, r,
            ImGui::ColorConvertFloat4ToU32({ col.x * 0.3f, col.y * 0.3f, col.z * 0.3f, 0.6f }),
            0, 1.0f);
    }
}

void DrawAccentLine(float width, ImVec4 color, float thickness)
{
    ImVec2     p  = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(
        p, { p.x + width, p.y + thickness },
        IM_COL32(0, 0, 0, 0),
        ImGui::ColorConvertFloat4ToU32({ color.x, color.y, color.z, 0.80f }),
        ImGui::ColorConvertFloat4ToU32({ color.x, color.y, color.z, 0.80f }),
        IM_COL32(0, 0, 0, 0));
    ImGui::Dummy({ width, thickness + 3.0f });
}

bool BMButton(const char* label, ImVec2 size, ImVec4 base, ImVec4 hov, ImVec4 act, ImVec4 textCol, float rounding)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,   rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, k_BorderOuter);
    if (textCol.w >= 0.0f) ImGui::PushStyleColor(ImGuiCol_Text, textCol);

    ImVec2 p0      = ImGui::GetCursorScreenPos();
    bool   pressed = ImGui::Button(label, size);
    ImVec2 p1      = { p0.x + size.x, p0.y + size.y };

    if (!ImGui::IsItemActive())
    {
        ImGui::GetWindowDrawList()->AddLine(
            { p0.x + rounding * 0.6f, p0.y + 1.0f },
            { p1.x - rounding * 0.6f, p0.y + 1.0f },
            ImGui::ColorConvertFloat4ToU32(k_BorderInner), 1.0f);
    }

    if (textCol.w >= 0.0f) ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    return pressed;
}

bool BMSlider(const char* id, float* val, float lo, float hi, const char* /*fmt*/, ImVec4 frameBg, ImVec4 grab, ImVec4 grabAct, float width)
{
    // Antes: ImGui::SliderFloat con estilos pisados — quedaba una barra
    // gruesa rellena a todo lo alto del frame (look "grueso" que se pidio
    // reemplazar por uno mas moderno). Ahora rutea a DS::ModernSlider
    // (track fino + thumb circular animado); frameBg/grab ya no se usan
    // como fondo de frame sino como track/acento para no romper la firma en
    // los 3 call sites existentes (ViewPanel PROGRAM/PREVIEW/volumen y el
    // scrub de Monitor Preview). grabAct queda sin uso: el feedback de
    // "activo" ahora lo da la animacion de crecimiento del thumb.
    (void)grabAct;
    ImU32 trackCol  = ImGui::ColorConvertFloat4ToU32(frameBg);
    ImU32 accentCol = ImGui::ColorConvertFloat4ToU32(grab);
    return DS::ModernSlider(id, val, lo, hi, width, accentCol, trackCol);
}

void DrawTimeRow(float innerW, float padLeft, int64_t curMs, int64_t lenMs)
{
    std::string full = FormatTime(curMs) + " / " + FormatTime(lenMs);
    ImVec2 tsz  = ImGui::CalcTextSize(full.c_str());
    float  xRight = padLeft + innerW - tsz.x;
    ImGui::SetCursorPosX(xRight);
    ImGui::PushStyleColor(ImGuiCol_Text, k_TextDim);
    ImGui::TextUnformatted(full.c_str());
    ImGui::PopStyleColor();
}

void DrawVideoFrame(Core::VLCBasePlayer* player, float w, float h, const char* placeholder,
                    const char* badgeLabel, ImVec4 badgeAccent, bool pulseBorder,
                    bool showBadge, ProyecThor::Shaders::PostProcessorFSR* fsr)
{
    void* texID = player ? player->GetTextureID() : nullptr;

    if (texID)
    {
        int vw = 0, vh = 0;
        player->GetVideoSize(vw, vh);
        float ratio = (vw > 0 && vh > 0)
            ? static_cast<float>(vw) / static_cast<float>(vh)
            : k_MonitorRatio;

        float imgW = w - 4.0f;
        float imgH = imgW / ratio;
        if (imgH > h - 4.0f) { imgH = h - 4.0f; imgW = imgH * ratio; }
        if (imgW > w - 4.0f) { imgW = w - 4.0f; imgH = imgW / ratio; }

        ImTextureID finalTex = reinterpret_cast<ImTextureID>(texID);

        // FSR: solo tiene sentido reescalando HACIA ARRIBA (fuente mas chica
        // que el destino, ej. Preview en pantalla completa) -- Process() ya
        // hace ese chequeo el mismo, pero Init/Resize al tamaño de destino
        // exacto (imgW x imgH, YA con la relacion de aspecto correcta,
        // calculada arriba) es responsabilidad de este call site.
        if (fsr && fsr->IsEnabled() && vw > 0 && vh > 0)
        {
            int outW = std::max(1, static_cast<int>(imgW));
            int outH = std::max(1, static_cast<int>(imgH));
            if (vw < outW || vh < outH)
            {
                if (!fsr->IsInitialized() || fsr->GetOutputW() != outW || fsr->GetOutputH() != outH)
                    fsr->Init(outW, outH);

                GLuint upscaled = fsr->Process(static_cast<GLuint>(reinterpret_cast<uintptr_t>(texID)), vw, vh);
                if (upscaled != 0)
                    finalTex = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(upscaled));
            }
        }

        ImGui::SetCursorPosX((w - imgW) * 0.5f);
        ImGui::SetCursorPosY((h - imgH) * 0.5f);
        ImGui::Image(finalTex, { imgW, imgH });
    }
    else
    {
        ImVec2 ts = ImGui::CalcTextSize(placeholder);
        ImGui::SetCursorPosX((w - ts.x) * 0.5f);
        ImGui::SetCursorPosY((h - ts.y) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, k_TextDim);
        ImGui::TextUnformatted(placeholder);
        ImGui::PopStyleColor();
    }

    ImVec2      winPos = ImGui::GetWindowPos();
    ImDrawList* dl     = ImGui::GetWindowDrawList();

    if (showBadge)
    {
        const char* label   = badgeLabel;
        ImVec2      labelSz = ImGui::CalcTextSize(label);
        float bx = winPos.x + 8.0f;
        float by = winPos.y + 8.0f;
        float bw = labelSz.x + 12.0f;
        float bh = labelSz.y + 6.0f;

        ImVec4 badgeBg = { badgeAccent.x * 0.15f, badgeAccent.y * 0.15f, badgeAccent.z * 0.15f, 0.92f };
        dl->AddRectFilled({ bx, by }, { bx + bw, by + bh },
            ImGui::ColorConvertFloat4ToU32(badgeBg), 4.0f);
        dl->AddRect({ bx, by }, { bx + bw, by + bh },
            ImGui::ColorConvertFloat4ToU32({ badgeAccent.x, badgeAccent.y, badgeAccent.z, 0.70f }),
            4.0f, 0, 1.0f);
        dl->AddText({ bx + 6.0f, by + 3.0f },
            ImGui::ColorConvertFloat4ToU32(badgeAccent), label);
    }

    if (pulseBorder)
    {
        float t     = static_cast<float>(ImGui::GetTime());
        float alpha = 0.40f + 0.28f * std::abs(std::sin(t * 2.6f));
        dl->AddRect(
            { winPos.x + 2.0f, winPos.y + 2.0f },
            { winPos.x + w - 2.0f, winPos.y + h - 2.0f },
            ImGui::ColorConvertFloat4ToU32({ 1.0f, 0.22f, 0.26f, alpha }),
            k_RLg, 0, 2.0f);
        ImVec2 dotCenter = { winPos.x + w - 14.0f, winPos.y + 14.0f };
        DrawStatusDot(dl, dotCenter, 4.5f, k_LiveAccent, true);
    }
}

int RenderTransportRow(const TransportConfig& cfg)
{
    const float bPlay  = std::max(60.0f, cfg.availW * 0.28f);
    const float bSkip  = std::max(42.0f, cfg.availW * 0.17f);
    const float bStop  = std::max(40.0f, cfg.availW * 0.16f);
    const float bHome  = cfg.showHome ? std::max(42.0f, cfg.availW * 0.15f) : 0.0f;
    const float hGap   = cfg.showHome ? cfg.gap : 0.0f;
    const float usedW  = bHome + hGap + bSkip + cfg.gap + bPlay + cfg.gap + bSkip + cfg.gap + bStop;
    const float startX = (cfg.availW - usedW) * 0.5f;

    float curX = std::max(0.0f, startX);
    ImGui::SetCursorPosX(curX);

    int action = 0;

    if (cfg.showHome) {
        if (BMButton(cfg.homeId, { bHome, cfg.transportH }, cfg.btnBase, cfg.btnHov, cfg.btnAct))
            action = 1;
        ImGui::SameLine(0, cfg.gap);
    }

    if (BMButton(cfg.skipBkId, { bSkip, cfg.transportH }, cfg.btnBase, cfg.btnHov, cfg.btnAct))
        action = 2;
    ImGui::SameLine(0, cfg.gap);

    if (cfg.isPlaying) {
        if (BMButton(cfg.pauseId, { bPlay, cfg.transportH }, cfg.btnAct, cfg.btnHov, cfg.btnBase, cfg.accentText))
            action = 3;
    } else {
        if (BMButton(cfg.playId, { bPlay, cfg.transportH }, cfg.playBase, cfg.playHov, cfg.playAct, cfg.playText))
            action = 3;
    }
    ImGui::SameLine(0, cfg.gap);

    if (BMButton(cfg.skipFwId, { bSkip, cfg.transportH }, cfg.btnBase, cfg.btnHov, cfg.btnAct))
        action = 4;
    ImGui::SameLine(0, cfg.gap);

    if (BMButton(cfg.stopId, { bStop, cfg.transportH }, cfg.btnBase, cfg.btnHov, cfg.btnAct))
        action = 5;

    return action;
}

bool RenderVolumeRow(const VolumeConfig& cfg)
{
    bool changed = false;
    const float muteW   = 58.0f;
    const float pctW    = 44.0f;
    const float sliderW = cfg.availW - muteW - 8.0f - pctW - 8.0f;

    ImGui::SetCursorPosX(0.0f);

    ImVec4 muteBase = *cfg.muted ? k_AmberBtn    : cfg.btnBase;
    ImVec4 muteHov  = *cfg.muted ? k_AmberBtnHov : cfg.btnHov;
    ImVec4 muteAct  = *cfg.muted ? k_AmberBtnAct : cfg.btnAct;
    ImVec4 muteTxt  = *cfg.muted ? k_AmberAccent : k_TextSecondary;

    if (BMButton(cfg.muteLabel, { muteW, cfg.volumeH }, muteBase, muteHov, muteAct, muteTxt, 4.0f))
    {
        *cfg.muted = !(*cfg.muted);
        changed = true;
    }
    ImGui::SameLine(0, 8.0f);

    // 1. Detectar zona de peligro ANTES de dibujar el slider (volumen > 100% y no muteado)
    // Usamos 1.001f en vez de 1.0f para evitar bugs de precisión de los floats al estar en exactamente 100%
    bool isDanger = (*cfg.volume > 1.001f) && !(*cfg.muted);

    // Si es peligro, forzamos rojo. Si no, usamos los colores normales que entraron por 'cfg'
    ImVec4 vGrab  = *cfg.muted ? ImVec4{ 0.22f, 0.18f, 0.10f, 0.50f } : (isDanger ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : cfg.grab);
    ImVec4 vGrabA = *cfg.muted ? ImVec4{ 0.30f, 0.24f, 0.14f, 0.60f } : (isDanger ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : cfg.grabAct);
    ImVec4 sBg    = isDanger ? ImVec4(0.4f, 0.1f, 0.1f, 1.0f) : cfg.sliderBg;

    // Aseguramos que el límite del slider sea 2.0f
    if (BMSlider(cfg.sliderId, cfg.volume, 0.0f, 2.0f, "", sBg, vGrab, vGrabA, sliderW))
        changed = true;

    ImGui::SameLine(0, 8.0f);
    
    // 2. RE-EVALUAR por si el usuario movió el slider y cruzó el umbral en este mismo frame
    isDanger = (*cfg.volume > 1.001f) && !(*cfg.muted);

    char buf[16];
    snprintf(buf, sizeof(buf), "%3.0f%%", (*cfg.volume) * 100.0f);
    
    // 3. Pintar el TEXTO del porcentaje de ROJO si estamos en peligro, sino usar el gris normal
    ImVec4 textCol = isDanger ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : k_TextDim;
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    ImGui::TextUnformatted(buf);
    ImGui::PopStyleColor();

    return changed;
}

} // namespace ProyecThor::UI::Components