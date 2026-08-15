#include "AudioDawPanel.h"
#include "DesignSystem.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "backend/core/FfmpegPath.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

#if defined(_WIN32)
#include "backend/core/HiddenProcess.h"
#endif

namespace ProyecThor::UI {

namespace fs = std::filesystem;

namespace {
constexpr float kTrackHeaderW = 170.0f;
constexpr float kTrackH       = 64.0f;
constexpr float kTrackGap     = 4.0f;
constexpr int   kTrackCount   = 4;

const char* kFormatLabels[4] = { "WAV", "MP3", "AAC (.m4a)", "OGG" };
const char* kFormatExt[4]    = { ".wav", ".mp3", ".m4a", ".ogg" };

double ParseDurationLine(const std::string& line) {
    auto pos = line.find("Duration:");
    if (pos == std::string::npos) return -1.0;
    int h = 0, m = 0; double s = 0.0;
    if (std::sscanf(line.c_str() + pos, "Duration: %d:%d:%lf", &h, &m, &s) == 3)
        return h * 3600.0 + m * 60.0 + s;
    return -1.0;
}

// ── Boton animado (hover/press lerp, highlight superior) ────────────────────
// Reemplaza los DS::GlassButton planos de antes -- pedido explicito
// ("mejora los botones y profezionaliza la UI"). Mismo patron ya establecido
// en QueueActionButton (MonitorQueueHelpers.h) / BroadcastAnimT
// (BroadcastPanel.cpp), reescrito local a proposito (mismo criterio de
// "helper chico duplicado" que el resto de la app).
float DawAnimT(ImGuiID id, ImU32 salt, bool target, float speed = 14.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float*        t       = storage->GetFloatRef(id ^ salt, target ? 1.0f : 0.0f);
    *t += ((target ? 1.0f : 0.0f) - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

bool DawActionButton(const char* strId, const char* label, ImVec2 size, ImU32 accent, bool active = false)
{
    ImGui::PushID(strId);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = { p0.x + size.x, p0.y + size.y };

    ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    bool pressed = ImGui::IsItemClicked();

    float t = DawAnimT(ImGui::GetID("##btn"), 0xD3u, hovered || active, 14.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 accentF = ImGui::ColorConvertU32ToFloat4(accent);
    float  baseA   = active ? 0.42f : 0.16f;
    float  hovA    = active ? 0.55f : 0.30f;
    float  a       = baseA + (hovA - baseA) * t;
    if (held) a *= 0.85f;

    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(ImVec4(accentF.x, accentF.y, accentF.z, a)), 8.0f);
    dl->AddRectFilled(p0, { p1.x, p0.y + size.y * 0.42f },
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.06f + t * 0.08f)), 8.0f, ImDrawFlags_RoundCornersTop);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(ImVec4(accentF.x, accentF.y, accentF.z, 0.35f + t * 0.35f)), 8.0f);

    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({ p0.x + (size.x - ts.x) * 0.5f, p0.y + (size.y - ts.y) * 0.5f },
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.90f)), label);

    ImGui::PopID();
    return pressed;
}
} // namespace

AudioDawPanel::AudioDawPanel()
{
    m_Tracks.resize(kTrackCount);
    static const char* kNames[kTrackCount] = { "Pista 1", "Pista 2", "Pista 3", "Pista 4" };
    for (int i = 0; i < kTrackCount; i++) m_Tracks[i].name = kNames[i];
}

AudioDawPanel::~AudioDawPanel()
{
    if (m_Recorder.IsRecording()) m_Recorder.Stop();
}

int64_t AudioDawPanel::ProbeDurationMs(const std::string& path)
{
#if defined(_WIN32)
    std::string cmd = Core::FfmpegPath() + " -hide_banner -i \"" + path + "\"";
    FILE* pipe = nullptr;
    void* proc = nullptr;
    double duration = -1.0;
    if (Core::StartHiddenProcess(cmd, false, nullptr, true, &pipe, &proc)) {
        char buf[512];
        while (pipe && std::fgets(buf, sizeof(buf), pipe)) {
            double d = ParseDurationLine(buf);
            if (d > 0.0) duration = d;
        }
        if (pipe) std::fclose(pipe);
        Core::WaitHiddenProcess(proc);
    }
    return duration > 0.0 ? (int64_t)(duration * 1000.0) : 0;
#else
    (void)path;
    return 0;
#endif
}

