
#include "Audio.h"
#include "audio/AudioHelpers.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/FileDeletionManager.h"
#include "frontend/panels/monitor/MonitorTheme.h"

namespace { namespace MT = ProyecThor::UI::MonitorTheme; }

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <array>
#endif

namespace fs = std::filesystem;

namespace {

inline ImU32 Col(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

inline void HsvToRgb(float h, float s, float v,
                     float& r, float& g, float& b) {
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
}

inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    float ar = ((a >>  0) & 0xFF) / 255.0f;
    float ag = ((a >>  8) & 0xFF) / 255.0f;
    float ab_ = ((a >> 16) & 0xFF) / 255.0f;
    float aa = ((a >> 24) & 0xFF) / 255.0f;
    float br = ((b >>  0) & 0xFF) / 255.0f;
    float bg = ((b >>  8) & 0xFF) / 255.0f;
    float bb_ = ((b >> 16) & 0xFF) / 255.0f;
    float ba = ((b >> 24) & 0xFF) / 255.0f;
    return IM_COL32(
        static_cast<int>((ar + (br - ar) * t) * 255),
        static_cast<int>((ag + (bg - ag) * t) * 255),
        static_cast<int>((ab_ + (bb_ - ab_) * t) * 255),
        static_cast<int>((aa + (ba - aa) * t) * 255));
}

using DrawIconFn = void (*)(ImDrawList*, ImVec2, float, ImU32);

static bool MonitorStyleButton(const char* strId, const char* iconKey, DrawIconFn drawFallback,
                               float iconSize, ImVec4 bgCol, ImVec4 hovCol, ImVec4 actCol,
                               ImVec2 btnSize, bool isActiveState = false)
{
    ImTextureID tex = (ImTextureID)0;
    if (iconKey) {
        auto it = StyleGeneralApp::Icons.find(iconKey);
        if (it != StyleGeneralApp::Icons.end() && it->second.textureID)
            tex = (ImTextureID)(intptr_t)it->second.textureID;
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        isActiveState ? actCol : bgCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  actCol);

    bool pressed = ImGui::Button(strId, btnSize);
    bool isHeld  = ImGui::IsItemActive();

    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 s = ImGui::GetItemRectSize();
    float  offsetY = isHeld ? 2.0f : 0.0f;
    ImVec2 iconOrigin(p.x + (s.x - iconSize) * 0.5f, p.y + (s.y - iconSize) * 0.5f + offsetY);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (tex != (ImTextureID)0) {
        ImU32 tint = isHeld ? IM_COL32(204, 204, 204, 255) : IM_COL32_WHITE;
        dl->AddImage(tex, iconOrigin, ImVec2(iconOrigin.x + iconSize, iconOrigin.y + iconSize),
                    ImVec2(0, 0), ImVec2(1, 1), tint);
    } else if (drawFallback) {
        ImU32 col = isHeld ? IM_COL32(204, 204, 204, 255) : IM_COL32_WHITE;
        drawFallback(dl, iconOrigin, iconSize, col);
    }

    ImGui::PopStyleColor(3);
    return pressed;
}

static void DrawSpeakerShape(ImDrawList* dl, ImVec2 o, float sz, ImU32 col, bool muted) {
    ImVec2 c = { o.x + sz * 0.5f, o.y + sz * 0.5f };

    float  boxHalfH = sz * 0.16f;
    ImVec2 boxMin   = { c.x - sz * 0.42f, c.y - boxHalfH };
    ImVec2 boxMax   = { c.x - sz * 0.16f, c.y + boxHalfH };
    dl->AddRectFilled(boxMin, boxMax, col, 1.0f);

    ImVec2 apex    = { boxMax.x, c.y };
    ImVec2 baseTop = { c.x + sz * 0.16f, c.y - sz * 0.34f };
    ImVec2 baseBot = { c.x + sz * 0.16f, c.y + sz * 0.34f };
    dl->AddTriangleFilled(apex, baseTop, baseBot, col);

    if (muted) {
        dl->AddLine({ o.x + sz * 0.06f, o.y + sz * 0.94f },
                    { o.x + sz * 0.94f, o.y + sz * 0.06f }, col, sz * 0.09f);
    } else {
        for (int i = 1; i <= 2; i++) {
            float r = sz * (0.14f + 0.13f * (float)i);
            dl->PathArcTo({ c.x + sz * 0.10f, c.y }, r, -0.62f, 0.62f, 10);
            dl->PathStroke(col, 0, sz * 0.055f);
        }
    }
}
static void DrawIcon_SpeakerOn(ImDrawList* dl, ImVec2 o, float sz, ImU32 col)    { DrawSpeakerShape(dl, o, sz, col, false); }
static void DrawIcon_SpeakerMuted(ImDrawList* dl, ImVec2 o, float sz, ImU32 col) { DrawSpeakerShape(dl, o, sz, col, true);  }

static void DrawIcon_Shuffle(ImDrawList* dl, ImVec2 o, float sz, ImU32 col) {
    float th = std::max(1.3f, sz * 0.11f);
    ImVec2 a0{ o.x, o.y + sz * 0.25f }, a1{ o.x + sz * 0.75f, o.y + sz * 0.75f };
    ImVec2 b0{ o.x, o.y + sz * 0.75f }, b1{ o.x + sz * 0.75f, o.y + sz * 0.25f };
    dl->AddLine(a0, a1, col, th);
    dl->AddLine(b0, b1, col, th);
    dl->AddTriangleFilled({ a1.x, a1.y - sz * 0.16f }, { a1.x + sz * 0.22f, a1.y }, { a1.x, a1.y + sz * 0.10f }, col);
    dl->AddTriangleFilled({ b1.x, b1.y - sz * 0.10f }, { b1.x + sz * 0.22f, b1.y }, { b1.x, b1.y + sz * 0.16f }, col);
}

static bool HorizontalFader(const char* id, float* value, float lo, float hi, ImVec2 size,
                            ImU32 trackCol, ImU32 fillCol, ImU32 capCol) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();
    bool changed = false;

    const float capW       = 14.0f;
    const float trackLeft  = pos.x + capW * 0.5f;
    const float trackRight = pos.x + size.x - capW * 0.5f;
    const float trackWpx   = std::max(1.0f, trackRight - trackLeft);

    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && hi > lo) {
        float t = std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / trackWpx, 0.0f, 1.0f);
        float newVal = lo + t * (hi - lo);
        if (newVal != *value) { *value = newVal; changed = true; }
    }

    float frac = (hi > lo) ? std::clamp((*value - lo) / (hi - lo), 0.0f, 1.0f) : 0.0f;
    float capX = trackLeft + frac * trackWpx;

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    const float trackH = 6.0f;
    float       cy     = pos.y + size.y * 0.5f;

    dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { trackRight, cy + trackH * 0.5f },
                      trackCol, trackH * 0.5f);
    if (capX - trackLeft > 0.5f)
        dl->AddRectFilled({ trackLeft, cy - trackH * 0.5f }, { capX, cy + trackH * 0.5f },
                          fillCol, trackH * 0.5f);

    for (int i = 0; i <= 4; i++) {
        float mx = trackLeft + trackWpx * (float)i / 4.0f;
        dl->AddLine({ mx, cy - size.y * 0.30f }, { mx, cy - trackH * 0.7f },
                    IM_COL32(255, 255, 255, 35), 1.0f);
    }

    float  capHalfH = size.y * 0.40f;
    ImVec2 capMin   = { capX - capW * 0.5f, cy - capHalfH };
    ImVec2 capMax   = { capX + capW * 0.5f, cy + capHalfH };
    ImU32  capBody  = capCol;
    if (hovered || active) {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(capCol);
        capBody = ImGui::ColorConvertFloat4ToU32(ImVec4(
            std::min(c.x + 0.10f, 1.0f), std::min(c.y + 0.10f, 1.0f), std::min(c.z + 0.10f, 1.0f), c.w));
    }

    dl->AddRectFilled(capMin, capMax, capBody, 3.0f);
    dl->AddRect(capMin, capMax, IM_COL32(0, 0, 0, 110), 3.0f, 0, 1.2f);
    dl->AddLine({ capX, capMin.y + 4.0f }, { capX, capMax.y - 4.0f }, IM_COL32(0, 0, 0, 130), 1.5f);

    return changed;
}

static void DrawDiscArtwork(ImDrawList* dl,
                             ImVec2      center,
                             float       radius,
                             float       hue,
                             const char* initials,
                             float       rotAngle) {
    const int   segments = 64;
    const float pi2      = 6.28318530718f;

    for (int s = 6; s >= 1; s--) {
        float sr = radius + s * 3.0f;
        dl->AddCircleFilled(center, sr,
            IM_COL32(0, 0, 0, static_cast<int>(30.0f - s * 3.5f)), segments);
    }

    dl->AddCircleFilled(center, radius, IM_COL32(18, 18, 22, 255), segments);

    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.70f, 0.85f, accentR, accentG, accentB);

    for (int ring = 1; ring <= 12; ring++) {
        float rr = radius * (0.35f + ring * 0.052f);
        if (rr >= radius) break;
        float alpha = (ring % 3 == 0) ? 0.20f : 0.07f;
        dl->AddCircle(center, rr,
            IM_COL32(static_cast<int>(accentR * 255),
                     static_cast<int>(accentG * 255),
                     static_cast<int>(accentB * 255),
                     static_cast<int>(alpha * 255)),
            segments, 1.0f);
    }

    float artRadius = radius * 0.52f;

    const int colorSectors = 6;
    for (int s = 0; s < colorSectors; s++) {
        float angleStart = rotAngle + (pi2 / colorSectors) * s;
        float angleEnd   = angleStart + (pi2 / colorSectors);

        float sH = std::fmod(hue + s * (1.0f / colorSectors), 1.0f);
        float sS = 0.55f + (s % 2) * 0.15f;
        float sV = 0.40f + (s % 3) * 0.12f;
        float sR, sG, sB;
        HsvToRgb(sH, sS, sV, sR, sG, sB);
        ImU32 sColor = IM_COL32(static_cast<int>(sR * 255),
                                static_cast<int>(sG * 255),
                                static_cast<int>(sB * 255), 220);

        const int subSegs = 8;
        for (int ss = 0; ss < subSegs; ss++) {
            float a0 = angleStart + (angleEnd - angleStart) * (ss     / static_cast<float>(subSegs));
            float a1 = angleStart + (angleEnd - angleStart) * ((ss+1) / static_cast<float>(subSegs));
            dl->AddTriangleFilled(
                center,
                ImVec2(center.x + std::cos(a0) * artRadius,
                       center.y + std::sin(a0) * artRadius),
                ImVec2(center.x + std::cos(a1) * artRadius,
                       center.y + std::sin(a1) * artRadius),
                sColor);
        }
    }

    const int fadeSegs = 32;
    for (int f = fadeSegs; f >= 1; f--) {
        float fr    = artRadius * (f / static_cast<float>(fadeSegs));
        float alpha = 0.0f + (1.0f - f / static_cast<float>(fadeSegs)) * 0.45f;
        dl->AddCircleFilled(center, fr,
            IM_COL32(10, 10, 14, static_cast<int>(alpha * 255)), 32);
    }

    float spindleR = radius * 0.08f;
    dl->AddCircleFilled(center, spindleR, IM_COL32(8, 8, 10, 255), 24);
    dl->AddCircle(center, spindleR,
        IM_COL32(static_cast<int>(accentR * 180),
                 static_cast<int>(accentG * 180),
                 static_cast<int>(accentB * 180), 200), 24, 1.5f);

    if (artRadius > 24.0f) {
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 textSz = ImGui::CalcTextSize(initials);
        float  scale  = std::min((artRadius * 0.9f) / std::max(textSz.x, 1.0f),
                                 (artRadius * 0.6f) / std::max(textSz.y, 1.0f));
        scale = std::min(scale, 1.6f);

        ImVec2 tPos = ImVec2(center.x - textSz.x * scale * 0.5f,
                             center.y - textSz.y * scale * 0.5f);
        dl->AddText(nullptr, ImGui::GetFontSize() * scale, tPos,
            IM_COL32(255, 255, 255, 160), initials);
    }

    dl->AddCircle(center, radius,
        IM_COL32(static_cast<int>(accentR * 255),
                 static_cast<int>(accentG * 255),
                 static_cast<int>(accentB * 255), 80),
        segments, 1.5f);
}

