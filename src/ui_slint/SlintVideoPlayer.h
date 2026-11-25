#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
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
    using FrameReadyCallback = std::function<void()>;

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

    void SetFrameReadyCallback(FrameReadyCallback cb);
    void SetMaxFps(int fps);
    void SetGpuBackend(int hwdecMode); // 0=Auto, 1=NVIDIA(nvdec/cuda), 2=D3D11VA, 3=VAAPI/Vulkan, 4=CPU
    void SetUpscaler(int upscalerMode, float sharpness01, bool smoothMotion60);

    std::string GetActiveHwdec() const;
    double      GetSourceFps() const { return m_SourceFps.load(std::memory_order_relaxed); }

    void SetLowResMode(bool lowRes);
    void SetRenderSize(int width, int height);
    void SetAudioOnlyMode(bool audioOnly);
    void SetScaleMode(int mode); // 0=Cover(panscan=1), 1=Fill(keepaspect=no), 2=Contain(keepaspect=yes)
    void SetLanCaptureEnabled(bool enabled);
    void SetVideoFilters(int brightness, int contrast, int saturation, int gamma, bool blur, int colorPreset, bool fsrSharpen = false);
    void SetAudioEqualizer(bool enabled, float bassDb, float midDb, float trebleDb);
    void SetMusicAudioFx(bool eqEnabled, bool dynamicEq, float b60, float b230, float b910, float b3600, float b12000);

    // Non-blocking O(1) swap of latest background-rendered frame into outImage.
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
    void RenderWorkerLoop();
    void ApplyHwdecOption();
    void ApplyScalerOptions();
    void RebuildVideoFilterChain();
    static void OnRenderUpdate(void* ctx);

    mpv_handle*         m_Mpv       = nullptr;
    mpv_render_context* m_RenderCtx = nullptr;
    std::thread         m_EventThread;
    std::thread         m_RenderThread;
    std::atomic<bool>   m_Quit{false};
    bool                m_AudioOnly = false;
    int                 m_ScaleMode = 0;

    // GPU Hardware Decoder & Real-Time Upscaler state
    std::atomic<int>    m_HwdecMode{0};     // 0=Auto, 1=NVIDIA, 2=D3D11VA, 3=VAAPI, 4=CPU
    std::atomic<int>    m_UpscalerMode{2};  // 0=Off(Bilinear), 1=FSR1, 2=CAS+, 3=NVIDIA NIS, 4=LanczosHQ
    std::atomic<float>  m_UpscalerSharpness{0.55f};
    std::atomic<bool>   m_SmoothMotion60{false};
    std::atomic<int>    m_MaxFps{60};
    std::atomic<double> m_SourceFps{0.0};

    // Cached color/shader params so RebuildVideoFilterChain merges Upscaler + Color Grading + Motion
    int  m_FiltBrightness  = 0;
    int  m_FiltContrast    = 0;
    int  m_FiltSaturation  = 0;
    int  m_FiltGamma       = 0;
    bool m_FiltBlur        = false;
    int  m_FiltColorPreset = 0;
    std::mutex m_FilterMutex;

    // Deduplication caches so switching videos never tears down/rebuilds NVDEC or lavfi needlessly
    std::string m_LastHwdecApplied;
    std::string m_LastSwsScalerApplied;
    bool        m_LastSwsFastApplied   = false;
    bool        m_HasSwsApplied        = false;
    int         m_LastScaleModeApplied = -1;
    std::string m_LastVfApplied;
    std::string m_LastCacheApplied;
    std::string m_LastLoopApplied;

    mutable std::mutex  m_InfoMutex;
    std::string         m_ActiveHwdec = "auto";

    // Async Render Worker synchronization (mutex-guarded wake flag -> zero lost wakeups!)
    std::mutex              m_RenderWakeMutex;
    std::condition_variable m_RenderCv;
    bool                    m_RenderWakeFlag = false;
    std::atomic<bool>       m_ForceRedraw{false};
    std::atomic<bool>       m_AcceptingFrames{false};

    // Single-slot latest-frame mailbox (worker -> UI thread)
    std::mutex                                    m_ReadyMutex;
    slint::SharedPixelBuffer<slint::Rgba8Pixel>   m_ReadyBuffer;
    bool                                          m_HasReadyFrame = false;
    std::atomic<bool>                             m_UiNotifyPending{false};
    FrameReadyCallback                            m_OnFrameReady;

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