void AudioDawPanel::RefreshMediaList()
{
    m_MediaList.clear();
    static const char* kExts[] = { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" };

    std::error_code ec;
    const std::string& dir = ProyecThor::Audio::GetAudioPath();
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = (char)tolower((unsigned char)c);
        bool match = false;
        for (const char* e : kExts) if (ext == e) { match = true; break; }
        if (!match) continue;
        m_MediaList.push_back({ entry.path().string(), entry.path().stem().string() });
    }
    m_MediaListLoaded = true;
}

void AudioDawPanel::RefreshInputDevices()
{
    m_InputDevices = Core::ListAudioInputDevices();
    m_InputDevicesLoaded = true;
}

void AudioDawPanel::AddClipToTrack(int trackIdx, const std::string& path, const std::string& displayName)
{
    if (trackIdx < 0 || trackIdx >= (int)m_Tracks.size()) return;

    int64_t durMs = ProbeDurationMs(path);
    if (durMs <= 0) durMs = 5000; // no se pudo sondear -- valor por defecto, el operador puede recortar a mano

    DawClip clip;
    clip.sourcePath   = path;
    clip.displayName  = displayName;
    clip.sourceOffsetMs = 0;
    clip.durationMs    = durMs;
    clip.sourceTotalMs = durMs;

    auto& track = m_Tracks[trackIdx];
    int64_t endMs = 0;
    for (const auto& c : track.clips)
        endMs = std::max(endMs, c.timelinePosMs + c.durationMs);
    clip.timelinePosMs = endMs;

    track.clips.push_back(clip);
}

void AudioDawPanel::PreviewClip(const DawClip& clip)
{
    m_PreviewPlayer.Play(clip.sourcePath, false, false);
    int64_t len = m_PreviewPlayer.GetLength();
    if (len > 0)
        m_PreviewPlayer.SetPosition((float)clip.sourceOffsetMs / (float)len);
    m_PreviewPlayer.SetPause(false);
    m_PreviewPlaying = true;
}

void AudioDawPanel::StopPreview()
{
    m_PreviewPlayer.Stop();
    m_PreviewPlaying = false;
}

int AudioDawPanel::FindClipAt(int trackIdx, int64_t posMs) const
{
    if (trackIdx < 0 || trackIdx >= (int)m_Tracks.size()) return -1;
    const auto& clips = m_Tracks[trackIdx].clips;
    for (int i = 0; i < (int)clips.size(); i++) {
        const auto& c = clips[i];
        if (posMs >= c.timelinePosMs && posMs < c.timelinePosMs + c.durationMs)
            return i;
    }
    return -1;
}

void AudioDawPanel::StartMixPlayback()
{
    if (m_MixPlaying) return;
    StopPreview(); // evita dos fuentes sonando a la vez

    // Si el cabezal quedo en o despues del final del proyecto (llego al
    // final la vez anterior), se reinicia a 0 -- si no, "Reproducir todo"
    // no arrancaria nada (ver UpdateMixPlayback, corta apenas pasa el final).
    int64_t projectEndMs = 0;
    for (const auto& t : m_Tracks)
        for (const auto& c : t.clips)
            projectEndMs = std::max(projectEndMs, c.timelinePosMs + c.durationMs);
    if (m_MixPlayheadMs >= (double)projectEndMs) m_MixPlayheadMs = 0.0;

    m_TrackPlayers.resize(m_Tracks.size());
    m_TrackActiveClip.assign(m_Tracks.size(), -1);
    for (auto& p : m_TrackPlayers)
        if (!p) p = std::make_unique<Core::VLCBasePlayer>();

    m_MixPlaying        = true;
    m_MixLastFrameTime  = ImGui::GetTime();
    UpdateMixPlayback(); // arranca los clips que correspondan en el instante actual, sin esperar al proximo frame
}