static void DrawTonearm(ImDrawList* dl,
                        ImVec2      discCenter,
                        float       discRadius,
                        float       armAngle,
                        ImU32       color) {
    float pivotX = discCenter.x + discRadius * 1.10f;
    float pivotY = discCenter.y - discRadius * 0.60f;

    float armLen = discRadius * 1.25f;

    float tipX = pivotX + std::cos(armAngle + 3.14159f) * armLen;
    float tipY = pivotY + std::sin(armAngle + 3.14159f) * armLen;

    dl->AddLine(ImVec2(pivotX, pivotY), ImVec2(tipX, tipY), color, 2.0f);

    dl->AddCircleFilled(ImVec2(pivotX, pivotY), 5.0f, color, 12);
    dl->AddCircleFilled(ImVec2(pivotX, pivotY), 2.5f, IM_COL32(20, 20, 26, 255), 12);

    float headSize = 5.0f;
    float perpAngle = armAngle + 3.14159f + 1.5708f;
    ImVec2 h0 = ImVec2(tipX + std::cos(perpAngle) * headSize,
                        tipY + std::sin(perpAngle) * headSize);
    ImVec2 h1 = ImVec2(tipX - std::cos(perpAngle) * headSize,
                        tipY - std::sin(perpAngle) * headSize);
    dl->AddLine(h0, h1, color, 3.0f);
}

static void DrawTextCenteredFree(ImDrawList* dl, ImFont* font, float fontSize,
                                  ImVec2 center, ImU32 color, const char* text) {
    ImVec2 sz = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text)
                     : ImGui::CalcTextSize(text);
    dl->AddText(font, fontSize,
                ImVec2(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f),
                color, text);
}

#ifndef _WIN32
static std::string OpenAudioFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar audio\" "
        "--file-filter=\"Audio | *.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma *.opus *.aiff\" 2>/dev/null",
        "kdialog --getopenfilename . "
        "\"*.mp3 *.flac *.wav *.ogg *.aac *.m4a *.wma *.opus *.aiff|Audio\" 2>/dev/null"
    };

    for (const char* cmd : commands) {
        std::array<char, 1024> buffer{};
        std::string result;

        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;

        while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
            result += buffer.data();

        int status = pclose(pipe);
        if (status != 0) continue;

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();

        if (!result.empty())
            return result;
    }
    return {};
}
#endif

}

namespace ProyecThor::UI {

const char* AudioVisualStyleName(AudioVisualStyle style) {
    switch (style) {
        case AudioVisualStyle::Minimal: return "Minimal";
        case AudioVisualStyle::Bars:    return "Ondas";
        default:                        return "Vinilo";
    }
}

void AudioPanel::RenderLibraryList()
{
    Update();
    RenderHeader();
    ImGui::Spacing();
    if (m_ViewQueueTab) {
        RenderQueueList();
    } else {
        RenderPlaylist();
    }
}

void AudioPanel::RenderPlayerView()
{
    RenderNowPlayingCard();
    RenderProgressBar();
    RenderTransportControls();
    RenderVolumeRow();
}

void AudioPanel::Render()
{
    RenderLibraryList();
    RenderPlayerView();
}

static AudioPanel* s_ActiveAudioPanel = nullptr;

AudioPanel* AudioPanel::GetActiveInstance() {
    return s_ActiveAudioPanel;
}

static bool s_AudioHookRegistered = []() {
    Core::FileDeletionManager::RegisterUsageReleaseHook([](const std::string& path) {
        if (s_ActiveAudioPanel) {
            s_ActiveAudioPanel->StopIfPathMatches(path);
        }
    });
    return true;
}();

AudioPanel::AudioPanel() {
    s_ActiveAudioPanel = this;
    try {
        fs::create_directories(ProyecThor::Audio::GetAudioPath());
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] No se pudo crear carpeta: " << e.what() << '\n';
    }

    for (int i = 0; i < kWaveBars; i++) {
        m_WaveBars[i]    = 0.02f;
        m_WaveTargets[i] = 0.02f;
    }

    m_VlcPlayer.SetVolume(ComputeEffectiveVolume());
    RefreshLibrary();
}

AudioPanel::~AudioPanel()
{
    if (s_ActiveAudioPanel == this)
        s_ActiveAudioPanel = nullptr;

    if (m_LyricsImportThread.joinable())
        m_LyricsImportThread.join();

    for (auto& track : m_Tracks)
        ProyecThor::Audio::FreeAlbumArtTexture(track.coverArt);
}

void AudioPanel::StopIfPathMatches(const std::string& path) {
    if (path.empty()) return;

    std::string normTarget = path;
    std::replace(normTarget.begin(), normTarget.end(), '\\', '/');
    std::transform(normTarget.begin(), normTarget.end(), normTarget.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    bool shouldStop = false;
    if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size())) {
        std::string curPath = m_Tracks[m_CurrentTrack].fullPath;
        std::replace(curPath.begin(), curPath.end(), '\\', '/');
        std::transform(curPath.begin(), curPath.end(), curPath.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (curPath == normTarget || normTarget.find(curPath) != std::string::npos || curPath.find(normTarget) != std::string::npos) {
            shouldStop = true;
        }
    }
    if (!m_LastExternalSelection.empty()) {
        std::string curSel = m_LastExternalSelection;
        std::replace(curSel.begin(), curSel.end(), '\\', '/');
        std::transform(curSel.begin(), curSel.end(), curSel.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (curSel == normTarget || normTarget.find(curSel) != std::string::npos || curSel.find(normTarget) != std::string::npos) {
            m_LastExternalSelection.clear();
            shouldStop = true;
        }
    }
    if (shouldStop) {
        Stop();
        RefreshLibrary();
    }
}

float AudioPanel::DbToLinear(float dB) {
    return std::pow(10.0f, dB / 20.0f);
}

int AudioPanel::ComputeEffectiveVolume() const {
    float gain    = DbToLinear(m_GainDb);
    float scaled  = static_cast<float>(m_Volume) * gain;
    int   clamped = static_cast<int>(std::round(scaled));
    return std::max(0, std::min(200, clamped));
}

std::string AudioPanel::FormatTime(int64_t ms) const {
    if (ms < 0) ms = 0;
    int totalSec = static_cast<int>(ms / 1000);
    int minutes  = totalSec / 60;
    int seconds  = totalSec % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes
        << ':' << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

void AudioPanel::ComputeTrackAccent(AudioTrack& track) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : track.displayName)
        hash = (hash ^ c) * 16777619u;
    track.accentH = static_cast<float>(hash % 1000) / 1000.0f;
}

void AudioPanel::Play(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(m_Tracks.size())) return;

    m_CurrentTrack = trackIndex;
    const std::string& path = m_Tracks[trackIndex].fullPath;
    m_LastExternalSelection = m_Tracks[trackIndex].filename;

    Core::LibrarySelection sel;
    sel.type = Core::ItemType::Audio;
    sel.title = m_Tracks[trackIndex].filename;
    sel.contentData = { m_Tracks[trackIndex].fullPath };
    Core::PresentationCore::Get().SetSelection(sel);

    m_VlcPlayer.Play(path, false, false);
    m_VlcPlayer.SetVolume(ComputeEffectiveVolume());
    ApplyEqualizerToPlayer();

    m_IsPlaying     = true;
    m_IsPaused      = false;
    m_Progress      = 0.0f;
    m_CurrentTimeMs = 0;
    m_TotalTimeMs   = 0;

    m_Disc.targetSpeed = 2.0f;
    m_Disc.needleLifted = false;

    EnsureCoverLoaded(trackIndex);

    if (m_IsLiveBackground)
        RefreshLiveLyrics();
}

void AudioPanel::PlayCurrent() {
    Play(m_CurrentTrack);
}

bool AudioPanel::PlayFileLive(const std::string& filename) {
    auto findIndex = [this, &filename]() -> int {
        for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++)
            if (m_Tracks[i].filename == filename) return i;
        return -1;
    };

    int idx = findIndex();
    if (idx < 0) {
        RefreshLibrary();
        idx = findIndex();
    }
    if (idx < 0) return false;

    Play(idx);

    auto& core = Core::PresentationCore::Get();
    core.SetBackgroundAudio();
    core.SetProjecting(true);
    SetLiveBackground(true);
    return true;
}

void AudioPanel::Stop() {
    m_VlcPlayer.Stop();
    m_IsPlaying     = false;
    m_IsPaused      = false;
    m_Progress      = 0.0f;
    m_CurrentTimeMs = 0;

    m_Disc.targetSpeed  = 0.0f;
    m_Disc.needleLifted = true;
}

void AudioPanel::Pause() {
    if (!m_IsPlaying) return;
    m_IsPaused = !m_IsPaused;
    m_VlcPlayer.SetPause(m_IsPaused);

    m_Disc.targetSpeed = m_IsPaused ? 0.0f : 2.0f;
}

void AudioPanel::TogglePlayPause() {
    if (m_Tracks.empty()) return;
    if (m_CurrentTrack < 0) m_CurrentTrack = 0;

    if (m_IsPlaying) {
        m_IsPaused = !m_IsPaused;
        m_VlcPlayer.SetPause(m_IsPaused);
        m_Disc.targetSpeed  = m_IsPaused ? 0.0f : 2.0f;
        m_Disc.needleLifted = m_IsPaused;
    } else {
        PlayCurrent();
    }
}

void AudioPanel::Next() {
    if (m_Tracks.empty()) return;

    if (m_ViewQueueTab && !m_AudioQueue.empty()) {
        int nextQueue = m_QueueCurrentIndex + 1;
        if (nextQueue >= static_cast<int>(m_AudioQueue.size())) {
            if (m_RepeatMode == AudioRepeatMode::All) nextQueue = 0;
            else { Stop(); return; }
        }
        PlayQueueIndex(nextQueue);
        return;
    }

    if (m_Tracks.empty()) return;

    int next = -1;
    if (m_Shuffle) {
        if (m_Tracks.size() == 1) { next = 0; }
        else {
            do { next = std::rand() % static_cast<int>(m_Tracks.size()); }
            while (next == m_CurrentTrack);
        }
    } else {
        next = m_CurrentTrack + 1;
        if (next >= static_cast<int>(m_Tracks.size())) {
            if (m_RepeatMode == AudioRepeatMode::All) next = 0;
            else { Stop(); return; }
        }
    }
    Play(next);
}

