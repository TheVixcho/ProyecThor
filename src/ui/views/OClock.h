#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace ProyecThor::UI {

class GlassRenderer;

enum class OClockTransmitMode {
    Off,
    LAN
};

enum class OClockDirection {
    CountUp,
    CountDown
};

enum class OClockMode {
    Timer,
    WallClock
};

class OClock {
public:
    OClock();

    void Update();

    void Render(GlassRenderer& glass);

    bool IsLive() const { return m_TransmitMode != OClockTransmitMode::Off; }

    void StopTransmitting() { m_TransmitMode = OClockTransmitMode::Off; }

    void AddExtraTime(int seconds);
    void SetTitle(const std::string& title);
    void ClearTitle();

private:
    void Start(int minutes, int seconds);
    void Stop();
    void Reset();
    void ApplyPreset(int minutes);

    std::string GetFormattedTime() const;
    float       GetProgressRatio() const;
    void        SyncTransmission(const std::string& timeStr);

    void        RenderDisplayCard(float w, const std::string& timeStr);
    void        RenderTransportControls(float w);
    void        RenderModeSelector(float w);
    void        RenderTimeConfig(float w);
    void        RenderDirectionSelector(float w);
    void        RenderWallClockOptions(float w);
    void        RenderTitleSection(float w);
    void        RenderOutputsSection(float w);
    void        RenderStyleSelector();

    std::string GetCurrentTitle() const;
    void        AdvanceTitle();

    OClockMode m_Mode = OClockMode::Timer;

    bool m_IsRunning  = false;
    bool m_IsOvertime = false;
    std::chrono::steady_clock::time_point m_StartTime;
    std::chrono::duration<double> m_ElapsedTime{0};
    std::chrono::duration<double> m_PausedElapsed{0};
    std::chrono::seconds m_TargetTime{0};

    OClockDirection m_Direction = OClockDirection::CountUp;

    int m_InputMin = 5;
    int m_InputSec = 0;

    bool m_WallClock24h        = true;
    bool m_WallClockShowSeconds = true;

    bool m_ShowProgressBar = true;
    bool m_ShowSignPrefix  = false;

    OClockTransmitMode m_TransmitMode     = OClockTransmitMode::Off;
    OClockTransmitMode m_PrevTransmitMode = OClockTransmitMode::Off;

    std::string m_StyleName;
    std::string m_FinalStyleName;

    std::vector<std::string> m_Titles;
    int  m_TitleIndex = -1;
    char m_TitleInputBuf[128] = "";
};

}