void AudioDawPanel::StopMixPlayback()
{
    m_MixPlaying = false;
    for (int t = 0; t < (int)m_TrackPlayers.size(); t++) {
        if (m_TrackPlayers[t]) m_TrackPlayers[t]->SetPause(true);
        if (t < (int)m_TrackActiveClip.size()) m_TrackActiveClip[t] = -1;
    }
}

void AudioDawPanel::UpdateMixPlayback()
{
    if (!m_MixPlaying) return;

    double now = ImGui::GetTime();
    double dt  = std::max(0.0, now - m_MixLastFrameTime);
    m_MixLastFrameTime = now;
    m_MixPlayheadMs += dt * 1000.0;

    int64_t projectEndMs = 0;
    bool anyClip = false;
    for (int t = 0; t < (int)m_Tracks.size(); t++) {
        for (const auto& c : m_Tracks[t].clips) {
            projectEndMs = std::max(projectEndMs, c.timelinePosMs + c.durationMs);
            anyClip = true;
        }

        if (t >= (int)m_TrackPlayers.size() || !m_TrackPlayers[t]) continue;
        auto& player = *m_TrackPlayers[t];

        int clipIdx = m_Tracks[t].muted ? -1 : FindClipAt(t, (int64_t)m_MixPlayheadMs);
        if (clipIdx != m_TrackActiveClip[t]) {
            if (clipIdx < 0) {
                player.SetPause(true);
            } else {
                const auto& clip = m_Tracks[t].clips[clipIdx];
                player.Play(clip.sourcePath, false, false);
                int64_t posInClip = (int64_t)m_MixPlayheadMs - clip.timelinePosMs + clip.sourceOffsetMs;
                int64_t len = player.GetLength();
                if (len > 0)
                    player.SetPosition(std::clamp((float)posInClip / (float)len, 0.0f, 1.0f));
                player.SetPause(false);
            }
            m_TrackActiveClip[t] = clipIdx;
        }
    }

    if (!anyClip || (int64_t)m_MixPlayheadMs > projectEndMs)
        StopMixPlayback();
}

void AudioDawPanel::Update()
{
    UpdateMixPlayback();

    bool success = false;
    std::string msg;
    if (m_Recorder.PollFinished(success, msg)) {
        m_StatusMessage = msg;
        m_StatusIsError = !success;
        if (success && !m_RecordingPath.empty()) {
            AddClipToTrack(m_SelectedTrack, m_RecordingPath, fs::path(m_RecordingPath).stem().string());
            m_MediaListLoaded = false; // se re-lista solo si esta grabado adentro de la carpeta de audio
        }
        m_RecordingPath.clear();
    }
    if (m_Mixdown.PollFinished(success, msg)) {
        m_StatusMessage = msg;
        m_StatusIsError = !success;
    }
}

void AudioDawPanel::Render()
{
    if (!m_MediaListLoaded) RefreshMediaList();
    if (!m_InputDevicesLoaded) RefreshInputDevices();
    if (!m_ExportPathInit) {
        std::string def = ProyecThor::Audio::GetAudioPath() + "/mezcla" + kFormatExt[m_ExportFormatIdx];
        std::snprintf(m_ExportPathBuf, sizeof(m_ExportPathBuf), "%s", def.c_str());
        m_ExportPathInit = true;
    }

    RenderTransport();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    float avail = ImGui::GetContentRegionAvail().y;
    float exportBarH = 60.0f;
    float bodyH = std::max(120.0f, avail - exportBarH);

    ImGui::BeginChild("##dawBody", ImVec2(0.0f, bodyH), false);
    RenderMediaPanel(200.0f, bodyH);
    ImGui::SameLine();
    ImGui::BeginChild("##dawTracks", ImVec2(0.0f, bodyH), false);
    RenderTracks();
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    RenderExportBar();
}

