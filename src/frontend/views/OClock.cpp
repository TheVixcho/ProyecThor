#include "OClock.h"
#include "GlassRenderer.h"
#include "DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/WikiHelp.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace ProyecThor::UI {

static ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}
static ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}

static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
        int alpha = static_cast<int>(34.0f - (i * 5.0f));
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 3.0f),
            ImVec2(p1.x + i, p1.y + i + 3.0f),
            IM_COL32(0, 0, 0, std::max(0, alpha)), rounding + i);
    }
}

static constexpr int kPresetMinutes[] = { 5, 10, 15, 20, 30, 45 };

namespace {
    constexpr float kGapTight  = 5.0f;
    constexpr float kGapNormal = 7.0f;
    constexpr float kGapWide   = 12.0f;
    constexpr float kCardH     = 48.0f;
}

OClock::OClock() : m_ElapsedTime(std::chrono::seconds(0)) {
    m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
}

void OClock::Start(int minutes, int seconds) {
    if (m_PausedElapsed.count() <= 0.0) {
        m_TargetTime = std::chrono::minutes(minutes) + std::chrono::seconds(seconds);
    }
    auto now    = std::chrono::steady_clock::now();
    m_StartTime = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_PausedElapsed);
    m_IsRunning = true;
}

void OClock::Stop() {
    m_PausedElapsed = m_ElapsedTime;
    m_IsRunning      = false;
}

void OClock::Reset() {
    m_IsRunning     = false;
    m_IsOvertime    = false;
    m_ElapsedTime   = std::chrono::seconds(0);
    m_PausedElapsed = std::chrono::seconds(0);
    m_TargetTime    = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
}

void OClock::ApplyPreset(int minutes) {
    m_InputMin = minutes;
    m_InputSec = 0;
    if (!m_IsRunning && m_PausedElapsed.count() <= 0.0) {
        m_TargetTime = std::chrono::minutes(minutes);
    }
}

void OClock::AddExtraTime(int seconds) {
    if (m_Mode == OClockMode::Timer) {
        int curSec = static_cast<int>(m_TargetTime.count());
        if (curSec <= 0) {
            curSec = m_InputMin * 60 + m_InputSec;
        }
        curSec = std::max(0, curSec + seconds);
        m_TargetTime = std::chrono::seconds(curSec);
        m_InputMin = curSec / 60;
        m_InputSec = curSec % 60;
        if (m_IsRunning) {
            m_IsOvertime = m_ElapsedTime >= m_TargetTime;
        }
    }
}

void OClock::SetTitle(const std::string& title) {
    if (title.empty()) {
        ClearTitle();
        return;
    }
    auto it = std::find(m_Titles.begin(), m_Titles.end(), title);
    if (it != m_Titles.end()) {
        m_TitleIndex = static_cast<int>(std::distance(m_Titles.begin(), it));
    } else {
        m_Titles.push_back(title);
        m_TitleIndex = static_cast<int>(m_Titles.size()) - 1;
    }
}

void OClock::ClearTitle() {
    m_TitleIndex = -1;
}

std::string OClock::GetFormattedTime() const {
    if (m_Mode == OClockMode::WallClock) {
        auto        now = std::chrono::system_clock::now();
        std::time_t tt   = std::chrono::system_clock::to_time_t(now);
        std::tm     localTm{};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        int hour = localTm.tm_hour;
        int mins = localTm.tm_min;
        int secs = localTm.tm_sec;

        std::string suffix;
        if (!m_WallClock24h) {
            suffix = (hour >= 12) ? " PM" : " AM";
            hour   = hour % 12;
            if (hour == 0) hour = 12;
        }

        char buffer[24];
        if (m_WallClockShowSeconds)
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, mins, secs);
        else
            snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, mins);

        return std::string(buffer) + suffix;
    }

    int targetSecs  = static_cast<int>(m_TargetTime.count());
    int elapsedSecs = std::max<int>(
        0, (int)std::chrono::duration_cast<std::chrono::seconds>(m_ElapsedTime).count());

    int displaySecs;
    if (m_Direction == OClockDirection::CountDown) {
        int remaining = targetSecs - elapsedSecs;
        displaySecs = (remaining >= 0) ? remaining : -remaining;
    } else {
        displaySecs = elapsedSecs;
    }

    int mins = displaySecs / 60;
    int secs = displaySecs % 60;
    char buffer[20];

    if (m_ShowSignPrefix && m_IsOvertime) {
        char sign = (m_Direction == OClockDirection::CountDown) ? '+' : '+';
        snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign, mins, secs);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    }
    return std::string(buffer);
}

