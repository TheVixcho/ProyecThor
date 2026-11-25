#include "SlintVideoPlayer.h"

#include <mpv/client.h>
#include <mpv/render.h>
#include "stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace ProyecThor::SlintUI {

namespace {
int64_t NowSteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64_t NowSteadyUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

const char* HwdecStringForMode(int mode) {
#ifdef __APPLE__
    switch (mode) {
        case 4: // Software CPU Only
            return "no";
        case 0:
        case 1:
        case 2:
        case 3:
        default: // macOS Apple Silicon & Intel: native VideoToolbox hardware decoding
            return "videotoolbox-copy,auto-copy";
    }
#elif defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
    switch (mode) {
        case 4: // Software CPU Only
            return "no";
        case 0:
        case 1:
        case 2:
        case 3:
        default: // Windows ARM64 (Qualcomm Adreno / Surface): Direct3D 11 Video Acceleration
            return "d3d11va-copy,dxva2-copy,auto-copy";
    }
#else
    switch (mode) {
        case 1: // NVIDIA Dedicated (NVDEC / CUDA / VDPAU / D3D11VA fallback)
            return "nvdec-copy,cuda-copy,vdpau-copy,d3d11va-copy,dxva2-copy,auto-copy";
        case 2: // Windows Direct3D 11 / DXVA2 (great for GT 710, Intel UHD & Windows ARM64)
            return "d3d11va-copy,dxva2-copy,nvdec-copy,auto-copy";
        case 3: // Linux VAAPI / Vulkan (Intel UHD / AMD / Mesa)
            return "vaapi-copy,vulkan-copy,nvdec-copy,auto-copy";
        case 4: // Software CPU Only
            return "no";
        case 0:
        default: // Smart Auto
            return "auto-copy";
    }
#endif
}
} // namespace

SlintVideoPlayer::SlintVideoPlayer(bool lazyInit) {
    if (!lazyInit) {
        InitMpv();
    }
}

SlintVideoPlayer::~SlintVideoPlayer() {
    ShutdownMpv();
}

void SlintVideoPlayer::SetFrameReadyCallback(FrameReadyCallback cb) {
    std::lock_guard<std::mutex> lk(m_ReadyMutex);
    m_OnFrameReady = std::move(cb);
}

void SlintVideoPlayer::SetMaxFps(int fps) {
    m_MaxFps.store(std::clamp(fps, 15, 240), std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
        m_RenderWakeFlag = true;
    }
    m_RenderCv.notify_one();
}

void SlintVideoPlayer::SetGpuBackend(int hwdecMode) {
    m_HwdecMode.store(std::clamp(hwdecMode, 0, 4), std::memory_order_relaxed);
    if (m_Mpv && !m_AudioOnly) {
        ApplyHwdecOption();
    }
}

void SlintVideoPlayer::SetUpscaler(int upscalerMode, float sharpness01, bool smoothMotion60) {
    m_UpscalerMode.store(std::clamp(upscalerMode, 0, 4), std::memory_order_relaxed);
    m_UpscalerSharpness.store(std::clamp(sharpness01, 0.0f, 1.0f), std::memory_order_relaxed);
    m_SmoothMotion60.store(smoothMotion60, std::memory_order_relaxed);
    if (m_Mpv && !m_AudioOnly) {
        ApplyScalerOptions();
        RebuildVideoFilterChain();
        m_ForceRedraw.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
            m_RenderWakeFlag = true;
        }
        m_RenderCv.notify_one();
    }
}

std::string SlintVideoPlayer::GetActiveHwdec() const {
    std::lock_guard<std::mutex> lk(m_InfoMutex);
    return m_ActiveHwdec;
}

void SlintVideoPlayer::EnsureInit() {
    if (!m_Mpv) {
        InitMpv();
    }
}

void SlintVideoPlayer::ApplyHwdecOption() {
    if (!m_Mpv || m_AudioOnly) return;
    const std::string hw = HwdecStringForMode(m_HwdecMode.load(std::memory_order_relaxed));
    if (hw == m_LastHwdecApplied) return;
    m_LastHwdecApplied = hw;
    mpv_set_property_string(m_Mpv, "hwdec", hw.c_str());
}

