#include <GL/glew.h>
#include "BroadcastPanel.h"
#include "backend/settings/SettingsManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ProyecThor::UI {

using ProyecThor::Settings::SettingsManager;

// Progreso animado (0..1) de hover por-item -- mismo patron que el resto de
// la pasada de modernizacion de Ajustes (ver CategoryTheme.cpp/AnimT):
// ImGuiStorage + lerp con DeltaTime, sin necesitar un campo de estado
// dedicado por boton.
static float BroadcastAnimT(ImGuiID id, ImU32 salt, bool target, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(id ^ salt, target ? 1.0f : 0.0f);
    float dst = target ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// Sombra suave apilando rectangulos semitransparentes -- mismo truco que
// StreamingPanel::DrawSoftShadow / SettingsPanel::DrawFloatingIslandShadow,
// reescrito acá liviano para no crear una dependencia cruzada entre paneles
// por un helper tan chico.
static void BroadcastSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
        float alpha = 26.0f - (i * 4.0f);
        dl->AddRectFilled({ p0.x - i, p0.y - i + 3.0f }, { p1.x + i, p1.y + i + 3.0f },
                          IM_COL32(0, 0, 0, (int)alpha), rounding + i);
    }
}

BroadcastPanel::~BroadcastPanel() {
    m_Encoder.Stop();
}

void BroadcastPanel::RenderCaptureSection() {
    m_Capture.RenderContent();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const auto& theme = SettingsManager::Get().GetSettings().theme;
    ImVec4 success(theme.success[0], theme.success[1], theme.success[2], 1.0f);

    bool live = m_Capture.IsLive();
    bool on   = m_ShowInLayer;

    ImGui::PushID("##showInLayer");
    ImGui::BeginDisabled(!live);

    ImVec2 btnSize(230.0f, 40.0f);
    ImGui::InvisibleButton("##btn", btnSize);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (clicked) m_ShowInLayer = !m_ShowInLayer;

    ImGuiID id     = ImGui::GetID("##btn");
    float   hoverT = BroadcastAnimT(id, 0xF1u, hovered && live, 14.0f);
    ImVec2  p0 = ImGui::GetItemRectMin();
    ImVec2  p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 base  = on ? ImVec4(success.x, success.y, success.z, 0.20f + hoverT * 0.06f)
                       : ImVec4(theme.surface2[0], theme.surface2[1], theme.surface2[2], theme.surface2[3] + hoverT * 0.05f);
    ImVec4 brd   = on ? ImVec4(success.x, success.y, success.z, 0.55f) : ImVec4(1, 1, 1, 0.12f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(base), 9.0f);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(brd), 9.0f, 0, on ? 1.5f : 1.0f);

    const char* label = on ? "Mostrando en Layer" : "Mostrar en Layer";
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 textPos((p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f);
    if (on) {
        float pulse = 0.65f + 0.35f * std::sin((float)ImGui::GetTime() * 3.0f);
        dl->AddCircleFilled(ImVec2(textPos.x - 12.0f, (p0.y + p1.y) * 0.5f), 4.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(success.x, success.y, success.z, pulse)));
        textPos.x += 4.0f;
    }
    ImVec4 textPri(theme.textPrimary[0], theme.textPrimary[1], theme.textPrimary[2], theme.textPrimary[3]);
    dl->AddText(textPos, ImGui::ColorConvertFloat4ToU32(on ? ImVec4(1, 1, 1, 1) : textPri), label);

    ImGui::EndDisabled();
    ImGui::PopID();

    if (!live) {
        ImGui::SameLine();
        ImGui::TextDisabled("Prende una fuente de captura primero.");
    }
}

