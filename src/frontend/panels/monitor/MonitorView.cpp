#include "MonitorView.h"
#include "MonitorTheme.h"
#include "MonitorQueueIO.h"
#include "MonitorQueueHelpers.h"
#include "backend/core/PresentationCore.h"
#include "backend/media/VLCBasePlayer.h"
#include "StyleGeneralApp.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/AppPaths.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "frontend/views/audio/AudioAlbumArt.h"
#include <filesystem>

namespace ProyecThor::UI {

using namespace MonitorTheme;

void MonitorView::Update()
{
    m_QueueEngine.Update();

    // Boton "Loop" (ver MonitorCenterColumn::RenderCenterColumn /
    // PresentationCore::GetLiveLoop) para un video enviado DIRECTO a
    // publico via "TRANSMITIR" (SetBackgroundMedia -- no pasa por la cola,
    // ver el handler de "btn_transmit" en MonitorCenterColumn.cpp). Sin
    // esto el flag liveLoop no lo leia nadie: el clip terminaba y quedaba
    // congelado, nada volvia a dispararlo.
    //
    // ConsumeEndReached() es "se consume una sola vez por clip" -- por eso
    // esto SOLO corre si la cola NO esta activa (ella ya lo consume arriba
    // en m_QueueEngine.Update(), con su propio criterio de loop). Los dos
    // consumidores nunca deben pisarse sobre el mismo reproductor.
    if (!m_QueueEngine.IsActive())
    {
        auto& core  = Core::PresentationCore::Get();
        auto  state = core.GetState();
        if (state.bgType == Core::PresentationState::BackgroundType::Video)
        {
            if (Core::VLCBasePlayer* bg = core.GetBackgroundPlayer())
            {
                if (bg->ConsumeEndReached())
                {
                    bool hadError = bg->ConsumeHadError();
                    if (!hadError && core.GetLiveLoop())
                    {
                        std::string path = bg->GetCurrentPath();
                        if (!path.empty())
                        {
                            bool allowAudio = core.GetContentAllowsAudio();
                            std::string norm = path;
                            std::replace(norm.begin(), norm.end(), '\\', '/');
                            if (norm.find("/backgrounds/") != std::string::npos ||
                                norm.find("assets/backgrounds") != std::string::npos) {
                                allowAudio = false;
                            }
                            core.SetBackgroundMedia(path, /*isVideo=*/true, allowAudio);
                        }
                    }
                }
            }
        }
    }
}

void MonitorView::Render(Core::VLCBasePlayer* player)
{
    if (!player) {
        ImGui::PushStyleColor(ImGuiCol_Text, k_TextDim);
        ImGui::TextUnformatted("No hay reproductor disponible");
        ImGui::PopStyleColor();
        return;
    }

    Core::VLCBasePlayer* bg            = Core::PresentationCore::Get().GetBackgroundPlayer();
    bool                 isSharedPlayer = (player == bg);

    // Antes se recalculaba al principio de RenderLiveControls (ahora vive en
    // ViewPanel::RenderLiveTransport). RenderCenterColumn y RenderPreviewControls
    // siguen dependiendo de que estos 3 esten frescos cada frame.
    m_LivePlaying = bg && !bg->IsPaused();
    m_LiveMuted   = Core::PresentationCore::Get().GetLiveMute();
    m_LiveVolume  = static_cast<float>(Core::PresentationCore::Get().GetLiveVolume()) * 0.01f;

    // ── Cambio de seleccion → cargar en preview ────────────────────────────
    static std::string s_LastTitle;
    auto currentSel = Core::PresentationCore::Get().PeekSelection();

    // FIX (crash en Windows/Wine): la cola del Monitor tambien pasa por
    // SetSelection() al arrancar/avanzar cada item (ver MonitorQueueEngine::
    // PlayIndex), solo para que el titulo se muestre en pantalla — no es
    // una eleccion manual del operador en la Biblioteca. Antes este bloque
    // no distinguia el origen, asi que cada avance de la cola disparaba
    // TAMBIEN una carga en el reproductor de Preview del mismo archivo que
    // la cola ya esta reproduciendo (o precargando en standby) — dos/tres
    // instancias de VLC abriendo el mismo archivo a la vez, lo que
    // crasheaba en Windows.
    bool fromQueue = Core::PresentationCore::Get().IsSelectionFromQueue();

    if (currentSel.title != s_LastTitle)
    {
        s_LastTitle = currentSel.title;

        if (!isSharedPlayer && !fromQueue)
        {
           if (currentSel.type == Core::ItemType::Video && !currentSel.title.empty())
{
    std::string path = currentSel.title;
    if (path.rfind("http", 0) != 0 && !std::filesystem::path(path).is_absolute())
        path = GetAssetsPath() + "/videos/" + path;

    // Carga en un hilo aparte (ver PresentationCore::RequestPreviewLoad):
    // el video en vivo al publico nunca debe esperar a que el Preview
    // termine de abrir un archivo. startMuted refleja m_PreviewAudioEnabled
    // (ver RenderPreviewAudioToggle) en vez de ir siempre mudo -- si el
    // operador ya activo el audio de Preview, un clip nuevo debe seguir
    // sonando en vez de volver a silenciarse solo.
    Core::PresentationCore::Get().RequestPreviewLoad(path, /*loop=*/false, /*startMuted=*/!m_PreviewAudioEnabled);
    m_ImageView.Clear();
    m_PreviewPlaying = true;
}
            else if (currentSel.type == Core::ItemType::Audio && !currentSel.title.empty())
            {
                std::string path = currentSel.title;
                if (!std::filesystem::path(path).is_absolute())
                    path = Audio::GetAudioPath() + "/" + path;
                // Reproduce igual que un video (el preview sigue mudo por
                // forceSilent salvo que el operador lo active, ver arriba)
                // para que el tiempo/seek funcionen; el disco animado se
                // dibuja en vez de la textura de video.
                Core::PresentationCore::Get().RequestPreviewLoad(path, /*loop=*/false, /*startMuted=*/!m_PreviewAudioEnabled);
                m_ImageView.Clear();
                m_PreviewPlaying = true;

                if (m_CurrentAudioArt) {
                    GLuint tex = static_cast<GLuint>(m_CurrentAudioArt);
                    glDeleteTextures(1, &tex);
                    m_CurrentAudioArt = 0;
                }
                Audio::AlbumArt art = Audio::ExtractAlbumArt(path);
                if (art.HasData()) {
                    Audio::UploadAlbumArtToGL(art);
                    if (art.HasTexture())
                        m_CurrentAudioArt = static_cast<ImTextureID>(art.texID);
                }
            }
            else if (currentSel.type == Core::ItemType::Image && !currentSel.title.empty())
            {
                Core::PresentationCore::Get().RequestPreviewStop();
                m_PreviewPlaying = false;
                m_ImageView.LoadImageFromFile(GetAssetsPath() + "/images/" + currentSel.title);
                m_ImageView.ResetAdjustments();
            }
            else
            {
                player->SetPause(true);
                player->SetMute(true);
                m_ImageView.Clear();
                m_PreviewPlaying = false;
            }
        }
    }

    // ── Inicializacion unica ──────────────────────────────────────────────────
    if (!m_Initialized)
    {
        m_Initialized = true;
        LoadPlayQueue();
        if (!isSharedPlayer)
        {
            player->SetPause(true);
            player->SetMute(true);
        }
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    // Preview toma TODO el alto y ancho disponible con controles flotantes (HUD),
    // y la Cola es plegable hacia el borde derecho para maximizar el espacio.
    const float totalW   = ImGui::GetContentRegionAvail().x;
    const float totalH   = ImGui::GetContentRegionAvail().y;

    const float queueExpandedW = std::clamp(totalW * 0.30f, 220.0f, 320.0f);
    const float targetQueueW   = m_QueueCollapsed ? 34.0f : queueExpandedW;
    if (m_QueueAnimW <= 0.0f)
        m_QueueAnimW = targetQueueW;
    else
        m_QueueAnimW += (targetQueueW - m_QueueAnimW) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);

    const float queueW   = m_QueueAnimW;
    const float previewW = std::max(120.0f, totalW - queueW - 6.0f);
    const float previewH = totalH;

    RenderPreviewMonitor(player, previewW, previewH);

    ImGui::SameLine(0, 6);
    RenderQueue(queueW);
}

// ---------------------------------------------------------------------------
bool MonitorView::DrawIconButton(const char* iconName, float size,
                                  ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                                  ImVec2 btnSize, bool isActiveState)
{
    ImVec4 finalBg = isActiveState ? act : bgCol;

    ImGui::PushStyleColor(ImGuiCol_Button,        finalBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    bool pressed = ImGui::Button("", btnSize);
    bool isHeld  = ImGui::IsItemActive();

    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 s = ImGui::GetItemRectSize();

    float offsetY = isHeld ? 1.5f : 0.0f;
    ImVec2 center = { p.x + s.x * 0.5f, p.y + s.y * 0.5f + offsetY };

    const float luma = 0.299f * finalBg.x + 0.587f * finalBg.y + 0.114f * finalBg.z;
    const ImU32 tintCol = (luma > 0.55f) ? IM_COL32(20, 20, 24, 255) : IM_COL32(245, 245, 250, 255);

    const char* symbolGlyph = nullptr;
    std::string iName = iconName ? iconName : "";
    if (iName == "skip_prev")       symbolGlyph = "\xE2\x8F\xAE"; // ⏮
    else if (iName == "replay_10")  symbolGlyph = "\xE2\x8F\xAA"; // ⏪
    else if (iName == "play")       symbolGlyph = "\xE2\x96\xB6"; // ▶
    else if (iName == "pause")      symbolGlyph = "\xE2\x8F\xB8"; // ⏸
    else if (iName == "forward_10") symbolGlyph = "\xE2\x8F\xA9"; // ⏩
    else if (iName == "stop")       symbolGlyph = "\xE2\x8F\xB9"; // ⏹
    else if (iName == "arrow_forward") symbolGlyph = "\xF0\x9F\x9A\x80"; // 🚀
    else if (iName == "repeat")     symbolGlyph = "\xF0\x9F\x94\x81"; // 🔁
    else if (iName == "volume_up")  symbolGlyph = "\xF0\x9F\x94\x8A"; // 🔊
    else if (iName == "no_sound")   symbolGlyph = "\xF0\x9F\x94\x87"; // 🔇

    if (symbolGlyph)
    {
        ImFont* font = ImGui::GetFont();
        float fontSz = size * 1.05f;
        ImVec2 glyphSz = font->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, symbolGlyph);
        ImVec2 glyphPos = { center.x - glyphSz.x * 0.5f, center.y - glyphSz.y * 0.5f };
        ImGui::GetWindowDrawList()->AddText(font, fontSz, glyphPos, tintCol, symbolGlyph);
    }
    else
    {
        ImTextureID tex = (ImTextureID)0;
        auto it = StyleGeneralApp::Icons.find(iconName);
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
            tex = (ImTextureID)(intptr_t)it->second.textureID;

        if (tex) {
            ImGui::GetWindowDrawList()->AddImage(
                tex,
                { center.x - size * 0.5f, center.y - size * 0.5f },
                { center.x + size * 0.5f, center.y + size * 0.5f },
                ImVec2(0, 0), ImVec2(1, 1), tintCol);
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return pressed;
}

} // namespace ProyecThor::UI