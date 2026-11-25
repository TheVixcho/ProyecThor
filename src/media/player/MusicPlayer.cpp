#include "MusicPlayer.h"
#include "media/audio/WindowsAudioSession.h"

#include <mpv/client.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace ProyecThor::Core {

namespace {

std::string ResolveLocalPath(const std::string& path)
{
    if (path.empty()) return path;
    std::error_code ec;
    std::filesystem::path u8p = std::filesystem::u8path(path);
    if (std::filesystem::exists(u8p, ec))
        return u8p.make_preferred().u8string();
    ec.clear();
    std::filesystem::path acp = std::filesystem::path(path);
    if (std::filesystem::exists(acp, ec))
        return acp.make_preferred().u8string();
    return path;
}

double NowSeconds()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

} // namespace

MusicPlayer::MusicPlayer()
{
    std::setlocale(LC_NUMERIC, "C");

    m_Mpv = mpv_create();
    if (!m_Mpv) {
        std::cerr << "[MusicPlayer] No se pudo crear la instancia de libmpv.\n";
        return;
    }

    mpv_set_option_string(m_Mpv, "terminal", "no");
    mpv_set_option_string(m_Mpv, "msg-level", "all=error");
    mpv_set_option_string(m_Mpv, "vid", "no");
    mpv_set_option_string(m_Mpv, "vo", "null");
    mpv_set_option_string(m_Mpv, "osd-level", "0");
    mpv_set_option_string(m_Mpv, "idle", "yes");
    mpv_set_option_string(m_Mpv, "keep-open", "no");
    mpv_set_option_string(m_Mpv, "ytdl", "no");
    mpv_set_option_string(m_Mpv, "volume-max", "200");
    mpv_set_option_string(m_Mpv, "audio-exclusive", "no");

    if (mpv_initialize(m_Mpv) < 0) {
        std::cerr << "[MusicPlayer] Error al inicializar libmpv.\n";
        mpv_terminate_destroy(m_Mpv);
        m_Mpv = nullptr;
        return;
    }

    mpv_observe_property(m_Mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 3, "pause",    MPV_FORMAT_FLAG);

    ApplyVolume();
    ApplyEqualizer();

    m_EventThread = std::thread([this]() { EventLoop(); });
}

MusicPlayer::~MusicPlayer()
{
    m_Quit.store(true, std::memory_order_release);
    if (m_Mpv) {
        mpv_wakeup(m_Mpv);
    }
    if (m_EventThread.joinable()) {
        m_EventThread.join();
    }
    if (m_Mpv) {
        mpv_terminate_destroy(m_Mpv);
        m_Mpv = nullptr;
    }

#ifdef _WIN32
    if (m_MeterPtr) {
        static_cast<IAudioMeterInformation*>(m_MeterPtr)->Release();
        m_MeterPtr = nullptr;
    }
    if (m_ComInitialized)
        CoUninitialize();
#endif
}

void MusicPlayer::EventLoop()
{
    while (!m_Quit.load(std::memory_order_acquire) && m_Mpv) {
        mpv_event* ev = mpv_wait_event(m_Mpv, 0.25);
        if (!ev || ev->event_id == MPV_EVENT_NONE)
            continue;

        switch (ev->event_id) {
        case MPV_EVENT_SHUTDOWN:
            return;

        case MPV_EVENT_FILE_LOADED:
            m_Playing.store(true, std::memory_order_relaxed);
            break;

        case MPV_EVENT_END_FILE: {
            auto* ef = static_cast<mpv_event_end_file*>(ev->data);
            m_Playing.store(false, std::memory_order_relaxed);
            if (!ef) break;
            if (ef->reason == MPV_END_FILE_REASON_EOF) {
                m_EndReached.store(true, std::memory_order_relaxed);
            } else if (ef->reason == MPV_END_FILE_REASON_ERROR) {
                std::cerr << "[MusicPlayer] Error de libmpv: " << mpv_error_string(ef->error) << "\n";
                m_HadError.store(true, std::memory_order_relaxed);
                m_EndReached.store(true, std::memory_order_relaxed);
            }
            break;
        }

        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* prop = static_cast<mpv_event_property*>(ev->data);
            if (!prop || !prop->data) break;
            switch (ev->reply_userdata) {
            case 1: // time-pos
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    double sec = *static_cast<double*>(prop->data);
                    m_TimeMs.store(std::max<int64_t>(0, static_cast<int64_t>(sec * 1000.0)), std::memory_order_relaxed);
                }
                break;
            case 2: // duration
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    double sec = *static_cast<double*>(prop->data);
                    m_LengthMs.store(std::max<int64_t>(0, static_cast<int64_t>(sec * 1000.0)), std::memory_order_relaxed);
                }
                break;
            case 3: // pause
                if (prop->format == MPV_FORMAT_FLAG) {
                    int flag = *static_cast<int*>(prop->data);
                    m_Paused.store(flag != 0, std::memory_order_relaxed);
                }
                break;
            default:
                break;
            }
            break;
        }

        default:
            break;
        }
    }
}

