#include "MPVBasePlayer.h"
#include "core/plugins/PluginManager.h"
#include "core/settings/SettingsManager.h"
#include "media/audio/WindowsAudioSession.h"

#ifdef _WIN32
#include <basetsd.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#pragma comment(lib, "ole32.lib")
#endif

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace {

std::atomic<int> g_NextMpvInstanceId{0};

std::string GetDirectYoutubeURL(const std::string& youtubeURL)
{
    return ProyecThor::Core::PluginManager::Get().ResolveStreamUrl(youtubeURL);
}

std::string ResolveLocalPathForMPV(const std::string& path)
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

std::string NormalizePathForCompare(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string MapHwDecoderToMpv(const std::string& dec, bool useHw, bool nativeWindow)
{
    if (!useHw || dec == "none" || dec == "no")
        return "no";
    if (dec.empty() || dec == "any" || dec == "auto")
        return "auto-safe";
    if (dec == "d3d11va")
        return nativeWindow ? "d3d11va" : "d3d11va-copy";
    if (dec == "dxva2")
        return nativeWindow ? "dxva2" : "dxva2-copy";
    if (dec == "vaapi")
        return "vaapi";
    if (dec == "vdpau")
        return "vdpau";
    if (dec == "nvdec")
        return "nvdec";
    return dec;
}

std::string MapVideoOutputToMpv(const std::string& vout)
{
    if (vout.empty() || vout == "auto" || vout == "any" ||
        vout == "direct3d11" || vout == "d3d11" || vout == "direct3d9" ||
        vout == "opengl" || vout == "gl" || vout == "wayland" || vout == "gpu")
        return "gpu";
    if (vout == "gpu-next")
        return "gpu-next";
    if (vout == "xvideo" || vout == "xcb_xv" || vout == "xv")
        return "xv";
    if (vout == "xcb_x11" || vout == "x11")
        return "x11";
    return "gpu";
}

void* GetProcAddressGL(void* /*ctx*/, const char* name)
{
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

#ifdef _WIN32

class ProcessAudioMeter {
public:
    static ProcessAudioMeter& Get() { static ProcessAudioMeter m; return m; }

    void Read(float& left, float& right)
    {
        left = right = 0.0f;
        std::lock_guard<std::mutex> lk(m_Mutex);
        const auto now = std::chrono::steady_clock::now();
        if (now >= m_NextRefresh) {
            Refresh();
            m_NextRefresh = now + std::chrono::milliseconds(1500);
        }
        for (auto* meter : m_Meters) {
            UINT channels = 0;
            float peaks[8] = { 0.0f };
            if (SUCCEEDED(meter->GetMeteringChannelCount(&channels)) && channels > 0 && channels <= 8 &&
                SUCCEEDED(meter->GetChannelsPeakValues(channels, peaks))) {
                left  = std::max(left,  peaks[0]);
                right = std::max(right, channels > 1 ? peaks[1] : peaks[0]);
            }
        }
    }

private:
    ProcessAudioMeter() = default;
    ~ProcessAudioMeter() = default;

    void ReleaseAll()
    {
        for (auto* m : m_Meters) m->Release();
        m_Meters.clear();
    }

    void Refresh()
    {
        ReleaseAll();
        const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        (void)hrInit;

        IMMDeviceEnumerator* en = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                    __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en))) || !en)
            return;
        IMMDevice* dev = nullptr;
        if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev)) && dev) {
            IAudioSessionManager2* mgr = nullptr;
            if (SUCCEEDED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr,
                                        reinterpret_cast<void**>(&mgr))) && mgr) {
                IAudioSessionEnumerator* sessions = nullptr;
                if (SUCCEEDED(mgr->GetSessionEnumerator(&sessions)) && sessions) {
                    int count = 0;
                    sessions->GetCount(&count);
                    const DWORD myPid = GetCurrentProcessId();
                    for (int i = 0; i < count; ++i) {
                        IAudioSessionControl* ctl = nullptr;
                        if (FAILED(sessions->GetSession(i, &ctl)) || !ctl) continue;
                        IAudioSessionControl2* ctl2 = nullptr;
                        if (SUCCEEDED(ctl->QueryInterface(__uuidof(IAudioSessionControl2),
                                                          reinterpret_cast<void**>(&ctl2))) && ctl2) {
                            DWORD pid = 0;
                            if (SUCCEEDED(ctl2->GetProcessId(&pid)) && pid == myPid) {
                                IAudioMeterInformation* meter = nullptr;
                                if (SUCCEEDED(ctl->QueryInterface(__uuidof(IAudioMeterInformation),
                                                                  reinterpret_cast<void**>(&meter))) && meter)
                                    m_Meters.push_back(meter);
                            }
                            ctl2->Release();
                        }
                        ctl->Release();
                    }
                    sessions->Release();
                }
                mgr->Release();
            }
            dev->Release();
        }
        en->Release();
        ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
    }

    std::mutex m_Mutex;
    std::vector<IAudioMeterInformation*> m_Meters;
    std::chrono::steady_clock::time_point m_NextRefresh{};
};