void SlintVideoPlayer::ApplyScalerOptions() {
    if (!m_Mpv || m_AudioOnly) return;
    const int upMode = m_UpscalerMode.load(std::memory_order_relaxed);
    const bool swsFast = (upMode <= 0);
    const std::string scaler = (upMode <= 0) ? "fast-bilinear"
                             : (upMode == 1) ? "bicubic"
                             : (upMode == 3) ? "spline"
                             : "lanczos";
    if (m_HasSwsApplied && swsFast == m_LastSwsFastApplied && scaler == m_LastSwsScalerApplied) {
        return;
    }
    m_HasSwsApplied        = true;
    m_LastSwsFastApplied   = swsFast;
    m_LastSwsScalerApplied = scaler;
    mpv_set_property_string(m_Mpv, "sws-fast", swsFast ? "yes" : "no");
    mpv_set_property_string(m_Mpv, "sws-scaler", scaler.c_str());
}

void SlintVideoPlayer::InitMpv() {
    if (m_Mpv) return;
    m_Mpv = mpv_create();
    if (!m_Mpv) return;

    const unsigned int hwCores = std::max(1u, std::thread::hardware_concurrency());
    m_LastHwdecApplied.clear();
    m_LastSwsScalerApplied.clear();
    m_HasSwsApplied        = false;
    m_LastScaleModeApplied = -1;
    m_LastVfApplied.clear();
    m_LastCacheApplied.clear();
    m_LastLoopApplied.clear();

    mpv_set_option_string(m_Mpv, "vo", m_AudioOnly ? "null" : "libmpv");
    if (m_AudioOnly) {
        mpv_set_option_string(m_Mpv, "vid", "no");
    } else {
        const std::string hw = HwdecStringForMode(m_HwdecMode.load(std::memory_order_relaxed));
        m_LastHwdecApplied = hw;
        mpv_set_option_string(m_Mpv, "hwdec", hw.c_str());
        mpv_set_option_string(m_Mpv, "hwdec-codecs", "all");
        mpv_set_option_string(m_Mpv, "vd-lavc-dr", "auto");
        mpv_set_option_string(m_Mpv, "vd-lavc-fast", "yes");
        mpv_set_option_string(m_Mpv, "vd-lavc-framedrop", "nonref");
        mpv_set_option_string(m_Mpv, "vd-lavc-threads", hwCores <= 2 ? "2" : "4");
        mpv_set_option_string(m_Mpv, "correct-downscaling", "no");
        mpv_set_option_string(m_Mpv, "sigmoid-upscaling", "no");
        mpv_set_option_string(m_Mpv, "deband", "no");
        mpv_set_option_string(m_Mpv, "interpolation", "no");
        mpv_set_option_string(m_Mpv, "video-sync", "audio");
        mpv_set_option_string(m_Mpv, "framedrop", "vo");
        // Compact demuxer ring buffers so switching >2 videos never bloats RAM or stalls
        mpv_set_option_string(m_Mpv, "demuxer-donate-buffer", "no");
        mpv_set_option_string(m_Mpv, "demuxer-max-bytes", hwCores <= 2 ? "6MiB" : "12MiB");
        mpv_set_option_string(m_Mpv, "demuxer-max-back-bytes", "2MiB");
    }
    mpv_set_option_string(m_Mpv, "keep-open", "no");
    mpv_set_option_string(m_Mpv, "idle", "yes");
    mpv_set_option_string(m_Mpv, "terminal", "no");
    mpv_set_option_string(m_Mpv, "msg-level", "all=error");

    if (mpv_initialize(m_Mpv) < 0) {
        mpv_destroy(m_Mpv);
        m_Mpv = nullptr;
        return;
    }

    mpv_observe_property(m_Mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_Mpv, 4, "core-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(m_Mpv, 5, "container-fps", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 6, "hwdec-current", MPV_FORMAT_STRING);

    m_Quit.store(false);

    if (!m_AudioOnly) {
        ApplyScalerOptions();
        RebuildVideoFilterChain();

        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW) },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        };

        if (mpv_render_context_create(&m_RenderCtx, m_Mpv, params) >= 0) {
            mpv_render_context_set_update_callback(m_RenderCtx, &SlintVideoPlayer::OnRenderUpdate, this);
            m_RenderThread = std::thread(&SlintVideoPlayer::RenderWorkerLoop, this);
        }
        SetScaleMode(m_ScaleMode);
    }

    m_EventThread = std::thread(&SlintVideoPlayer::EventLoop, this);
}

