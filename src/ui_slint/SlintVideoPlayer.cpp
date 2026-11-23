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
} // namespace

SlintVideoPlayer::SlintVideoPlayer(bool lazyInit) {
    if (!lazyInit) {
        InitMpv();
    }
}

SlintVideoPlayer::~SlintVideoPlayer() {
    ShutdownMpv();
}

void SlintVideoPlayer::EnsureInit() {
    if (!m_Mpv) {
        InitMpv();
    }
}

void SlintVideoPlayer::InitMpv() {
    if (m_Mpv) return;
    m_Mpv = mpv_create();
    if (!m_Mpv) return;

    mpv_set_option_string(m_Mpv, "vo", m_AudioOnly ? "null" : "libmpv");
    if (m_AudioOnly) {
        mpv_set_option_string(m_Mpv, "vid", "no");
    } else {
        mpv_set_option_string(m_Mpv, "hwdec", "auto-copy");
        mpv_set_option_string(m_Mpv, "sws-fast", "yes");
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

    if (!m_AudioOnly) {
        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW) },
            { MPV_RENDER_PARAM_INVALID, nullptr }
        };

        if (mpv_render_context_create(&m_RenderCtx, m_Mpv, params) >= 0) {
            mpv_render_context_set_update_callback(m_RenderCtx, &SlintVideoPlayer::OnRenderUpdate, this);
        }
        SetScaleMode(m_ScaleMode);
    }

    m_Quit.store(false);
    m_EventThread = std::thread(&SlintVideoPlayer::EventLoop, this);
}

void SlintVideoPlayer::ShutdownMpv() {
    m_Quit.store(true);
    if (m_Mpv) {
        mpv_wakeup(m_Mpv);
    }
    if (m_EventThread.joinable()) {
        m_EventThread.join();
    }
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    if (m_RenderCtx) {
        mpv_render_context_set_update_callback(m_RenderCtx, nullptr, nullptr);
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
    if (self) {
        self->m_FrameDirty.store(true, std::memory_order_release);
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
            }
        }
    }
}

void SlintVideoPlayer::Play(const std::string& path, bool loop, bool muted) {
    if (path.empty()) return;
    EnsureInit();
    if (!m_Mpv) return;

    mpv_set_property_string(m_Mpv, "cache", "auto");
    mpv_set_property_string(m_Mpv, "loop-file", loop ? "inf" : "no");
    SetMute(muted);
    SetVolume(m_Volume.load());
    if (!m_AudioOnly) SetScaleMode(m_ScaleMode);

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

    mpv_set_property_string(m_Mpv, "cache", lowLatency ? "no" : "yes");
    mpv_set_property_string(m_Mpv, "loop-file", loop ? "inf" : "no");
    SetMute(muted);
    SetVolume(m_Volume.load());
    if (!m_AudioOnly) SetScaleMode(m_ScaleMode);

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
    m_FrameDirty.store(false, std::memory_order_release);
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
    std::lock_guard<std::mutex> lock(m_RenderMutex);
    m_TargetW = std::clamp(width, 64, 1920);
    m_TargetH = std::clamp(height, 36, 1080);
    m_ForceRedraw.store(true, std::memory_order_release);
}

void SlintVideoPlayer::SetAudioOnlyMode(bool audioOnly) {
    m_AudioOnly = audioOnly;
    if (!m_Mpv) return;
    mpv_set_property_string(m_Mpv, "vid", audioOnly ? "no" : "auto");
}