#endif

} // namespace

namespace ProyecThor::Core {

std::string MPVBasePlayer::s_HwDecoder   = "auto";
std::string MPVBasePlayer::s_VideoOutput = "auto";
std::string MPVBasePlayer::s_Deinterlace = "discard";

void MPVBasePlayer::SetDefaultHwDecoder(const std::string& dec)
{
    s_HwDecoder = dec.empty() ? "auto" : dec;
}

std::string MPVBasePlayer::GetDefaultHwDecoder()
{
    return s_HwDecoder;
}

void MPVBasePlayer::SetDefaultVideoOutput(const std::string& vout)
{
    s_VideoOutput = vout.empty() ? "auto" : vout;
}

std::string MPVBasePlayer::GetDefaultVideoOutput()
{
    return s_VideoOutput;
}

void MPVBasePlayer::SetDefaultDeinterlace(const std::string& deint)
{
    s_Deinterlace = deint.empty() ? "discard" : deint;
}

std::string MPVBasePlayer::GetDefaultDeinterlace()
{
    return s_Deinterlace;
}

void MPVBasePlayer::ApplyDeinterlace(const std::string& mode)
{
    if (!m_Mpv) return;
    const char* val = (mode.empty() || mode == "discard" || mode == "none" || mode == "no") ? "no" : "yes";
    mpv_set_property_string(m_Mpv, "deinterlace", val);
}

MPVBasePlayer::MPVBasePlayer(int decodeThreads, bool useHardwareDecode, bool forceSilent,
                             bool nativeWindowOutput)
    : m_DecodeThreads(decodeThreads)
    , m_UseHardwareDecode(useHardwareDecode)
    , m_NativeWindowOutput(nativeWindowOutput)
{
    m_InstanceId = g_NextMpvInstanceId.fetch_add(1, std::memory_order_relaxed);
    m_ForceSilent.store(forceSilent, std::memory_order_relaxed);

    std::cerr << "[MPV#" << m_InstanceId << "] Construido. forceSilent="
              << (forceSilent ? "true" : "false")
              << " nativeWindow=" << (nativeWindowOutput ? "true" : "false") << "\n";

    InitMPV();
}

MPVBasePlayer::~MPVBasePlayer()
{
    DestroyMPV();
    DestroyFramebuffer();
}

void MPVBasePlayer::InitMPV()
{
    // libmpv exige LC_NUMERIC="C" para inicializar correctamente.
    std::setlocale(LC_NUMERIC, "C");

    m_Mpv = mpv_create();
    if (!m_Mpv) {
        std::cerr << "[MPV#" << m_InstanceId << "] Error al crear instancia libmpv.\n";
        return;
    }

    const bool forceSilent = m_ForceSilent.load(std::memory_order_relaxed);

    mpv_set_option_string(m_Mpv, "terminal", "no");
    mpv_set_option_string(m_Mpv, "msg-level", "all=error");
    mpv_set_option_string(m_Mpv, "osd-level", "0");
    mpv_set_option_string(m_Mpv, "osc", "no");
    mpv_set_option_string(m_Mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_Mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_Mpv, "idle", "yes");
    mpv_set_option_string(m_Mpv, "keep-open", "no");
    mpv_set_option_string(m_Mpv, "sub-auto", "no");
    mpv_set_option_string(m_Mpv, "sid", "no");
    mpv_set_option_string(m_Mpv, "ytdl", "no");
    mpv_set_option_string(m_Mpv, "volume-max", "200");
    mpv_set_option_string(m_Mpv, "audio-exclusive", "no");
    mpv_set_option_string(m_Mpv, "video-timing-offset", "0");

    if (forceSilent) {
        mpv_set_option_string(m_Mpv, "audio", "no");
        mpv_set_option_string(m_Mpv, "aid", "no");
    }

    if (m_NativeWindowOutput) {
        if (m_NativeWindowHandle) {
            int64_t wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(m_NativeWindowHandle));
            mpv_set_option(m_Mpv, "wid", MPV_FORMAT_INT64, &wid);
            std::string vo = MapVideoOutputToMpv(s_VideoOutput);
            mpv_set_option_string(m_Mpv, "vo", vo.c_str());
        } else {
            mpv_set_option_string(m_Mpv, "vo", "null");
        }
    } else {
        mpv_set_option_string(m_Mpv, "vo", "libmpv");
    }