void AudioPanel::Previous() {
    if (m_CurrentTimeMs > 3000 && m_CurrentTrack >= 0) {
        SeekTo(0.0f);
        return;
    }
    if (m_ViewQueueTab && !m_AudioQueue.empty()) {
        int prevQueue = m_QueueCurrentIndex - 1;
        if (prevQueue < 0) {
            prevQueue = (m_RepeatMode == AudioRepeatMode::All)
                ? static_cast<int>(m_AudioQueue.size()) - 1
                : 0;
        }
        PlayQueueIndex(prevQueue);
        return;
    }
    if (m_Tracks.empty()) return;
    int prev = m_CurrentTrack - 1;
    if (prev < 0) {
        prev = (m_RepeatMode == AudioRepeatMode::All)
            ? static_cast<int>(m_Tracks.size()) - 1
            : 0;
    }
    Play(prev);
}

void AudioPanel::SkipSeconds(int seconds) {
    if (!m_IsPlaying || m_TotalTimeMs <= 0) return;
    int64_t newTimeMs = std::clamp(m_CurrentTimeMs + static_cast<int64_t>(seconds) * 1000,
                                   (int64_t)0, m_TotalTimeMs);
    float newPos = static_cast<float>(newTimeMs) / static_cast<float>(m_TotalTimeMs);
    SeekTo(newPos);
}

void AudioPanel::SeekTo(float normalizedPosition) {
    normalizedPosition = std::max(0.0f, std::min(1.0f, normalizedPosition));
    m_VlcPlayer.SetPosition(normalizedPosition);
    m_Progress = normalizedPosition;
}

int AudioPanel::FindTrackIndexByPath(const std::string& path) const {
    for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++) {
        if (m_Tracks[i].fullPath == path || m_Tracks[i].filename == path)
            return i;
    }
    return -1;
}

void AudioPanel::AddToQueue(int trackIndex) {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_Tracks.size())) {
        m_AudioQueue.push_back(m_Tracks[trackIndex].fullPath);
    }
}

void AudioPanel::AddToQueue(const std::string& fullPath) {
    if (!fullPath.empty()) {
        m_AudioQueue.push_back(fullPath);
    }
}

void AudioPanel::PlayQueueIndex(int queueIdx) {
    if (queueIdx < 0 || queueIdx >= static_cast<int>(m_AudioQueue.size())) return;
    m_QueueCurrentIndex = queueIdx;
    const std::string& path = m_AudioQueue[queueIdx];
    int tIdx = FindTrackIndexByPath(path);
    if (tIdx >= 0) {
        Play(tIdx);
    } else {
        RefreshLibrary();
        tIdx = FindTrackIndexByPath(path);
        if (tIdx >= 0) Play(tIdx);
    }
}

void AudioPanel::RemoveFromQueue(int queueIdx) {
    if (queueIdx >= 0 && queueIdx < static_cast<int>(m_AudioQueue.size())) {
        m_AudioQueue.erase(m_AudioQueue.begin() + queueIdx);
        if (m_QueueCurrentIndex == queueIdx) {
            if (m_QueueCurrentIndex >= static_cast<int>(m_AudioQueue.size()))
                m_QueueCurrentIndex = static_cast<int>(m_AudioQueue.size()) - 1;
        } else if (m_QueueCurrentIndex > queueIdx) {
            m_QueueCurrentIndex--;
        }
    }
}

void AudioPanel::MoveQueueItem(int from, int to) {
    if (from < 0 || from >= static_cast<int>(m_AudioQueue.size()) ||
        to < 0 || to >= static_cast<int>(m_AudioQueue.size()) || from == to)
        return;
    std::string item = m_AudioQueue[from];
    m_AudioQueue.erase(m_AudioQueue.begin() + from);
    m_AudioQueue.insert(m_AudioQueue.begin() + to, item);
    if (m_QueueCurrentIndex == from) {
        m_QueueCurrentIndex = to;
    } else if (from < m_QueueCurrentIndex && to >= m_QueueCurrentIndex) {
        m_QueueCurrentIndex--;
    } else if (from > m_QueueCurrentIndex && to <= m_QueueCurrentIndex) {
        m_QueueCurrentIndex++;
    }
}

void AudioPanel::ClearQueue() {
    m_AudioQueue.clear();
    m_QueueCurrentIndex = -1;
}

void AudioPanel::SetVolume(int volume) {
    m_Volume = std::max(0, std::min(200, volume));
    if (!m_Muted)
        m_VlcPlayer.SetVolume(ComputeEffectiveVolume());
}

void AudioPanel::ApplyGain(float gainDb) {
    m_GainDb = std::max(-20.0f, std::min(20.0f, gainDb));
    if (m_Muted) return;
    m_VlcPlayer.SetVolume(ComputeEffectiveVolume());
}

void AudioPanel::ApplyEqualizerToPlayer() {
    m_VlcPlayer.SetEqualizerEnabled(m_EqEnabled);
    if (!m_EqEnabled) return;
    m_VlcPlayer.SetEqualizerPreamp(m_EqPreamp);
    for (int b = 0; b < kEqBands; b++)
        m_VlcPlayer.SetEqualizerBand(b, m_EqBands[b]);
}

void AudioPanel::RefreshLibrary()
{
    for (auto& track : m_Tracks)
        ProyecThor::Audio::FreeAlbumArtTexture(track.coverArt);

    m_Tracks.clear();
    const std::string& base = ProyecThor::Audio::GetAudioPath();

    static const std::vector<std::string> kAudioExts = {
        ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff"
    };

    try {
#ifdef _WIN32
        fs::path basePath(ProyecThor::Audio::Utf8ToWide(base));
#else
        fs::path basePath(base);
#endif
        if (!fs::exists(basePath)) { fs::create_directories(basePath); return; }

        for (const auto& entry : fs::directory_iterator(basePath)) {
            if (!entry.is_regular_file()) continue;

#ifdef _WIN32
            std::string ext = ProyecThor::Audio::WideToUtf8(
                entry.path().extension().wstring());
#else
            std::string ext = entry.path().extension().string();
#endif
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool supported = false;
            for (const auto& e : kAudioExts)
                if (ext == e) { supported = true; break; }
            if (!supported) continue;

            AudioTrack track;
#ifdef _WIN32
            track.fullPath    = ProyecThor::Audio::WideToUtf8(entry.path().wstring());
            track.filename    = ProyecThor::Audio::WideToUtf8(entry.path().filename().wstring());
            track.displayName = ProyecThor::Audio::WideToUtf8(entry.path().stem().wstring());
#else
            track.fullPath    = entry.path().string();
            track.filename    = entry.path().filename().string();
            track.displayName = entry.path().stem().string();
#endif
            ComputeTrackAccent(track);
            LoadTrackLyricsSidecar(track);
            m_Tracks.push_back(std::move(track));
        }
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] RefreshLibrary error: " << e.what() << '\n';
    }

    std::sort(m_Tracks.begin(), m_Tracks.end(),
              [](const AudioTrack& a, const AudioTrack& b) {
                  return a.displayName < b.displayName;
              });

    if (m_CurrentTrack >= static_cast<int>(m_Tracks.size()))
        m_CurrentTrack = m_Tracks.empty() ? -1 : 0;
}

static fs::path LyricsSidecarPath(const std::string& fullPath) {
    std::string p = fullPath + ".lyrics.json";
#ifdef _WIN32
    return fs::path(ProyecThor::Audio::Utf8ToWide(p));
#else
    return fs::path(p);
#endif
}

void AudioPanel::LoadTrackLyricsSidecar(AudioTrack& track) const {
    std::ifstream f(LyricsSidecarPath(track.fullPath));
    if (!f.is_open()) return;
    try {
        nlohmann::json j;
        f >> j;
        track.sourceUrl     = j.value("url", std::string());
        track.lyricsText    = j.value("lyrics", std::string());
        track.lyricsEnabled = j.value("enabled", false);
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] Sidecar de letra invalido para " << track.filename
                   << ": " << e.what() << '\n';
    }
}

void AudioPanel::SaveTrackLyricsSidecar(const AudioTrack& track) const {
    fs::path path = LyricsSidecarPath(track.fullPath);
    if (track.lyricsText.empty()) {
        std::error_code ec;
        fs::remove(path, ec);
        return;
    }
    try {
        nlohmann::json j;
        j["url"]     = track.sourceUrl;
        j["lyrics"]  = track.lyricsText;
        j["enabled"] = track.lyricsEnabled;
        std::ofstream f(path);
        if (f.is_open()) f << j.dump(2);
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] No se pudo guardar la letra de " << track.filename
                   << ": " << e.what() << '\n';
    }
}

void AudioPanel::RefreshLiveLyrics() {
    auto& core = Core::PresentationCore::Get();
    if (m_IsLiveBackground &&
        m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size())) {
        const AudioTrack& track = m_Tracks[m_CurrentTrack];
        if (track.lyricsEnabled && !track.lyricsText.empty()) {
            core.SetLayer2_Text(track.lyricsText);
            return;
        }
    }
    core.ClearLayer2();
}

void AudioPanel::SetLiveBackground(bool v) {
    if (m_IsLiveBackground == v) return;
    m_IsLiveBackground = v;
    RefreshLiveLyrics();
}

void AudioPanel::RequestLyricsImport(const std::string& url) {
    if (m_LyricsImportThread.joinable())
        m_LyricsImportThread.join();

    m_LyricsImportError.clear();
    m_LyricsImportRunning = true;
    {
        std::lock_guard<std::mutex> lk(m_LyricsImportMutex);
        m_LyricsImportResult.reset();
    }
    m_LyricsImportThread = std::thread([this, url]() {
        Core::SubtitleFetchResult res = Core::FetchSubtitlesAsLyrics(url);
        std::lock_guard<std::mutex> lk(m_LyricsImportMutex);
        m_LyricsImportResult  = std::move(res);
        m_LyricsImportRunning = false;
    });
}

void AudioPanel::ImportAudioFile() {
#ifdef _WIN32
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFilter =
        L"Audio\0*.mp3;*.flac;*.wav;*.ogg;*.aac;*.m4a;*.wma;*.opus;*.aiff\0"
        L"Todos\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile  = MAX_PATH;
    ofn.Flags     = OFN_EXPLORER | OFN_FILEMUSTEXIST |
                    OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        try {
            fs::path src{std::wstring(filename)};
            fs::path destDir{ProyecThor::Audio::Utf8ToWide(ProyecThor::Audio::GetAudioPath())};
            fs::path dest = destDir / src.filename();
            fs::create_directories(destDir);
            fs::copy(src, dest, fs::copy_options::overwrite_existing);
            RefreshLibrary();
        } catch (const std::exception& e) {
            std::cerr << "[AudioPanel] Import error: " << e.what() << '\n';
        }
    }