float OClock::GetProgressRatio() const {
    if (m_Mode != OClockMode::Timer) return 0.0f;

    double targetSecs = static_cast<double>(m_TargetTime.count());
    if (targetSecs <= 0.0) return 0.0f;
    double elapsedSecs = m_ElapsedTime.count();
    return static_cast<float>(std::clamp(elapsedSecs / targetSecs, 0.0, 1.0));
}

std::string OClock::GetCurrentTitle() const {
    if (m_TitleIndex < 0 || m_TitleIndex >= (int)m_Titles.size()) return "";
    return m_Titles[m_TitleIndex];
}

void OClock::AdvanceTitle() {
    if (m_Titles.empty()) return;
    m_TitleIndex = (m_TitleIndex + 1) % (int)m_Titles.size();
}

void OClock::SyncTransmission(const std::string& timeStr) {
    auto& core = Core::PresentationCore::Get();

    bool wasLAN = (m_PrevTransmitMode == OClockTransmitMode::LAN);
    bool isLAN  = (m_TransmitMode     == OClockTransmitMode::LAN);

    bool usingFinalStyle = m_IsOvertime && !m_FinalStyleName.empty();
    const std::string& styleToApply = usingFinalStyle ? m_FinalStyleName : m_StyleName;

    ImVec4 dangerV4 = ImGui::ColorConvertU32ToFloat4(DS::DangerColor);
    float  dangerRGBA[4] = { dangerV4.x, dangerV4.y, dangerV4.z, dangerV4.w };
    const float* colorOverride = (m_IsOvertime && !usingFinalStyle) ? dangerRGBA : nullptr;

    std::string title    = GetCurrentTitle();
    std::string fullText = title.empty() ? timeStr : (title + "\n" + timeStr);

    if (isLAN) {
        core.SetLiveQuickNoteLAN(fullText, colorOverride, styleToApply);
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();
    }

    core.SetLiveOverlayClockText(fullText, colorOverride);

    m_PrevTransmitMode = m_TransmitMode;
}

void OClock::Update() {
    for (auto& text : Core::PresentationCore::Get().DrainRemoteClockTitles()) {
        if (text.empty()) continue;
        m_Titles.push_back(text);
        m_TitleIndex = (int)m_Titles.size() - 1;
    }

    if (m_Mode == OClockMode::Timer) {
        if (m_IsRunning) {
            auto now      = std::chrono::steady_clock::now();
            m_ElapsedTime = now - m_StartTime;
            m_IsOvertime  = m_ElapsedTime >= m_TargetTime;
        }
    } else {
        m_IsOvertime = false;
    }

    SyncTransmission(GetFormattedTime());

    std::string clockCue = Core::PresentationCore::Get().ConsumeClockStyleCue();
    if (!clockCue.empty())
        m_StyleName = clockCue;
}

static ImU32 ColAf(ImU32 col, float a) {
    int ai = static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(ai) << 24);
}