    std::string hw = MapHwDecoderToMpv(s_HwDecoder, m_UseHardwareDecode, m_NativeWindowOutput);
    mpv_set_option_string(m_Mpv, "hwdec", hw.c_str());

    if (m_DecodeThreads > 0) {
        std::string thr = std::to_string(m_DecodeThreads);
        mpv_set_option_string(m_Mpv, "vd-lavc-threads", thr.c_str());
    }

    if (mpv_initialize(m_Mpv) < 0) {
        std::cerr << "[MPV#" << m_InstanceId << "] Error al inicializar libmpv.\n";
        mpv_terminate_destroy(m_Mpv);
        m_Mpv = nullptr;
        return;
    }

    mpv_observe_property(m_Mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_Mpv, 3, "pause",    MPV_FORMAT_FLAG);
    mpv_observe_property(m_Mpv, 4, "dwidth",   MPV_FORMAT_INT64);
    mpv_observe_property(m_Mpv, 5, "dheight",  MPV_FORMAT_INT64);

    ApplyDeinterlace(s_Deinterlace);
    ApplyAudioState();

    m_EventStop.store(false, std::memory_order_relaxed);
    m_EventThread = std::thread([this]() { EventLoop(); });
}

void MPVBasePlayer::DestroyMPV()
{
    Stop();

    m_EventStop.store(true, std::memory_order_release);
    if (m_Mpv) {
        mpv_wakeup(m_Mpv);
    }
    if (m_EventThread.joinable()) {
        m_EventThread.join();
    }

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

void MPVBasePlayer::EventLoop()
{
    while (!m_EventStop.load(std::memory_order_acquire) && m_Mpv) {
        mpv_event* ev = mpv_wait_event(m_Mpv, 0.25);
        if (!ev || ev->event_id == MPV_EVENT_NONE)
            continue;

        switch (ev->event_id) {
        case MPV_EVENT_SHUTDOWN:
            return;

        case MPV_EVENT_FILE_LOADED:
            m_MpvIsPlaying.store(true, std::memory_order_relaxed);
            m_AudioNeedsSync.store(true, std::memory_order_relaxed);
            break;

        case MPV_EVENT_END_FILE: {
            auto* ef = static_cast<mpv_event_end_file*>(ev->data);
            m_MpvIsPlaying.store(false, std::memory_order_relaxed);
            if (ef) {
                if (ef->reason == MPV_END_FILE_REASON_EOF) {
                    m_EndReached.store(true, std::memory_order_relaxed);
                } else if (ef->reason == MPV_END_FILE_REASON_ERROR) {
                    std::cerr << "[MPV#" << m_InstanceId << "] Error de reproduccion: "
                              << mpv_error_string(ef->error) << "\n";
                    m_HadError.store(true, std::memory_order_relaxed);
                    m_LoadHasError.store(true, std::memory_order_relaxed);
                    m_EndReached.store(true, std::memory_order_relaxed);
                }
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
                    m_TimeMs.store(static_cast<int64_t>(sec * 1000.0), std::memory_order_relaxed);
                }
                break;
            case 2: // duration
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    double sec = *static_cast<double*>(prop->data);
                    m_LengthMs.store(static_cast<int64_t>(sec * 1000.0), std::memory_order_relaxed);
                }
                break;
            case 3: // pause
                if (prop->format == MPV_FORMAT_FLAG) {
                    int flag = *static_cast<int*>(prop->data);
                    m_Paused.store(flag != 0, std::memory_order_relaxed);
                }
                break;
            case 4: // dwidth
                if (prop->format == MPV_FORMAT_INT64) {
                    int64_t w = *static_cast<int64_t*>(prop->data);
                    m_ReportedW.store(static_cast<int>(w), std::memory_order_relaxed);
                }
                break;
            case 5: // dheight
                if (prop->format == MPV_FORMAT_INT64) {
                    int64_t h = *static_cast<int64_t*>(prop->data);
                    m_ReportedH.store(static_cast<int>(h), std::memory_order_relaxed);
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

void MPVBasePlayer::OnMpvRenderUpdate(void* userData)
{
    auto* self = static_cast<MPVBasePlayer*>(userData);
    if (self) {
        self->m_NeedsRender.store(true, std::memory_order_release);
    }
}

void MPVBasePlayer::EnsureRenderContext()
{
    if (m_RenderCtx || !m_Mpv || m_NativeWindowOutput)
        return;

    mpv_opengl_init_params glInit{};
    glInit.get_proc_address     = &GetProcAddressGL;
    glInit.get_proc_address_ctx = nullptr;

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };

    int err = mpv_render_context_create(&m_RenderCtx, m_Mpv, params);
    if (err < 0) {
        std::cerr << "[MPV#" << m_InstanceId << "] Error al crear mpv_render_context OpenGL: "
                  << mpv_error_string(err) << "\n";
        m_RenderCtx = nullptr;
        return;
    }

    mpv_render_context_set_update_callback(m_RenderCtx, &MPVBasePlayer::OnMpvRenderUpdate, this);
    m_NeedsRender.store(true, std::memory_order_release);
}

void MPVBasePlayer::EnsureFramebuffer(int w, int h)
{
    if (m_TextureID && m_FboID && m_VideoW == w && m_VideoH == h)
        return;

    DestroyFramebuffer();

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_FboID);
    GLint prevFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));

    m_VideoW = w;
    m_VideoH = h;
}

void MPVBasePlayer::DestroyFramebuffer()
{
    if (m_FboID) {
        glDeleteFramebuffers(1, &m_FboID);
        m_FboID = 0;
    }
    if (m_TextureID) {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
    m_VideoW = 0;
    m_VideoH = 0;
}

void MPVBasePlayer::Play(const std::string& path, bool loop, bool startMuted)
{
    if (!m_Mpv) return;

    if (m_NativeWindowOutput && !m_NativeWindowHandle) {
        std::cerr << "[MPV#" << m_InstanceId << "] Play() omitido: reproductor nativo sin ventana asignada.\n";
        return;
    }

    if (m_PathBlocked && NormalizePathForCompare(m_BlockedPath) == NormalizePathForCompare(path)) {
        std::cerr << "[MPV#" << m_InstanceId << "] Play() ignorado, ruta bloqueada: " << path << "\n";
        return;
    }

    if (!path.empty() && path == m_CurrentPath) {
        std::cerr << "[MPV#" << m_InstanceId << "] Play() ignorado, reentrante para ruta ya activa/cargando: "
                  << path << "\n";
        return;
    }
    m_CurrentPath = path;

    uint64_t myGen = ++m_LoadGeneration;
    m_HasEverPlayed.store(true, std::memory_order_relaxed);

    if (startMuted)
        SetMute(true);

    LoadAndPlay(path, loop, startMuted, myGen);
}

void MPVBasePlayer::BlockPath(const std::string& path)
{
    m_BlockedPath = path;
    m_PathBlocked = true;
}

void MPVBasePlayer::UnblockPath()
{
    m_PathBlocked = false;
    m_BlockedPath.clear();
}

void MPVBasePlayer::LoadAndPlay(const std::string& path, bool loop, bool /*startMuted*/, uint64_t myGeneration)
{
    std::string finalPath = path;
    if (finalPath.find("youtube.com") != std::string::npos ||
        finalPath.find("youtu.be")    != std::string::npos)
    {
        std::string direct = GetDirectYoutubeURL(finalPath);
        if (!direct.empty())
            finalPath = direct;
        else
            std::cerr << "[Plugins] Fallo al resolver URL de streaming con el plugin ytdlp.\n";
    }

    if (m_NativeWindowOutput && !m_NativeWindowHandle)
        return;

    if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        return;

    const bool isUrl = (finalPath.rfind("http", 0) == 0 || finalPath.rfind("rtsp", 0) == 0);
    if (!isUrl) {
        finalPath = ResolveLocalPathForMPV(finalPath);
    }

    std::lock_guard<std::mutex> lock(m_MediaSwapMutex);
    if (!m_Mpv || m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        return;

    m_EndReached.store(false, std::memory_order_relaxed);
    m_HadError.store(false, std::memory_order_relaxed);
    m_LoadHasError.store(false, std::memory_order_relaxed);
    m_MpvIsPlaying.store(false, std::memory_order_relaxed);
    m_HasVideoFrame.store(false, std::memory_order_relaxed);
    m_TimeMs.store(0, std::memory_order_relaxed);
    m_LengthMs.store(-1, std::memory_order_relaxed);
    m_ReportedW.store(0, std::memory_order_relaxed);
    m_ReportedH.store(0, std::memory_order_relaxed);

    mpv_set_property_string(m_Mpv, "loop-file", loop ? "inf" : "no");

    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    std::string hw = MapHwDecoderToMpv(s_HwDecoder, m_UseHardwareDecode, m_NativeWindowOutput);
    mpv_set_property_string(m_Mpv, "hwdec", hw.c_str());

    int effectiveThreads = m_DecodeThreads;
    if (perf.lowMemoryMode) {
        effectiveThreads = (m_DecodeThreads == 1) ? 1 : 2;
        mpv_set_property_string(m_Mpv, "vd-lavc-fast", "yes");
        mpv_set_property_string(m_Mpv, "vd-lavc-skiploopfilter", "nonref");
    } else if (perf.performanceMode) {
        if (effectiveThreads <= 0 || effectiveThreads > 2) effectiveThreads = 2;
        mpv_set_property_string(m_Mpv, "vd-lavc-fast", "yes");
        mpv_set_property_string(m_Mpv, "vd-lavc-skiploopfilter", "default");
    } else {
        mpv_set_property_string(m_Mpv, "vd-lavc-fast", "no");
        mpv_set_property_string(m_Mpv, "vd-lavc-skiploopfilter", "default");
    }
    if (effectiveThreads > 0) {
        int64_t thr = effectiveThreads;
        mpv_set_property(m_Mpv, "vd-lavc-threads", MPV_FORMAT_INT64, &thr);
    }

    ApplyDeinterlace(s_Deinterlace);
    ApplyAudioState();

    if (!m_AudioDeviceId.empty() && m_AudioDeviceId != "default" &&
        !std::isdigit(static_cast<unsigned char>(m_AudioDeviceId[0]))) {
        mpv_set_property_string(m_Mpv, "audio-device", m_AudioDeviceId.c_str());
    }

    int unpause = 0;
    mpv_set_property(m_Mpv, "pause", MPV_FORMAT_FLAG, &unpause);
    m_Paused.store(false, std::memory_order_relaxed);
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    m_AudioNeedsSync.store(true, std::memory_order_relaxed);

    const char* cmd[] = { "loadfile", finalPath.c_str(), "replace", nullptr };
    int rc = mpv_command_async(m_Mpv, myGeneration, cmd);
    if (rc < 0) {
        std::cerr << "[MPV#" << m_InstanceId << "] Error en loadfile: " << mpv_error_string(rc) << "\n";
        m_HadError.store(true, std::memory_order_relaxed);
        m_LoadHasError.store(true, std::memory_order_relaxed);
        m_EndReached.store(true, std::memory_order_relaxed);
    }
}

void MPVBasePlayer::Stop()
{
    ++m_LoadGeneration;

    m_EndReached.store(false, std::memory_order_relaxed);
    m_MpvIsPlaying.store(false, std::memory_order_relaxed);
    m_HasVideoFrame.store(false, std::memory_order_relaxed);
    m_CurrentPath.clear();

    std::lock_guard<std::mutex> lock(m_MediaSwapMutex);
    if (m_Mpv) {
        const char* cmd[] = { "stop", nullptr };
        mpv_command(m_Mpv, cmd);
    }
}

void MPVBasePlayer::ReleaseIdleResources()
{
    if (m_MpvIsPlaying.load(std::memory_order_relaxed) || !m_CurrentPath.empty())
        return;

    m_HasVideoFrame.store(false, std::memory_order_relaxed);
    DestroyFramebuffer();
}

bool MPVBasePlayer::ConsumeEndReached()
{
    return m_EndReached.exchange(false, std::memory_order_relaxed);
}

bool MPVBasePlayer::ConsumeHadError()
{
    return m_HadError.exchange(false, std::memory_order_relaxed);
}

void MPVBasePlayer::ApplyAudioState()
{
    if (!m_Mpv) return;

    const bool silent = m_ForceSilent.load(std::memory_order_relaxed) ||
                        !m_AudioActive.load(std::memory_order_relaxed) ||
                        m_Muted.load(std::memory_order_relaxed);
    int muteFlag = silent ? 1 : 0;
    mpv_set_property_async(m_Mpv, 0, "mute", MPV_FORMAT_FLAG, &muteFlag);

    if (!silent) {
        double vol = static_cast<double>(m_VolumeMultiplier.load(std::memory_order_relaxed)) * 100.0;
        if (vol < 0.0) vol = 0.0;
        if (vol > 200.0) vol = 200.0;
        mpv_set_property_async(m_Mpv, 0, "volume", MPV_FORMAT_DOUBLE, &vol);
#ifdef _WIN32
        ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
    }
}

void MPVBasePlayer::SetMute(bool mute)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    bool effectiveMute = mute || m_ForceSilent.load(std::memory_order_relaxed);
    bool prev = m_Muted.exchange(effectiveMute, std::memory_order_relaxed);
    if (prev == effectiveMute) return;

    ApplyAudioState();
}

void MPVBasePlayer::SetAudioActive(bool active)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    bool effectiveActive = active && !m_ForceSilent.load(std::memory_order_relaxed);
    bool prev = m_AudioActive.exchange(effectiveActive, std::memory_order_relaxed);
    if (prev == effectiveActive) return;

    ApplyAudioState();
}

void MPVBasePlayer::SetVolume(int volume)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    if (m_ForceSilent.load(std::memory_order_relaxed))
        volume = 0;

    float multiplier = static_cast<float>(volume) / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);

    ApplyAudioState();
}