void AudioDawPanel::RenderTransport()
{
    bool hasSel = (m_SelClipTrack >= 0 && m_SelClipIdx >= 0 &&
                   m_SelClipTrack < (int)m_Tracks.size() &&
                   m_SelClipIdx < (int)m_Tracks[m_SelClipTrack].clips.size());

    // ── "Reproducir todo" (todas las pistas a la vez) ──────────────────────
    int totalClipsAll = 0;
    for (const auto& t : m_Tracks) totalClipsAll += (int)t.clips.size();
    ImGui::BeginDisabled(totalClipsAll == 0);
    if (m_MixPlaying) {
        if (DawActionButton("mixStop", "Detener todo", ImVec2(120.0f, 30.0f), DS::DangerColor, true))
            StopMixPlayback();
    } else {
        if (DawActionButton("mixPlay", "Reproducir todo", ImVec2(140.0f, 30.0f), DS::SuccessColor))
            StartMixPlayback();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 14.0f);
    ImGui::BeginDisabled(!hasSel || m_MixPlaying);
    if (m_PreviewPlaying) {
        if (DawActionButton("previewStop", "Detener", ImVec2(90.0f, 30.0f), DS::AccentColor)) StopPreview();
    } else {
        if (DawActionButton("previewPlay", "Reproducir clip", ImVec2(140.0f, 30.0f), DS::AccentColor) && hasSel)
            PreviewClip(m_Tracks[m_SelClipTrack].clips[m_SelClipIdx]);
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 10.0f);
    if (hasSel) {
        const auto& c = m_Tracks[m_SelClipTrack].clips[m_SelClipIdx];
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "%s (%.1fs)",
            c.displayName.c_str(), c.durationMs / 1000.0f);
        ImGui::SameLine(0.0f, 10.0f);
        if (DawActionButton("deleteClip", "Eliminar clip", ImVec2(120.0f, 28.0f), DS::DangerColor)) {
            m_Tracks[m_SelClipTrack].clips.erase(m_Tracks[m_SelClipTrack].clips.begin() + m_SelClipIdx);
            m_SelClipTrack = -1;
            m_SelClipIdx   = -1;
        }
    } else {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "Ningun clip seleccionado");
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    // ── Grabacion ────────────────────────────────────────────────────────
    bool recording = m_Recorder.IsRecording();
    ImGui::BeginDisabled(recording || m_InputDevices.empty());
    ImGui::SetNextItemWidth(240.0f);
    const char* curDevice = m_InputDevices.empty() ? "(sin microfonos)"
        : m_InputDevices[std::clamp(m_SelectedInputDevice, 0, (int)m_InputDevices.size() - 1)].name.c_str();
    if (ImGui::BeginCombo("##dawInputDevice", curDevice)) {
        for (int i = 0; i < (int)m_InputDevices.size(); i++) {
            bool sel = (i == m_SelectedInputDevice);
            if (ImGui::Selectable(m_InputDevices[i].name.c_str(), sel)) m_SelectedInputDevice = i;
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, 10.0f);
    if (!recording) {
        ImGui::BeginDisabled(m_InputDevices.empty());
        if (DawActionButton("recordStart", "Grabar", ImVec2(110.0f, 28.0f), DS::DangerColor)) {
            std::string recDir = ProyecThor::Audio::GetAudioPath() + "/grabaciones";
            std::error_code ec;
            fs::create_directories(fs::path(recDir), ec);
            std::time_t now = std::time(nullptr);
            std::tm lt{};
#ifdef _WIN32
            localtime_s(&lt, &now);
#else
            localtime_r(&now, &lt);
#endif
            char buf[64];
            std::strftime(buf, sizeof(buf), "grabacion_%Y%m%d_%H%M%S.wav", &lt);
            m_RecordingPath = recDir + "/" + buf;

            std::string err;
            if (!m_Recorder.Start(m_InputDevices[m_SelectedInputDevice].name, m_RecordingPath, &err)) {
                m_StatusMessage = err;
                m_StatusIsError = true;
                m_RecordingPath.clear();
            }
        }
        ImGui::EndDisabled();
    } else {
        if (DawActionButton("recordStop", "Detener grabación", ImVec2(160.0f, 28.0f), DS::DangerColor, true))
            m_Recorder.Stop();
        ImGui::SameLine(0.0f, 10.0f);
        float pulse = 0.5f + 0.5f * std::abs(sinf((float)ImGui::GetTime() * 3.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, pulse), "REC %.1fs", m_Recorder.GetElapsedSeconds());
    }

    if (!m_StatusMessage.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextColored(m_StatusIsError ? ImGui::ColorConvertU32ToFloat4(DS::DangerColor)
                                            : ImGui::ColorConvertU32ToFloat4(DS::SuccessColor),
                            "%s", m_StatusMessage.c_str());
    }
}