void MusicPlayer::Play(const std::string& utf8Path)
{
    if (utf8Path.empty() || !m_Mpv) return;

    uint64_t gen = ++m_ReqGen;
    m_EndReached.store(false, std::memory_order_relaxed);
    m_HadError.store(false, std::memory_order_relaxed);
    m_Paused.store(false, std::memory_order_relaxed);
    m_TimeMs.store(0, std::memory_order_relaxed);
    m_LengthMs.store(0, std::memory_order_relaxed);

    const bool isUrl = (utf8Path.rfind("http", 0) == 0 || utf8Path.rfind("rtsp", 0) == 0);
    std::string finalPath = isUrl ? utf8Path : ResolveLocalPath(utf8Path);

    int unpause = 0;
    mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &unpause);
    ApplyVolume();

    const char* cmd[] = { "loadfile", finalPath.c_str(), "replace", nullptr };
    int rc = mpv_command_async(m_Mpv, gen, cmd);
    if (rc < 0) {
        std::cerr << "[MusicPlayer] Error al cargar pista: " << mpv_error_string(rc) << "\n";
        m_HadError.store(true, std::memory_order_relaxed);
        m_EndReached.store(true, std::memory_order_relaxed);
    }
#ifdef _WIN32
    ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
}

void MusicPlayer::Stop()
{
    ++m_ReqGen;
    m_EndReached.store(false, std::memory_order_relaxed);
    m_Paused.store(false, std::memory_order_relaxed);
    m_Playing.store(false, std::memory_order_relaxed);
    m_TimeMs.store(0, std::memory_order_relaxed);

    if (m_Mpv) {
        const char* cmd[] = { "stop", nullptr };
        mpv_command_async(m_Mpv, 0, cmd);
    }
}

void MusicPlayer::SetPause(bool paused)
{
    m_Paused.store(paused, std::memory_order_relaxed);
    if (m_Mpv) {
        int flag = paused ? 1 : 0;
        mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
    }
}