void BroadcastPanel::RenderLayerSection() {
    const auto& theme = SettingsManager::Get().GetSettings().theme;
    ImVec4 success(theme.success[0], theme.success[1], theme.success[2], 1.0f);

    ImGui::TextUnformatted("Layer");
    ImGui::SameLine();
    ImGui::TextDisabled("(esto es lo que se transmite)");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    float avail = ImGui::GetContentRegionAvail().x;
    float h     = avail * 9.0f / 16.0f;
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool live = m_ShowInLayer && m_Capture.IsLive();

    BroadcastSoftShadow(dl, pos, { pos.x + avail, pos.y + h }, 10.0f);
    dl->AddRectFilled(pos, { pos.x + avail, pos.y + h }, IM_COL32(10, 11, 16, 255), 10.0f);
    dl->AddRect(pos, { pos.x + avail, pos.y + h },
        ImGui::ColorConvertFloat4ToU32(live ? ImVec4(success.x, success.y, success.z, 0.55f) : ImVec4(1, 1, 1, 0.10f)),
        10.0f, 0, live ? 1.5f : 1.0f);

    if (live) {
        void* tex = m_Capture.GetPreviewTexture();
        int   fw  = m_Capture.GetFrameWidth();
        int   fh  = m_Capture.GetFrameHeight();
        if (tex && fw > 0 && fh > 0) {
            float srcR = (float)fw / (float)fh;
            float dstR = avail / h;
            float dw = avail, dh = h, ox = pos.x, oy = pos.y;
            if (srcR > dstR) { dh = avail / srcR; oy += (h - dh) * 0.5f; }
            else             { dw = h * srcR;     ox += (avail - dw) * 0.5f; }
            dl->AddImage(tex, { ox, oy }, { ox + dw, oy + dh });
        }

        // Insignia "LIVE" con pulso, esquina superior izquierda del frame.
        float pulse = 0.55f + 0.45f * std::sin((float)ImGui::GetTime() * 3.0f);
        ImVec2 badgeP0(pos.x + 10.0f, pos.y + 10.0f);
        ImVec2 badgeSz(52.0f, 22.0f);
        dl->AddRectFilled(badgeP0, { badgeP0.x + badgeSz.x, badgeP0.y + badgeSz.y },
            ImGui::ColorConvertFloat4ToU32(ImVec4(success.x * 0.5f, success.y * 0.5f, success.z * 0.5f, 0.85f)), 5.0f);
        dl->AddCircleFilled({ badgeP0.x + 11.0f, badgeP0.y + badgeSz.y * 0.5f }, 3.5f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, pulse)));
        dl->AddText({ badgeP0.x + 20.0f, badgeP0.y + 4.0f }, IM_COL32(255, 255, 255, 255), "LIVE");
    } else {
        const char* hint = "Sin fuente todavia -- anda a Capture y activa \"Mostrar en Layer\".";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        ImVec4 textFaint(theme.textFaint[0], theme.textFaint[1], theme.textFaint[2], theme.textFaint[3]);
        dl->AddText({ pos.x + (avail - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f },
                    ImGui::ColorConvertFloat4ToU32(textFaint), hint);
    }

    ImGui::Dummy({ avail, h });
}