#else
    std::string selected = OpenAudioFileDialogUnix();
    if (selected.empty()) return;

    try {
        fs::path src(selected);
        fs::path destDir(ProyecThor::Audio::GetAudioPath());
        fs::path dest = destDir / src.filename();
        fs::create_directories(destDir);
        fs::copy(src, dest, fs::copy_options::overwrite_existing);
        RefreshLibrary();
    } catch (const std::exception& e) {
        std::cerr << "[AudioPanel] Import error: " << e.what() << '\n';
    }
#endif
}

void AudioPanel::Update() {
    float now = static_cast<float>(ImGui::GetTime());
    float dt  = now - m_LastTime;
    if (dt > 0.1f) dt = 0.1f;
    m_LastTime = now;

    {
        auto sel = Core::PresentationCore::Get().PeekSelection();
        if (sel.type == Core::ItemType::Audio && !sel.title.empty() &&
            sel.title != "Audio" && sel.title != m_LastExternalSelection)
        {
            m_LastExternalSelection = sel.title;

            int idx = -1;
            for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++) {
                if (m_Tracks[i].filename == sel.title || m_Tracks[i].fullPath == sel.title) {
                    idx = i;
                    break;
                }
            }
            if (idx < 0) {
                RefreshLibrary();
                for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++) {
                    if (m_Tracks[i].filename == sel.title || m_Tracks[i].fullPath == sel.title) {
                        idx = i;
                        break;
                    }
                }
            }
            if (idx >= 0 && idx != m_CurrentTrack) Play(idx);
        }
    }

    if (m_IsPlaying && !m_IsSeeking) {
        m_CurrentTimeMs = m_VlcPlayer.GetTime();
        m_TotalTimeMs   = m_VlcPlayer.GetLength();
        m_Progress      = (m_TotalTimeMs > 0)
            ? std::clamp(static_cast<float>(m_CurrentTimeMs) / static_cast<float>(m_TotalTimeMs), 0.0f, 1.0f)
            : 0.0f;
    }

    if (m_VlcPlayer.ConsumeEndReached()) {
        if (m_RepeatMode == AudioRepeatMode::One) {
            PlayCurrent();
        } else if (!m_AudioQueue.empty() && m_QueueCurrentIndex >= 0) {
            int nextQueue = m_QueueCurrentIndex + 1;
            if (nextQueue < static_cast<int>(m_AudioQueue.size())) {
                PlayQueueIndex(nextQueue);
            } else if (m_RepeatMode == AudioRepeatMode::All) {
                PlayQueueIndex(0);
            } else {
                Stop();
            }
        } else {
            Next();
        }
    }

    const float speedLerp = 2.2f;
    m_Disc.currentSpeed += (m_Disc.targetSpeed - m_Disc.currentSpeed) * speedLerp * dt;

    if (m_Disc.currentSpeed > 0.001f) {
        m_Disc.rotationAngle += m_Disc.currentSpeed * dt;
        if (m_Disc.rotationAngle > 6.28318530718f)
            m_Disc.rotationAngle -= 6.28318530718f;
    }

    float needleTarget = m_Disc.needleLifted ? -0.30f : -0.52f;
    m_Disc.needleAngle += (needleTarget - m_Disc.needleAngle) * 3.5f * dt;

    float levelL = 0.0f, levelR = 0.0f;
    m_VlcPlayer.GetAudioLevels(levelL, levelR);
    float peak = std::max(levelL, levelR);

    m_WaveTimer += dt;
    if (m_WaveTimer >= 0.040f) {
        m_WaveTimer = 0.0f;
        bool active = m_IsPlaying && !m_IsPaused;
        for (int i = 0; i < kWaveBars - 1; i++)
            m_WaveTargets[i] = m_WaveTargets[i + 1];
        m_WaveTargets[kWaveBars - 1] = active ? std::clamp(peak, 0.04f, 1.0f) : 0.02f;
    }

    for (int i = 0; i < kWaveBars; i++) {
        float lerp = m_IsPlaying ? 12.0f : 5.0f;
        m_WaveBars[i] += (m_WaveTargets[i] - m_WaveBars[i]) * lerp * dt;
    }
}

void AudioPanel::RenderHeader() {
    const float barH = 46.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::BeginChild("##AudioHeader", ImVec2(0.0f, barH), false,
                      ImGuiWindowFlags_NoScrollbar);

    float centerY = (barH - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::SetCursorPos(ImVec2(14.0f, centerY));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
    ImGui::TextUnformatted("🎵 Reproductor");
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::SetCursorPosY(centerY);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.45f, 0.58f, 1.0f));
    ImGui::Text("— %d pistas", static_cast<int>(m_Tracks.size()));
    ImGui::PopStyleColor();

    float availW = ImGui::GetContentRegionAvail().x;
    float searchW = std::clamp(availW * 0.35f, 130.0f, 210.0f);
    float rightButtonsW = 190.0f;
    float searchX = ImGui::GetWindowWidth() - rightButtonsW - searchW - 20.0f;
    if (searchX > 200.0f) {
        ImGui::SameLine(searchX);
        ImGui::SetCursorPosY((barH - 26.0f) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(24, 26, 32, 220));
        ImGui::SetNextItemWidth(searchW);
        ImGui::InputTextWithHint("##AudioSearch", "🔍 Buscar pista...", m_SearchBuffer, sizeof(m_SearchBuffer));
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (m_SearchBuffer[0] != '\0') {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::SetCursorPosY((barH - 24.0f) * 0.5f);
            if (ImGui::SmallButton("✕##clearAudioSearch")) {
                m_SearchBuffer[0] = '\0';
            }
        }
    }

    float btnY  = (barH - 26.0f) * 0.5f;
    float rightX = ImGui::GetWindowWidth() - rightButtonsW - 12.0f;

    ImGui::SameLine(rightX, 0.0f);
    ImGui::SetCursorPosY(btnY);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.26f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.38f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.35f, 0.92f, 0.55f, 1.0f));
    if (ImGui::Button("  + Importar  ", ImVec2(0.0f, 26.0f)))
        ImportAudioFile();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetCursorPosY(btnY);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.55f, 0.65f, 0.80f, 1.0f));
    if (ImGui::Button("  ↺ Actualizar  ", ImVec2(0.0f, 26.0f)))
        RefreshLibrary();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

std::string ExtractInitials(const std::string& name) {
    std::string initials;
    bool newWord = true;
    for (unsigned char c : name) {
        if (c == ' ' || c == '_' || c == '-') {
            newWord = true;
            continue;
        }
        if (newWord && initials.size() < 2) {
            initials += static_cast<char>(std::toupper(c));
            newWord = false;
        }
    }
    return initials.empty() ? "?" : initials;
}
void AudioPanel::RenderSpinningDisc(float cx, float cy, float radius) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 center(cx, cy);

    float hue = 0.58f;
    std::string initials = "PT";

    ImTextureID coverTexture = (ImTextureID)0;

if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
{
    const auto& track = m_Tracks[m_CurrentTrack];
    hue      = track.accentH;
    initials = ExtractInitials(track.displayName);

    if (track.coverArt.HasTexture())
        coverTexture = static_cast<ImTextureID>(track.coverArt.texID);
}

    if (m_VisualStyle == AudioVisualStyle::Bars)
        return;

    if (m_VisualStyle == AudioVisualStyle::Minimal) {
        float half = radius * 0.85f;
        ImVec2 pMin(cx - half, cy - half), pMax(cx + half, cy + half);

        dl->AddRectFilled(ImVec2(pMin.x + 3.0f, pMin.y + 5.0f), ImVec2(pMax.x + 3.0f, pMax.y + 5.0f),
                          IM_COL32(0, 0, 0, 70), 14.0f);

        if (coverTexture != (ImTextureID)0) {
            dl->AddImageRounded(coverTexture, pMin, pMax, {0, 0}, {1, 1}, IM_COL32_WHITE, 14.0f);
        } else {
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.8f, r, g, b);
            dl->AddRectFilled(pMin, pMax,
                IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255), static_cast<int>(b * 255), 255),
                14.0f);
            ImVec2 ts = ImGui::CalcTextSize(initials.c_str());
            dl->AddText(ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), IM_COL32(255, 255, 255, 235), initials.c_str());
        }
        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 40), 14.0f, 0, 1.5f);
        return;
    }

    float baseRadius = radius + 10.0f;

    dl->AddCircleFilled(ImVec2(cx + 3.0f, cy + 5.0f), baseRadius, IM_COL32(0, 0, 0, 80), 72);
    dl->AddCircleFilled(center, baseRadius, IM_COL32(28, 28, 34, 255), 72);
    dl->AddCircle(center, baseRadius, IM_COL32(65, 68, 80, 255), 72, 1.5f);
    DrawDiscArtwork(dl, center, radius, hue, initials.c_str(), m_Disc.rotationAngle);

    float labelRadius = radius * 0.33f;

    if (coverTexture != (ImTextureID)0) {
        dl->AddCircleFilled(center, labelRadius + 1.0f, IM_COL32(15, 15, 15, 255), 64);

        ImVec2 pMin(cx - labelRadius, cy - labelRadius);
        ImVec2 pMax(cx + labelRadius, cy + labelRadius);
        dl->AddImageRounded(coverTexture, pMin, pMax, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, labelRadius);
    } else {
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.8f, r, g, b);
        ImU32 labelColor = IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), 255);

        dl->AddCircleFilled(center, labelRadius, labelColor, 64);

        ImVec2 textSize = ImGui::CalcTextSize(initials.c_str());
        dl->AddText(ImVec2(cx - textSize.x / 2.0f, cy - textSize.y / 2.0f), IM_COL32(255, 255, 255, 255), initials.c_str());
    }

    dl->AddCircleFilled(center, radius * 0.04f, IM_COL32(20, 20, 24, 255), 24);
    dl->AddCircle(center, radius * 0.04f, IM_COL32(120, 120, 130, 255), 24, 1.0f);

    float accentR, accentG, accentB;
    ImGui::ColorConvertHSVtoRGB(hue, 0.30f, 0.85f, accentR, accentG, accentB);
    ImU32 armColor = IM_COL32((int)(accentR * 220), (int)(accentG * 220), (int)(accentB * 220), 230);

    DrawTonearm(dl, center, radius, m_Disc.needleAngle, armColor);
}