void SlintVideoPlayer::SetScaleMode(int mode) {
    m_ScaleMode = std::clamp(mode, 0, 2);
    if (!m_Mpv || m_AudioOnly) return;

    if (m_ScaleMode == 0) {
        // 0 = Recortar / Llenar (Cover — fills 100% without black bars, preserves aspect)
        mpv_set_property_string(m_Mpv, "keepaspect", "yes");
        mpv_set_property_string(m_Mpv, "panscan", "1.0");
    } else if (m_ScaleMode == 1) {
        // 1 = Estirar (Fill — stretches video to 100% width x height)
        mpv_set_property_string(m_Mpv, "panscan", "0.0");
        mpv_set_property_string(m_Mpv, "keepaspect", "no");
    } else {
        // 2 = Original / Ajustar (Contain — shows entire frame without cropping or stretching)
        mpv_set_property_string(m_Mpv, "panscan", "0.0");
        mpv_set_property_string(m_Mpv, "keepaspect", "yes");
    }
    m_FrameDirty.store(true, std::memory_order_release);
    m_ForceRedraw.store(true, std::memory_order_release);
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
    if (!m_Mpv) return;

    int b = std::clamp(brightness, -50, 50);
    int c = std::clamp(contrast, -50, 50);
    int s = std::clamp(saturation, -100, 100);
    int g = std::clamp(gamma, -50, 50);

    // Apply color grading preset adjustments
    std::string vf;
    auto appendVf = [&vf](const std::string& f) {
        if (f.empty()) return;
        if (!vf.empty()) vf += ",";
        vf += f;
    };

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

    // Use real FFmpeg lavfi eq filter so brightness/contrast/saturation/gamma ALWAYS apply in SW libmpv!
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
    }

    if (fsrSharpen) {
        // Contrast-adaptive unsharp pass complementing FSR/Lanczos scaling
        appendVf("unsharp=5:5:0.75:5:5:0.0");
    }

    if (vf.empty()) {
        mpv_set_property_string(m_Mpv, "vf", "");
    } else {
        std::string lavfi = "lavfi=[" + vf + "]";
        mpv_set_property_string(m_Mpv, "vf", lavfi.c_str());
    }
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

    // Real-time 5-band parametric equalizer (applied when EQ or Dynamic EQ is active)
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

    // ProyecThor Dynamic EQ DSP: real-time adaptive loudness & smooth peak control
    if (dynamicEq) {
        appendAf("dynaudnorm=f=150:g=15:p=0.90:m=5.0:s=10");
    }

    std::string lavfi = "lavfi=[" + af + "]";
    mpv_set_property_string(m_Mpv, "af", lavfi.c_str());
}

bool SlintVideoPlayer::PollFrame(slint::Image& outImage) {
    const bool force = m_ForceRedraw.exchange(false, std::memory_order_acq_rel);
    const bool dirty = m_FrameDirty.exchange(false, std::memory_order_acq_rel);
    if (!m_RenderCtx || (!dirty && !force)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_RenderMutex);
    if (!m_RenderCtx) return false;

    uint64_t flags = mpv_render_context_update(m_RenderCtx);
    if (!force && !(flags & MPV_RENDER_UPDATE_FRAME)) {
        return false;
    }

    // Zero-copy direct render into Slint pixel buffer!
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        static_cast<uint32_t>(m_TargetW), static_cast<uint32_t>(m_TargetH));
    int size[2] = { m_TargetW, m_TargetH };
    size_t stride = static_cast<size_t>(m_TargetW) * 4;

    char fmt[] = "rgba";
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_SW_SIZE, size },
        { MPV_RENDER_PARAM_SW_FORMAT, fmt },
        { MPV_RENDER_PARAM_SW_STRIDE, &stride },
        { MPV_RENDER_PARAM_SW_POINTER, buffer.begin() },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };

    if (mpv_render_context_render(m_RenderCtx, params) < 0) {
        return false;
    }

    // Only keep a copy of RGBA bytes when Video LAN encoding is actively turned ON
    if (m_CaptureForLan.load(std::memory_order_relaxed)) {
        size_t bytes = stride * static_cast<size_t>(m_TargetH);
        m_LanPixelBuf.resize(bytes);
        std::memcpy(m_LanPixelBuf.data(), buffer.begin(), bytes);
        m_LanBufW = m_TargetW;
        m_LanBufH = m_TargetH;
    }

    outImage = slint::Image(buffer);
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