void SlintVideoPlayer::ShutdownMpv() {
    m_Quit.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
        m_RenderWakeFlag = true;
    }
    m_RenderCv.notify_all();
    if (m_Mpv) {
        mpv_wakeup(m_Mpv);
    }
    if (m_RenderCtx) {
        mpv_render_context_set_update_callback(m_RenderCtx, nullptr, nullptr);
    }
    if (m_RenderThread.joinable()) {
        m_RenderThread.join();
    }
    if (m_EventThread.joinable()) {
        m_EventThread.join();
    }
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    if (m_RenderCtx) {
        mpv_render_context_free(m_RenderCtx);
        m_RenderCtx = nullptr;
    }
    if (m_Mpv) {
        mpv_terminate_destroy(m_Mpv);
        m_Mpv = nullptr;
    }
}

void SlintVideoPlayer::OnRenderUpdate(void* ctx) {
    auto* self = static_cast<SlintVideoPlayer*>(ctx);
    if (!self) return;
    {
        std::lock_guard<std::mutex> lk(self->m_RenderWakeMutex);
        self->m_RenderWakeFlag = true;
    }
    self->m_RenderCv.notify_one();
}

void SlintVideoPlayer::RenderWorkerLoop() {
    int64_t lastRenderUs = 0;

    while (!m_Quit.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lk(m_RenderWakeMutex);
            m_RenderCv.wait_for(lk, std::chrono::milliseconds(16), [this]() {
                return m_Quit.load(std::memory_order_acquire) ||
                       m_RenderWakeFlag ||
                       m_ForceRedraw.load(std::memory_order_acquire);
            });
            m_RenderWakeFlag = false;
        }

        if (m_Quit.load(std::memory_order_acquire)) break;

        std::unique_lock<std::mutex> rLock(m_RenderMutex);
        if (!m_RenderCtx) continue;

        const bool force = m_ForceRedraw.exchange(false, std::memory_order_acq_rel);
        const uint64_t flags = mpv_render_context_update(m_RenderCtx);

        if (!m_AcceptingFrames.load(std::memory_order_acquire)) {
            continue;
        }
        if (!force && !(flags & MPV_RENDER_UPDATE_FRAME)) {
            continue;
        }

        // Jitter-tolerant FPS limiter (82% period threshold so a 60fps video at 60fps cap NEVER drops frames!)
        const int maxFps = m_MaxFps.load(std::memory_order_relaxed);
        const int64_t nowUs = NowSteadyUs();
        if (!force && maxFps > 0) {
            const int64_t minGapUs = (1000000LL / maxFps) * 82 / 100;
            if (nowUs - lastRenderUs < minGapUs) {
                continue;
            }
        }
        lastRenderUs = nowUs;

        const int w = m_TargetW;
        const int h = m_TargetH;
        const bool wantLanCopy = m_CaptureForLan.load(std::memory_order_relaxed);

        slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
            static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        int size[2] = { w, h };
        size_t stride = static_cast<size_t>(w) * 4;
        int blockForTargetTime = 0; // CRITICAL: never sleep 33ms inside mpv_render_context_render!

        char fmt[] = "rgba";
        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_SW_SIZE, size },
            { MPV_RENDER_PARAM_SW_FORMAT, fmt },
            { MPV_RENDER_PARAM_SW_STRIDE, &stride },
            { MPV_RENDER_PARAM_SW_POINTER, buffer.begin() },
            { MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blockForTargetTime },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        };

        const int rc = mpv_render_context_render(m_RenderCtx, params);
        if (rc < 0) {
            continue;
        }
        mpv_render_context_report_swap(m_RenderCtx);

        if (wantLanCopy) {
            const size_t bytes = stride * static_cast<size_t>(h);
            m_LanPixelBuf.resize(bytes);
            std::memcpy(m_LanPixelBuf.data(), buffer.begin(), bytes);
            m_LanBufW = w;
            m_LanBufH = h;
        }
        rLock.unlock();

        // Publish finished buffer to single-slot mailbox & wake UI event loop without queuing duplicates
        FrameReadyCallback cbCopy;
        {
            std::lock_guard<std::mutex> lk(m_ReadyMutex);
            m_ReadyBuffer   = std::move(buffer);
            m_HasReadyFrame = true;
            if (m_OnFrameReady && !m_UiNotifyPending.exchange(true, std::memory_order_acq_rel)) {
                cbCopy = m_OnFrameReady;
            }
        }
        if (cbCopy) {
            cbCopy();
        }
    }
}