void AudioPanel::RenderStylePopup() {
    if (m_ShowStylePopup) {
        ImGui::OpenPopup("##audioStylePopup");
        m_ShowStylePopup = false;
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, MT::k_Bg2);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, MT::k_RLg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    if (ImGui::BeginPopup("##audioStylePopup")) {
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextSecondary);
        ImGui::TextUnformatted("Estilo del \"now playing\"");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        static const AudioVisualStyle kStyles[] = {
            AudioVisualStyle::Vinyl, AudioVisualStyle::Minimal, AudioVisualStyle::Bars
        };
        for (AudioVisualStyle s : kStyles) {
            bool selected = (m_VisualStyle == s);
            ImVec4 sel = MT::k_PrevBtn; sel.w = 0.55f;
            ImVec4 selHov = MT::k_PrevBtnHov; selHov.w = 0.65f;
            ImGui::PushStyleColor(ImGuiCol_Header,        sel);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selHov);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  selHov);
            if (ImGui::Selectable(AudioVisualStyleName(s), selected, 0, ImVec2(140.0f, 24.0f)))
                m_VisualStyle = s;
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_CheckMark,      MT::k_QueueAccent);
        ImGui::PushStyleColor(ImGuiCol_Text,           MT::k_TextSecondary);
        ImGui::Checkbox("Mostrar ondas", &m_ShowWaveform);
        ImGui::PopStyleColor(4);

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void AudioPanel::RenderNowPlayingCard() {
    const float cardH   = 168.0f;
    const float discR   = 62.0f;
    const float padding = 14.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg1);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::BeginChild("##NowPlaying", ImVec2(0.0f, cardH), true,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      winPos = ImGui::GetWindowPos();
    float       winW   = ImGui::GetWindowWidth();

    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;

    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.70f, 0.85f, accentR, accentG, accentB);
    ImU32 accentColor = IM_COL32(static_cast<int>(accentR * 255),
                                  static_cast<int>(accentG * 255),
                                  static_cast<int>(accentB * 255), 255);

    ImU32 gradTop = IM_COL32(static_cast<int>(accentR * 255),
                             static_cast<int>(accentG * 255),
                             static_cast<int>(accentB * 255), 28);
    ImU32 gradBot = IM_COL32(20, 22, 28, 0);
    dl->AddRectFilledMultiColor(winPos, ImVec2(winPos.x + winW, winPos.y + cardH), gradTop, gradTop, gradBot, gradBot);
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winW, winPos.y + 3.0f), accentColor, 8.0f, ImDrawFlags_RoundCornersTop);

    {
        ImVec2 stylesSize(64.0f, 24.0f);
        ImVec2 stylesPos(winPos.x + winW - stylesSize.x - padding, winPos.y + 42.0f);
        ImGui::SetCursorScreenPos(stylesPos);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
        if (ImGui::Button("🎨 Estilos##audio_styles", stylesSize))
            m_ShowStylePopup = true;
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        RenderStylePopup();

        ImGui::SetCursorScreenPos(ImVec2(stylesPos.x - 42.0f, stylesPos.y));
        RenderEqualizerButton();

        ImGui::SetCursorScreenPos(ImVec2(stylesPos.x - 42.0f - 56.0f, stylesPos.y));
        RenderLyricsButton();
    }

    {
        bool  live       = m_IsLiveBackground;
        bool  canGoLive  = live || (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()));
        const char* label = live ? "● EN VIVO" : "🚀 Enviar en vivo";
        ImVec2 btnSize(live ? 88.0f : 124.0f, 26.0f);
        ImVec2 btnPos(winPos.x + winW - btnSize.x - padding, winPos.y + 10.0f);

        ImGui::SetCursorScreenPos(btnPos);
        ImGui::BeginDisabled(!canGoLive);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        live ? MT::k_LiveBtn    : MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, live ? MT::k_LiveBtnHov : MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  live ? MT::k_LiveBtnAct : MT::k_NeutBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextWhite);

        if (ImGui::Button(label, btnSize)) {
            auto& core = Core::PresentationCore::Get();
            if (live) {
                SetLiveBackground(false);
                core.StopBackgroundMedia();
            } else {
                core.SetBackgroundAudio();
                core.SetProjecting(true);
                SetLiveBackground(true);
            }
        }

        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        ImGui::EndDisabled();
    }

    float discCX = winPos.x + padding + discR + 6.0f;
    float discCY = winPos.y + cardH * 0.50f;
    RenderSpinningDisc(discCX, discCY, discR);

    float textStartX = discCX + discR + 22.0f;
    float textAreaW  = winW - (textStartX - winPos.x) - padding;

    if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size())) {
        const auto& track = m_Tracks[m_CurrentTrack];

        const char* statusStr = m_IsPlaying
            ? (m_IsPaused ? "⏸ PAUSADO" : "▶ REPRODUCIENDO")
            : "⏹ DETENIDO";
        ImU32 statusColor = m_IsPlaying
            ? (m_IsPaused ? IM_COL32(245, 180, 50, 240) : IM_COL32(40, 220, 120, 240))
            : IM_COL32(110, 115, 130, 220);

        dl->AddText(nullptr, ImGui::GetFontSize() * 0.78f,
                    ImVec2(textStartX, winPos.y + 14.0f),
                    statusColor, statusStr);

        std::string displayStr = track.displayName;
        if (displayStr.size() > 32) displayStr = displayStr.substr(0, 30) + "...";
        dl->AddText(nullptr, ImGui::GetFontSize() * 1.15f,
                    ImVec2(textStartX, winPos.y + 32.0f),
                    IM_COL32(245, 248, 252, 255), displayStr.c_str());

        dl->AddText(nullptr, ImGui::GetFontSize() * 0.82f,
                    ImVec2(textStartX, winPos.y + 56.0f),
                    IM_COL32(115, 122, 140, 255), track.filename.c_str());

        const float waveAreaY = winPos.y + 82.0f;
        const float waveH     = 48.0f;
        const float barW      = std::max(2.0f, std::min(6.0f, (textAreaW - (kWaveBars - 1) * 2.0f) / kWaveBars));
        const float barGap    = 2.0f;
        const float totalWaveW = kWaveBars * (barW + barGap) - barGap;
        float waveStartX = textStartX;

        for (int i = 0; i < kWaveBars; i++) {
            float barHeight = m_WaveBars[i] * waveH;
            if (barHeight < 3.0f) barHeight = 3.0f;

            float bx  = waveStartX + i * (barW + barGap);
            float by0 = waveAreaY + (waveH - barHeight) * 0.5f;
            float by1 = by0 + barHeight;

            float brightness = 0.45f + m_WaveBars[i] * 0.55f;
            float barR, barG, barB;
            float barHue = std::fmod(hue + (float)i * 0.008f, 1.0f);
            HsvToRgb(barHue, 0.70f, brightness, barR, barG, barB);
            ImU32 barColor = IM_COL32(static_cast<int>(barR * 255),
                                      static_cast<int>(barG * 255),
                                      static_cast<int>(barB * 255),
                                      static_cast<int>(180 + m_WaveBars[i] * 75));

            dl->AddRectFilled(ImVec2(bx, by0), ImVec2(bx + barW, by1), barColor, 1.5f);
        }

        if (m_IsPlaying && m_TotalTimeMs > 0) {
            float posX = waveStartX + m_Progress * totalWaveW;
            dl->AddLine(ImVec2(posX, waveAreaY - 2.0f),
                        ImVec2(posX, waveAreaY + waveH + 2.0f),
                        IM_COL32(255, 255, 255, 200), 2.0f);
            dl->AddCircleFilled(ImVec2(posX, waveAreaY), 3.0f, IM_COL32(255, 255, 255, 240));
        }

    } else {
        dl->AddText(nullptr, ImGui::GetFontSize() * 1.05f,
                    ImVec2(textStartX, winPos.y + cardH * 0.38f),
                    IM_COL32(85, 92, 110, 255), "Sin pista seleccionada");
        dl->AddText(nullptr, ImGui::GetFontSize() * 0.82f,
                    ImVec2(textStartX, winPos.y + cardH * 0.38f + 24.0f),
                    IM_COL32(60, 66, 80, 255),
                    "Selecciona una pista de la lista o pulsa '+ Importar'");
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void AudioPanel::RenderLiveBackground(float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;

    float topR, topG, topB, botR, botG, botB;
    HsvToRgb(hue, 0.35f, 0.09f, topR, topG, topB);
    HsvToRgb(hue, 0.45f, 0.02f, botR, botG, botB);
    ImU32 topCol = IM_COL32(static_cast<int>(topR * 255), static_cast<int>(topG * 255),
                             static_cast<int>(topB * 255), 255);
    ImU32 botCol = IM_COL32(static_cast<int>(botR * 255), static_cast<int>(botG * 255),
                             static_cast<int>(botB * 255), 255);
    dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), topCol, topCol, botCol, botCol);

    float discR  = std::min(w, h) * 0.26f;
    float discCX = x + w * 0.5f;
    float discCY = y + h * 0.42f;
    RenderSpinningDisc(discCX, discCY, discR);

    if (m_CurrentTrack < 0 || m_CurrentTrack >= static_cast<int>(m_Tracks.size()))
        return;

    const auto& track = m_Tracks[m_CurrentTrack];

    float titleSize = std::clamp(h * 0.032f, ImGui::GetFontSize(), ImGui::GetFontSize() * 4.0f);
    float scaleFactor = titleSize / ImGui::GetFontSize();
    ImVec2 baseTs = ImGui::CalcTextSize(track.displayName.c_str());
    ImVec2 titleTs = ImVec2(baseTs.x * scaleFactor, baseTs.y * scaleFactor);
    float titleY = discCY + discR + h * 0.06f;
    dl->AddText(nullptr, titleSize,
                ImVec2(x + (w - titleTs.x) * 0.5f, titleY),
                IM_COL32(240, 242, 245, 255), track.displayName.c_str());

    if (!m_ShowWaveform) return;

    float waveAreaY  = titleY + titleTs.y + h * 0.035f;
    float waveH      = h * 0.09f;
    float waveAreaW  = w * 0.46f;
    float barGap     = 4.0f;
    float barW       = waveAreaW / static_cast<float>(kWaveBars) - barGap;
    float waveStartX = x + (w - waveAreaW) * 0.5f;

    for (int i = 0; i < kWaveBars; i++) {
        float barHeight = m_WaveBars[i] * waveH;
        if (barHeight < 3.0f) barHeight = 3.0f;

        float bx  = waveStartX + i * (barW + barGap);
        float by0 = waveAreaY + (waveH - barHeight) * 0.5f;
        float by1 = by0 + barHeight;

        float brightness = 0.45f + m_WaveBars[i] * 0.55f;
        float barR, barG, barB;
        HsvToRgb(hue, 0.65f, brightness, barR, barG, barB);
        ImU32 barColor = IM_COL32(static_cast<int>(barR * 255), static_cast<int>(barG * 255),
                                   static_cast<int>(barB * 255),
                                   static_cast<int>(190 + m_WaveBars[i] * 65));

        dl->AddRectFilled(ImVec2(bx, by0), ImVec2(bx + barW, by1), barColor, barW * 0.3f);
    }
}

