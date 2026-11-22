#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <slint.h>

struct mpv_handle;
struct mpv_render_context;

namespace ProyecThor::SlintUI {

class SlintVideoPlayer {
public:
    SlintVideoPlayer();
    ~SlintVideoPlayer();

    SlintVideoPlayer(const SlintVideoPlayer&) = delete;
    SlintVideoPlayer& operator=(const SlintVideoPlayer&) = delete;

    void Play(const std::string& path, bool loop, bool muted);
    void PlayLiveStream(const std::string& uri, bool loop, bool muted, bool lowLatency = true);
    void Stop();

    void SetPause(bool paused);
    bool IsPaused() const { return m_Paused.load(); }

    void SetMute(bool muted);
    bool IsMuted() const { return m_Muted.load(); }

    void SetVolume(int volume);
    int  GetVolume() const { return m_Volume.load(); }

    void  SetPosition(float normalized);
    float GetPosition() const;
    int64_t GetTimeMs() const { return m_TimeMs.load(); }
    int64_t GetLengthMs() const { return m_LengthMs.load(); }
    bool  IsPlaying() const { return m_Playing.load(); }

    void SetLowResMode(bool lowRes);
    void SetVideoFilters(int brightness, int contrast, int saturation, int gamma, bool blur, int colorPreset);
    void SetAudioEqualizer(bool enabled, float bassDb, float midDb, float trebleDb);

    // Renders latest frame into outImage if dirty; returns true when updated.
    bool PollFrame(slint::Image& outImage);

private:
    void InitMpv();
    void ShutdownMpv();
    void EventLoop();
    static void OnRenderUpdate(void* ctx);

    mpv_handle*         m_Mpv       = nullptr;
    mpv_render_context* m_RenderCtx = nullptr;
    std::thread         m_EventThread;
    std::atomic<bool>   m_Quit{false};

    std::atomic<bool>   m_FrameDirty{false};
    std::atomic<bool>   m_Playing{false};
    std::atomic<bool>   m_Paused{false};
    std::atomic<bool>   m_Muted{false};
    std::atomic<int>    m_Volume{100};
    std::atomic<int64_t> m_TimeMs{0};
    std::atomic<int64_t> m_LengthMs{0};

    int m_TargetW = 960;
    int m_TargetH = 540;
    std::mutex m_RenderMutex;
    std::vector<uint8_t> m_PixelBuf;
};

} // namespace ProyecThor::SlintUI