void AudioDawPanel::RenderMediaPanel(float w, float h)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::BeginChild("##dawMedia", ImVec2(w, h), true);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "MEDIA (Biblioteca > Audio)");
    ImGui::SameLine(std::max(0.0f, w - 66.0f));
    if (ImGui::SmallButton("Refrescar")) m_MediaListLoaded = false;
    ImGui::Separator();

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "Agrega a: %s",
        m_Tracks[std::clamp(m_SelectedTrack, 0, (int)m_Tracks.size() - 1)].name.c_str());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (m_MediaList.empty()) {
        ImGui::TextWrapped("Sin audio importado todavia. Agrega archivos desde Biblioteca > Audio.");
    }
    for (const auto& [path, name] : m_MediaList) {
        ImGui::PushID(path.c_str());
        float rowW = ImGui::GetContentRegionAvail().x;
        ImGui::TextWrapped("%s", name.c_str());
        if (DawActionButton("addClip", "+ Agregar a pista", ImVec2(rowW, 22.0f), DS::AccentColor))
            AddClipToTrack(m_SelectedTrack, path, name);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AudioDawPanel::RenderTracks()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float avail = ImGui::GetContentRegionAvail().x;
    float timelineW = std::max(200.0f, avail - kTrackHeaderW - 8.0f);

    // Regla de tiempo -- clickeable para mover el cabezal (ver
    // m_MixPlayheadMs, usado por "Reproducir todo"/UpdateMixPlayback).
    ImVec2 rulerOrigin = ImGui::GetCursorScreenPos();
    float  laneX0 = rulerOrigin.x + kTrackHeaderW + 8.0f;
    for (int s = 0; s * 1000.0f * m_PixelsPerMs < timelineW; s += 5) {
        float x = laneX0 + s * 1000.0f * m_PixelsPerMs;
        dl->AddLine({ x, rulerOrigin.y }, { x, rulerOrigin.y + 14.0f },
                    ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.25f)));
        char lbl[16]; std::snprintf(lbl, sizeof(lbl), "%ds", s);
        dl->AddText({ x + 2.0f, rulerOrigin.y }, DS::TextHint, lbl);
    }
    ImGui::SetCursorScreenPos({ laneX0, rulerOrigin.y });
    ImGui::InvisibleButton("##dawRuler", ImVec2(timelineW, 16.0f));
    if (ImGui::IsItemClicked() || (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
        float localX = ImGui::GetMousePos().x - laneX0;
        m_MixPlayheadMs = std::max(0.0f, localX / m_PixelsPerMs);
        if (m_MixPlaying) // fuerza a los reproductores a re-evaluar el clip activo en la nueva posicion
            for (int t = 0; t < (int)m_TrackActiveClip.size(); t++) m_TrackActiveClip[t] = -2;
    }
    ImGui::SetCursorScreenPos(rulerOrigin);
    ImGui::Dummy(ImVec2(timelineW + kTrackHeaderW, 16.0f));

    for (int t = 0; t < (int)m_Tracks.size(); t++) {
        DawTrack& track = m_Tracks[t];
        ImVec2 rowP0 = ImGui::GetCursorScreenPos();

        // ── Header de pista ──────────────────────────────────────────────
        bool trackSel = (m_SelectedTrack == t);
        ImVec2 hP0 = rowP0, hP1 = { rowP0.x + kTrackHeaderW, rowP0.y + kTrackH };
        dl->AddRectFilled(hP0, hP1, trackSel ? DS::AccentColorDim : DS::BtnDefaultFill, DS::RadiusSmall);
        dl->AddText({ hP0.x + 10.0f, hP0.y + 8.0f }, DS::TextPrimary, track.name.c_str());

        ImGui::SetCursorScreenPos(hP0);
        ImGui::PushID(t);
        ImGui::InvisibleButton("##trackHeader", ImVec2(kTrackHeaderW, kTrackH));
        if (ImGui::IsItemClicked()) m_SelectedTrack = t;

        ImGui::SetCursorScreenPos({ hP0.x + 10.0f, hP0.y + kTrackH - 26.0f });
        ImGui::Checkbox("Mute", &track.muted);
        ImGui::PopID();

        // ── Lane de la linea de tiempo ────────────────────────────────────
        ImVec2 lP0 = { hP1.x + 8.0f, rowP0.y };
        ImVec2 lP1 = { lP0.x + timelineW, rowP0.y + kTrackH };
        dl->AddRectFilled(lP0, lP1, ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.02f)), DS::RadiusSmall);

        for (int c = 0; c < (int)track.clips.size(); c++) {
            DawClip& clip = track.clips[c];
            ImVec2 cP0 = { lP0.x + clip.timelinePosMs * m_PixelsPerMs, lP0.y + 2.0f };
            ImVec2 cP1 = { cP0.x + std::max(6.0f, clip.durationMs * m_PixelsPerMs), lP1.y - 2.0f };

            bool isSel = (m_SelClipTrack == t && m_SelClipIdx == c);
            bool isDragTarget = isSel && m_DragMode != DragMode::None;

            ImVec2 mouse = ImGui::GetMousePos();
            bool hoveringWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            bool hoverBody = hoveringWindow && ImGui::IsMouseHoveringRect(cP0, cP1);
            const float handleW = 8.0f;
            bool hoverLeft  = hoverBody && (mouse.x - cP0.x) < handleW;
            bool hoverRight = hoverBody && (cP1.x - mouse.x) < handleW;

            if (m_DragMode == DragMode::None && hoverBody && !ImGui::IsAnyItemActive() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_SelClipTrack = t;
                m_SelClipIdx   = c;
                m_SelectedTrack = t;
                m_DragStartMouse    = mouse;
                m_DragStartPosMs    = clip.timelinePosMs;
                m_DragStartOffsetMs = clip.sourceOffsetMs;
                m_DragStartDurMs    = clip.durationMs;
                m_DragMode = hoverLeft ? DragMode::TrimLeft : hoverRight ? DragMode::TrimRight : DragMode::Move;
                isDragTarget = true;
                isSel = true;
            }

            if (isDragTarget) {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    float dxPx = ImGui::GetMousePos().x - m_DragStartMouse.x;
                    int64_t dMs = (int64_t)(dxPx / m_PixelsPerMs);
                    if (m_DragMode == DragMode::Move) {
                        clip.timelinePosMs = std::max<int64_t>(0, m_DragStartPosMs + dMs);
                    } else if (m_DragMode == DragMode::TrimLeft) {
                        int64_t maxOffset = m_DragStartOffsetMs + m_DragStartDurMs - 100;
                        int64_t newOffset = std::clamp<int64_t>(m_DragStartOffsetMs + dMs, 0, std::max<int64_t>(0, maxOffset));
                        int64_t applied = newOffset - m_DragStartOffsetMs;
                        clip.sourceOffsetMs = newOffset;
                        clip.durationMs     = m_DragStartDurMs - applied;
                        clip.timelinePosMs  = std::max<int64_t>(0, m_DragStartPosMs + applied);
                    } else if (m_DragMode == DragMode::TrimRight) {
                        int64_t maxDur = clip.sourceTotalMs > 0
                            ? std::max<int64_t>(100, clip.sourceTotalMs - clip.sourceOffsetMs)
                            : (m_DragStartDurMs + 600000);
                        clip.durationMs = std::clamp<int64_t>(m_DragStartDurMs + dMs, 100, maxDur);
                    }
                } else {
                    m_DragMode = DragMode::None;
                }
            }

            if (hoverLeft || hoverRight)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            ImU32 fill = isSel ? ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.55f, 0.95f, 0.55f))
                               : ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.45f, 0.55f, 0.45f));
            dl->AddRectFilled(cP0, cP1, fill, 4.0f);
            dl->AddRect(cP0, cP1, isSel ? DS::AccentLight : DS::BtnDefaultBord, 4.0f, 0, isSel ? 2.0f : 1.0f);

            if (cP1.x - cP0.x > 30.0f) {
                dl->PushClipRect(cP0, cP1, true);
                dl->AddText({ cP0.x + 6.0f, cP0.y + 4.0f }, DS::TextPrimary, clip.displayName.c_str());
                dl->PopClipRect();
            }
        }

        ImGui::SetCursorScreenPos({ rowP0.x, rowP0.y + kTrackH + kTrackGap });
    }

    // Cabezal de reproduccion -- solo tiene sentido visual mientras
    // "Reproducir todo" esta corriendo (ver UpdateMixPlayback); en reposo no
    // se dibuja para no sugerir una posicion "actual" que no significa nada.
    if (m_MixPlaying) {
        float x = laneX0 + (float)m_MixPlayheadMs * m_PixelsPerMs;
        float bottomY = rulerOrigin.y + 16.0f + (float)m_Tracks.size() * (kTrackH + kTrackGap);
        dl->AddLine({ x, rulerOrigin.y }, { x, bottomY }, DS::DangerColor, 2.0f);
    }
}