void AudioPanel::RenderProgressBar() {
    ImGui::Spacing();

    float hue = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()))
        ? m_Tracks[m_CurrentTrack].accentH : 0.58f;
    float accentR, accentG, accentB;
    HsvToRgb(hue, 0.65f, 0.90f, accentR, accentG, accentB);

    ImGui::SetCursorPosX(14.0f);

    std::string tCurrent = FormatTime(m_CurrentTimeMs);
    std::string tTotal   = FormatTime(m_TotalTimeMs);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.65f, 0.75f, 1.0f));
    ImGui::TextUnformatted(tCurrent.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 10.0f);
    float barWidth = ImGui::GetContentRegionAvail().x - 68.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImVec4(0.14f, 0.16f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
        ImVec4(accentR, accentG, accentB, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
        ImVec4(std::min(accentR + 0.18f, 1.0f),
               std::min(accentG + 0.18f, 1.0f),
               std::min(accentB + 0.18f, 1.0f), 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    ImGui::SetNextItemWidth(barWidth);
    bool wasSeekingLastFrame = m_IsSeeking;
    ImGui::SliderFloat("##progress", &m_Progress, 0.0f, 1.0f, "");
    m_IsSeeking = ImGui::IsItemActive();
    bool isBarHovered = ImGui::IsItemHovered();

    if (wasSeekingLastFrame && !m_IsSeeking)
        SeekTo(m_Progress);

    if (isBarHovered && m_TotalTimeMs > 0) {
        float mouseRelX = (ImGui::GetIO().MousePos.x - ImGui::GetItemRectMin().x) / ImGui::GetItemRectSize().x;
        mouseRelX = std::clamp(mouseRelX, 0.0f, 1.0f);
        int64_t hoveredMs = static_cast<int64_t>(mouseRelX * static_cast<float>(m_TotalTimeMs));
        ImGui::SetTooltip("⏱ %s", FormatTime(hoveredMs).c_str());
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.45f, 0.55f, 1.0f));
    ImGui::TextUnformatted(tTotal.c_str());
    ImGui::PopStyleColor();
}

void AudioPanel::RenderTransportControls() {
    ImGui::Spacing();

    const float btnSize      = 38.0f;
    const float mainPlaySize = 44.0f;
    const float smallSize    = 30.0f;
    const float iconMain     = 18.0f;
    const float iconSmall    = 14.0f;

    const float totalW = smallSize * 2.0f + btnSize * 4.0f + mainPlaySize + 6.0f * 6.0f;
    float startX = (ImGui::GetContentRegionAvail().x - totalW) * 0.5f;
    if (startX < 0.0f) startX = 0.0f;
    ImGui::SetCursorPosX(startX);

    if (MonitorStyleButton("##shuffle", nullptr, DrawIcon_Shuffle, iconSmall,
                           MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_PrevBtnAct,
                           ImVec2(smallSize, smallSize), m_Shuffle))
        m_Shuffle = !m_Shuffle;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_Shuffle ? "Modo aleatorio: Activado" : "Modo aleatorio: Desactivado");

    ImGui::SameLine(0.0f, 6.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
    if (ImGui::Button("-10s##skipback", ImVec2(btnSize, smallSize))) {
        SkipSeconds(-10);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Retroceder 10 segundos");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 6.0f);

    if (MonitorStyleButton("##prev", "skip_prev", nullptr, iconMain,
                           MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, ImVec2(btnSize, btnSize)))
        Previous();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pista anterior");

    ImGui::SameLine(0.0f, 6.0f);

    {
        const char* ppIconKey = (m_IsPlaying && !m_IsPaused) ? "pause" : "play";
        ImVec4 playBg = (m_IsPlaying && !m_IsPaused) ? ImVec4(0.20f, 0.45f, 0.95f, 1.0f) : MT::k_PrevBtn;
        ImVec4 playHov = (m_IsPlaying && !m_IsPaused) ? ImVec4(0.30f, 0.55f, 1.0f, 1.0f) : MT::k_PrevBtnHov;
        ImVec4 playAct = ImVec4(0.15f, 0.35f, 0.85f, 1.0f);

        if (MonitorStyleButton("##pp", ppIconKey, nullptr, iconMain + 6.0f,
                               playBg, playHov, playAct,
                               ImVec2(mainPlaySize, btnSize), true))
            TogglePlayPause();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip((m_IsPlaying && !m_IsPaused) ? "Pausar" : "Reproducir");
    }

    ImGui::SameLine(0.0f, 6.0f);

    if (MonitorStyleButton("##next", "skip_next", nullptr, iconMain,
                           MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_NeutBtnAct, ImVec2(btnSize, btnSize)))
        Next();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pista siguiente");

    ImGui::SameLine(0.0f, 6.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
    if (ImGui::Button("+10s##skipfwd", ImVec2(btnSize, smallSize))) {
        SkipSeconds(10);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Avanzar 10 segundos");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 6.0f);

    const char* repeatIconKey = (m_RepeatMode == AudioRepeatMode::One) ? "repeat_one" : "repeat";
    if (MonitorStyleButton("##repeat", repeatIconKey, nullptr, iconSmall,
                           MT::k_NeutBtn, MT::k_NeutBtnHov, MT::k_AmberBtnAct,
                           ImVec2(smallSize, smallSize), m_RepeatMode != AudioRepeatMode::None)) {
        switch (m_RepeatMode) {
            case AudioRepeatMode::None: m_RepeatMode = AudioRepeatMode::All;  break;
            case AudioRepeatMode::All:  m_RepeatMode = AudioRepeatMode::One;  break;
            case AudioRepeatMode::One:  m_RepeatMode = AudioRepeatMode::None; break;
        }
    }
    if (ImGui::IsItemHovered()) {
        const char* repTip = (m_RepeatMode == AudioRepeatMode::None) ? "Repetir: Desactivado"
                           : (m_RepeatMode == AudioRepeatMode::All) ? "Repetir: Toda la lista"
                           : "Repetir: Esta pista";
        ImGui::SetTooltip("%s", repTip);
    }
}

void AudioPanel::RenderVolumeRow() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float labelW  = 72.0f;
    const float muteW   = 24.0f;
    const float gapW    = 6.0f;
    const float valueW  = 52.0f;
    const float sliderW = ImGui::GetContentRegionAvail().x
                          - labelW - muteW - gapW - valueW - 24.0f;

    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextSecondary);
    ImGui::TextUnformatted("Volumen");
    ImGui::PopStyleColor();
    ImGui::SameLine(labelW);

    if (MonitorStyleButton("##mute", nullptr, m_Muted ? DrawIcon_SpeakerMuted : DrawIcon_SpeakerOn,
                           muteW * 0.62f,
                           m_Muted ? MT::k_LiveBtn : MT::k_NeutBtn,
                           m_Muted ? MT::k_LiveBtnHov : MT::k_NeutBtnHov,
                           m_Muted ? MT::k_LiveBtnAct : MT::k_NeutBtnAct,
                           ImVec2(muteW, muteW), m_Muted)) {
        if (!m_Muted) {
            m_VolumeBeforeMute = m_Volume;
            m_Muted = true;
            m_VlcPlayer.SetMute(true);
        } else {
            m_Muted  = false;
            m_Volume = m_VolumeBeforeMute;
            m_VlcPlayer.SetMute(false);
            m_VlcPlayer.SetVolume(ComputeEffectiveVolume());
        }
    }
    ImGui::SameLine(0.0f, gapW);

    {
        float volF = static_cast<float>(m_Volume);
        ImU32 trackCol = ImGui::GetColorU32(m_Muted ? MT::k_LiveTrack : MT::k_PrevTrack);
        ImU32 grabCol  = ImGui::GetColorU32(m_Muted ? MT::k_LiveGrab  : MT::k_PrevGrab);
        if (HorizontalFader("##vol", &volF, 0.0f, 200.0f, ImVec2(sliderW, muteW), trackCol, grabCol, grabCol)) {
            m_Volume = static_cast<int>(volF);
            SetVolume(m_Volume);
        }
    }

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
    ImGui::Text("%3d%%", m_Volume);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::SetCursorPosX(12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextSecondary);
    ImGui::TextUnformatted("Ganancia");
    ImGui::PopStyleColor();
    ImGui::SameLine(labelW + muteW + gapW);

    ImVec4 gainGrab;
    if      (m_GainDb < -0.5f) gainGrab = ImVec4(0.35f, 0.55f, 0.90f, 1.0f);
    else if (m_GainDb >  6.0f) gainGrab = ImVec4(0.95f, 0.40f, 0.25f, 1.0f);
    else if (m_GainDb >  0.5f) gainGrab = ImVec4(0.90f, 0.72f, 0.20f, 1.0f);
    else                       gainGrab = ImVec4(0.35f, 0.82f, 0.55f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,          MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       gainGrab);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
        ImVec4(gainGrab.x + 0.10f, gainGrab.y + 0.10f, gainGrab.z + 0.10f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding,  5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(sliderW);
    if (ImGui::SliderFloat("##gain", &m_GainDb, -20.0f, 20.0f, ""))
        ApplyGain(m_GainDb);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, gainGrab);
    ImGui::Text("%+.1fdB", m_GainDb);
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    if (ImGui::SmallButton("0dB")) ApplyGain(0.0f);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

void AudioPanel::RenderEqualizerButton() {
    const float eqBtnW = 34.0f, eqBtnH = 22.0f;

    ImGui::PushStyleColor(ImGuiCol_Button,        m_EqEnabled ? MT::k_AmberBtn    : MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  m_EqEnabled ? MT::k_AmberBtnHov : MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   m_EqEnabled ? MT::k_AmberBtnAct : MT::k_NeutBtnAct);
    ImGui::PushStyleColor(ImGuiCol_Text,           m_EqEnabled ? MT::k_AmberAccent : MT::k_TextDim);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    if (ImGui::Button("EQ##audio_eq", { eqBtnW, eqBtnH }))
        ImGui::OpenPopup("##audio_eq_popup");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Ecualizador");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    RenderEqualizerPopup();
}

void AudioPanel::RenderEqualizerPopup() {
    if (!ImGui::BeginPopup("##audio_eq_popup"))
        return;

    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
    ImGui::TextUnformatted("ECUALIZADOR");
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::Checkbox("Activar", &m_EqEnabled))
        ApplyEqualizerToPlayer();

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Reset")) {
        m_EqPreamp = 0.0f;
        for (int b = 0; b < kEqBands; b++) m_EqBands[b] = 0.0f;
        ApplyEqualizerToPlayer();
    }

    ImGui::SetNextItemWidth(224.0f);
    if (ImGui::SliderFloat("Preamp", &m_EqPreamp, -20.0f, 20.0f, "%.1f dB"))
        m_VlcPlayer.SetEqualizerPreamp(m_EqPreamp);

    ImGui::Spacing();

    for (int b = 0; b < kEqBands; b++) {
        ImGui::PushID(b);
        ImGui::BeginGroup();
        if (ImGui::VSliderFloat("##band", ImVec2(20.0f, 90.0f), &m_EqBands[b], -20.0f, 20.0f, ""))
            m_VlcPlayer.SetEqualizerBand(b, m_EqBands[b]);
        ImVec2 lblSz = ImGui::CalcTextSize(kBandLabels[b]);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (20.0f - lblSz.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
        ImGui::TextUnformatted(kBandLabels[b]);
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ImGui::PopID();
        if (b < kEqBands - 1) ImGui::SameLine();
    }

    ImGui::EndPopup();
}

void AudioPanel::RenderLyricsButton() {
    const bool hasTrack  = (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size()));
    const bool hasLyrics = hasTrack && !m_Tracks[m_CurrentTrack].lyricsText.empty();
    const bool showingLive = hasTrack && m_Tracks[m_CurrentTrack].lyricsEnabled;

    ImVec4 bg, bgHov, bgAct, txt;
    if (showingLive)    { bg = MT::k_AmberBtn; bgHov = MT::k_AmberBtnHov; bgAct = MT::k_AmberBtnAct; txt = MT::k_AmberAccent; }
    else if (hasLyrics) { bg = MT::k_PrevBtn;  bgHov = MT::k_PrevBtnHov;  bgAct = MT::k_PrevBtnAct;  txt = MT::k_PrevAccent; }
    else                { bg = MT::k_NeutBtn;  bgHov = MT::k_NeutBtnHov;  bgAct = MT::k_NeutBtnAct;  txt = MT::k_TextDim; }

    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  bgAct);
    ImGui::PushStyleColor(ImGuiCol_Text,          txt);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::BeginDisabled(!hasTrack);
    if (ImGui::Button("Letra##audio_lyrics", { 46.0f, 22.0f })) {
        const AudioTrack& t = m_Tracks[m_CurrentTrack];
        std::strncpy(m_LyricsUrlBuffer, t.sourceUrl.c_str(), sizeof(m_LyricsUrlBuffer) - 1);
        m_LyricsUrlBuffer[sizeof(m_LyricsUrlBuffer) - 1] = '\0';
        m_LyricsImportError.clear();
        ImGui::OpenPopup("##audio_lyrics_popup");
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(hasLyrics ? "Letra importada -- click para editar"
                                     : "Importar letra desde una URL (yt-dlp)");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    RenderLyricsPopup();
}

void AudioPanel::RenderLyricsPopup() {
    bool resultReady = false;
    Core::SubtitleFetchResult resultCopy;
    {
        std::lock_guard<std::mutex> lk(m_LyricsImportMutex);
        if (m_LyricsImportResult.has_value() && !m_LyricsImportRunning) {
            resultCopy  = *m_LyricsImportResult;
            resultReady = true;
            m_LyricsImportResult.reset();
        }
    }
    if (resultReady) {
        if (m_LyricsImportThread.joinable())
            m_LyricsImportThread.join();

        if (m_CurrentTrack >= 0 && m_CurrentTrack < static_cast<int>(m_Tracks.size())) {
            AudioTrack& track = m_Tracks[m_CurrentTrack];
            if (resultCopy.success) {
                track.sourceUrl     = m_LyricsUrlBuffer;
                track.lyricsText    = resultCopy.lyrics;
                track.lyricsEnabled = true;
                SaveTrackLyricsSidecar(track);
                m_LyricsImportError.clear();
                if (m_IsLiveBackground) RefreshLiveLyrics();
            } else {
                m_LyricsImportError = resultCopy.error;
            }
        }
    }

    if (!ImGui::BeginPopup("##audio_lyrics_popup"))
        return;

    if (m_CurrentTrack < 0 || m_CurrentTrack >= static_cast<int>(m_Tracks.size())) {
        ImGui::TextUnformatted("Selecciona una pista primero.");
        ImGui::EndPopup();
        return;
    }
    AudioTrack& track = m_Tracks[m_CurrentTrack];

    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
    ImGui::TextUnformatted("LETRA DESDE URL");
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextSecondary);
    ImGui::TextWrapped("Pega el link del video -- se buscan sus subtitulos y se guardan "
                        "como letra de esta pista, para activarla o quitarla cuando quieras.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(300.0f);
    ImGui::BeginDisabled(m_LyricsImportRunning);
    bool enterPressed = ImGui::InputTextWithHint("##audio_lyrics_url",
        "https://www.youtube.com/watch?v=...",
        m_LyricsUrlBuffer, sizeof(m_LyricsUrlBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::EndDisabled();

    ImGui::Spacing();

    bool wantStart = false;
    if (m_LyricsImportRunning) {
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_PrevAccent);
        ImGui::TextUnformatted("Buscando subtitulos...");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_PrevBtn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_PrevBtnHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  MT::k_PrevBtnAct);
        ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextWhite);
        if (ImGui::Button(track.lyricsText.empty() ? "Importar" : "Reimportar", ImVec2(110.0f, 28.0f)))
            wantStart = true;
        ImGui::PopStyleColor(4);
        if (enterPressed) wantStart = true;

        if (!track.lyricsText.empty()) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
            ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
            if (ImGui::Button("Quitar", ImVec2(80.0f, 28.0f))) {
                track.sourceUrl.clear();
                track.lyricsText.clear();
                track.lyricsEnabled = false;
                SaveTrackLyricsSidecar(track);
                if (m_IsLiveBackground) RefreshLiveLyrics();
            }
            ImGui::PopStyleColor(3);
        }
    }

    if (wantStart && !m_LyricsImportRunning && m_LyricsUrlBuffer[0] != '\0')
        RequestLyricsImport(m_LyricsUrlBuffer);

    if (!m_LyricsImportError.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", m_LyricsImportError.c_str());
        ImGui::PopStyleColor();
    }

    if (!track.lyricsText.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        MT::k_NeutBtn);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, MT::k_NeutBtnHov);
        ImGui::PushStyleColor(ImGuiCol_CheckMark,      MT::k_QueueAccent);
        ImGui::PushStyleColor(ImGuiCol_Text,           MT::k_TextSecondary);
        if (ImGui::Checkbox("Mostrar en vivo", &track.lyricsEnabled)) {
            SaveTrackLyricsSidecar(track);
            if (m_IsLiveBackground) RefreshLiveLyrics();
        }
        ImGui::PopStyleColor(4);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextDim);
        ImGui::TextUnformatted("Vista previa:");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
        ImGui::BeginChild("##audio_lyrics_preview", ImVec2(320.0f, 120.0f), true);
        ImGui::PushStyleColor(ImGuiCol_Text, MT::k_TextSecondary);
        ImGui::TextWrapped("%s", track.lyricsText.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::EndPopup();
}

