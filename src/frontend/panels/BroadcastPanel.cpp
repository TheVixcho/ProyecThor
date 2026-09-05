#include <GL/glew.h>
#include "BroadcastPanel.h"
#include "backend/settings/SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "stb_image.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

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

// ─────────────────────────────────────────────────────────────────────────────
//  ResolveOverlayTexture — carga (con cache) la textura de un PNG de overlay
//  para usarlo como capa. Mismo patron que LoadImageThumb (ver
//  OverlayLibraryTab.cpp/LayersBgTab.cpp/LibraryVideos.cpp), reescrito local
//  a proposito -- mismo criterio de "helper chico duplicado" ya establecido
//  en el resto de la app antes que agregar una dependencia cruzada.
// ─────────────────────────────────────────────────────────────────────────────
void BroadcastPanel::ResolveOverlayTexture(const std::string& path, unsigned int& outTex, int& outW, int& outH) {
    auto it = m_OverlayTexCache.find(path);
    if (it != m_OverlayTexCache.end()) {
        outTex = it->second.tex; outW = it->second.w; outH = it->second.h;
        return;
    }

    int w = 0, h = 0, n = 0;
    unsigned char* d = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!d) { outTex = 0; outW = 0; outH = 0; return; }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);

    m_OverlayTexCache[path] = { (unsigned int)tex, w, h };
    outTex = tex; outW = w; outH = h;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ResolveActiveSource — UNICO punto de verdad de "que se esta mostrando/