void AudioDawPanel::RenderExportBar()
{
    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary), "RENDERIZAR / EXPORTAR");

    bool running = m_Mixdown.IsRunning();
    ImGui::BeginDisabled(running);
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("##dawFormat", kFormatLabels[m_ExportFormatIdx])) {
        for (int i = 0; i < 4; i++) {
            bool sel = (i == m_ExportFormatIdx);
            if (ImGui::Selectable(kFormatLabels[i], sel)) {
                m_ExportFormatIdx = i;
                // Actualiza la extension del path sugerido, conserva el resto.
                std::string cur = m_ExportPathBuf;
                auto dot = cur.find_last_of('.');
                if (dot != std::string::npos) cur = cur.substr(0, dot);
                cur += kFormatExt[i];
                std::snprintf(m_ExportPathBuf, sizeof(m_ExportPathBuf), "%s", cur.c_str());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
    ImGui::InputText("##dawExportPath", m_ExportPathBuf, sizeof(m_ExportPathBuf));
    ImGui::EndDisabled();

    int totalClips = 0;
    for (const auto& t : m_Tracks) totalClips += (int)t.clips.size();

    if (running) {
        float prog = m_Mixdown.GetProgress();
        ImGui::ProgressBar(prog < 0.0f ? -1.0f : prog, ImVec2(200.0f, 0.0f));
        ImGui::SameLine();
        if (DawActionButton("cancelExport", "Cancelar", ImVec2(100.0f, 24.0f), DS::DangerColor)) m_Mixdown.Cancel();
    } else {
        ImGui::BeginDisabled(totalClips == 0);
        if (DawActionButton("renderMix", "Renderizar mezcla", ImVec2(180.0f, 28.0f), DS::SuccessColor)) {
            std::vector<Core::AudioMixdownClip> clips;
            bool anySolo = false; // (sin solo implementado todavia -- placeholder para el futuro)
            (void)anySolo;
            for (const auto& track : m_Tracks) {
                if (track.muted) continue;
                for (const auto& c : track.clips)
                    clips.push_back({ c.sourcePath, c.sourceOffsetMs, c.durationMs, c.timelinePosMs });
            }
            std::string err;
            auto fmt = static_cast<Core::AudioExportFormat>(m_ExportFormatIdx);
            if (!m_Mixdown.Start(clips, m_ExportPathBuf, fmt, &err)) {
                m_StatusMessage = err;
                m_StatusIsError = true;
            } else {
                m_StatusMessage = "Renderizando...";
                m_StatusIsError = false;
            }
        }
        ImGui::EndDisabled();
    }
}

} // namespace ProyecThor::UI