void MusicPlayer::SetPosition(float normalized)
{
    if (!m_Mpv) return;
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    double pct = static_cast<double>(normalized) * 100.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", pct);
    const char* cmd[] = { "seek", buf, "absolute-percent", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
}

int64_t MusicPlayer::GetTime() const
{
    return m_TimeMs.load(std::memory_order_relaxed);
}

int64_t MusicPlayer::GetLength() const
{
    return m_LengthMs.load(std::memory_order_relaxed);
}

void MusicPlayer::SetVolume(int percent)
{
    m_Volume.store(std::clamp(percent, 0, 200), std::memory_order_relaxed);
    ApplyVolume();
}

void MusicPlayer::SetMute(bool mute)
{
    m_Muted.store(mute, std::memory_order_relaxed);
    ApplyVolume();
}

void MusicPlayer::ApplyVolume()
{
    if (!m_Mpv) return;

    const bool muted = m_Muted.load(std::memory_order_relaxed);
    int muteFlag = muted ? 1 : 0;
    mpv_set_property_async(m_Mpv, 0, "mute", MPV_FORMAT_FLAG, &muteFlag);

    if (!muted) {
        double vol = static_cast<double>(m_Volume.load(std::memory_order_relaxed));
        mpv_set_property_async(m_Mpv, 0, "volume", MPV_FORMAT_DOUBLE, &vol);
#ifdef _WIN32
        ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
    }
}

void MusicPlayer::ApplyEqualizer()
{
    if (!m_Mpv) return;

    std::lock_guard<std::mutex> lk(m_EqMutex);
    if (!m_EqEnabled) {
        mpv_set_property_string(m_Mpv, "af", "");
        return;
    }

    static constexpr int kBandFreqs[kEqualizerBands] = {
        60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000
    };

    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << std::setprecision(2);
    ss << "lavfi=[volume=" << m_EqPreamp << "dB";
    for (int b = 0; b < kEqualizerBands; ++b) {
        ss << ",equalizer=f=" << kBandFreqs[b] << ":t=o:w=1:g=" << m_EqBands[b];
    }
    ss << "]";
    std::string filterStr = ss.str();
    mpv_set_property_string(m_Mpv, "af", filterStr.c_str());
}

void MusicPlayer::SetEqualizerEnabled(bool enabled)
{
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqEnabled = enabled; }
    ApplyEqualizer();
}

void MusicPlayer::SetEqualizerPreamp(float preampDb)
{
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqPreamp = preampDb; }
    ApplyEqualizer();
}

void MusicPlayer::SetEqualizerBand(int index, float ampDb)
{
    if (index < 0 || index >= kEqualizerBands) return;
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqBands[index] = ampDb; }
    ApplyEqualizer();
}

bool MusicPlayer::ConsumeEndReached()
{
    return m_EndReached.exchange(false, std::memory_order_relaxed);
}

bool MusicPlayer::ConsumeHadError()
{
    return m_HadError.exchange(false, std::memory_order_relaxed);
}

void MusicPlayer::GetAudioLevels(float& left, float& right)
{
    left = right = 0.0f;

    const bool active = m_Mpv && !m_Paused.load(std::memory_order_relaxed) &&
                        m_Playing.load(std::memory_order_relaxed) &&
                        !m_Muted.load(std::memory_order_relaxed);
    if (!active) {
        m_LastL *= 0.85f; m_LastR *= 0.85f;
        left = m_LastL;   right = m_LastR;
        return;
    }

#ifdef _WIN32
    if (!m_MeterPtr && NowSeconds() >= m_MeterRetryAt) {
        m_MeterRetryAt = NowSeconds() + 3.0;

        HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hrInit == S_OK || hrInit == S_FALSE) m_ComInitialized = true;

        IMMDeviceEnumerator* enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(&enumerator))) && enumerator) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device)) && device) {
                IAudioMeterInformation* meter = nullptr;
                if (SUCCEEDED(device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL,
                                               nullptr, reinterpret_cast<void**>(&meter))) && meter)
                    m_MeterPtr = meter;
                device->Release();
            }
            enumerator->Release();
        }
    }

    if (m_MeterPtr) {
        auto* meter = static_cast<IAudioMeterInformation*>(m_MeterPtr);
        UINT channels = 0;
        float peaks[8] = { 0.0f };
        if (SUCCEEDED(meter->GetMeteringChannelCount(&channels)) && channels > 0 && channels <= 8 &&
            SUCCEEDED(meter->GetChannelsPeakValues(channels, peaks))) {
            const float l = peaks[0];
            const float r = channels > 1 ? peaks[1] : peaks[0];
            m_LastL = l; m_LastR = r;
            left = l;    right = r;
            return;
        }
        meter->Release();
        m_MeterPtr = nullptr;
    }
#endif

    const float t = static_cast<float>(NowSeconds());
    const float v = 0.35f + 0.25f * std::sin(t * 7.3f) + 0.15f * std::sin(t * 13.1f + 1.7f);
    m_LastL = m_LastR = std::clamp(v, 0.05f, 1.0f);
    left = right = m_LastL;
}

}