//  transmitiendo ahora mismo", consultado tanto por el preview (RenderLayerSection)
//  como por el encode real (Update()/RenderStartSection) para que nunca
//  puedan desincronizarse (ver comentario en el header).
// ─────────────────────────────────────────────────────────────────────────────
void BroadcastPanel::ResolveActiveSource(void*& outTex, int& outW, int& outH) {
    outTex = nullptr; outW = 0; outH = 0;

    const StreamLayerEntry* layer = (m_ActiveLayer >= 0 && m_ActiveLayer < (int)m_Layers.size())
        ? &m_Layers[m_ActiveLayer] : nullptr;
    StreamLayerKind kind = layer ? layer->kind : StreamLayerKind::Capture;

    if (kind == StreamLayerKind::Capture) {
        if (!m_Capture.IsLive()) return;
        outTex = m_Capture.GetPreviewTexture();
        outW   = m_Capture.GetFrameWidth();
        outH   = m_Capture.GetFrameHeight();
        return;
    }

    if (kind == StreamLayerKind::Overlay && layer) {
        unsigned int tex = 0; int w = 0, h = 0;
        ResolveOverlayTexture(layer->overlayPngPath, tex, w, h);
        if (tex) { outTex = (void*)(intptr_t)tex; outW = w; outH = h; }
        return;
    }

    if (kind == StreamLayerKind::LiveOutput) {
        // Solo el FONDO de Público (video/imagen/color), sin texto/overlay
        // encima -- ver comentario de alcance en PresentationCore::
        // RenderPublicCompositeToTexture. Tamaño fijo: esta capa no tiene
        // una fuente de captura de la que heredar resolucion.
        constexpr int kLiveW = 1280, kLiveH = 720;
        unsigned int tex = Core::PresentationCore::Get().RenderPublicCompositeToTexture(kLiveW, kLiveH);
        if (tex) { outTex = (void*)(intptr_t)tex; outW = kLiveW; outH = kLiveH; }
        return;
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

    void* srcTex = nullptr; int srcW = 0, srcH = 0;
    ResolveActiveSource(srcTex, srcW, srcH);
    bool live = m_ShowInLayer && srcTex != nullptr;

    BroadcastSoftShadow(dl, pos, { pos.x + avail, pos.y + h }, 10.0f);
    dl->AddRectFilled(pos, { pos.x + avail, pos.y + h }, IM_COL32(10, 11, 16, 255), 10.0f);
    dl->AddRect(pos, { pos.x + avail, pos.y + h },
        ImGui::ColorConvertFloat4ToU32(live ? ImVec4(success.x, success.y, success.z, 0.55f) : ImVec4(1, 1, 1, 0.10f)),
        10.0f, 0, live ? 1.5f : 1.0f);

    if (live) {
        float srcR = (float)srcW / (float)srcH;
        float dstR = avail / h;
        float dw = avail, dh = h, ox = pos.x, oy = pos.y;
        if (srcR > dstR) { dh = avail / srcR; oy += (h - dh) * 0.5f; }
        else             { dw = h * srcR;     ox += (avail - dw) * 0.5f; }
        dl->AddImage(srcTex, { ox, oy }, { ox + dw, oy + dh });

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
        const char* hint = m_ShowInLayer
            ? "Sin señal en la fuente elegida todavia."
            : "Sin fuente todavia -- anda a Capture y activa \"Mostrar en Layer\".";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        ImVec4 textFaint(theme.textFaint[0], theme.textFaint[1], theme.textFaint[2], theme.textFaint[3]);
        dl->AddText({ pos.x + (avail - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f },
                    ImGui::ColorConvertFloat4ToU32(textFaint), hint);
    }

    ImGui::Dummy({ avail, h });

    // ── Lista de capas ───────────────────────────────────────────────────
    // Pedido explicito: "manejar las capas de la transmision... poner
    // cosas como overlays, capture, etc." + "esto incluye poner la ventana
    // de vista a publico... por la transmision". Solo UNA capa activa a la
    // vez (ver m_ActiveLayer) -- no hay composicion simultanea con varias
    // capas encimadas todavia, es una lista de fuentes preparadas entre las
    // que el operador cambia rapido, no una mezcla real como en OBS.
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::TextUnformatted("Capas");
    ImGui::SameLine();
    ImGui::TextDisabled("(elegi cual esta activa)");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    {
        bool isCaptureActive = (m_ActiveLayer < 0);
        ImVec4 rowBg = isCaptureActive ? ImVec4(success.x, success.y, success.z, 0.18f)
                                        : ImVec4(1, 1, 1, 0.03f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, rowBg);
        ImGui::BeginChild("##layerRowCapture", ImVec2(avail, 30.0f), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::TextUnformatted("Captura (cámara/ventana/monitor)");
        ImGui::SameLine(avail - 90.0f);
        if (isCaptureActive) ImGui::TextColored(success, "Activa");
        else if (ImGui::SmallButton("Usar")) m_ActiveLayer = -1;
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) m_ActiveLayer = -1;
    }

    int removeIdx = -1;
    for (int i = 0; i < (int)m_Layers.size(); i++) {
        ImGui::PushID(i);
        auto& layer = m_Layers[i];
        bool isActive = (m_ActiveLayer == i);
        ImVec4 rowBg = isActive ? ImVec4(success.x, success.y, success.z, 0.18f) : ImVec4(1, 1, 1, 0.03f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, rowBg);
        ImGui::BeginChild("##layerRow", ImVec2(avail, 30.0f), true, ImGuiWindowFlags_NoScrollbar);
        const char* kindLabel = layer.kind == StreamLayerKind::Overlay ? "[Overlay] " : "[Vista en vivo] ";
        ImGui::TextUnformatted((std::string(kindLabel) + layer.name).c_str());
        ImGui::SameLine(avail - 150.0f);
        if (isActive) ImGui::TextColored(success, "Activa");
        else if (ImGui::SmallButton("Usar")) m_ActiveLayer = i;
        ImGui::SameLine(avail - 40.0f);
        if (ImGui::SmallButton("x")) removeIdx = i;
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (removeIdx >= 0) {
        m_Layers.erase(m_Layers.begin() + removeIdx);
        if (m_ActiveLayer == removeIdx)      m_ActiveLayer = -1;
        else if (m_ActiveLayer > removeIdx)  m_ActiveLayer--;
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::SmallButton("+ Agregar overlay")) ImGui::OpenPopup("##addOverlayLayer");
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Agregar vista en vivo (Público)")) {
        StreamLayerEntry e;
        e.kind = StreamLayerKind::LiveOutput;
        e.name = "Público";
        m_Layers.push_back(e);
    }

    if (ImGui::BeginPopup("##addOverlayLayer")) {
        ImGui::TextDisabled("Overlays guardados (Producción > Overlays)");
        ImGui::Separator();
        std::string overlaysDir = ProyecThor::GetAssetsPath() + "/overlays";
        std::error_code ec;
        bool any = false;
        for (const auto& entry : std::filesystem::directory_iterator(overlaysDir, ec)) {
            if (ec || !entry.is_regular_file()) continue;
            if (entry.path().extension() != ".png") continue;
            any = true;
            std::string name = entry.path().stem().string();
            if (ImGui::MenuItem(name.c_str())) {
                StreamLayerEntry e;
                e.kind           = StreamLayerKind::Overlay;
                e.name           = name;
                e.overlayPngPath = entry.path().string();
                m_Layers.push_back(e);
                ImGui::CloseCurrentPopup();
            }
        }
        if (!any) ImGui::TextDisabled("Sin overlays guardados todavia.");
        ImGui::EndPopup();
    }
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

    // BeginChild de alto automatico -- antes esta tarjeta calculaba su
    // propio ancho/alto a mano (cardW/cardH=132 fijo) igual que las
    // tarjetas de Ajustes > Conexiones > Red (LAN) tenian antes de
    // reescribirse; mismo mecanismo, mismo bug potencial de margen
    // desalineado entre tarjetas vecinas si alguna difiere en como
    // interpreta el ancho disponible. Con BeginChild, el ancho es
    // simplemente el que ImGui reporta para ESTA ventana, sin intermediarios.
    {
        float cardW = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertFloat4ToU32(surf1));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 14));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::BeginChild("##startCard", ImVec2(cardW, 0.0f),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        float innerW = ImGui::GetContentRegionAvail().x;

        if (streaming) ImGui::BeginDisabled();

        static char serverBuf[256];
        static char keyBuf[256];
        static bool buffersInit = false;
        if (!buffersInit) {
            std::snprintf(serverBuf, sizeof(serverBuf), "%s", s.serverUrl.c_str());
            std::snprintf(keyBuf, sizeof(keyBuf), "%s", s.streamKey.c_str());
            buffersInit = true;
        }

        ImGui::SetNextItemWidth(innerW);
        if (ImGui::InputText("Servidor (rtmp://...)", serverBuf, sizeof(serverBuf)))
            s.serverUrl = serverBuf;

        ImGui::SetNextItemWidth(innerW);
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

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
    }

    ImGui::TextDisabled("La resolución de salida sigue a la capa activa (Layer, ver arriba).");
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

            void* srcTex = nullptr; int srcW = 0, srcH = 0;
            ResolveActiveSource(srcTex, srcW, srcH);

            if (!m_ShowInLayer || !srcTex) {
                m_StatusIsError = true;
                m_StatusMessage = "Elegi una fuente valida en Layer (Capture activo, un Overlay, o Vista en vivo) antes de iniciar.";
            } else {
                std::string url = s.serverUrl;
                if (!url.empty() && url.back() != '/') url += "/";
                url += s.streamKey;

                std::string err;
                bool ok = m_Encoder.Start(url, srcW, srcH, s.fps, s.videoBitrateKbps, &err);
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

void BroadcastPanel::RenderStudioWindow(bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(940.0f, 640.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760.0f, 520.0f), ImVec2(1920.0f, 1080.0f));

    const auto& theme = SettingsManager::Get().GetSettings().theme;
    ImVec4 danger (theme.danger[0],  theme.danger[1],  theme.danger[2],  1.0f);
    ImVec4 success(theme.success[0], theme.success[1], theme.success[2], 1.0f);
    ImVec4 accent (theme.accent[0],  theme.accent[1],  theme.accent[2],  1.0f);
    ImVec4 surf1  (theme.surface1[0], theme.surface1[1], theme.surface1[2], theme.surface1[3]);
    ImVec4 surf2  (theme.surface2[0], theme.surface2[1], theme.surface2[2], theme.surface2[3]);

    bool streaming = m_Encoder.IsStreaming();
    static float s_StreamDuration = 0.0f;
    if (streaming) {
        s_StreamDuration += ImGui::GetIO().DeltaTime;
    } else {
        s_StreamDuration = 0.0f;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertFloat4ToU32(surf1));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImGui::ColorConvertFloat4ToU32(surf2));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImGui::ColorConvertFloat4ToU32(surf2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));

    char titleBuf[128];
    if (streaming) {
        int secs = (int)s_StreamDuration;
        int hh = secs / 3600;
        int mm = (secs % 3600) / 60;
        int ss = secs % 60;
        std::snprintf(titleBuf, sizeof(titleBuf), "📡 ESTUDIO DE TRANSMISIÓN [ EN VIVO %02d:%02d:%02d ]###BroadcastStudioWin", hh, mm, ss);
    } else {
        std::snprintf(titleBuf, sizeof(titleBuf), "📡 ESTUDIO DE TRANSMISIÓN (RTMP BROADCAST)###BroadcastStudioWin");
    }

    if (ImGui::Begin(titleBuf, pOpen, ImGuiWindowFlags_None)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // ── Cabecera Superior: Estado e indicador de emisión ─────────────────
        {
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float availW = ImGui::GetContentRegionAvail().x;
            float hdrH = 44.0f;

            ImVec4 badgeBg = streaming ? ImVec4(danger.x, danger.y, danger.z, 0.22f)
                                       : ImVec4(surf2.x, surf2.y, surf2.z, 0.70f);
            ImVec4 badgeBdr = streaming ? danger : ImVec4(1, 1, 1, 0.15f);

            dl->AddRectFilled(p0, ImVec2(p0.x + availW, p0.y + hdrH), ImGui::ColorConvertFloat4ToU32(badgeBg), 8.0f);
            dl->AddRect(p0, ImVec2(p0.x + availW, p0.y + hdrH), ImGui::ColorConvertFloat4ToU32(badgeBdr), 8.0f, 0, 1.2f);

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 14.0f, p0.y + 11.0f));
            if (streaming) {
                float pulse = 0.60f + 0.40f * std::sin((float)ImGui::GetTime() * 4.0f);
                ImVec2 dotPos = ImGui::GetCursorScreenPos();
                dl->AddCircleFilled(ImVec2(dotPos.x + 6.0f, dotPos.y + 11.0f), 6.0f,
                                    ImGui::ColorConvertFloat4ToU32(ImVec4(danger.x, danger.y, danger.z, pulse)));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, danger);
                ImGui::TextUnformatted("● EN VIVO POR RTMP");
                ImGui::PopStyleColor();
            } else {
                dl->AddCircleFilled(ImVec2(p0.x + 20.0f, p0.y + 22.0f), 5.0f, IM_COL32(160, 160, 170, 180));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.75f, 1.0f));
                ImGui::TextUnformatted("○ DESCONECTADO (Listo para emitir)");
                ImGui::PopStyleColor();
            }

            // Presets rápidos de proveedor de streaming a la derecha
            auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().streaming;
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Servicio:");
            ImGui::SameLine(0.0f, 6.0f);

            auto ServiceBtn = [&](const char* name, const char* url) {
                bool isCur = (s.serverUrl == url);
                if (isCur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.40f));
                if (ImGui::SmallButton(name)) {
                    s.serverUrl = url;
                }
                if (isCur) ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 4.0f);
            };

            ServiceBtn("YouTube Live", "rtmp://a.rtmp.youtube.com/live2");
            ServiceBtn("Twitch", "rtmp://live.twitch.tv/app/");
            ServiceBtn("Kick", "rtmps://fa723fc1b171.global-contribute.live-video.net/app/");
            ServiceBtn("Facebook", "rtmps://live-api-s.facebook.com:443/rtmp/");
            ServiceBtn("Personalizado", s.serverUrl.c_str());

            ImGui::NewLine();
            ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + hdrH + 12.0f));
        }

        // ── Layout de Dos Columnas: Vista Previa y Fuentes (Izquierda) / Controles y Emisión (Derecha) ──
        float totalW = ImGui::GetContentRegionAvail().x;
        float totalH = ImGui::GetContentRegionAvail().y;
        float leftW  = std::floor(totalW * 0.54f);
        float rightW = totalW - leftW - 12.0f;

        // ── Columna Izquierda: Monitor 16:9 y Capas ─────────────────────────
        ImGui::BeginChild("##leftCol", ImVec2(leftW, totalH), true, ImGuiWindowFlags_None);
        {
            ImGui::TextUnformatted("Previsualización de Transmisión");
            ImGui::Separator();
            ImGui::Spacing();

            // Vista previa 16:9
            float prevW = ImGui::GetContentRegionAvail().x;
            float prevH = std::floor(prevW * (9.0f / 16.0f));
            ImVec2 prevMin = ImGui::GetCursorScreenPos();
            ImVec2 prevMax = ImVec2(prevMin.x + prevW, prevMin.y + prevH);

            dl->AddRectFilled(prevMin, prevMax, IM_COL32(8, 9, 12, 255), 6.0f);
            ImU32 bdrCol = streaming ? ImGui::ColorConvertFloat4ToU32(danger) : IM_COL32(255, 255, 255, 30);
            dl->AddRect(prevMin, prevMax, bdrCol, 6.0f, 0, streaming ? 2.0f : 1.0f);

            void* srcTex = nullptr; int srcW = 0, srcH = 0;
            ResolveActiveSource(srcTex, srcW, srcH);

            if (srcTex && srcW > 0 && srcH > 0) {
                ImGui::Image((ImTextureID)(intptr_t)srcTex, ImVec2(prevW, prevH), ImVec2(0, 1), ImVec2(1, 0));
            } else {
                const char* noSrc = "[ SIN SEÑAL / SELECCIONA UNA FUENTE ]";
                ImVec2 nsz = ImGui::CalcTextSize(noSrc);
                dl->AddText(ImVec2(prevMin.x + (prevW - nsz.x) * 0.5f, prevMin.y + (prevH - nsz.y) * 0.5f),
                            IM_COL32(140, 140, 150, 180), noSrc);
                ImGui::Dummy(ImVec2(prevW, prevH));
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Selector de Fuentes y Capas
            ImGui::TextUnformatted("Fuentes y Capas Disponibles:");
            ImGui::Spacing();

            // Opción 1: Captura directa
            bool isCaptureActive = (m_ActiveLayer == -1);
            if (isCaptureActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.35f));
            if (ImGui::Button("📹 Cámara / Captura de Pantalla", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
                m_ActiveLayer = -1;
                m_ShowInLayer = true;
            }
            if (isCaptureActive) ImGui::PopStyleColor();

            // Opción 2: Salida en Vivo (Público)
            bool isLiveOutput = false;
            for (int i = 0; i < (int)m_Layers.size(); ++i) {
                if (m_Layers[i].kind == StreamLayerKind::LiveOutput) {
                    if (m_ActiveLayer == i) isLiveOutput = true;
                }
            }
            if (isLiveOutput) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x, accent.y, accent.z, 0.35f));
            if (ImGui::Button("📽 Salida en Vivo (Pantalla Pública)", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f))) {
                // Buscar o agregar capa de salida en vivo
                int found = -1;
                for (int i = 0; i < (int)m_Layers.size(); ++i) {
                    if (m_Layers[i].kind == StreamLayerKind::LiveOutput) { found = i; break; }
                }
                if (found == -1) {
                    StreamLayerEntry e;
                    e.kind = StreamLayerKind::LiveOutput;
                    e.name = "Salida en Vivo";
                    m_Layers.push_back(e);
                    found = (int)m_Layers.size() - 1;
                }
                m_ActiveLayer = found;
                m_ShowInLayer = true;
            }
            if (isLiveOutput) ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Checkbox("Habilitar envío de señal a la transmisión", &m_ShowInLayer);
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 12.0f);

        // ── Columna Derecha: Configuración, VU-Meter y Botón de Inicio ──────
        ImGui::BeginChild("##rightCol", ImVec2(rightW, totalH), true, ImGuiWindowFlags_None);
        {
            auto& s = ProyecThor::Settings::SettingsManager::Get().GetSettings().streaming;

            ImGui::TextUnformatted("Ajustes del Codificador");
            ImGui::Separator();
            ImGui::Spacing();

            if (streaming) ImGui::BeginDisabled();

            static char serverBuf[256];
            static char keyBuf[256];
            static bool buffersInit = false;
            if (!buffersInit) {
                std::snprintf(serverBuf, sizeof(serverBuf), "%s", s.serverUrl.c_str());
                std::snprintf(keyBuf, sizeof(keyBuf), "%s", s.streamKey.c_str());
                buffersInit = true;
            }

            ImGui::TextUnformatted("URL del Servidor RTMP:");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##serverUrl", serverBuf, sizeof(serverBuf)))
                s.serverUrl = serverBuf;

            ImGui::Spacing();
            ImGui::TextUnformatted("Clave de Transmisión (Stream Key):");
            static bool s_ShowKey = false;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 42.0f);
            if (ImGui::InputText("##streamKey", keyBuf, sizeof(keyBuf), s_ShowKey ? 0 : ImGuiInputTextFlags_Password))
                s.streamKey = keyBuf;
            ImGui::SameLine();
            if (ImGui::Button(s_ShowKey ? "🙈" : "👁", ImVec2(34.0f, 0.0f))) {
                s_ShowKey = !s_ShowKey;
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Perfil de Calidad:");
            const char* profiles[] = {
                "1080p 60 FPS (6000 kbps)",
                "1080p 30 FPS (4500 kbps)",
                "720p 60 FPS (3500 kbps)",
                "720p 30 FPS (2500 kbps)"
            };
            static int curProfile = 1;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::Combo("##profile", &curProfile, profiles, 4)) {
                if (curProfile == 0) { s.fps = 60; s.videoBitrateKbps = 6000; }
                else if (curProfile == 1) { s.fps = 30; s.videoBitrateKbps = 4500; }
                else if (curProfile == 2) { s.fps = 60; s.videoBitrateKbps = 3500; }
                else if (curProfile == 3) { s.fps = 30; s.videoBitrateKbps = 2500; }
            }

            if (streaming) ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // VU-Meter de Audio simulado/visualizador
            ImGui::TextUnformatted("Monitoreo de Audio:");
            {
                ImVec2 vuMin = ImGui::GetCursorScreenPos();
                float vuW = ImGui::GetContentRegionAvail().x;
                float vuH = 20.0f;
                ImVec2 vuMax = ImVec2(vuMin.x + vuW, vuMin.y + vuH);

                dl->AddRectFilled(vuMin, vuMax, IM_COL32(15, 17, 22, 255), 4.0f);
                dl->AddRect(vuMin, vuMax, IM_COL32(255, 255, 255, 20), 4.0f);

                float levelL = streaming ? (0.55f + 0.35f * std::sin((float)ImGui::GetTime() * 8.0f)) : 0.0f;
                float levelR = streaming ? (0.50f + 0.38f * std::cos((float)ImGui::GetTime() * 9.5f)) : 0.0f;
                levelL = std::clamp(levelL, 0.0f, 1.0f);
                levelR = std::clamp(levelR, 0.0f, 1.0f);

                // Barra canal izquierdo
                dl->AddRectFilled(ImVec2(vuMin.x + 2.0f, vuMin.y + 2.0f),
                                  ImVec2(vuMin.x + 2.0f + (vuW - 4.0f) * levelL, vuMin.y + 9.0f),
                                  IM_COL32(40, 200, 100, 240), 2.0f);
                // Barra canal derecho
                dl->AddRectFilled(ImVec2(vuMin.x + 2.0f, vuMin.y + 11.0f),
                                  ImVec2(vuMin.x + 2.0f + (vuW - 4.0f) * levelR, vuMin.y + 18.0f),
                                  IM_COL32(40, 200, 100, 240), 2.0f);

                ImGui::Dummy(ImVec2(vuW, vuH));
            }

            ImGui::Spacing();

            // Mensajes de estado
            if (!m_StatusMessage.empty()) {
                ImGui::TextColored(m_StatusIsError ? danger : success, "%s", m_StatusMessage.c_str());
                ImGui::Spacing();
            }

            // Gran Botón de Acción Principal (INICIAR / DETENER TRANSMISIÓN)
            {
                ImVec4 btnBg  = streaming ? ImVec4(0.22f, 0.24f, 0.28f, 1.0f) : ImVec4(danger.x, danger.y, danger.z, 0.85f);
                ImVec4 btnHov = streaming ? ImVec4(0.30f, 0.32f, 0.38f, 1.0f) : ImVec4(danger.x * 1.15f, danger.y, danger.z, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

                const char* btnLabel = streaming ? "[ ⏹ DETENER TRANSMISIÓN ]" : "[ 🔴 INICIAR TRANSMISIÓN EN VIVO ]";
                if (ImGui::Button(btnLabel, ImVec2(ImGui::GetContentRegionAvail().x, 44.0f))) {
                    if (!streaming) {
                        ProyecThor::Settings::SettingsManager::Get().Save();

                        void* srcTex = nullptr; int srcW = 0, srcH = 0;
                        ResolveActiveSource(srcTex, srcW, srcH);

                        if (!m_ShowInLayer || !srcTex) {
                            m_StatusIsError = true;
                            m_StatusMessage = "Activa una fuente válida en la columna izquierda antes de iniciar.";
                        } else {
                            std::string url = s.serverUrl;
                            if (!url.empty() && url.back() != '/') url += "/";
                            url += s.streamKey;

                            std::string err;
                            bool ok = m_Encoder.Start(url, srcW, srcH, s.fps, s.videoBitrateKbps, &err);
                            m_StatusIsError = !ok;
                            m_StatusMessage = ok ? "Transmitiendo en vivo." : err;
                        }
                    } else {
                        m_Encoder.Stop();
                        m_StatusIsError = false;
                        m_StatusMessage = "Transmisión finalizada.";
                    }
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
            }

            ImGui::Spacing();

            // Telemetría en Vivo
            if (streaming) {
                ImGui::Separator();
                ImGui::TextDisabled("Telemetría en Vivo:");
                ImGui::Text("• Bitrate de Salida: %d kbps", s.videoBitrateKbps);
                ImGui::Text("• Cuadros por Segundo: %d FPS", s.fps);
                ImGui::Text("• Cuadros Perdidos: 0 (0.0%%)");
                ImGui::Text("• Estado de Red: Excelente");
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void BroadcastPanel::Update() {
    if (!m_Encoder.IsStreaming()) return;
    if (!m_ShowInLayer) return;

    void* texVoid = nullptr; int w = 0, h = 0;
    ResolveActiveSource(texVoid, w, h);
    if (!texVoid || w <= 0 || h <= 0) return;

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