static bool DrawSegmentTab(ImDrawList* dl, const char* id, const char* label,
                           bool active, const ImVec2& p0, const ImVec2& p1,
                           ImU32 activeColor = DS::AccentColor)
{
    float w = p1.x - p0.x;
    float h = p1.y - p0.y;

    ImGui::SetCursorScreenPos(p0);
    bool clicked = ImGui::InvisibleButton(id, ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImU32 bg = active
        ? ColA(activeColor, hovered ? 75 : 50)
        : (hovered ? IM_COL32(255, 255, 255, 18) : IM_COL32(255, 255, 255, 8));
    ImU32 border = active
        ? ColA(activeColor, hovered ? 255 : 210)
        : (hovered ? IM_COL32(255, 255, 255, 60) : IM_COL32(255, 255, 255, 24));

    dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
    dl->AddRect(p0, p1, border, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

    ImVec2 labelSz = ImGui::CalcTextSize(label);
    float startX = p0.x + (w - labelSz.x) * 0.5f;
    float startY = p0.y + (h - labelSz.y) * 0.5f;
    dl->AddText(ImVec2(startX, startY), active ? DS::TextPrimary : DS::TextSecondary, label);

    return clicked;
}

static bool DrawChip(const char* label, bool active, const ImVec2& size, ImU32 accent = DS::AccentColor) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImU32 bg = active
        ? ColA(accent, hovered ? 85 : 60)
        : (hovered ? IM_COL32(255, 255, 255, 22) : IM_COL32(255, 255, 255, 10));
    ImU32 border = active
        ? ColA(accent, 240)
        : (hovered ? IM_COL32(255, 255, 255, 75) : IM_COL32(255, 255, 255, 26));

    dl->AddRectFilled(p0, p1, bg, size.y * 0.5f);
    dl->AddRect(p0, p1, border, size.y * 0.5f, 0, active ? 1.5f : 1.0f);

    const char* text_end = ImGui::FindRenderedTextEnd(label);
    ImVec2 txtSz = ImGui::CalcTextSize(label, text_end, true);
    dl->AddText(ImVec2(p0.x + (size.x - txtSz.x) * 0.5f, p0.y + (size.y - txtSz.y) * 0.5f),
                active ? DS::TextPrimary : DS::TextSecondary, label, text_end);

    return clicked;
}

void OClock::RenderDisplayCard(float w, const std::string& timeStr) {
    const float dispH = 104.0f;
    ImVec2 dispPos = ImGui::GetCursorScreenPos();
    ImVec2 dispEnd = ImVec2(dispPos.x + w, dispPos.y + dispH);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = static_cast<float>(ImGui::GetTime());

    ImU32 bgCol, borderCol, textCol, glowCol;
    const char* modeBadge = "";
    const char* statusBadge = "";
    ImU32 statusDotCol = 0;

    if (m_Mode == OClockMode::WallClock) {
        bgCol        = IM_COL32(11, 18, 24, 255);
        borderCol    = IM_COL32(35, 175, 215, 160);
        textCol      = IM_COL32(80, 225, 255, 255);
        glowCol      = IM_COL32(35, 175, 215, 35);
        modeBadge    = "🕒 HORA LOCAL";
        statusBadge  = "● EN VIVO";
        statusDotCol = IM_COL32(50, 215, 255, 255);
    } else if (m_IsOvertime) {
        float pulse  = 0.5f + 0.5f * std::sin(t * 5.0f);
        bgCol        = IM_COL32(28, 12, 16, 255);
        borderCol    = ColA(IM_COL32(255, 55, 75, 255), static_cast<int>(150 + 105 * pulse));
        textCol      = IM_COL32(255, 80, 95, 255);
        glowCol      = ColA(IM_COL32(255, 45, 65, 255), static_cast<int>(35 + 40 * pulse));
        modeBadge    = "⚠️ OVERTIME";
        statusBadge  = "● EXCEDIDO";
        statusDotCol = IM_COL32(255, 60, 80, 255);
    } else if (m_IsRunning) {
        bgCol        = IM_COL32(10, 22, 19, 255);
        borderCol    = IM_COL32(40, 200, 135, 170);
        textCol      = IM_COL32(75, 240, 170, 255);
        glowCol      = IM_COL32(40, 200, 135, 35);
        modeBadge    = (m_Direction == OClockDirection::CountDown) ? "⏱️ REGRESIVA" : "⏱️ CRONÓMETRO";
        statusBadge  = "● EN VIVO";
        statusDotCol = IM_COL32(60, 240, 160, 255);
    } else if (m_PausedElapsed.count() > 0.0) {
        bgCol        = IM_COL32(24, 18, 10, 255);
        borderCol    = IM_COL32(235, 165, 30, 160);
        textCol      = IM_COL32(255, 195, 60, 255);
        glowCol      = IM_COL32(235, 165, 30, 30);
        modeBadge    = (m_Direction == OClockDirection::CountDown) ? "⏱️ REGRESIVA" : "⏱️ CRONÓMETRO";
        statusBadge  = "❚❚ PAUSADO";
        statusDotCol = IM_COL32(255, 190, 50, 255);
    } else {
        bgCol        = IM_COL32(13, 16, 22, 255);
        borderCol    = IM_COL32(55, 65, 85, 140);
        textCol      = IM_COL32(220, 225, 235, 255);
        glowCol      = IM_COL32(55, 65, 85, 20);
        modeBadge    = (m_Direction == OClockDirection::CountDown) ? "⏱️ REGRESIVA" : "⏱️ CRONÓMETRO";
        statusBadge  = "○ LISTO";
        statusDotCol = IM_COL32(130, 140, 160, 255);
    }

    dl->AddRect(ImVec2(dispPos.x - 2.0f, dispPos.y - 2.0f), ImVec2(dispEnd.x + 2.0f, dispEnd.y + 2.0f),
                glowCol, DS::RadiusMedium + 2.0f, 0, 1.5f);

    dl->AddRectFilled(dispPos, dispEnd, bgCol, DS::RadiusMedium);
    dl->AddRect(dispPos, dispEnd, borderCol, DS::RadiusMedium, 0, 1.3f);

    float headerY = dispPos.y + 8.0f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.78f,
                ImVec2(dispPos.x + 10.0f, headerY), textCol, modeBadge);

    ImVec2 sbSz = ImGui::CalcTextSize(statusBadge);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.78f,
                ImVec2(dispEnd.x - 10.0f - sbSz.x * 0.78f, headerY), statusDotCol, statusBadge);

    std::string activeTitle = GetCurrentTitle();
    float timeCenterY = dispPos.y + 44.0f;
    if (!activeTitle.empty()) {
        std::string titleWrapped = "« " + activeTitle + " »";
        ImVec2 tSz = ImGui::CalcTextSize(titleWrapped.c_str());
        dl->AddText(ImVec2(dispPos.x + (w - tSz.x) * 0.5f, dispPos.y + 22.0f),
                    IM_COL32(235, 190, 80, 220), titleWrapped.c_str());
        timeCenterY += 4.0f;
    }

    ImFont* font = ImGui::GetFont();
    const float clockFontSize = 36.0f;
    ImVec2 textSz = font->CalcTextSizeA(clockFontSize, FLT_MAX, 0.0f, timeStr.c_str());
    ImVec2 textPos(dispPos.x + (w - textSz.x) * 0.5f, timeCenterY - textSz.y * 0.5f);

    dl->AddText(font, clockFontSize, ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 160), timeStr.c_str());
    dl->AddText(font, clockFontSize, textPos, textCol, timeStr.c_str());

    if (m_Mode == OClockMode::Timer) {
        float barH = 4.0f;
        float barPad = 12.0f;
        float barY = dispEnd.y - 20.0f;
        ImVec2 bMin(dispPos.x + barPad, barY);
        ImVec2 bMax(dispEnd.x - barPad, barY + barH);

        if (m_ShowProgressBar) {
            dl->AddRectFilled(bMin, bMax, IM_COL32(255, 255, 255, 14), 2.0f);

            if (m_IsOvertime) {
                float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
                dl->AddRectFilled(bMin, bMax, ColA(IM_COL32(255, 60, 70, 255), static_cast<int>(170 + 85 * pulse)), 2.0f);
            } else {
                float ratio = GetProgressRatio();
                float fillX = bMin.x + (bMax.x - bMin.x) * ratio;
                if (fillX > bMin.x) {
                    dl->AddRectFilled(bMin, ImVec2(fillX, bMax.y), IM_COL32(35, 210, 150, 240), 2.0f);
                }
            }
        }

        int tgtSecs = static_cast<int>(m_TargetTime.count());
        int elapsedSecs = std::max(0, static_cast<int>(m_ElapsedTime.count()));
        int remainingSecs = std::max(0, tgtSecs - elapsedSecs);

        char statsBuf[64];
        if (m_IsOvertime) {
            int extraSecs = elapsedSecs - tgtSecs;
            snprintf(statsBuf, sizeof(statsBuf), "+%02d:%02d sobre el tiempo", extraSecs / 60, extraSecs % 60);
        } else if (m_Direction == OClockDirection::CountDown) {
            snprintf(statsBuf, sizeof(statsBuf), "Meta: %02d:%02d  •  Restante: %02d:%02d",
                     tgtSecs / 60, tgtSecs % 60, remainingSecs / 60, remainingSecs % 60);
        } else {
            snprintf(statsBuf, sizeof(statsBuf), "Meta: %02d:%02d  •  Transcurrido: %02d:%02d",
                     tgtSecs / 60, tgtSecs % 60, elapsedSecs / 60, elapsedSecs % 60);
        }

        ImVec2 statSz = ImGui::CalcTextSize(statsBuf);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.74f,
                    ImVec2(dispPos.x + (w - statSz.x * 0.74f) * 0.5f, dispEnd.y - 14.0f),
                    m_IsOvertime ? IM_COL32(255, 100, 115, 230) : ColA(DS::TextHint, 200), statsBuf);
    } else {
        const char* wcInfo = m_WallClock24h ? "Formato 24 Horas" : "Formato 12 Horas (AM/PM)";
        ImVec2 wcSz = ImGui::CalcTextSize(wcInfo);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.74f,
                    ImVec2(dispPos.x + (w - wcSz.x * 0.74f) * 0.5f, dispEnd.y - 14.0f),
                    ColA(DS::TextHint, 190), wcInfo);
    }

    ImGui::Dummy(ImVec2(w, dispH));
}