void MPVBasePlayer::SetSoftwareVolume(float percent)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    if (m_ForceSilent.load(std::memory_order_relaxed))
        percent = 0.0f;

    float multiplier = percent / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);

    ApplyAudioState();
}

void MPVBasePlayer::EnforceSilenceIfNeeded()
{
    if (!m_Mpv) return;

    const bool shouldBeSilent = m_ForceSilent.load(std::memory_order_relaxed) ||
                                !m_AudioActive.load(std::memory_order_relaxed);
    const bool needsSync = m_AudioNeedsSync.exchange(false, std::memory_order_relaxed);
    const bool silenceChanged = m_SilenceEnforced.exchange(shouldBeSilent, std::memory_order_relaxed) != shouldBeSilent;

    if (needsSync || silenceChanged) {
        ApplyAudioState();
        if (!shouldBeSilent && !m_AudioDeviceId.empty() && m_AudioDeviceId != "default" &&
            !std::isdigit(static_cast<unsigned char>(m_AudioDeviceId[0]))) {
            mpv_set_property_string(m_Mpv, "audio-device", m_AudioDeviceId.c_str());
        }
    }
}

void MPVBasePlayer::ApplyEqualizer()
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

void MPVBasePlayer::SetEqualizerEnabled(bool enabled)
{
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqEnabled = enabled; }
    ApplyEqualizer();
}

