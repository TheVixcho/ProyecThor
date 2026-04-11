#include "MonitorView.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace {
    constexpr ImVec4 kBg0        { 0.118f, 0.118f, 0.118f, 1.f };
    constexpr ImVec4 kBg1        { 0.157f, 0.157f, 0.157f, 1.f };
    constexpr ImVec4 kBg2        { 0.196f, 0.196f, 0.196f, 1.f };
    constexpr ImVec4 kBg2Hov     { 0.235f, 0.235f, 0.235f, 1.f };
    constexpr ImVec4 kBg2Act     { 0.098f, 0.098f, 0.098f, 1.f };
    constexpr ImVec4 kAccent     { 0.227f, 0.525f, 0.800f, 1.f };
    constexpr ImVec4 kAccentDim  { 0.227f, 0.525f, 0.800f, 0.3f };
    constexpr ImVec4 kGreen      { 0.149f, 0.698f, 0.298f, 1.f };
    constexpr ImVec4 kGreenDim   { 0.149f, 0.698f, 0.298f, 0.2f };
    constexpr ImVec4 kRed        { 0.867f, 0.173f, 0.149f, 1.f };
    constexpr ImVec4 kYellow     { 0.933f, 0.706f, 0.094f, 1.f };
    constexpr ImVec4 kTxt0       { 0.918f, 0.918f, 0.918f, 1.f };
    constexpr ImVec4 kTxt1       { 0.600f, 0.600f, 0.600f, 1.f };
    constexpr ImVec4 kTxt2       { 0.380f, 0.380f, 0.380f, 1.f };
    constexpr ImVec4 kBorder     { 0.275f, 0.275f, 0.275f, 1.f };
    constexpr ImVec4 kVuG        { 0.118f, 0.780f, 0.322f, 1.f };
    constexpr ImVec4 kVuY        { 0.933f, 0.776f, 0.082f, 1.f };
    constexpr ImVec4 kVuR        { 0.918f, 0.180f, 0.141f, 1.f };

    inline ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

    inline ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
        return { a.x + (b.x - a.x) * t,
                 a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t,
                 a.w + (b.w - a.w) * t };
    }

    inline void SectionLabel(const char* text) {
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    static void OBSSeparator() {
        ImGui::PushStyleColor(ImGuiCol_Separator, U32(kBorder));
        ImGui::Separator();
        ImGui::PopStyleColor();
    }
}

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers estáticos
// ─────────────────────────────────────────────────────────────────────────────