void BroadcastPanel::RenderStartSection() {
    auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().streaming;
    const auto& theme = SettingsManager::Get().GetSettings().theme;
    ImVec4 danger (theme.danger[0],  theme.danger[1],  theme.danger[2],  1.0f);
    ImVec4 success(theme.success[0], theme.success[1], theme.success[2], 1.0f);
    ImVec4 surf1  (theme.surface1[0], theme.surface1[1], theme.surface1[2], theme.surface1[3]);

    ImGui::TextUnformatted("Configuración de la transmisión");
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    bool streaming = m_Encoder.IsStreaming();

    // Tarjeta con sombra suave alrededor de los campos de conexion --
    // mismo criterio visual que RenderLayerSection, para que las 3
    // subsecciones de Captura se sientan parte de un mismo panel.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cardP0 = ImGui::GetCursorScreenPos();
        float  cardW  = ImGui::GetContentRegionAvail().x;
        float  cardH  = 132.0f;
        BroadcastSoftShadow(dl, cardP0, { cardP0.x + cardW, cardP0.y + cardH }, 10.0f);
        dl->AddRectFilled(cardP0, { cardP0.x + cardW, cardP0.y + cardH },
            ImGui::ColorConvertFloat4ToU32(surf1), 10.0f);

        ImGui::SetCursorScreenPos({ cardP0.x + 16.0f, cardP0.y + 14.0f });
        ImGui::BeginGroup();

        if (streaming) ImGui::BeginDisabled();

        static char serverBuf[256];
        static char keyBuf[256];
        static bool buffersInit = false;
        if (!buffersInit) {
            std::snprintf(serverBuf, sizeof(serverBuf), "%s", s.serverUrl.c_str());
            std::snprintf(keyBuf, sizeof(keyBuf), "%s", s.streamKey.c_str());
            buffersInit = true;
        }

        ImGui::SetNextItemWidth(cardW - 32.0f);
        if (ImGui::InputText("Servidor (rtmp://...)", serverBuf, sizeof(serverBuf)))
            s.serverUrl = serverBuf;

        ImGui::SetNextItemWidth(cardW - 32.0f);
        if (ImGui::InputText("Clave de stream", keyBuf, sizeof(keyBuf), ImGuiInputTextFlags_Password))
            s.streamKey = keyBuf;

        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt("Bitrate (kbps)", &s.videoBitrateKbps, 100);
        s.videoBitrateKbps = std::clamp(s.videoBitrateKbps, 500, 20000);

        ImGui::SameLine(0.0f, 24.0f);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("FPS", &s.fps, 1);
        s.fps = std::clamp(s.fps, 10, 60);

        if (streaming) ImGui::EndDisabled();

        ImGui::EndGroup();
        ImGui::SetCursorScreenPos({ cardP0.x, cardP0.y + cardH + 10.0f });
    }

    ImGui::TextDisabled("La resolución de salida sigue a la fuente de Capture activa (no hay escalado).");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (!m_StatusMessage.empty()) {
        ImGui::TextColored(m_StatusIsError ? danger : success, "%s", m_StatusMessage.c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }

    // Boton animado (mismo InvisibleButton + hover-lerp que "Mostrar en
    // Layer" arriba) en vez de ImGui::Button con un solo PushStyleColor
    // plano -- da feedback de hover real, no solo el color fijo de ImGui.
    ImGui::PushID("##startBtn");
    ImVec2 btnSize(220.0f, 40.0f);
    ImGui::InvisibleButton("##btn", btnSize);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    ImGuiID id     = ImGui::GetID("##btn");
    float   hoverT = BroadcastAnimT(id, 0xF2u, hovered, 14.0f);
    ImVec2  p0 = ImGui::GetItemRectMin();
    ImVec2  p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 baseCol = streaming
        ? ImVec4(theme.surface2[0] + hoverT * 0.03f, theme.surface2[1] + hoverT * 0.03f, theme.surface2[2] + hoverT * 0.03f, theme.surface2[3])
        : ImVec4(danger.x, danger.y, danger.z, 0.75f + hoverT * 0.15f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(baseCol), 9.0f);

    const char* btnLabel = streaming ? "Detener transmisión" : "Iniciar transmisión";
    ImVec2 ts = ImGui::CalcTextSize(btnLabel);
    dl->AddText({ (p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f },
        IM_COL32(255, 255, 255, 255), btnLabel);

    ImGui::PopID();

    if (clicked) {
        if (!streaming) {
            ProyecThor::Settings::SettingsManager::Get().Save();

            if (!m_ShowInLayer || !m_Capture.IsLive()) {
                m_StatusIsError = true;
                m_StatusMessage = "Anda a Capture, prende una fuente y activa \"Mostrar en Layer\" antes de iniciar.";
            } else {
                std::string url = s.serverUrl;
                if (!url.empty() && url.back() != '/') url += "/";
                url += s.streamKey;

                std::string err;
                bool ok = m_Encoder.Start(url, m_Capture.GetFrameWidth(), m_Capture.GetFrameHeight(),
                                           s.fps, s.videoBitrateKbps, &err);
                m_StatusIsError = !ok;
                m_StatusMessage = ok ? "Transmitiendo." : err;
            }
        } else {
            m_Encoder.Stop();
            m_StatusIsError = false;
            m_StatusMessage = "Transmisión detenida.";
        }
    }

    if (streaming) {
        ImGui::SameLine(0.0f, 14.0f);
        float pulse = 0.55f + 0.45f * std::sin((float)ImGui::GetTime() * 3.0f);
        ImVec2 dotPos = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled({ dotPos.x + 6.0f, dotPos.y + 20.0f }, 5.0f,
            ImGui::ColorConvertFloat4ToU32(ImVec4(danger.x, danger.y, danger.z, pulse)));
        ImGui::Dummy(ImVec2(16.0f, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(danger, "EN VIVO");
    }
}

void BroadcastPanel::Update() {
    if (!m_Encoder.IsStreaming()) return;
    if (!m_ShowInLayer || !m_Capture.IsLive()) return;

    void* texVoid = m_Capture.GetPreviewTexture();
    if (!texVoid) return;

    int w = m_Capture.GetFrameWidth();
    int h = m_Capture.GetFrameHeight();
    if (w <= 0 || h <= 0) return;

    size_t need = (size_t)w * (size_t)h * 4;
    if (m_ReadbackBuffer.size() != need) m_ReadbackBuffer.resize(need);

    GLuint tex = (GLuint)(intptr_t)texVoid;
    GLint  prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_ReadbackBuffer.data());
    glBindTexture(GL_TEXTURE_2D, (GLuint)prevTex);

    m_Encoder.PushFrame(m_ReadbackBuffer.data(), w, h);
}

} // namespace ProyecThor::UI
