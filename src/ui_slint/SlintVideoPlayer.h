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
    explicit SlintVideoPlayer(bool lazyInit = false);
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
    bool  ConsumeEndReached();

    void SetLowResMode(bool lowRes);
    void SetRenderSize(int width, int height);
    void SetAudioOnlyMode(bool audioOnly);
    void SetScaleMode(int mode); // 0=Cover(panscan=1), 1=Fill(keepaspect=no), 2=Contain(keepaspect=yes)
    void SetLanCaptureEnabled(bool enabled);
    void SetVideoFilters(int brightness, int contrast, int saturation, int gamma, bool blur, int colorPreset, bool fsrSharpen = false);
    void SetAudioEqualizer(bool enabled, float bassDb, float midDb, float trebleDb);
    void SetMusicAudioFx(bool eqEnabled, bool dynamicEq, float b60, float b230, float b910, float b3600, float b12000);

    // Renders latest frame into outImage if dirty; returns true when updated.
    bool PollFrame(slint::Image& outImage);

    // Encodes the latest decoded RGBA frame into JPEG bytes for LAN streaming.
    bool EncodeLatestFrameJpeg(std::vector<uint8_t>& outJpeg, int quality = 75);

    // Copies the latest decoded frame into tight RGB24 bytes (for async FrameEncodeWorker).
    bool CopyLatestFrameRgb(std::vector<uint8_t>& outRgb, int maxW, int maxH, int& outW, int& outH);

private:
    void EnsureInit();
    void InitMpv();
    void ShutdownMpv();
    void EventLoop();
    static void OnRenderUpdate(void* ctx);

    mpv_handle*         m_Mpv       = nullptr;
    mpv_render_context* m_RenderCtx = nullptr;
    std::thread         m_EventThread;
    std::atomic<bool>   m_Quit{false};
    bool                m_AudioOnly = false;
    int                 m_ScaleMode = 0;

    std::atomic<bool>   m_FrameDirty{false};
    std::atomic<bool>   m_ForceRedraw{false};
    std::atomic<bool>   m_Playing{false};
    std::atomic<bool>   m_EndReached{false};
    std::atomic<bool>   m_Paused{false};
    std::atomic<bool>   m_Muted{false};
    std::atomic<bool>   m_CaptureForLan{false};
    std::atomic<int>    m_Volume{100};
    std::atomic<int64_t> m_TimeMs{0};
    std::atomic<int64_t> m_LengthMs{0};
    std::atomic<int64_t> m_SeekIgnoreUntilMs{0};

    int m_TargetW = 960;
    int m_TargetH = 540;
    int m_LanBufW = 0;
    int m_LanBufH = 0;
    std::mutex m_RenderMutex;
    std::vector<uint8_t> m_LanPixelBuf;
};

} // namespace ProyecThor::SlintUI