void MPVBasePlayer::SetEqualizerPreamp(float preampDb)
{
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqPreamp = preampDb; }
    if (m_EqEnabled) ApplyEqualizer();
}

void MPVBasePlayer::SetEqualizerBand(int index, float ampDb)
{
    if (index < 0 || index >= kEqualizerBands) return;
    { std::lock_guard<std::mutex> lk(m_EqMutex); m_EqBands[index] = ampDb; }
    if (m_EqEnabled) ApplyEqualizer();
}

void MPVBasePlayer::SetPause(bool paused)
{
    m_Paused.store(paused, std::memory_order_relaxed);
    if (m_Mpv) {
        int flag = paused ? 1 : 0;
        mpv_set_property_async(m_Mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
    }
}

void MPVBasePlayer::SetPosition(float pos)
{
    if (!m_Mpv) return;
    pos = std::clamp(pos, 0.0f, 1.0f);
    double pct = static_cast<double>(pos) * 100.0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", pct);
    const char* cmd[] = { "seek", buf, "absolute-percent", nullptr };
    mpv_command_async(m_Mpv, 0, cmd);
}

float MPVBasePlayer::GetPosition() const
{
    int64_t len = m_LengthMs.load(std::memory_order_relaxed);
    if (len <= 0) return 0.0f;
    int64_t t = m_TimeMs.load(std::memory_order_relaxed);
    return std::clamp(static_cast<float>(t) / static_cast<float>(len), 0.0f, 1.0f);
}

bool MPVBasePlayer::IsPlaying() const
{
    return m_Mpv && m_MpvIsPlaying.load(std::memory_order_relaxed) &&
           !m_Paused.load(std::memory_order_relaxed);
}

int64_t MPVBasePlayer::GetTime() const
{
    return m_TimeMs.load(std::memory_order_relaxed);
}

int64_t MPVBasePlayer::GetLength() const
{
    return m_LengthMs.load(std::memory_order_relaxed);
}

void* MPVBasePlayer::GetTextureID()
{
    return m_TextureID
        ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureID))
        : nullptr;
}