void OClock::RenderTransportControls(float w) {
    if (m_Mode != OClockMode::Timer) return;

    const float btnH   = 36.0f;
    const float gap    = 6.0f;
    const float bumpW  = 48.0f;
    const float resetW = 48.0f;
    const float playW  = std::max(70.0f, w - bumpW - resetW - gap * 2.0f);

    ImGui::PushID("transport");

    if (!m_IsRunning) {
        bool isPaused = (m_PausedElapsed.count() > 0.0);
        const char* label = isPaused ? "▶ REANUDAR" : "▶ INICIAR";
        ImU32 col = isPaused ? IM_COL32(35, 185, 225, 255) : DS::SuccessColor;
        if (DS::GlassButton(label, ImVec2(playW, btnH), col)) {
            Start(m_InputMin, m_InputSec);
        }
    } else {
        if (DS::GlassButton("⏸ PAUSAR", ImVec2(playW, btnH), IM_COL32(235, 155, 25, 255))) {
            Stop();
        }
    }

    ImGui::SameLine(0, gap);

    if (DS::GlassButton("⟳", ImVec2(resetW, btnH), DS::AccentColorDim)) {
        Reset();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reiniciar al tiempo establecido");
    }

    ImGui::SameLine(0, gap);

    if (DS::GlassButton("+1m", ImVec2(bumpW, btnH), IM_COL32(35, 155, 215, 255))) {
        AddExtraTime(60);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sumar 1 minuto en vivo sin reiniciar");
    }

    ImGui::PopID();
}

