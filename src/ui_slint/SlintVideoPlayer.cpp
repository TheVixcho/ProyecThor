#include "SlintVideoPlayer.h"

#include <mpv/client.h>
#include <mpv/render.h>

#include <algorithm>
#include <cstring>

namespace ProyecThor::SlintUI {

SlintVideoPlayer::SlintVideoPlayer() {
    InitMpv();
}

SlintVideoPlayer::~SlintVideoPlayer() {
    ShutdownMpv();
}

void SlintVideoPlayer::InitMpv() {
    m_Mpv = mpv_create();
    if (!m_Mpv) return;

    mpv_set_option_string(m_Mpv, "vo", "libmpv");
    mpv_set_option_string(m_Mpv, "hwdec", "auto-copy");
    mpv_set_option_string(m_Mpv, "keep-open", "no");
    mpv_set_option_string(m_Mpv, "idle", "yes");
    mpv_set_option_string(m_Mpv, "terminal", "no");
    mpv_set_option_string(m_Mpv, "msg-level", "all=error");
    mpv_set_option_string(m_Mpv, "ytdl", "yes");

    if (mpv_initialize(m_Mpv) < 0) {
        mpv_destroy(m_Mpv);
        m_Mpv = nullptr;
        return;
    }

    mpv_observe_property(m_Mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_Mpv, 4, "core-idle", MPV_FORMAT_FLAG);

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW) },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };

    if (mpv_render_context_create(&m_RenderCtx, m_Mpv, params) >= 0) {
        mpv_render_context_set_update_callback(m_RenderCtx, &SlintVideoPlayer::OnRenderUpdate, this);
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
        } else if (ev->event_id == MPV_EVENT_END_FILE) {
            m_Playing.store(false);
        } else if (ev->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto* prop = static_cast<mpv_event_property*>(ev->data);
            if (!prop || !prop->data) continue;
            if (ev->reply_userdata == 1 && prop->format == MPV_FORMAT_DOUBLE) {
                double t = *static_cast<double*>(prop->data);
                m_TimeMs.store(static_cast<int64_t>(std::max(0.0, t) * 1000.0));
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
    if (!m_Mpv || path.empty()) return;

    mpv_set_property_string(m_Mpv, "cache", "auto");
    mpv_set_property_string(m_Mpv, "loop-file", loop ? "inf" : "no");
    SetMute(muted);
    SetVolume(m_Volume.load());

    int pauseFlag = 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_Paused.store(false);

    const char* cmd[] = { "loadfile", path.c_str(), "replace", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(true);
}

void SlintVideoPlayer::PlayLiveStream(const std::string& uri, bool loop, bool muted, bool lowLatency) {
    if (!m_Mpv || uri.empty()) return;

    mpv_set_property_string(m_Mpv, "cache", lowLatency ? "no" : "yes");
    mpv_set_property_string(m_Mpv, "loop-file", loop ? "inf" : "no");
    SetMute(muted);
    SetVolume(m_Volume.load());

    int pauseFlag = 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseFlag);
    m_Paused.store(false);

    const char* cmd[] = { "loadfile", uri.c_str(), "replace", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(true);
}

void SlintVideoPlayer::Stop() {
    if (!m_Mpv) return;
    const char* cmd[] = { "stop", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
    m_Playing.store(false);
    m_TimeMs.store(0);
    m_LengthMs.store(0);
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
    double pct = static_cast<double>(normalized) * 100.0;
    std::string pctStr = std::to_string(pct);
    const char* cmd[] = { "seek", pctStr.c_str(), "absolute-percent", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
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

void SlintVideoPlayer::SetVideoFilters(int brightness, int contrast, int saturation, int gamma, bool blur, int colorPreset) {
    if (!m_Mpv) return;

    int b = std::clamp(brightness, -50, 50);
    int c = std::clamp(contrast, -50, 50);
    int s = std::clamp(saturation, -100, 100);
    int g = std::clamp(gamma, -50, 50);

    // Apply color grading preset adjustments
    std::string vf;
    if (colorPreset == 1) { // Cálido
        vf = "colorbalance=rs=0.08:bs=-0.08";
    } else if (colorPreset == 2) { // Cine / Frío
        vf = "colorbalance=rs=-0.06:bs=0.10";
    } else if (colorPreset == 3) { // Blanco y Negro (Noir)
        s = -100;
        c = std::clamp(c + 15, -50, 50);
    } else if (colorPreset == 4) { // Sepia
        vf = "colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131";
    } else if (colorPreset == 5) { // Alto Contraste
        c = std::clamp(c + 25, -50, 50);
        s = std::clamp(s + 20, -100, 100);
    }

    if (blur) {
        if (!vf.empty()) vf += ",";
        vf += "gblur=sigma=8";
    }

    int64_t vb = b, vc = c, vs = s, vg = g;
    mpv_set_property_async(m_Mpv, 0, "brightness", MPV_FORMAT_INT64, &vb);
    mpv_set_property_async(m_Mpv, 0, "contrast",   MPV_FORMAT_INT64, &vc);
    mpv_set_property_async(m_Mpv, 0, "saturation", MPV_FORMAT_INT64, &vs);
    mpv_set_property_async(m_Mpv, 0, "gamma",      MPV_FORMAT_INT64, &vg);

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

bool SlintVideoPlayer::PollFrame(slint::Image& outImage) {
    if (!m_RenderCtx || !m_FrameDirty.exchange(false, std::memory_order_acq_rel)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_RenderMutex);
    if (!m_RenderCtx) return false;

    uint64_t flags = mpv_render_context_update(m_RenderCtx);
    if (!(flags & MPV_RENDER_UPDATE_FRAME)) {
        return false;
    }

    int size[2] = { m_TargetW, m_TargetH };
    size_t stride = static_cast<size_t>(m_TargetW) * 4;
    m_PixelBuf.resize(stride * static_cast<size_t>(m_TargetH));

    char fmt[] = "rgba";
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_SW_SIZE, size },
        { MPV_RENDER_PARAM_SW_FORMAT, fmt },
        { MPV_RENDER_PARAM_SW_STRIDE, &stride },
        { MPV_RENDER_PARAM_SW_POINTER, m_PixelBuf.data() },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };

    if (mpv_render_context_render(m_RenderCtx, params) < 0) {
        return false;
    }

    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        static_cast<uint32_t>(m_TargetW), static_cast<uint32_t>(m_TargetH));
    std::memcpy(buffer.begin(), m_PixelBuf.data(), m_PixelBuf.size());
    outImage = slint::Image(buffer);
    return true;
}

} // namespace ProyecThor::SlintUI