void MPVBasePlayer::GetVideoSize(int& width, int& height)
{
    if (m_VideoW > 0 && m_VideoH > 0) {
        width  = m_VideoW;
        height = m_VideoH;
    } else {
        width  = m_ReportedW.load(std::memory_order_relaxed);
        height = m_ReportedH.load(std::memory_order_relaxed);
    }
}

bool MPVBasePlayer::HasVideoFrame() const
{
    return m_HasVideoFrame.load(std::memory_order_acquire) &&
           m_TextureID != 0 && m_VideoW > 0 && m_VideoH > 0;
}

MPVBasePlayer::LoadState MPVBasePlayer::GetLoadState() const
{
    if (!m_HasEverPlayed.load(std::memory_order_relaxed)) return LoadState::Idle;
    if (m_LoadHasError.load(std::memory_order_relaxed))   return LoadState::Error;

    bool ready = m_NativeWindowOutput
        ? m_MpvIsPlaying.load(std::memory_order_relaxed)
        : HasVideoFrame();
    if (ready) return LoadState::Ready;

    if (!m_MpvIsPlaying.load(std::memory_order_relaxed))  return LoadState::Opening;
    return LoadState::Buffering;
}

bool MPVBasePlayer::IsLoading() const
{
    LoadState s = GetLoadState();
    return s == LoadState::Opening || s == LoadState::Buffering;
}