void OClock::RenderModeSelector(float w) {
    float gap   = 4.0f;
    float halfW = (w - gap) * 0.5f;
    float h     = 32.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 m0Min = p0;
    ImVec2 m0Max = ImVec2(p0.x + halfW, p0.y + h);
    if (DrawSegmentTab(dl, "##modeTimer", "⏱️ Temporizador",
                       m_Mode == OClockMode::Timer, m0Min, m0Max, DS::AccentColor)) {
        m_Mode = OClockMode::Timer;
    }

    ImVec2 m1Min = ImVec2(p0.x + halfW + gap, p0.y);
    ImVec2 m1Max = ImVec2(m1Min.x + halfW, p0.y + h);
    if (DrawSegmentTab(dl, "##modeWallClock", "🕒 Hora Local",
                       m_Mode == OClockMode::WallClock, m1Min, m1Max, IM_COL32(35, 175, 215, 255))) {
        if (m_Mode != OClockMode::WallClock) {
            m_Mode = OClockMode::WallClock;
            Stop();
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
    ImGui::Dummy(ImVec2(w, h));
}

void OClock::RenderTimeConfig(float w) {
    ImGui::TextColored(ToVec4(DS::TextHint), "DURACIÓN:");
    ImGui::Spacing();

    float gap = 4.0f;
    float colonW = 12.0f;
    float inputW = (w - colonW - gap * 2.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(14, 18, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(20, 28, 40, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    ImGui::SetNextItemWidth(inputW);
    if (ImGui::InputInt("##min", &m_InputMin, 0, 0)) {
        if (m_InputMin < 0) m_InputMin = 0;
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Minutos");

    ImGui::SameLine(0, gap);
    ImGui::TextColored(ToVec4(DS::TextSecondary), ":");
    ImGui::SameLine(0, gap);

    ImGui::SetNextItemWidth(inputW);
    if (ImGui::InputInt("##sec", &m_InputSec, 0, 0)) {
        if (m_InputSec < 0) m_InputSec = 0;
        if (m_InputSec > 59) m_InputSec = 59;
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Segundos");

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    ImGui::PushID("steppers");
    float stepW = (w - gap * 3.0f) * 0.25f;
    if (DS::GlassButton("-5m", ImVec2(stepW, 28.0f), DS::AccentColorDim)) {
        m_InputMin = std::max(0, m_InputMin - 5);
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    ImGui::SameLine(0, gap);
    if (DS::GlassButton("-1m", ImVec2(stepW, 28.0f), DS::AccentColorDim)) {
        m_InputMin = std::max(0, m_InputMin - 1);
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    ImGui::SameLine(0, gap);
    if (DS::GlassButton("+1m", ImVec2(stepW, 28.0f), DS::AccentColorDim)) {
        m_InputMin += 1;
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    ImGui::SameLine(0, gap);
    if (DS::GlassButton("+5m", ImVec2(stepW, 28.0f), DS::AccentColorDim)) {
        m_InputMin += 5;
        if (!m_IsRunning && m_PausedElapsed.count() <= 0.0)
            m_TargetTime = std::chrono::minutes(m_InputMin) + std::chrono::seconds(m_InputSec);
    }
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "PRESETS:");
    ImGui::Spacing();

    ImGui::PushID("presets");
    const int presets[] = { 3, 5, 10, 15, 20, 30, 45, 60 };
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 4; ++col) {
            int idx = row * 4 + col;
            int mins = presets[idx];
            bool active = (m_InputMin == mins && m_InputSec == 0);
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "%d'", mins);
            if (DrawChip(lbl, active, ImVec2(stepW, 24.0f), DS::AccentColor)) {
                ApplyPreset(mins);
            }
            if (col < 3) ImGui::SameLine(0, gap);
        }
    }
    ImGui::PopID();

    ImGui::Spacing();

    RenderDirectionSelector(w);
}

void OClock::RenderDirectionSelector(float w) {
    ImGui::TextColored(ToVec4(DS::TextHint), "DIRECCIÓN:");
    ImGui::Spacing();

    float gap   = 4.0f;
    float halfW = (w - gap) * 0.5f;
    float h     = 30.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 c0Min = p0;
    ImVec2 c0Max = ImVec2(p0.x + halfW, p0.y + h);
    if (DrawSegmentTab(dl, "##dirDown", "⬇️ Regresiva",
                       m_Direction == OClockDirection::CountDown, c0Min, c0Max, DS::AccentColor)) {
        m_Direction = OClockDirection::CountDown;
    }

    ImVec2 c1Min = ImVec2(p0.x + halfW + gap, p0.y);
    ImVec2 c1Max = ImVec2(c1Min.x + halfW, p0.y + h);
    if (DrawSegmentTab(dl, "##dirUp", "⬆️ Ascendente",
                       m_Direction == OClockDirection::CountUp, c1Min, c1Max, DS::AccentColor)) {
        m_Direction = OClockDirection::CountUp;
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
    ImGui::Dummy(ImVec2(w, h));
}

void OClock::RenderWallClockOptions(float w) {
    ImGui::TextColored(ToVec4(DS::TextHint), "OPCIONES DE RELOJ:");
    ImGui::Spacing();

    float gap   = 4.0f;
    float halfW = (w - gap) * 0.5f;
    float h     = 30.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 b0Min = p0;
    ImVec2 b0Max = ImVec2(p0.x + halfW, p0.y + h);
    const char* lbl24 = m_WallClock24h ? "✔ 24 Horas" : "12 Horas";
    if (DrawSegmentTab(dl, "##wc24h", lbl24, m_WallClock24h, b0Min, b0Max, IM_COL32(35, 175, 215, 255))) {
        m_WallClock24h = !m_WallClock24h;
    }

    ImVec2 b1Min = ImVec2(p0.x + halfW + gap, p0.y);
    ImVec2 b1Max = ImVec2(b1Min.x + halfW, p0.y + h);
    const char* lblSec = m_WallClockShowSeconds ? "✔ Segundos" : "Solo Hora";
    if (DrawSegmentTab(dl, "##wcSec", lblSec, m_WallClockShowSeconds, b1Min, b1Max, IM_COL32(35, 175, 215, 255))) {
        m_WallClockShowSeconds = !m_WallClockShowSeconds;
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
    ImGui::Dummy(ImVec2(w, h));
}

void OClock::RenderTitleSection(float w) {
    std::string curTitle = GetCurrentTitle();
    if (curTitle.empty()) {
        DS::GlassSectionHeader("RÓTULO DE ESCENARIO");
    } else {
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "RÓTULO: « %s »", curTitle.c_str());
        DS::GlassSectionHeader(hdr);
    }
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(14, 18, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(20, 28, 40, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::SetNextItemWidth(w);
    bool enterPressed = ImGui::InputTextWithHint("##oclock_title_input", "Escribir rótulo de escenario...",
                                                  m_TitleInputBuf, sizeof(m_TitleInputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    float gap = 3.0f;
    float btnW = (w - gap) * 0.5f;

    ImGui::PushID("title_actions");
    if (DS::GlassButton("+ Asignar", ImVec2(btnW, 26.0f), DS::AccentColor) || enterPressed) {
        std::string text(m_TitleInputBuf);
        if (!text.empty()) {
            SetTitle(text);
            m_TitleInputBuf[0] = '\0';
        }
    }
    ImGui::SameLine(0, gap);
    if (DS::GlassButton("✕ Quitar", ImVec2(btnW, 26.0f), DS::DangerColorDim)) {
        ClearTitle();
    }
    ImGui::PopID();

    if (!m_Titles.empty()) {
        ImGui::Spacing();
        const float delBtnW = 26.0f;
        const float selW = w - delBtnW - 4.0f;

        for (int i = 0; i < (int)m_Titles.size(); ++i) {
            ImGui::PushID(i);
            bool isActive = (i == m_TitleIndex);

            ImGui::PushStyleColor(ImGuiCol_Text, isActive ? ToVec4(DS::AccentLight) : ToVec4(DS::TextSecondary));
            char rowLabel[160];
            snprintf(rowLabel, sizeof(rowLabel), "%s %s", isActive ? "✔" : "  ", m_Titles[i].c_str());
            if (ImGui::Selectable(rowLabel, isActive, 0, ImVec2(selW, 22.0f))) {
                m_TitleIndex = i;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 4.0f);
            if (DS::GlassButton("✕", ImVec2(delBtnW, 22.0f), DS::DangerColor)) {
                m_Titles.erase(m_Titles.begin() + i);
                if (m_TitleIndex == i)
                    m_TitleIndex = m_Titles.empty() ? -1 : std::min(i, (int)m_Titles.size() - 1);
                else if (m_TitleIndex > i)
                    m_TitleIndex--;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
    }
}

void OClock::RenderOutputsSection(float w) {
    auto& core = Core::PresentationCore::Get();
    DS::GlassSectionHeader("DESTINOS DE SALIDA");
    ImGui::Spacing();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    {
        std::string overlayPath = core.GetOverlayPath();
        bool hasOverlay = !overlayPath.empty();
        bool hasClockBox = core.HasOverlayClockLayer();
        bool isShowingOnPublic = hasOverlay && hasClockBox;

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardH = 34.0f;
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + cardH);

        ImU32 bg = isShowingOnPublic ? IM_COL32(10, 24, 20, 255) : IM_COL32(16, 18, 24, 255);
        ImU32 border = isShowingOnPublic ? IM_COL32(40, 195, 130, 160) : IM_COL32(50, 58, 72, 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, border, DS::RadiusMedium, 0, 1.0f);

        dl->AddCircleFilled(ImVec2(p0.x + 12.0f, p0.y + cardH * 0.5f), 4.0f,
                            isShowingOnPublic ? DS::SuccessColor : DS::TextHint);

        dl->AddText(ImVec2(p0.x + 24.0f, p0.y + (cardH - ImGui::GetFontSize()) * 0.5f),
                    isShowingOnPublic ? DS::TextPrimary : DS::TextSecondary,
                    "📺 Proyector (Overlay)");

        const char* statusTxt = isShowingOnPublic ? "EN PANTALLA" : "NO ACTIVO";
        ImVec2 sSz = ImGui::CalcTextSize(statusTxt);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.78f,
                    ImVec2(p1.x - 10.0f - sSz.x * 0.78f, p0.y + (cardH - ImGui::GetFontSize() * 0.78f) * 0.5f),
                    isShowingOnPublic ? DS::SuccessColor : ColA(DS::TextHint, 180), statusTxt);

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH));
        ImGui::Dummy(ImVec2(w, cardH));
    }

    ImGui::Spacing();

    {
        bool netAvailable = core.IsStreamingNet();
        bool isLAN = (m_TransmitMode == OClockTransmitMode::LAN);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float cardH = 34.0f;
        ImVec2 p1 = ImVec2(p0.x + w, p0.y + cardH);

        ImU32 bg = isLAN ? IM_COL32(10, 24, 20, 255) : IM_COL32(16, 18, 24, 255);
        ImU32 border = isLAN ? IM_COL32(40, 195, 130, 160) : IM_COL32(50, 58, 72, 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, border, DS::RadiusMedium, 0, 1.0f);

        dl->AddCircleFilled(ImVec2(p0.x + 12.0f, p0.y + cardH * 0.5f), 4.0f,
                            isLAN ? DS::SuccessColor : DS::TextHint);

        dl->AddText(ImVec2(p0.x + 24.0f, p0.y + (cardH - ImGui::GetFontSize()) * 0.5f),
                    isLAN ? DS::TextPrimary : DS::TextSecondary,
                    "📡 Red LAN (Stage)");

        float btnW = 68.0f;
        float btnH = 24.0f;
        ImVec2 bPos(p1.x - btnW - 6.0f, p0.y + (cardH - btnH) * 0.5f);

        const char* toggleLbl = isLAN ? "● EN VIVO" : "APAGADO";
        ImU32 toggleCol = isLAN ? DS::SuccessColor : DS::AccentColorDim;

        ImGui::BeginDisabled(!netAvailable);
        if (DrawSegmentTab(dl, "##lanToggle", toggleLbl, isLAN, bPos, ImVec2(bPos.x + btnW, bPos.y + btnH), toggleCol)) {
            m_TransmitMode = isLAN ? OClockTransmitMode::Off : OClockTransmitMode::LAN;
        }
        ImGui::EndDisabled();

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + cardH));
        ImGui::Dummy(ImVec2(w, cardH));

        if (!netAvailable) {
            ImGui::Spacing();
            ImGui::TextColored(ToVec4(DS::TextHint), "Inicia el servidor en Transmisión para habilitar salida LAN.");
        }
    }
}

void OClock::RenderStyleSelector() {
    auto& core = Core::PresentationCore::Get();
    std::vector<std::string> styleNames = core.GetSavedStyleNames();

    if (ImGui::CollapsingHeader("⚙️ Opciones Avanzadas")) {
        ImGui::Spacing();

        auto renderCombo = [&](const char* label, const char* comboId,
                               std::string& target, const char* emptyHint) {
            ImGui::TextColored(ToVec4(DS::TextHint), "%s", label);

            std::string preview = target.empty() ? "Usar estilo actual" : target;

            ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(14, 18, 26, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(20, 28, 40, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::SetNextItemWidth(-1.0f);

            if (ImGui::BeginCombo(comboId, preview.c_str())) {
                bool noneSelected = target.empty();
                if (ImGui::Selectable("Usar estilo actual", noneSelected))
                    target.clear();
                if (noneSelected) ImGui::SetItemDefaultFocus();

                for (const auto& name : styleNames) {
                    bool sel = (target == name);
                    if (ImGui::Selectable(name.c_str(), sel))
                        target = name;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);

            if (target.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
                ImGui::TextWrapped("%s", emptyHint);
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
        };

        renderCombo("Estilo tipográfico normal (LAN)", "##oclockStyle", m_StyleName,
            "Hereda la tipografía y color del estilo activo en la app.");

        renderCombo("Estilo al exceder tiempo (LAN)", "##oclockFinalStyle", m_FinalStyleName,
            "Aplica estilo especial o color de peligro automático al llegar a overtime.");

        ImGui::Spacing();
        ImGui::TextColored(ToVec4(DS::TextHint), "OPCIONES VISUALES");
        ImGui::Spacing();

        ImGui::Checkbox("Mostrar barra de progreso en el display", &m_ShowProgressBar);
        ImGui::Checkbox("Mostrar signo '+' en sobretiempo", &m_ShowSignPrefix);

        ImGui::Spacing();
    }
}

void OClock::Render(GlassRenderer& glass) {
    Update();
    (void)glass;
    std::string timeStr = GetFormattedTime();

    float w = ImGui::GetContentRegionAvail().x;

    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.82f, 0.92f, 1.0f));
        ImGui::SetWindowFontScale(1.12f);
        ImGui::TextUnformatted("⏱️  RELOJ & CRONÓMETRO");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        Wiki::InfoButton(Wiki::Topic::OClock);
    }

    DS::GlassSeparator();
    ImGui::Spacing();

    RenderDisplayCard(w, timeStr);

    ImGui::Spacing();

    RenderTransportControls(w);

    ImGui::Spacing();

    RenderModeSelector(w);

    ImGui::Spacing();

    if (m_Mode == OClockMode::Timer) {
        RenderTimeConfig(w);
    } else {
        RenderWallClockOptions(w);
    }

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    RenderTitleSection(w);

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    RenderOutputsSection(w);

    ImGui::Spacing();
    DS::GlassSeparator();
    ImGui::Spacing();

    RenderStyleSelector();
}

}