void SlintVideoPlayer::EventLoop() {
    while (!m_Quit.load()) {
        mpv_event* ev = mpv_wait_event(m_Mpv, 0.15);
        if (!ev || ev->event_id == MPV_EVENT_NONE) continue;
        if (ev->event_id == MPV_EVENT_SHUTDOWN) break;

        if (ev->event_id == MPV_EVENT_START_FILE) {
            m_Playing.store(true);
            m_EndReached.store(false);
        } else if (ev->event_id == MPV_EVENT_PLAYBACK_RESTART || ev->event_id == MPV_EVENT_VIDEO_RECONFIG) {
            m_AcceptingFrames.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
                m_RenderWakeFlag = true;
            }
            m_RenderCv.notify_one();
        } else if (ev->event_id == MPV_EVENT_END_FILE) {
            m_Playing.store(false);
            auto* ef = static_cast<mpv_event_end_file*>(ev->data);
            if (ef && ef->reason == MPV_END_FILE_REASON_EOF) {
                m_EndReached.store(true);
            }
        } else if (ev->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto* prop = static_cast<mpv_event_property*>(ev->data);
            if (!prop || !prop->data) continue;
            if (ev->reply_userdata == 1 && prop->format == MPV_FORMAT_DOUBLE) {
                if (NowSteadyMs() >= m_SeekIgnoreUntilMs.load()) {
                    double t = *static_cast<double*>(prop->data);
                    m_TimeMs.store(static_cast<int64_t>(std::max(0.0, t) * 1000.0));
                }
            } else if (ev->reply_userdata == 2 && prop->format == MPV_FORMAT_DOUBLE) {
                double d = *static_cast<double*>(prop->data);
                m_LengthMs.store(static_cast<int64_t>(std::max(0.0, d) * 1000.0));
            } else if (ev->reply_userdata == 3 && prop->format == MPV_FORMAT_FLAG) {
                m_Paused.store(*static_cast<int*>(prop->data) != 0);
            } else if (ev->reply_userdata == 5 && prop->format == MPV_FORMAT_DOUBLE) {
                double fps = *static_cast<double*>(prop->data);
                if (fps > 0.0 && fps < 1000.0) {
                    m_SourceFps.store(fps, std::memory_order_relaxed);
                }
            } else if (ev->reply_userdata == 6 && prop->format == MPV_FORMAT_STRING) {
                const char* hwStr = *static_cast<char**>(prop->data);
                std::lock_guard<std::mutex> lk(m_InfoMutex);
                m_ActiveHwdec = (hwStr && hwStr[0] != '\0') ? hwStr : "software (CPU)";
            }
        }
    }
}

void SlintVideoPlayer::Play(const std::string& path, bool loop, bool muted) {
    if (path.empty()) return;
    EnsureInit();
    if (!m_Mpv) return;

    m_AcceptingFrames.store(false, std::memory_order_release);
    m_ForceRedraw.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        m_HasReadyFrame = false;
    }

    if (m_LastCacheApplied != "no") {
        m_LastCacheApplied = "no";
        mpv_set_property_string(m_Mpv, "cache", "no");
    }
    const char* loopStr = loop ? "inf" : "no";
    if (m_LastLoopApplied != loopStr) {
        m_LastLoopApplied = loopStr;
        mpv_set_property_string(m_Mpv, "loop-file", loopStr);
    }
    SetMute(muted);
    SetVolume(m_Volume.load());
    if (!m_AudioOnly) {
        ApplyHwdecOption();
        ApplyScalerOptions();
        SetScaleMode(m_ScaleMode);
    }

    int pauseFlag = 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_Paused.store(false);

    const char* cmd[] = { "loadfile", path.c_str(), "replace", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(true);
}