bool MPVBasePlayer::UpdateTexture()
{
    if (!m_Mpv || m_NativeWindowOutput)
        return false;

    EnforceSilenceIfNeeded();
    EnsureRenderContext();
    if (!m_RenderCtx)
        return false;

    const bool signaled = m_NeedsRender.exchange(false, std::memory_order_acq_rel);
    if (!signaled && m_HasVideoFrame.load(std::memory_order_relaxed))
        return false;

    uint64_t flags = mpv_render_context_update(m_RenderCtx);
    if (!(flags & MPV_RENDER_UPDATE_FRAME))
        return false;

    int vw = m_ReportedW.load(std::memory_order_relaxed);
    int vh = m_ReportedH.load(std::memory_order_relaxed);
    if (vw <= 0 || vh <= 0) {
        int64_t w64 = 0, h64 = 0;
        if (mpv_get_property(m_Mpv, "dwidth", MPV_FORMAT_INT64, &w64) >= 0 &&
            mpv_get_property(m_Mpv, "dheight", MPV_FORMAT_INT64, &h64) >= 0) {
            vw = static_cast<int>(w64);
            vh = static_cast<int>(h64);
            m_ReportedW.store(vw, std::memory_order_relaxed);
            m_ReportedH.store(vh, std::memory_order_relaxed);
        }
    }
    if (vw <= 0 || vh <= 0)
        return false;

    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    int maxW = 0, maxH = 0;
    if (perf.lowMemoryMode) {
        maxW = 854;
        maxH = 480;
    } else if (perf.performanceMode) {
        maxW = 1280;
        maxH = 720;
    }
    int tw = vw, th = vh;
    if (maxW > 0 && maxH > 0 && (tw > maxW || th > maxH)) {
        const double scale = std::min(static_cast<double>(maxW) / tw,
                                      static_cast<double>(maxH) / th);
        tw = std::max(16, (static_cast<int>(tw * scale + 0.5) + 1) & ~1);
        th = std::max(16, (static_cast<int>(th * scale + 0.5) + 1) & ~1);
    }

    EnsureFramebuffer(tw, th);
    if (!m_FboID || !m_TextureID)
        return false;

    // Guardar estado OpenGL que mpv_render_context_render modifica
    GLint prevDrawFbo = 0, prevReadFbo = 0;
    GLint prevViewport[4] = { 0, 0, 0, 0 };
    GLboolean prevScissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean prevBlend   = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    mpv_opengl_fbo fbo{};
    fbo.fbo             = static_cast<int>(m_FboID);
    fbo.w               = tw;
    fbo.h               = th;
    fbo.internal_format = GL_RGBA8;

    int flipY = 0;
    mpv_render_param renderParams[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flipY },
        { MPV_RENDER_PARAM_INVALID, nullptr }
    };

    mpv_render_context_render(m_RenderCtx, renderParams);

    // Restaurar estado OpenGL
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDrawFbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevReadFbo));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    if (prevScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (prevBlend)   glEnable(GL_BLEND);        else glDisable(GL_BLEND);

    m_HasVideoFrame.store(true, std::memory_order_release);
    return true;
}