std::string MonitorView::FormatTime(int64_t ms) {
    if (ms < 0) ms = 0;
    const int total   = static_cast<int>(ms / 1000);
    const int hours   = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int seconds = total % 60;
    std::ostringstream oss;
    if (hours > 0)
        oss << std::setfill('0') << std::setw(2) << hours << ":";
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

std::string MonitorView::TruncateLabel(const std::string& s, int maxChars) {
    if (static_cast<int>(s.size()) <= maxChars) return s;
    return s.substr(0, maxChars - 1) + "\xe2\x80\xa6";
}

bool MonitorView::IsVideoOrImage(const QueueEntry& e) const {
    return e.type == "video" || e.type == "image";
}

// ─────────────────────────────────────────────────────────────────────────────
//  DUAL AUDIO
// ─────────────────────────────────────────────────────────────────────────────

void MonitorView::ApplyAudioDevice(Core::VLCVideoLayer* player) {
    auto& core = Core::PresentationCore::Get();
    auto* livePlayer = core.GetOverlayPlayer();

    // 1. Enviar el audio del monitor a la salida de operador (si hay player local)
    if (player && !m_MonitorDeviceId.empty()) {
        player->SetAudioDevice(m_MonitorDeviceId);
    }
    // 2. Enviar el audio público al proyector
    if (livePlayer && !m_PublicDeviceId.empty()) {
        livePlayer->SetAudioDevice(m_PublicDeviceId);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  OPERACIONES DE CONTROL (Fix sincronización Previo <-> Vivo)
// ─────────────────────────────────────────────────────────────────────────────

void MonitorView::DualPlay(Core::VLCVideoLayer* player, const std::string& path) {
    if (player) player->PlayVideo(path);
    m_IsPlaying = true;

    auto& core = Core::PresentationCore::Get();
    if (core.GetState().isProjecting) {
        core.SetOverlayMedia(path);
    }
    
    // Aplicamos los ruteos de audio inmediatamente
    ApplyAudioDevice(player);
}

void MonitorView::DualPause(Core::VLCVideoLayer* player, bool pause) {
    // Pausar monitor local
    if (player) player->SetPause(pause);
    m_IsPlaying = !pause;

    // Pausar salida pública (Fix principal)
    auto* livePlayer = Core::PresentationCore::Get().GetOverlayPlayer();
    if (livePlayer && livePlayer != player) {
        livePlayer->SetPause(pause);
    }
}

void MonitorView::DualSeek(Core::VLCVideoLayer* player, float normalizedPos) {
    if (player) player->SetPosition(normalizedPos);

    auto* livePlayer = Core::PresentationCore::Get().GetOverlayPlayer();
    if (livePlayer && livePlayer != player) {
        livePlayer->SetPosition(normalizedPos);
    }
}

void MonitorView::DualStop(Core::VLCVideoLayer* player) {
    if (player) {
        player->SetPosition(0.f);
        player->SetPause(true);
    }
    m_IsPlaying = false;

    auto& core = Core::PresentationCore::Get();
    if (core.GetState().isProjecting) {
        core.StopOverlayMedia();
    }

    auto* livePlayer = core.GetOverlayPlayer();
    if (livePlayer && livePlayer != player) {
        livePlayer->SetPosition(0.f);
        livePlayer->SetPause(true);
    }
}

void MonitorView::DualSetVolume(Core::VLCVideoLayer* player, int vol) {
    if (player) player->SetVolume(vol);

    auto* livePlayer = Core::PresentationCore::Get().GetOverlayPlayer();
    if (livePlayer && livePlayer != player) {
        livePlayer->SetVolume(vol);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cola
// ─────────────────────────────────────────────────────────────────────────────

void MonitorView::EnqueueItem(const QueueEntry& entry) {
    if (!m_Queue.empty() && m_Queue.back().fullPath == entry.fullPath)
        return;
    m_Queue.push_back(entry);
}

void MonitorView::ClearQueue() {
    m_Queue.clear();
    m_CurrentIndex = -1;
}

void MonitorView::PlayIndex(int index, Core::VLCVideoLayer* player) {
    if (index < 0 || index >= static_cast<int>(m_Queue.size())) return;

    m_CurrentIndex = index;
    QueueEntry& e  = m_Queue[index];
    e.played       = true;

    m_CurrentTitle = e.title;
    m_CurrentPath  = e.fullPath;
    m_CurrentType  = e.type;

    if (IsVideoOrImage(e)) {
        DualPlay(player, e.fullPath);
    } else {
        DualStop(player);
        m_IsPlaying     = false;
        m_SelectedVerse = -1;
    }
}

void MonitorView::PlayNext(Core::VLCVideoLayer* player) {
    if (m_Queue.empty()) return;
    int next = m_CurrentIndex + 1;
    if (next >= static_cast<int>(m_Queue.size())) {
        if (m_IsLoopingQueue) next = 0;
        else                  return;
    }
    PlayIndex(next, player);
}

void MonitorView::PlayPrev(Core::VLCVideoLayer* player) {
    if (m_Queue.empty()) return;
    int prev = m_CurrentIndex - 1;
    if (prev < 0)
        prev = m_IsLoopingQueue ? static_cast<int>(m_Queue.size()) - 1 : 0;
    PlayIndex(prev, player);
}

// ─────────────────────────────────────────────────────────────────────────────
//  VU-Meter
// ─────────────────────────────────────────────────────────────────────────────

void MonitorView::DrawVUMeter(const char* id, float level, float peak,
                               float width, float height) {
    constexpr int   kSegs = 20;
    constexpr float kGap  = 2.f;

    ImGui::PushID(id);
    ImVec2      pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl  = ImGui::GetWindowDrawList();

    const float segH = (height - kGap * (kSegs - 1)) / static_cast<float>(kSegs);
    const int   lit  = static_cast<int>(std::clamp(level, 0.f, 1.f) * kSegs);
    const int   pk   = static_cast<int>(std::clamp(peak,  0.f, 1.f) * kSegs);

    for (int i = 0; i < kSegs; ++i) {
        float  y  = pos.y + height - static_cast<float>(i + 1) * (segH + kGap) + kGap;
        ImVec2 p0 { pos.x,         y        };
        ImVec2 p1 { pos.x + width, y + segH };

        ImVec4 col = (i >= kSegs - 3) ? kVuR
                   : (i >= kSegs - 7) ? kVuY : kVuG;
        bool active = (i < lit) || (i == pk && pk > 0);
        col.w = active ? 1.0f : 0.10f;
        dl->AddRectFilled(p0, p1, U32(col), 1.5f);
    }

    ImGui::Dummy(ImVec2(width, height));
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Boton de transporte
// ─────────────────────────────────────────────────────────────────────────────

bool MonitorView::TransportButton(const char* label, const char* tooltip,
                                   float w, float h, bool highlighted,
                                   ImVec4 hlColor) {
    if (highlighted) {
        ImGui::PushStyleColor(ImGuiCol_Button,        U32(hlColor));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Mix(hlColor, kTxt0, 0.12f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(Mix(hlColor, kBg0, 0.30f)));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        U32(kBg2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kBg2Hov));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    bool clicked = ImGui::Button(label, ImVec2(w, h));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

// ═════════════════════════════════════════════════════════════════════════════
//  RENDER PRINCIPAL
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::Render(Core::VLCVideoLayer* player) {

    // ── Cache de dispositivos de audio ───────────────────────────────────────
    if (m_AudioDevices.empty() && player) {
        m_AudioDevices = player->GetAvailableAudioDevices();
        if (!m_AudioDevices.empty()) {
            m_MonitorDeviceId = m_AudioDevices[0].id;
            m_PublicDeviceId  = m_AudioDevices.size() > 1
                              ? m_AudioDevices[1].id
                              : m_AudioDevices[0].id;
        }
    }

    // ── Sincronizar seleccion de biblioteca ──────────────────────────────────
    {
        auto& core      = Core::PresentationCore::Get();
        auto  selection = core.GetSelection();

        if (!selection.title.empty()) {
            bool alreadyQueued = false;
            for (const auto& e : m_Queue)
                if (e.title == selection.title) { alreadyQueued = true; break; }

            if (!alreadyQueued) {
                QueueEntry entry;
                entry.title  = selection.title;
                entry.played = false;

                if (selection.type == Core::ItemType::Video) {
                    entry.type     = "video";
                    entry.fullPath = "assets/videos/" + selection.title;
                } else if (selection.type == Core::ItemType::Image) {
                    entry.type     = "image";
                    entry.fullPath = "assets/images/" + selection.title;
                } else if (selection.type == Core::ItemType::Song) {
                    entry.type     = "song";
                    entry.fullPath = "";
                }

                EnqueueItem(entry);
            }

            if (m_CurrentIndex < 0 && !m_Queue.empty())
                PlayIndex(static_cast<int>(m_Queue.size()) - 1, player);
        }
    }

    // ── Volumen ──────────────────────────────────────────────────────────────
    {
        const int vol = static_cast<int>(m_IsMuted ? 0.f : m_Volume * 100.f);
        DualSetVolume(player, vol);
    }

    // ── Avance automatico ────────────────────────────────────────────────────
    if (player && m_IsPlaying && m_AutoAdvance && m_CurrentType == "video") {
        const int64_t lenMs = player->GetLength();
        const int64_t curMs = player->GetTime();
        if (lenMs > 0 && curMs >= lenMs - 300) {
            if (m_IsLooping) {
                DualSeek(player, 0.f);
                DualPause(player, false);
            } else {
                PlayNext(player);
            }
        }
    }

    // ── Suavizado VU-meter ───────────────────────────────────────────────────
    {
        float rawLevel = (player && m_IsPlaying) ? 0.42f : 0.f;
        const float dt      = ImGui::GetIO().DeltaTime;
        const float attack  = 1.f - std::exp(-dt * 30.f);
        const float release = 1.f - std::exp(-dt * 4.f);
        m_VULevel += (rawLevel > m_VULevel ? attack : release) * (rawLevel - m_VULevel);
        if (m_VULevel > m_VUPeak) {
            m_VUPeak  = m_VULevel;
            m_VUDecay = 0.f;
        } else {
            m_VUDecay += dt;
            if (m_VUDecay > 1.5f)
                m_VUPeak = std::max(0.f, m_VUPeak - dt * 0.15f);
        }
    }

    // ── Layout principal ─────────────────────────────────────────────────────
    const float availW = ImGui::GetContentRegionAvail().x;
    
    // Hemos ensanchado la cola cuando se muestra (38% del ancho) para dar espacio a la cuadrícula.
    const float queueW = m_ShowQueue ? std::max(280.f, availW * 0.38f) : 0.f;
    const float mainW  = availW - queueW - (m_ShowQueue ? 8.f : 0.f);

    ImGui::BeginGroup();
    {
        RenderTopBar();
        OBSSeparator();
        ImGui::Spacing();
        RenderVideoPreview(player, mainW);
        ImGui::Spacing();

        if (m_CurrentType == "song")
            RenderSongPanel(player);
        else
            RenderTransportBar(player);

        OBSSeparator();
        ImGui::Spacing();
        RenderAudioSection(player);
        OBSSeparator();
        ImGui::Spacing();
        RenderStatusBar(player);
    }
    ImGui::EndGroup();

    if (m_ShowQueue) {
        ImGui::SameLine(0.f, 8.f);
        ImGui::BeginGroup();
        RenderQueue(player);
        ImGui::EndGroup();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  TOP BAR
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderTopBar() {
    auto& core = Core::PresentationCore::Get();

    if (core.GetState().isProjecting) {
        ImGui::PushStyleColor(ImGuiCol_Button,        U32(kRed));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(Mix(kRed, kTxt0, 0.1f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kRed));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
        ImGui::SmallButton(" LIVE ");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0.f, 8.f);
    }

    if (!m_CurrentTitle.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt0));
        ImGui::TextUnformatted(TruncateLabel(m_CurrentTitle, 48).c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt2));
        ImGui::TextUnformatted("Sin elemento activo");
        ImGui::PopStyleColor();
    }

    const float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(availW - 100.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);

    {
        ImGui::PushStyleColor(ImGuiCol_Button,        m_ShowQueue ? U32(kAccentDim) : U32(kBg2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kBg2Hov));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
        if (ImGui::SmallButton(" Cola ")) m_ShowQueue = !m_ShowQueue;
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine(0.f, 4.f);
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        m_ShowAudio ? U32(kAccentDim) : U32(kBg2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kBg2Hov));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
        if (ImGui::SmallButton(" Audio ")) m_ShowAudio = !m_ShowAudio;
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar();
    ImGui::Spacing();
}

// ═════════════════════════════════════════════════════════════════════════════
//  VIDEO PREVIEW
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderVideoPreview(Core::VLCVideoLayer* player, float width) {
    const float height = width / m_PreviewAspect;
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      pos    = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(pos, { pos.x + width, pos.y + height },
                      IM_COL32(6, 6, 6, 255), 4.f);

    if (player) {
        void* tid = player->GetTextureID();
        if (tid) {
            dl->AddImage(reinterpret_cast<ImTextureID>(tid),
                         pos, { pos.x + width, pos.y + height },
                         ImVec2(0, 0), ImVec2(1, 1));
        } else {
            ImVec2 c { pos.x + width * 0.5f, pos.y + height * 0.5f };
            const char* msg = m_IsPlaying ? "Decodificando..." : "Sin senal";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText({ c.x - ts.x * 0.5f, c.y - ts.y * 0.5f },
                        U32(kTxt2), msg);
        }
    }

    auto&  core   = Core::PresentationCore::Get();
    bool   isLive = core.GetState().isProjecting;
    ImU32  bColor = isLive      ? U32(kRed)
                  : m_IsPlaying ? U32(kGreen)
                  :               U32(kBorder);
    float  bWidth = (isLive || m_IsPlaying) ? 2.f : 1.f;
    dl->AddRect(pos, { pos.x + width, pos.y + height }, bColor, 4.f, 0, bWidth);

    if (isLive || m_IsPlaying) {
        const char* tag    = isLive ? "LIVE" : "PREV";
        ImVec4      tagCol = isLive ? kRed : kGreen;
        ImVec2      tp     = { pos.x + 8.f, pos.y + 8.f };
        dl->AddRectFilled(tp, { tp.x + 42.f, tp.y + 18.f }, U32(tagCol), 3.f);
        dl->AddText({ tp.x + 5.f, tp.y + 2.f }, IM_COL32(255, 255, 255, 255), tag);
    }

    ImGui::Dummy(ImVec2(width, height));
}

// ═════════════════════════════════════════════════════════════════════════════
//  TRANSPORT BAR
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderTransportBar(Core::VLCVideoLayer* player) {
    if (!player) return;

    float   posNorm = player->GetPosition();
    int64_t curMs   = player->GetTime();
    int64_t lenMs   = player->GetLength();

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       U32(kAccent));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, U32(Mix(kAccent, kTxt0, 0.2f)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          U32(kBg1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   U32(kBg2Hov));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  4.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::SliderFloat("##tl", &posNorm, 0.f, 1.f, ""))
        DualSeek(player, posNorm);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
    const int64_t remainMs = lenMs > 0 ? lenMs - curMs : 0LL;
    ImGui::Text(" %s", FormatTime(curMs).c_str());
    ImGui::SameLine();
    ImGui::Text("-%s", FormatTime(remainMs).c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 52.f);
    ImGui::Text("%s", FormatTime(lenMs).c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();

    constexpr float bW  = 32.f;
    constexpr float bH  = 26.f;
    constexpr float bPb = 44.f;

    constexpr float totalW = bW * 6.f + bPb + 4.f * 7.f + 14.f + bW * 2.f;
    const float     startX = ImGui::GetCursorPosX()
                           + (ImGui::GetContentRegionAvail().x - totalW) * 0.5f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), startX));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.f, 4.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

    if (TransportButton("|<",  "Ir al inicio",   bW, bH)) DualStop(player);
    ImGui::SameLine();
    if (TransportButton("|<<", "Anterior",       bW, bH)) PlayPrev(player);
    ImGui::SameLine();
    if (TransportButton("-10", "Retroceder 10s", bW, bH)) {
        if (lenMs > 0)
            DualSeek(player, static_cast<float>(std::max(0LL, curMs - 10000LL))
                             / static_cast<float>(lenMs));
    }
    ImGui::SameLine();

    if (m_IsPlaying) {
        if (TransportButton("||",  "Pausar",     bPb, bH, true, kYellow))
            DualPause(player, true);
    } else {
        if (TransportButton(" > ", "Reproducir", bPb, bH, true, kGreen))
            DualPause(player, false);
    }
    ImGui::SameLine();

    if (TransportButton("+10", "Avanzar 10s", bW, bH)) {
        if (lenMs > 0)
            DualSeek(player, static_cast<float>(std::min(lenMs, curMs + 10000LL))
                             / static_cast<float>(lenMs));
    }
    ImGui::SameLine();
    if (TransportButton(">>|", "Siguiente",   bW, bH)) PlayNext(player);
    ImGui::SameLine();
    if (TransportButton("[]",  "Detener",     bW, bH)) DualStop(player);
    ImGui::SameLine(0.f, 14.f);

    if (TransportButton("1x", "Repetir item", bW, bH, m_IsLooping,      kAccent))
        m_IsLooping = !m_IsLooping;
    ImGui::SameLine();
    if (TransportButton("Qx", "Repetir cola", bW, bH, m_IsLoopingQueue, kAccent))
        m_IsLoopingQueue = !m_IsLoopingQueue;

    ImGui::PopStyleVar(2);
    ImGui::Spacing();
}

// ═════════════════════════════════════════════════════════════════════════════
//  AUDIO SECTION
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderAudioSection(Core::VLCVideoLayer* player) {
    if (!m_ShowAudio) return;

    ImGui::Spacing();
    SectionLabel("  AUDIO");
    ImGui::Spacing();

    constexpr float vuW = 10.f;
    constexpr float vuH = 56.f;

    DrawVUMeter("vu_L", m_VULevel,         m_VUPeak,         vuW, vuH);
    ImGui::SameLine(0.f, 3.f);
    DrawVUMeter("vu_R", m_VULevel * 0.94f, m_VUPeak * 0.94f, vuW, vuH);
    ImGui::SameLine(0.f, 10.f);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       U32(kAccent));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, U32(Mix(kAccent, kTxt0, 0.2f)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,          U32(kBg1));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,   U32(kBg2Hov));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  3.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
    ImGui::SetNextItemWidth(110.f);
    if (ImGui::SliderFloat("##vol", &m_Volume, 0.f, 1.f, ""))
        DualSetVolume(player, static_cast<int>(m_IsMuted ? 0.f : m_Volume * 100.f));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Volumen: %d%%", static_cast<int>(m_Volume * 100.f));

    ImGui::SameLine(0.f, 6.f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        m_IsMuted ? U32(kRed) : U32(kBg2));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_IsMuted ? U32(Mix(kRed, kTxt0, 0.1f)) : U32(kBg2Hov));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
    if (ImGui::Button(m_IsMuted ? "MUT" : " M ", ImVec2(36.f, 22.f))) {
        m_IsMuted = !m_IsMuted;
        DualSetVolume(player, static_cast<int>(m_IsMuted ? 0.f : m_Volume * 100.f));
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Silenciar");

    ImGui::SameLine(0.f, 6.f);
    ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
    ImGui::Text("%3d%%", static_cast<int>(m_Volume * 100.f));
    ImGui::PopStyleColor();

    if (!m_AudioDevices.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);

        auto FindDescription = [&](const std::string& activeId) -> std::string {
            for (const auto& d : m_AudioDevices)
                if (d.id == activeId) return d.description;
            return activeId;
        };

        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
        ImGui::TextUnformatted("  Monitor:");
        ImGui::PopStyleColor();
        ImGui::SameLine(90.f);
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##devMonitor",
            m_MonitorDeviceId.empty() ? "Defecto" : TruncateLabel(FindDescription(m_MonitorDeviceId), 22).c_str()))
        {
            for (const auto& dev : m_AudioDevices) {
                bool sel = (dev.id == m_MonitorDeviceId);
                if (ImGui::Selectable(TruncateLabel(dev.description, 30).c_str(), sel)) {
                    m_MonitorDeviceId = dev.id;
                    ApplyAudioDevice(player);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
        ImGui::TextUnformatted("  Público:");
        ImGui::PopStyleColor();
        ImGui::SameLine(90.f);
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::BeginCombo("##devPublic",
            m_PublicDeviceId.empty() ? "Defecto" : TruncateLabel(FindDescription(m_PublicDeviceId), 22).c_str()))
        {
            for (const auto& dev : m_AudioDevices) {
                bool sel = (dev.id == m_PublicDeviceId);
                if (ImGui::Selectable(TruncateLabel(dev.description, 30).c_str(), sel)) {
                    m_PublicDeviceId = dev.id;
                    ApplyAudioDevice(player);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar();
    }
    ImGui::Spacing();
}

// ═════════════════════════════════════════════════════════════════════════════
//  COLA (DISEÑO GRID)
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderQueue(Core::VLCVideoLayer* player) {
    const float kW = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, U32(kBg1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.f);
    ImGui::BeginChild("##Queue", ImVec2(kW, ImGui::GetContentRegionAvail().y),
                      true, ImGuiWindowFlags_NoScrollbar);

    ImGui::Spacing();
    SectionLabel("  COLA");
    ImGui::SameLine(kW - 68.f);

    ImGui::PushStyleColor(ImGuiCol_Button,        U32(kBg2));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kBg2Hov));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
    if (ImGui::SmallButton("Limpiar")) {
        ClearQueue();
        DualStop(player);
        m_CurrentTitle.clear();
        m_CurrentPath.clear();
        m_CurrentType.clear();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    OBSSeparator();
    ImGui::Spacing();

    if (m_Queue.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt2));
        ImGui::TextWrapped("  Selecciona un elemento en la biblioteca.");
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::BeginChild("##QItems", ImVec2(0.f, ImGui::GetContentRegionAvail().y), false);

    int toDelete = -1;
    int swapA    = -1;
    int swapB    = -1;

    // Calculamos columnas dinámicas para la cuadrícula de la cola
    const float itemMinW = 135.f; 
    int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / itemMinW));

    if (ImGui::BeginTable("QueueGridTable", cols, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp)) {
        for (int i = 0; i < static_cast<int>(m_Queue.size()); ++i) {
            ImGui::TableNextColumn();
            ImGui::PushID(i);

            const QueueEntry& e  = m_Queue[i];
            const bool isCurr    = (i == m_CurrentIndex);
            const bool wasPlayed = e.played && !isCurr;

            ImVec2      itemPos = ImGui::GetCursorScreenPos();
            const float itemW   = ImGui::GetContentRegionAvail().x - 4.f; // Ajustar al ancho de la celda
            constexpr float itemH = 58.f; // Alto fijo de tarjeta




            ImGui::Dummy(ImVec2(itemW, itemH));
            ImGui::SetCursorScreenPos(itemPos);




            // Fondo de la tarjeta
            ImU32 bg = isCurr ? U32(kGreenDim) : wasPlayed ? IM_COL32(18, 18, 18, 255) : U32(kBg2);
            ImGui::GetWindowDrawList()->AddRectFilled(
                itemPos, { itemPos.x + itemW, itemPos.y + itemH }, bg, 4.f);
            
            // Borde verde si está activo
            if (isCurr)
                ImGui::GetWindowDrawList()->AddRect(
                    itemPos, { itemPos.x + itemW, itemPos.y + itemH },
                    U32(kGreen), 4.f, 0, 1.5f);

            // Botón invisible que cubre toda la tarjeta para darle Play
            ImGui::SetCursorScreenPos(itemPos);
            if (ImGui::InvisibleButton("##row", ImVec2(itemW, itemH - 20.f))) 
                PlayIndex(i, player);
            
            if (ImGui::IsItemHovered()) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    itemPos, { itemPos.x + itemW, itemPos.y + itemH },
                    IM_COL32(255, 255, 255, 6), 4.f);
                ImGui::SetTooltip("Reproducir: %s", e.title.c_str());
            }

            // Textos decorativos
            ImGui::SetCursorScreenPos({ itemPos.x + 6.f, itemPos.y + 6.f });
            ImGui::PushStyleColor(ImGuiCol_Text, U32(isCurr ? kGreen : kTxt2));
            ImGui::Text("%02d", i + 1);
            ImGui::PopStyleColor();

            const char* icon = (e.type == "video") ? "[V]" : (e.type == "image") ? "[I]" : "[S]";
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt2));
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();

            // Título
            ImGui::SetCursorScreenPos({ itemPos.x + 6.f, itemPos.y + 24.f });
            ImGui::PushStyleColor(ImGuiCol_Text, U32(wasPlayed ? kTxt2 : kTxt0));
            ImGui::TextUnformatted(TruncateLabel(e.title, static_cast<int>(itemW / 8.f)).c_str());
            ImGui::PopStyleColor();

            // Controles de miniatura (Prev/Next/Eliminar)
            constexpr float bSz = 16.f;
            const float bx = itemPos.x + itemW - (bSz * 3.f) - 6.f;
            const float by = itemPos.y + itemH - bSz - 4.f;

            auto SmallQueueBtn = [&](const char* lbl, float x, float y, float w, float h) -> bool {
                ImGui::SetCursorScreenPos({ x, y });
                ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kBg2Hov));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  U32(kBg2Act));
                ImGui::PushStyleColor(ImGuiCol_Text,          U32(kTxt1));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
                bool r = ImGui::Button(lbl, ImVec2(w, h));
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
                return r;
            };

            // Mover hacia atrás en la cola
            if (SmallQueueBtn("<", bx, by, bSz, bSz) && i > 0)
                { swapA = i; swapB = i - 1; }
            
            // Mover hacia adelante en la cola
            if (SmallQueueBtn(">", bx + bSz + 2.f, by, bSz, bSz) && i < static_cast<int>(m_Queue.size()) - 1)
                { swapA = i; swapB = i + 1; }

            // Botón eliminar
            ImGui::SetCursorScreenPos({ bx + (bSz * 2.f) + 4.f, by });
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, U32(kRed));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text,          U32(kTxt2));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
            if (ImGui::Button("x", ImVec2(bSz, bSz))) toDelete = i;
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);

            // Reservar el espacio exacto para la tarjeta en la celda del Table
            ImGui::SetCursorScreenPos({ itemPos.x, itemPos.y + itemH + 4.f });
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (toDelete >= 0) {
        m_Queue.erase(m_Queue.begin() + toDelete);
        if      (m_CurrentIndex > toDelete)    --m_CurrentIndex;
        else if (m_CurrentIndex == toDelete)     m_CurrentIndex = -1;
    }
    if (swapA >= 0 && swapB >= 0) {
        std::swap(m_Queue[swapA], m_Queue[swapB]);
        if      (m_CurrentIndex == swapA) m_CurrentIndex = swapB;
        else if (m_CurrentIndex == swapB) m_CurrentIndex = swapA;
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ═════════════════════════════════════════════════════════════════════════════
//  SONG PANEL (DISEÑO GRID)
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderSongPanel(Core::VLCVideoLayer* player) {
    (void)player;
    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.GetSelection();

    const float availH = std::max(50.f, ImGui::GetContentRegionAvail().y - 40.f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, U32(kBg1));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.f);
    ImGui::BeginChild("##Verses", ImVec2(0.f, availH), false);

    // Calculamos columnas automáticas según el ancho disponible (aprox 200px por tarjeta de estrofa)
    const float minCardWidth = 200.f;
    int cols = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / minCardWidth));

    if (ImGui::BeginTable("VersesGridTable", cols, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp)) {
        for (int i = 0; i < static_cast<int>(selection.contentData.size()); ++i) {
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            
            const bool        sel   = (m_SelectedVerse == i);
            const std::string& verse = selection.contentData[i];

            ImVec2      bPos = ImGui::GetCursorScreenPos();
            const float bW   = ImGui::GetContentRegionAvail().x - 4.f;
            constexpr float bH = 75.f; // Más alto para que quepa bien el texto de la estrofa

            // Dibujar fondo tarjeta
            ImGui::GetWindowDrawList()->AddRectFilled(
                bPos, { bPos.x + bW, bPos.y + bH },
                sel ? U32(kGreenDim) : U32(kBg2), 6.f);
            
            if (sel)
                ImGui::GetWindowDrawList()->AddRect(
                    bPos, { bPos.x + bW, bPos.y + bH },
                    U32(kGreen), 6.f, 0, 1.5f);

            // Botón interactivo
            ImGui::SetCursorScreenPos(bPos);
            if (ImGui::InvisibleButton("##v", ImVec2(bW, bH))) {
                core.SetLayer2_Text(verse);
                core.SetProjecting(true);
                m_SelectedVerse = i;
            }
            if (ImGui::IsItemHovered())
                ImGui::GetWindowDrawList()->AddRectFilled(
                    bPos, { bPos.x + bW, bPos.y + bH },
                    IM_COL32(255, 255, 255, 6), 6.f);

            // Título de la tarjeta (Número de estrofa)
            ImGui::SetCursorScreenPos({ bPos.x + 8.f, bPos.y + 6.f });
            ImGui::PushStyleColor(ImGuiCol_Text, U32(sel ? kGreen : kTxt2));
            ImGui::Text("Estrofa %d", i + 1);
            ImGui::PopStyleColor();

            // Texto de la estrofa dentro de la tarjeta
            ImGui::SetCursorScreenPos({ bPos.x + 8.f, bPos.y + 24.f });
            ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt0));
            // Hacemos que el texto se envuelva dentro de la tarjeta
            ImGui::PushTextWrapPos(bPos.x + bW - 8.f);
            ImGui::TextUnformatted(TruncateLabel(verse, 90).c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            // Avanzar el cursor para que ImGui sepa cuánto mide la fila
            ImGui::SetCursorScreenPos({ bPos.x, bPos.y + bH + 6.f });
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ═════════════════════════════════════════════════════════════════════════════
//  STATUS BAR
// ═════════════════════════════════════════════════════════════════════════════

void MonitorView::RenderStatusBar(Core::VLCVideoLayer* player) {
    (void)player;

    ImGui::PushStyleColor(ImGuiCol_Text, U32(m_IsPlaying ? kGreen : kTxt2));
    ImGui::TextUnformatted(m_IsPlaying ? "  Reproduciendo" : "  En pausa");
    ImGui::PopStyleColor();

    if (!m_CurrentTitle.empty()) {
        ImGui::SameLine(0.f, 6.f);
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt1));
        ImGui::Text("- %s", TruncateLabel(m_CurrentTitle, 38).c_str());
        ImGui::PopStyleColor();
    }

    if (!m_Queue.empty() && m_CurrentIndex >= 0) {
        ImGui::SameLine();
        const float rightEdge = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightEdge - 60.f);
        ImGui::PushStyleColor(ImGuiCol_Text, U32(kTxt2));
        ImGui::Text("%d / %d", m_CurrentIndex + 1, static_cast<int>(m_Queue.size()));
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
}

} // namespace ProyecThor::UI