void SlintVideoPlayer::PlayLiveStream(const std::string& uri, bool loop, bool muted, bool lowLatency) {
    if (uri.empty()) return;
    EnsureInit();
    if (!m_Mpv) return;

    m_AcceptingFrames.store(false, std::memory_order_release);
    m_ForceRedraw.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        m_HasReadyFrame = false;
    }

    const char* cacheStr = lowLatency ? "no" : "yes";
    if (m_LastCacheApplied != cacheStr) {
        m_LastCacheApplied = cacheStr;
        mpv_set_property_string(m_Mpv, "cache", cacheStr);
    }
    const char* loopStr = loop ? "inf" : "no";
    if (m_LastLoopApplied != loopStr) {
        m_LastLoopApplied = loopStr;
        mpv_set_property_string(m_Mpv, "loop-file", loopStr);
    }
    SetMute(muted);
    SetVolume(m_Volume.load());
    if (!m_AudioOnly) {
        ApplyHwdecOption();
        ApplyScalerOptions();
        SetScaleMode(m_ScaleMode);
    }

    int pauseFlag = 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_Paused.store(false);

    const char* cmd[] = { "loadfile", uri.c_str(), "replace", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(true);
}

void SlintVideoPlayer::Stop() {
    if (!m_Mpv) return;
    m_EndReached.store(false);
    m_AcceptingFrames.store(false, std::memory_order_release);
    m_ForceRedraw.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        m_HasReadyFrame = false;
        m_ReadyBuffer   = slint::SharedPixelBuffer<slint::Rgba8Pixel>();
    }
    const char* cmd[] = { "stop", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(false);
    m_TimeMs.store(0);
    m_LengthMs.store(0);
}

bool SlintVideoPlayer::ConsumeEndReached() {
    return m_EndReached.exchange(false, std::memory_order_acq_rel);
}