void AudioPanel::RenderPlaylist() {
    ImGui::Spacing();
    ImGui::SetCursorPosX(14.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.40f, 0.85f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.48f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    std::string allLabel = "🎵 Pistas (" + std::to_string(m_Tracks.size()) + ")";
    ImGui::Button(allLabel.c_str(), ImVec2(0.0f, 26.0f));
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
    std::string queueLabel = "📑 Cola (" + std::to_string(m_AudioQueue.size()) + ")";
    if (ImGui::Button(queueLabel.c_str(), ImVec2(0.0f, 26.0f))) {
        m_ViewQueueTab = true;
    }
    ImGui::PopStyleColor(3);

    if (!m_Tracks.empty()) {
        ImGui::SameLine();
        float rightX = ImGui::GetWindowWidth() - 145.0f;
        if (rightX > ImGui::GetCursorPosX() + 10.0f) {
            ImGui::SetCursorPosX(rightX);
            ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
            ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
            if (ImGui::Button("+ Toda a la cola", ImVec2(0.0f, 26.0f))) {
                for (size_t t = 0; t < m_Tracks.size(); t++) {
                    AddToQueue((int)t);
                }
            }
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
    ImGui::BeginChild("##AudioPlaylist", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (m_Tracks.empty()) {
        ImGui::SetCursorPos(ImVec2(14.0f, 16.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextWrapped("No hay archivos de audio disponibles.\n"
                           "Haz clic en '+ Importar' arriba para agregar canciones.");
        ImGui::PopStyleColor();
    }

    std::string searchFilter = m_SearchBuffer;
    std::transform(searchFilter.begin(), searchFilter.end(), searchFilter.begin(),
                   [](unsigned char c){ return (char)::tolower(c); });

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    const float rowH   = 40.0f;
    const float availW = ImGui::GetContentRegionAvail().x;

    for (int i = 0; i < static_cast<int>(m_Tracks.size()); i++) {
        const auto& track = m_Tracks[i];

        if (!searchFilter.empty()) {
            std::string nameLo = track.displayName + " " + track.filename;
            std::transform(nameLo.begin(), nameLo.end(), nameLo.begin(),
                           [](unsigned char c){ return (char)::tolower(c); });
            if (nameLo.find(searchFilter) == std::string::npos)
                continue;
        }

        bool isCurrent = (m_CurrentTrack == i);
        ImGui::PushID(i);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + availW, p0.y + rowH);

        bool clicked = ImGui::InvisibleButton("##row", ImVec2(availW, rowH));
        bool hovered = ImGui::IsItemHovered();

        if (isCurrent) {
            float hue = track.accentH;
            float r, g, b;
            HsvToRgb(hue, 0.60f, 0.35f, r, g, b);
            dl->AddRectFilled(p0, p1,
                IM_COL32(static_cast<int>(r * 255),
                         static_cast<int>(g * 255),
                         static_cast<int>(b * 255),
                         static_cast<int>(m_IsPlaying && !m_IsPaused ? 190 : 130)),
                6.0f);

            dl->AddRect(p0, p1,
                IM_COL32(static_cast<int>(r * 255),
                         static_cast<int>(g * 255),
                         static_cast<int>(b * 255), 240),
                6.0f, 0, 1.5f);

            float aR, aG, aB;
            HsvToRgb(hue, 0.70f, 0.95f, aR, aG, aB);
            dl->AddRectFilled(p0,
                ImVec2(p0.x + 4.0f, p1.y),
                IM_COL32(static_cast<int>(aR * 255),
                         static_cast<int>(aG * 255),
                         static_cast<int>(aB * 255), 255), 6.0f, ImDrawFlags_RoundCornersLeft);
        } else if (hovered) {
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18), 6.0f);
        } else {
            dl->AddRectFilled(p0, p1, IM_COL32(30, 32, 38, 140), 6.0f);
        }

        std::string numStr = std::to_string(i + 1);
        dl->AddText(ImVec2(p0.x + 12.0f, p0.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            isCurrent
                ? [&]() -> ImU32 {
                      float r, g, b;
                      HsvToRgb(track.accentH, 0.55f, 1.0f, r, g, b);
                      return IM_COL32(static_cast<int>(r*255),
                                     static_cast<int>(g*255),
                                     static_cast<int>(b*255), 255);
                  }()
                : IM_COL32(80, 88, 105, 255),
            numStr.c_str());

        float iconAreaX = p0.x + 36.0f;
        float centerY   = p0.y + rowH * 0.5f;

        if (isCurrent && m_IsPlaying && !m_IsPaused) {
            float t = static_cast<float>(ImGui::GetTime());
            float aR, aG, aB;
            HsvToRgb(track.accentH, 0.70f, 1.0f, aR, aG, aB);
            ImU32 barColor = IM_COL32(static_cast<int>(aR*255),
                                      static_cast<int>(aG*255),
                                      static_cast<int>(aB*255), 255);
            for (int bar = 0; bar < 3; bar++) {
                float phase = t * 4.0f + bar * 1.4f;
                float bh    = 4.0f + std::abs(std::sin(phase)) * 12.0f;
                float bx    = iconAreaX + bar * 6.0f;
                dl->AddRectFilled(
                    ImVec2(bx, centerY - bh * 0.5f),
                    ImVec2(bx + 4.0f, centerY + bh * 0.5f),
                    barColor, 2.0f);
            }
        } else {
            float r, g, b;
            HsvToRgb(track.accentH, 0.6f, 0.85f, r, g, b);
            ImU32 noteCol = isCurrent ? IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), 255)
                                      : IM_COL32(75, 80, 95, 200);
            dl->AddCircleFilled(ImVec2(iconAreaX + 6.0f, centerY), 6.0f, noteCol);
        }

        float textX = p0.x + 64.0f;
        dl->AddText(ImVec2(textX, p0.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            isCurrent ? IM_COL32(245, 248, 255, 255)
                      : (hovered ? IM_COL32(225, 230, 242, 255) : IM_COL32(165, 172, 188, 255)),
            track.displayName.c_str());

        std::string ext = "";
        auto dotPos = track.filename.rfind('.');
        if (dotPos != std::string::npos) {
            ext = track.filename.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return (char)::toupper(c); });
        }

        float badgeRightX = p1.x - 12.0f;
        if (!ext.empty()) {
            ImVec2 extSz = ImGui::CalcTextSize(ext.c_str());
            float badgeW = extSz.x + 10.0f;
            float badgeH = extSz.y + 4.0f;
            ImVec2 badgePos(badgeRightX - badgeW, p0.y + (rowH - badgeH) * 0.5f);

            dl->AddRectFilled(badgePos, ImVec2(badgePos.x + badgeW, badgePos.y + badgeH),
                              IM_COL32(20, 22, 28, 200), 4.0f);
            dl->AddText(ImVec2(badgePos.x + 5.0f, badgePos.y + 2.0f),
                        IM_COL32(110, 118, 138, 255), ext.c_str());
            badgeRightX = badgePos.x - 8.0f;
        }

        {
            ImVec2 btnPos(badgeRightX - 52.0f, p0.y + (rowH - 22.0f) * 0.5f);
            ImGui::SetCursorScreenPos(btnPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        hovered ? IM_COL32(45, 50, 65, 220) : IM_COL32(32, 36, 46, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 70, 95, 255));
            ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(160, 175, 205, 255));
            if (ImGui::Button("+ Cola##qbtn", ImVec2(48.0f, 22.0f))) {
                AddToQueue(i);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            Play(i);
        } else if (clicked) {
            m_CurrentTrack = i;
            m_LastExternalSelection = track.filename;
            Core::LibrarySelection sel;
            sel.type = Core::ItemType::Audio;
            sel.title = track.filename;
            sel.contentData = { track.fullPath };
            Core::PresentationCore::Get().SetSelection(sel);
            EnsureCoverLoaded(i);
        }

        if (ImGui::BeginPopupContextItem(("##track_ctx_" + std::to_string(i)).c_str())) {
            if (ImGui::MenuItem("▶ Reproducir ahora")) {
                Play(i);
            }
            if (ImGui::MenuItem("➕ Agregar a la cola")) {
                AddToQueue(i);
            }
            if (ImGui::MenuItem("🚀 Enviar en vivo al proyector")) {
                Play(i);
                auto& core = Core::PresentationCore::Get();
                core.SetBackgroundAudio();
                core.SetProjecting(true);
                SetLiveBackground(true);
            }
            if (ImGui::MenuItem("📝 Letra / Subtítulos...")) {
                m_CurrentTrack = i;
                const AudioTrack& t = m_Tracks[i];
                std::strncpy(m_LyricsUrlBuffer, t.sourceUrl.c_str(), sizeof(m_LyricsUrlBuffer) - 1);
                m_LyricsUrlBuffer[sizeof(m_LyricsUrlBuffer) - 1] = '\0';
                m_LyricsImportError.clear();
                m_ShowLyricsPopup = true;
                ImGui::OpenPopup("##audio_lyrics_popup");
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, MT::k_LiveAccent);
            if (ImGui::MenuItem("🗑 Eliminar pista")) {
                Core::FileDeletionManager::ForceDeleteFile(track.fullPath);
                RefreshLibrary();
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void AudioPanel::RenderQueueList() {
    ImGui::Spacing();
    ImGui::SetCursorPosX(14.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Button,        MT::k_NeutBtn);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, MT::k_NeutBtnHov);
    ImGui::PushStyleColor(ImGuiCol_Text,          MT::k_TextSecondary);
    std::string allLabel = "🎵 Pistas (" + std::to_string(m_Tracks.size()) + ")";
    if (ImGui::Button(allLabel.c_str(), ImVec2(0.0f, 26.0f))) {
        m_ViewQueueTab = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.90f, 0.55f, 0.15f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.65f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    std::string queueLabel = "📑 Cola (" + std::to_string(m_AudioQueue.size()) + ")";
    ImGui::Button(queueLabel.c_str(), ImVec2(0.0f, 26.0f));
    ImGui::PopStyleColor(3);

    if (!m_AudioQueue.empty()) {
        ImGui::SameLine();
        float rightX = ImGui::GetWindowWidth() - 110.0f;
        if (rightX > ImGui::GetCursorPosX() + 10.0f) {
            ImGui::SetCursorPosX(rightX);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.35f, 0.12f, 0.12f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.95f, 0.60f, 0.60f, 1.0f));
            if (ImGui::Button("🗑 Vaciar cola", ImVec2(0.0f, 26.0f))) {
                ClearQueue();
            }
            ImGui::PopStyleColor(3);
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, MT::k_Bg3);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
    ImGui::BeginChild("##AudioQueueChild", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    if (m_AudioQueue.empty()) {
        ImGui::SetCursorPos(ImVec2(14.0f, 20.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.45f, 0.55f, 1.0f));
        ImGui::TextWrapped("La cola de reproducción está vacía.\n\n"
                           "Cambia a la pestaña 'Pistas' y usa '+ Cola' o clic derecho "
                           "para agregar canciones a la cola consecutiva.");
        ImGui::PopStyleColor();
    }

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    const float rowH   = 42.0f;
    const float availW = ImGui::GetContentRegionAvail().x;

    for (int q = 0; q < static_cast<int>(m_AudioQueue.size()); q++) {
        const std::string& path = m_AudioQueue[q];
        int trackIdx = FindTrackIndexByPath(path);

        std::string displayName;
        if (trackIdx >= 0) {
            displayName = m_Tracks[trackIdx].displayName;
        } else {
            displayName = fs::path(path).stem().u8string();
        }

        bool isCurrent = (m_QueueCurrentIndex == q);
        ImGui::PushID(100000 + q);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + availW, p0.y + rowH);

        bool clicked = ImGui::InvisibleButton("##qrow", ImVec2(availW, rowH));
        bool hovered = ImGui::IsItemHovered();

        if (isCurrent) {
            dl->AddRectFilled(p0, p1, IM_COL32(230, 140, 30, (m_IsPlaying && !m_IsPaused ? 190 : 130)), 6.0f);
            dl->AddRect(p0, p1, IM_COL32(255, 170, 50, 240), 6.0f, 0, 1.5f);
            dl->AddRectFilled(p0, ImVec2(p0.x + 4.0f, p1.y), IM_COL32(255, 180, 60, 255), 6.0f, ImDrawFlags_RoundCornersLeft);
        } else if (hovered) {
            dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18), 6.0f);
        } else {
            dl->AddRectFilled(p0, p1, IM_COL32(30, 32, 38, 140), 6.0f);
        }

        std::string numStr = "#" + std::to_string(q + 1);
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            isCurrent ? IM_COL32(255, 240, 210, 255) : IM_COL32(110, 120, 145, 255),
            numStr.c_str());

        float iconAreaX = p0.x + 44.0f;
        float centerY   = p0.y + rowH * 0.5f;
        if (isCurrent && m_IsPlaying && !m_IsPaused) {
            float t = static_cast<float>(ImGui::GetTime());
            ImU32 barColor = IM_COL32(255, 200, 80, 255);
            for (int bar = 0; bar < 3; bar++) {
                float phase = t * 4.0f + bar * 1.4f;
                float bh    = 4.0f + std::abs(std::sin(phase)) * 12.0f;
                float bx    = iconAreaX + bar * 6.0f;
                dl->AddRectFilled(
                    ImVec2(bx, centerY - bh * 0.5f),
                    ImVec2(bx + 4.0f, centerY + bh * 0.5f),
                    barColor, 2.0f);
            }
        } else {
            dl->AddCircleFilled(ImVec2(iconAreaX + 6.0f, centerY), 6.0f,
                isCurrent ? IM_COL32(255, 180, 50, 255) : IM_COL32(75, 80, 95, 200));
        }

        float textX = p0.x + 72.0f;
        dl->AddText(ImVec2(textX, p0.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f),
            isCurrent ? IM_COL32(255, 255, 255, 255)
                      : (hovered ? IM_COL32(230, 235, 245, 255) : IM_COL32(170, 175, 190, 255)),
            displayName.c_str());

        float btnAreaRight = p1.x - 8.0f;

        {
            ImVec2 xPos(btnAreaRight - 24.0f, p0.y + (rowH - 22.0f) * 0.5f);
            ImGui::SetCursorScreenPos(xPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(50, 30, 30, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(180, 40, 40, 255));
            ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(240, 140, 140, 255));
            if (ImGui::Button("✕##rmq", ImVec2(24.0f, 22.0f))) {
                RemoveFromQueue(q);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            btnAreaRight = xPos.x - 4.0f;
        }

        if (q < static_cast<int>(m_AudioQueue.size()) - 1) {
            ImVec2 dPos(btnAreaRight - 24.0f, p0.y + (rowH - 22.0f) * 0.5f);
            ImGui::SetCursorScreenPos(dPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(35, 40, 52, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 75, 105, 255));
            ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(180, 190, 210, 255));
            if (ImGui::Button("▼##dnq", ImVec2(24.0f, 22.0f))) {
                MoveQueueItem(q, q + 1);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            btnAreaRight = dPos.x - 4.0f;
        }

        if (q > 0) {
            ImVec2 uPos(btnAreaRight - 24.0f, p0.y + (rowH - 22.0f) * 0.5f);
            ImGui::SetCursorScreenPos(uPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(35, 40, 52, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 75, 105, 255));
            ImGui::PushStyleColor(ImGuiCol_Text,          IM_COL32(180, 190, 210, 255));
            if (ImGui::Button("▲##upq", ImVec2(24.0f, 22.0f))) {
                MoveQueueItem(q, q - 1);
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }

        if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            PlayQueueIndex(q);
        } else if (clicked) {
            m_QueueCurrentIndex = q;
            int tIdx = FindTrackIndexByPath(path);
            if (tIdx >= 0) {
                m_CurrentTrack = tIdx;
                m_LastExternalSelection = m_Tracks[tIdx].filename;
                Core::LibrarySelection sel;
                sel.type = Core::ItemType::Audio;
                sel.title = m_Tracks[tIdx].filename;
                sel.contentData = { m_Tracks[tIdx].fullPath };
                Core::PresentationCore::Get().SetSelection(sel);
                EnsureCoverLoaded(tIdx);
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}
void AudioPanel::EnsureCoverLoaded(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int>(m_Tracks.size()))
        return;

    AudioTrack& track = m_Tracks[trackIndex];
    if (track.coverLoaded)
        return;

    track.coverLoaded = true;

    track.coverArt = ProyecThor::Audio::ExtractAlbumArt(track.fullPath);

    if (track.coverArt.HasData())
        ProyecThor::Audio::UploadAlbumArtToGL(track.coverArt);
}

}