std::vector<MPVBasePlayer::AudioDevice> MPVBasePlayer::GetAvailableAudioDevices()
{
    std::vector<AudioDevice> devices;
    devices.push_back({ "default", "Dispositivo predeterminado del sistema" });
    if (!m_Mpv) return devices;

    mpv_node node{};
    if (mpv_get_property(m_Mpv, "audio-device-list", MPV_FORMAT_NODE, &node) < 0)
        return devices;

    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        for (int i = 0; i < node.u.list->num; ++i) {
            const mpv_node& item = node.u.list->values[i];
            if (item.format != MPV_FORMAT_NODE_MAP || !item.u.list) continue;
            std::string name, desc;
            for (int j = 0; j < item.u.list->num; ++j) {
                const char* key = item.u.list->keys[j];
                const mpv_node& val = item.u.list->values[j];
                if (!key || val.format != MPV_FORMAT_STRING || !val.u.string) continue;
                if (std::strcmp(key, "name") == 0) name = val.u.string;
                else if (std::strcmp(key, "description") == 0) desc = val.u.string;
            }
            if (!name.empty() && name != "auto") {
                devices.push_back({ name, desc.empty() ? name : desc });
            }
        }
    }
    mpv_free_node_contents(&node);
    return devices;
}

void MPVBasePlayer::SetAudioDevice(const std::string& deviceId)
{
    m_AudioDeviceId = deviceId;
    if (!m_Mpv) return;

    if (deviceId.empty() || deviceId == "default" ||
        std::isdigit(static_cast<unsigned char>(deviceId[0]))) {
        mpv_set_property_string(m_Mpv, "audio-device", "auto");
        return;
    }
    mpv_set_property_string(m_Mpv, "audio-device", deviceId.c_str());
}

void MPVBasePlayer::GetAudioLevels(float& left, float& right)
{
    left = right = 0.0f;
#ifdef _WIN32
    if (ProyecThor::Settings::SettingsManager::Get().GetSettings().performance.performanceMode)
        return;
    const bool audible = m_Mpv && !m_Paused.load(std::memory_order_relaxed) &&
                         !m_Muted.load(std::memory_order_relaxed) &&
                         m_AudioActive.load(std::memory_order_relaxed) &&
                         !m_ForceSilent.load(std::memory_order_relaxed) &&
                         m_MpvIsPlaying.load(std::memory_order_relaxed);
    if (!audible) return;
    ProcessAudioMeter::Get().Read(left, right);
#endif
}

void MPVBasePlayer::AttachNativeWindow(void* nativeHandle)
{
    if (m_NativeWindowHandle == nativeHandle) return;
    m_NativeWindowHandle = nativeHandle;
    if (!m_Mpv || !nativeHandle) return;

    int64_t wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(nativeHandle));
    mpv_set_property(m_Mpv, "wid", MPV_FORMAT_INT64, &wid);
    std::string vo = MapVideoOutputToMpv(s_VideoOutput);
    mpv_set_property_string(m_Mpv, "vo", vo.c_str());
}

void MPVBasePlayer::DetachNativeWindow()
{
    m_NativeWindowHandle = nullptr;
    if (!m_Mpv) return;
    mpv_set_property_string(m_Mpv, "vo", "null");
    int64_t zero = 0;
    mpv_set_property(m_Mpv, "wid", MPV_FORMAT_INT64, &zero);
}

void MPVBasePlayer::Reinit()
{
    if (m_NativeWindowOutput && !m_NativeWindowHandle) return;

    std::string prevPath = m_CurrentPath;
    bool wasPlaying      = m_MpvIsPlaying.load(std::memory_order_relaxed);
    int64_t prevTime     = GetTime();
    bool prevMuted       = m_Muted.load(std::memory_order_relaxed);
    void* prevHandle     = m_NativeWindowHandle;

    DestroyMPV();
    m_NativeWindowHandle = prevHandle;
    InitMPV();

    if (!prevPath.empty()) {
        m_CurrentPath.clear();
        if (wasPlaying) {
            Play(prevPath, false, prevMuted);
            if (prevTime > 0 && m_Mpv) {
                double sec = static_cast<double>(prevTime) / 1000.0;
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.3f", sec);
                const char* cmd[] = { "seek", buf, "absolute", nullptr };
                mpv_command_async(m_Mpv, 0, cmd);
            }
        }
    }
}

}