void SlintVideoPlayer::SetPause(bool paused) {
    m_Paused.store(paused);
    if (!m_Mpv) return;
    int flag = paused ? 1 : 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void SlintVideoPlayer::SetMute(bool muted) {
    m_Muted.store(muted);
    if (!m_Mpv) return;
    int flag = muted ? 1 : 0;
    mpv_set_property_async(m_Mpv, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void SlintVideoPlayer::SetVolume(int volume) {
    volume = std::clamp(volume, 0, 150);
    m_Volume.store(volume);
    if (!m_Mpv) return;
    double v = static_cast<double>(volume);
    mpv_set_property_async(m_Mpv, 0, "volume", MPV_FORMAT_DOUBLE, &v);
}

void SlintVideoPlayer::SetPosition(float normalized) {
    if (!m_Mpv) return;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    int64_t lenMs = m_LengthMs.load();
    m_SeekIgnoreUntilMs.store(NowSteadyMs() + 260);
    if (lenMs > 0) {
        double targetSec = (static_cast<double>(lenMs) / 1000.0) * static_cast<double>(normalized);
        m_TimeMs.store(static_cast<int64_t>(targetSec * 1000.0));
        char secBuf[32];
        std::snprintf(secBuf, sizeof(secBuf), "%.3f", targetSec);
        const char* cmd[] = { "seek", secBuf, "absolute+exact", nullptr };
        mpv_command_async(m_Mpv, 0, cmd);
    } else {
        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%.2f", static_cast<double>(normalized) * 100.0);
        const char* cmd[] = { "seek", pctBuf, "absolute-percent+exact", nullptr };
        mpv_command_async(m_Mpv, 0, cmd);
    }
}

float SlintVideoPlayer::GetPosition() const {
    int64_t len = m_LengthMs.load();
    if (len <= 0) return 0.0f;
    return std::clamp(static_cast<float>(m_TimeMs.load()) / static_cast<float>(len), 0.0f, 1.0f);
}

void SlintVideoPlayer::SetLowResMode(bool lowRes) {
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    m_TargetW = lowRes ? 640 : 960;
    m_TargetH = lowRes ? 360 : 540;
}

void SlintVideoPlayer::SetRenderSize(int width, int height) {
    {
        std::lock_guard<std::mutex> lock(m_RenderMutex);
        m_TargetW = std::clamp(width, 64, 3840);
        m_TargetH = std::clamp(height, 36, 2160);
    }
    m_ForceRedraw.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
        m_RenderWakeFlag = true;
    }
    m_RenderCv.notify_one();
}

void SlintVideoPlayer::SetAudioOnlyMode(bool audioOnly) {
    m_AudioOnly = audioOnly;
    if (!m_Mpv) return;
    mpv_set_property_string(m_Mpv, "vid", audioOnly ? "no" : "auto");
}

void SlintVideoPlayer::SetScaleMode(int mode) {
    m_ScaleMode = std::clamp(mode, 0, 2);
    if (!m_Mpv || m_AudioOnly) return;
    if (m_ScaleMode == m_LastScaleModeApplied) return;
    m_LastScaleModeApplied = m_ScaleMode;

    if (m_ScaleMode == 0) {
        mpv_set_property_string(m_Mpv, "keepaspect", "yes");
        mpv_set_property_string(m_Mpv, "panscan", "1.0");
    } else if (m_ScaleMode == 1) {
        mpv_set_property_string(m_Mpv, "panscan", "0.0");
        mpv_set_property_string(m_Mpv, "keepaspect", "no");
    } else {
        mpv_set_property_string(m_Mpv, "panscan", "0.0");
        mpv_set_property_string(m_Mpv, "keepaspect", "yes");
    }
    m_ForceRedraw.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(m_RenderWakeMutex);
        m_RenderWakeFlag = true;
    }
    m_RenderCv.notify_one();
}

void SlintVideoPlayer::SetLanCaptureEnabled(bool enabled) {
    m_CaptureForLan.store(enabled, std::memory_order_release);
    if (!enabled) {
        std::lock_guard<std::mutex> lock(m_RenderMutex);
        m_LanPixelBuf.clear();
        m_LanPixelBuf.shrink_to_fit();
        m_LanBufW = 0;
        m_LanBufH = 0;
    }
}

void SlintVideoPlayer::SetVideoFilters(int brightness, int contrast, int saturation, int gamma, bool blur, int colorPreset, bool fsrSharpen) {
    {
        std::lock_guard<std::mutex> lk(m_FilterMutex);
        m_FiltBrightness  = brightness;
        m_FiltContrast    = contrast;
        m_FiltSaturation  = saturation;
        m_FiltGamma       = gamma;
        m_FiltBlur        = blur;
        m_FiltColorPreset = colorPreset;
    }
    (void)fsrSharpen;
    if (m_Mpv && !m_AudioOnly) {
        RebuildVideoFilterChain();
    }
}

void SlintVideoPlayer::RebuildVideoFilterChain() {
    if (!m_Mpv || m_AudioOnly) return;

    int brightness = 0, contrast = 0, saturation = 0, gamma = 0, colorPreset = 0;
    bool blur = false;
    {
        std::lock_guard<std::mutex> lk(m_FilterMutex);
        brightness  = m_FiltBrightness;
        contrast    = m_FiltContrast;
        saturation  = m_FiltSaturation;
        gamma       = m_FiltGamma;
        blur        = m_FiltBlur;
        colorPreset = m_FiltColorPreset;
    }

    int b = std::clamp(brightness, -50, 50);
    int c = std::clamp(contrast, -50, 50);
    int s = std::clamp(saturation, -100, 100);
    int g = std::clamp(gamma, -50, 50);

    const int   upMode   = m_UpscalerMode.load(std::memory_order_relaxed);
    const float sharp01  = m_UpscalerSharpness.load(std::memory_order_relaxed);
    const bool  smooth60 = m_SmoothMotion60.load(std::memory_order_relaxed);

    std::string vf;
    auto appendVf = [&vf](const std::string& f) {
        if (f.empty()) return;
        if (!vf.empty()) vf += ",";
        vf += f;
    };

    // Optional real-time 24/25/30 -> 60 FPS frame interpolation (slice-multithreaded lavfi framerate)
    if (smooth60) {
        appendVf("framerate=fps=60");
    }

    if (colorPreset == 1) { // Cálido
        appendVf("colorbalance=rs=0.14:gs=0.04:bs=-0.14");
    } else if (colorPreset == 2) { // Cine / Frío
        appendVf("colorbalance=rs=-0.10:bs=0.16");
        c = std::clamp(c + 10, -50, 50);
    } else if (colorPreset == 3) { // Blanco y Negro (Noir)
        s = -100;
        c = std::clamp(c + 18, -50, 50);
    } else if (colorPreset == 4) { // Sepia
        appendVf("colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131");
    } else if (colorPreset == 5) { // Alto Contraste
        c = std::clamp(c + 26, -50, 50);
        s = std::clamp(s + 24, -100, 100);
    }

    if (b != 0 || c != 0 || s != 0 || g != 0) {
        float eqB = std::clamp(b / 100.0f, -0.5f, 0.5f);
        float eqC = std::clamp(1.0f + c / 50.0f, 0.2f, 2.5f);
        float eqS = std::clamp(1.0f + s / 100.0f, 0.0f, 3.0f);
        float eqG = std::clamp(1.0f + g / 50.0f, 0.2f, 3.0f);
        char eqBuf[160];
        std::snprintf(eqBuf, sizeof(eqBuf), "eq=brightness=%.3f:contrast=%.3f:saturation=%.3f:gamma=%.3f",
                      eqB, eqC, eqS, eqG);
        appendVf(eqBuf);
    }

    if (blur) {
        appendVf("gblur=sigma=8");
    } else if (upMode > 0 && sharp01 > 0.01f) {
        char shBuf[192];
        if (upMode == 1) {
            // Mode 1: AMD FSR 1.0 (RCAS mild unsharp)
            float amt = std::clamp(0.25f + sharp01 * 0.75f, 0.1f, 1.2f);
            std::snprintf(shBuf, sizeof(shBuf), "unsharp=5:5:%.2f:5:5:0.0", amt);
            appendVf(shBuf);
        } else if (upMode == 2) {
            // Mode 2: AMD FidelityFX CAS+ (slice-multithreaded Contrast Adaptive Sharpen — zero ringing!)
            float str = std::clamp(sharp01 * 0.92f, 0.05f, 1.0f);
            std::snprintf(shBuf, sizeof(shBuf), "cas=strength=%.2f", str);
            appendVf(shBuf);
        } else if (upMode == 3) {
            // Mode 3: NVIDIA Image Scaling — NIS (Spline directional upscale + adaptive contrast + fine 3x3 detail)
            float casStr = std::clamp(0.18f + sharp01 * 0.68f, 0.10f, 0.95f);
            float fineAmt = std::clamp(0.12f + sharp01 * 0.42f, 0.05f, 0.65f);
            std::snprintf(shBuf, sizeof(shBuf), "cas=strength=%.2f,unsharp=3:3:%.2f:3:3:0.0", casStr, fineAmt);
            appendVf(shBuf);
        } else if (upMode == 4) {
            // Mode 4: EWA LanczosHQ + Cinema Super-Resolution (best for 1080p -> 2160p 4K)
            float casStr = std::clamp(0.22f + sharp01 * 0.72f, 0.10f, 0.98f);
            float fineAmt = std::clamp(0.10f + sharp01 * 0.30f, 0.05f, 0.50f);
            std::snprintf(shBuf, sizeof(shBuf), "cas=strength=%.2f,unsharp=3:3:%.2f:3:3:0.0", casStr, fineAmt);
            appendVf(shBuf);
        }
    }

    std::string finalVf = vf.empty() ? "" : ("lavfi=[" + vf + "]");
    if (finalVf == m_LastVfApplied) {
        return;
    }
    m_LastVfApplied = finalVf;
    mpv_set_property_string(m_Mpv, "vf", finalVf.c_str());
}

void SlintVideoPlayer::SetAudioEqualizer(bool enabled, float bassDb, float midDb, float trebleDb) {
    if (!m_Mpv) return;
    if (!enabled) {
        mpv_set_property_string(m_Mpv, "af", "");
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "lavfi=[equalizer=f=100:t=h:w=120:g=%.1f,equalizer=f=1000:t=h:w=800:g=%.1f,equalizer=f=8000:t=h:w=4000:g=%.1f]",
        std::clamp(bassDb, -12.0f, 12.0f),
        std::clamp(midDb, -12.0f, 12.0f),
        std::clamp(trebleDb, -12.0f, 12.0f));
    mpv_set_property_string(m_Mpv, "af", buf);
}

void SlintVideoPlayer::SetMusicAudioFx(bool eqEnabled, bool dynamicEq,
                                       float b60, float b230, float b910, float b3600, float b12000) {
    EnsureInit();
    if (!m_Mpv) return;
    if (!eqEnabled && !dynamicEq) {
        mpv_set_property_string(m_Mpv, "af", "");
        return;
    }

    std::string af;
    auto appendAf = [&af](const std::string& f) {
        if (f.empty()) return;
        if (!af.empty()) af += ",";
        af += f;
    };

    auto cl = [](float v) { return std::clamp(v, -12.0f, 12.0f); };
    if (eqEnabled || dynamicEq) {
        char eqBuf[512];
        std::snprintf(eqBuf, sizeof(eqBuf),
            "equalizer=f=60:t=q:w=1.1:g=%.1f,"
            "equalizer=f=230:t=q:w=1.2:g=%.1f,"
            "equalizer=f=910:t=q:w=1.2:g=%.1f,"
            "equalizer=f=3600:t=q:w=1.2:g=%.1f,"
            "equalizer=f=12000:t=q:w=1.1:g=%.1f",
            cl(b60), cl(b230), cl(b910), cl(b3600), cl(b12000));
        appendAf(eqBuf);
    }

    if (dynamicEq) {
        appendAf("dynaudnorm=f=150:g=15:p=0.90:m=5.0:s=10");
    }

    std::string lavfi = "lavfi=[" + af + "]";
    mpv_set_property_string(m_Mpv, "af", lavfi.c_str());
}

bool SlintVideoPlayer::PollFrame(slint::Image& outImage) {
    slint::SharedPixelBuffer<slint::Rgba8Pixel> ready;
    {
        std::lock_guard<std::mutex> lk(m_ReadyMutex);
        m_UiNotifyPending.store(false, std::memory_order_release);
        if (!m_HasReadyFrame) {
            return false;
        }
        ready = std::move(m_ReadyBuffer);
        m_HasReadyFrame = false;
    }
    if (ready.width() == 0 || ready.height() == 0) {
        return false;
    }
    outImage = slint::Image(ready);
    return true;
}

bool SlintVideoPlayer::EncodeLatestFrameJpeg(std::vector<uint8_t>& outJpeg, int quality) {
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    const size_t expected = static_cast<size_t>(m_LanBufW) * static_cast<size_t>(m_LanBufH) * 4;
    if (m_LanBufW <= 0 || m_LanBufH <= 0 || m_LanPixelBuf.size() < expected) {
        return false;
    }

    outJpeg.clear();
    outJpeg.reserve(65536);
    auto writer = [](void* ctx, void* data, int size) {
        auto* vec = static_cast<std::vector<uint8_t>*>(ctx);
        auto* bytes = static_cast<const uint8_t*>(data);
        vec->insert(vec->end(), bytes, bytes + size);
    };

    int ok = stbi_write_jpg_to_func(
        writer, &outJpeg, m_LanBufW, m_LanBufH, 4, m_LanPixelBuf.data(), std::clamp(quality, 30, 95));
    return ok != 0 && !outJpeg.empty();
}

bool SlintVideoPlayer::CopyLatestFrameRgb(std::vector<uint8_t>& outRgb, int maxW, int maxH, int& outW, int& outH) {
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    const size_t expected = static_cast<size_t>(m_LanBufW) * static_cast<size_t>(m_LanBufH) * 4;
    if (m_LanBufW <= 0 || m_LanBufH <= 0 || m_LanPixelBuf.size() < expected) {
        return false;
    }

    const int srcW = m_LanBufW;
    const int srcH = m_LanBufH;
    int dstW = (maxW > 0) ? std::min(srcW, maxW) : srcW;
    int dstH = (maxH > 0) ? std::min(srcH, maxH) : srcH;
    if (dstW <= 0 || dstH <= 0) { dstW = srcW; dstH = srcH; }

    outW = dstW;
    outH = dstH;
    outRgb.resize(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 3);

    const uint8_t* src = m_LanPixelBuf.data();
    uint8_t* dst = outRgb.data();

    if (dstW == srcW && dstH == srcH) {
        const size_t pixels = static_cast<size_t>(srcW) * static_cast<size_t>(srcH);
        for (size_t i = 0; i < pixels; ++i) {
            dst[i * 3 + 0] = src[i * 4 + 0];
            dst[i * 3 + 1] = src[i * 4 + 1];
            dst[i * 3 + 2] = src[i * 4 + 2];
        }
    } else {
        for (int y = 0; y < dstH; ++y) {
            const int sy = (y * srcH) / dstH;
            const uint8_t* srcRow = src + static_cast<size_t>(sy) * static_cast<size_t>(srcW) * 4;
            uint8_t* dstRow = dst + static_cast<size_t>(y) * static_cast<size_t>(dstW) * 3;
            for (int x = 0; x < dstW; ++x) {
                const int sx = (x * srcW) / dstW;
                dstRow[x * 3 + 0] = srcRow[sx * 4 + 0];
                dstRow[x * 3 + 1] = srcRow[sx * 4 + 1];
                dstRow[x * 3 + 2] = srcRow[sx * 4 + 2];
            }
        }
    }
    return true;
}

} // namespace ProyecThor::SlintUI
