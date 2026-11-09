#include "VLCBasePlayer.h"
#include "core/plugins/PluginManager.h"
#include "core/settings/SettingsManager.h"
#include "media/audio/WindowsAudioSession.h"

#ifdef _WIN32
#include <basetsd.h>
#include <windows.h>
#endif

#include <vlc/vlc.h>
#include <GL/glew.h>
#include <iostream>
#include <cstring>
#include <mutex>
#include <array>
#include <memory>
#include <cstdio>
#include <cmath>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace {

std::atomic<int> g_NextVlcInstanceId{0};

std::string GetDirectYoutubeURL(const std::string& youtubeURL)
{
    return ProyecThor::Core::PluginManager::Get().ResolveStreamUrl(youtubeURL);
}

std::string ResolveLocalPathForVLC(const std::string& path)
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

#ifdef _WIN32

// Medidor de picos de las sesiones de audio de ESTE proceso (WASAPI). VLC
// reproduce por su salida nativa (mmdevice); el medidor solo observa, no toca
// el flujo de audio, asi que no puede causar cortes ni desfases.
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
        // COM por hilo (idempotente). Se deja inicializado: el hilo de UI ya lo usa.
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

struct VLCVideoCtx {
    std::mutex mutex;
    libvlc_media_player_t* mp = nullptr;
    void*    frontBuf = nullptr;
    void*    backBuf  = nullptr;
    unsigned width  = 0;
    unsigned height = 0;
    unsigned pitch  = 0;
    bool     dirty  = false;

    bool     everHadFrame = false;
};

static unsigned vlc_format(void** opaque, char* chroma, unsigned* width, unsigned* height,
                            unsigned* pitches, unsigned* lines)
{
    auto* ctx = static_cast<VLCVideoCtx*>(*opaque);
    std::lock_guard<std::mutex> lock(ctx->mutex);

#ifdef _WIN32
    std::memcpy(chroma, "RGBA", 4);
#else
    std::memcpy(chroma, "RV32", 4);
#endif

    // En Modo Rendimiento o Baja Memoria, limitamos la resolución de salida del
    // callback vmem para que libVLC escale directamente en su decodificador/swscale
    // interno, reduciendo de 2.25x a 20x la memoria RAM de frontBuf/backBuf y el
    // ancho de banda de subida a textura OpenGL por fotograma.
    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    unsigned maxW = 0, maxH = 0;
    if (perf.lowMemoryMode) {
        maxW = 854u;
        maxH = 480u;
    } else if (perf.performanceMode) {
        maxW = 1280u;
        maxH = 720u;
    }
    if (maxW > 0 && maxH > 0 && *width > 0 && *height > 0 && (*width > maxW || *height > maxH)) {
        const double sx = static_cast<double>(maxW) / static_cast<double>(*width);
        const double sy = static_cast<double>(maxH) / static_cast<double>(*height);
        const double scale = std::min(sx, sy);
        *width  = std::max(16u, (static_cast<unsigned>(*width  * scale + 0.5) + 1u) & ~1u);
        *height = std::max(16u, (static_cast<unsigned>(*height * scale + 0.5) + 1u) & ~1u);
    }

    const unsigned alignedW = (*width  + 7u)  & ~7u;
    const unsigned alignedH = (*height + 31u) & ~31u;

    ctx->width  = *width;
    ctx->height = *height;
    ctx->pitch  = alignedW * 4;
    *pitches    = ctx->pitch;
    *lines      = alignedH;

    size_t sz = static_cast<size_t>(ctx->pitch) * alignedH;
    delete[] static_cast<uint8_t*>(ctx->frontBuf);
    delete[] static_cast<uint8_t*>(ctx->backBuf);
    ctx->frontBuf = new uint8_t[sz];
    ctx->backBuf  = new uint8_t[sz];
    std::memset(ctx->frontBuf, 0, sz);
    std::memset(ctx->backBuf,  0, sz);
    ctx->dirty        = false;
    ctx->everHadFrame = false;
    return 1;
}

static void vlc_cleanup(void* opaque)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    if (!ctx) return;
    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    if (perf.lowMemoryMode || perf.performanceMode)
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        delete[] static_cast<uint8_t*>(ctx->frontBuf);
        delete[] static_cast<uint8_t*>(ctx->backBuf);
        ctx->frontBuf     = nullptr;
        ctx->backBuf      = nullptr;
        ctx->width        = 0;
        ctx->height       = 0;
        ctx->pitch        = 0;
        ctx->dirty        = false;
        ctx->everHadFrame = false;
    }
}

static void* vlc_lock(void* opaque, void** planes)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    ctx->mutex.lock();
    if (!ctx->backBuf)
    {
        const unsigned alignedH = (ctx->height + 31u) & ~31u;
        const size_t sz = std::max<size_t>(64u * 64u * 4u, static_cast<size_t>(ctx->pitch) * alignedH);
        ctx->frontBuf = new uint8_t[sz]();
        ctx->backBuf  = new uint8_t[sz]();
    }
    *planes = ctx->backBuf;
    return nullptr;
}

static void vlc_unlock(void* opaque, void* , void* const* )
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    std::swap(ctx->frontBuf, ctx->backBuf);
    ctx->everHadFrame = true;
    ctx->dirty        = true;
    ctx->mutex.unlock();
}

static void vlc_display(void* opaque, void* )
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->dirty = true;
}

}

namespace ProyecThor::Core {

std::string VLCBasePlayer::s_HwDecoder   = "any";
std::string VLCBasePlayer::s_VideoOutput = "auto";
std::string VLCBasePlayer::s_Deinterlace = "discard";

void VLCBasePlayer::SetDefaultHwDecoder(const std::string& dec)
{
    s_HwDecoder = dec.empty() ? "any" : dec;
}

std::string VLCBasePlayer::GetDefaultHwDecoder()
{
    return s_HwDecoder;
}

void VLCBasePlayer::SetDefaultVideoOutput(const std::string& vout)
{
    s_VideoOutput = vout.empty() ? "auto" : vout;
}

std::string VLCBasePlayer::GetDefaultVideoOutput()
{
    return s_VideoOutput;
}

void VLCBasePlayer::SetDefaultDeinterlace(const std::string& deint)
{
    s_Deinterlace = deint.empty() ? "discard" : deint;
}

std::string VLCBasePlayer::GetDefaultDeinterlace()
{
    return s_Deinterlace;
}

void VLCBasePlayer::ApplyDeinterlace(const std::string& mode)
{
    if (!m_MediaPlayer) return;
    if (mode.empty() || mode == "discard" || mode == "none") {
        libvlc_video_set_deinterlace(m_MediaPlayer, nullptr);
    } else {
        libvlc_video_set_deinterlace(m_MediaPlayer, mode.c_str());
    }
}

VLCBasePlayer::VLCBasePlayer(int decodeThreads, bool useHardwareDecode, bool forceSilent,
                             bool nativeWindowOutput)
    : m_DecodeThreads(decodeThreads)
    , m_UseHardwareDecode(useHardwareDecode)
    , m_NativeWindowOutput(nativeWindowOutput)
{
    m_InstanceId = g_NextVlcInstanceId.fetch_add(1, std::memory_order_relaxed);
    m_ForceSilent.store(forceSilent, std::memory_order_relaxed);

    std::cerr << "[VLC#" << m_InstanceId << "] Construido. forceSilent="
              << (forceSilent ? "true" : "false") << "\n";

    InitVLC();
    CreatePersistentPlayer();
}

VLCBasePlayer::~VLCBasePlayer()
{
    DestroyVLC();
    if (m_TextureID)
    {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
}

void VLCBasePlayer::InitVLC()
{
    const bool forceSilent = m_ForceSilent.load(std::memory_order_relaxed);
    const bool hasCustomNativeVout = m_NativeWindowOutput &&
                                     !s_VideoOutput.empty() &&
                                     s_VideoOutput != "auto" &&
                                     s_VideoOutput != "any";

    // Reutilizamos una única instancia base de libVLC (una con audio y otra muda)
    // mediante libvlc_retain, evitando duplicar el banco completo de plugins y
    // estructuras internas de libVLC (~25-35 MB de RAM por instancia extra).
    static std::mutex         s_SharedVlcMutex;
    static libvlc_instance_t* s_SharedNormalInstance = nullptr;
    static libvlc_instance_t* s_SharedSilentInstance = nullptr;

    auto CreateInstanceWithArgs = [](bool silent, const std::string& customVout) -> libvlc_instance_t* {
        std::vector<std::string> argStorage;
        argStorage.push_back("--no-xlib");
        argStorage.push_back("--quiet");
        argStorage.push_back("--no-osd");
        argStorage.push_back("--no-video-title-show");
        argStorage.push_back("--file-caching=250");
        argStorage.push_back("--network-caching=800");
        argStorage.push_back("--app-name=ProyecThor");
        argStorage.push_back("--app-id=ProyecThor");
        argStorage.push_back("--no-mmdevice-volume-save");
        argStorage.push_back("--no-stats");
        argStorage.push_back("--no-sub-autodetect-file");

        if (silent) {
            argStorage.push_back("--no-audio");
        }
        if (!customVout.empty()) {
            argStorage.push_back("--vout=" + customVout);
        }

        std::vector<const char*> args;
        args.reserve(argStorage.size());
        for (const auto& a : argStorage) {
            args.push_back(a.c_str());
        }

        libvlc_instance_t* inst = libvlc_new(static_cast<int>(args.size()), args.data());
        if (!inst) {
            const char* fallbackArgs[] = {
                "--no-xlib",
                "--quiet",
                "--no-osd",
                "--no-video-title-show",
                "--file-caching=250",
                "--network-caching=800",
            };
            inst = libvlc_new(sizeof(fallbackArgs) / sizeof(fallbackArgs[0]), fallbackArgs);
        }
        return inst;
    };

    if (!hasCustomNativeVout) {
        std::lock_guard<std::mutex> lk(s_SharedVlcMutex);
        libvlc_instance_t*& sharedSlot = forceSilent ? s_SharedSilentInstance : s_SharedNormalInstance;
        if (!sharedSlot) {
            sharedSlot = CreateInstanceWithArgs(forceSilent, "");
        }
        if (sharedSlot) {
            libvlc_retain(sharedSlot);
            m_Instance = sharedSlot;
        }
    } else {
        m_Instance = CreateInstanceWithArgs(forceSilent, s_VideoOutput);
    }

    if (!m_Instance)
        std::cerr << "[VLC] Error al crear instancia libVLC.\n";
}

void VLCBasePlayer::OnVlcEvent(const libvlc_event_t* evt, void* userData)
{

    auto* self = static_cast<VLCBasePlayer*>(userData);
    if (evt->type == libvlc_MediaPlayerEndReached ||
        evt->type == libvlc_MediaPlayerEncounteredError)
    {
        if (evt->type == libvlc_MediaPlayerEncounteredError) {
            self->m_HadError.store(true, std::memory_order_relaxed);
            self->m_LoadHasError.store(true, std::memory_order_relaxed);
            const char* msg = libvlc_errmsg();
            std::cerr << "[VLC#" << self->m_InstanceId << "] Error reportado por libVLC: "
                      << (msg ? msg : "desconocido") << "\n";
            libvlc_clearerr();
        }
        self->m_EndReached.store(true, std::memory_order_relaxed);
    }
    else if (evt->type == libvlc_MediaPlayerPlaying)
    {
        self->m_VlcIsPlaying.store(true, std::memory_order_relaxed);
        self->m_AudioNeedsSync.store(true, std::memory_order_relaxed);
    }
}

void VLCBasePlayer::CreatePersistentPlayer()
{
    if (!m_Instance) return;

    m_MediaPlayer = libvlc_media_player_new(m_Instance);
    if (!m_MediaPlayer)
    {
        std::cerr << "[VLC] No se pudo crear el reproductor.\n";
        return;
    }

    if (!m_NativeWindowOutput)
    {
        auto* vCtx = new VLCVideoCtx();
        vCtx->mp   = m_MediaPlayer;
        m_VideoCtx = vCtx;
        libvlc_video_set_format_callbacks(m_MediaPlayer, vlc_format, vlc_cleanup);
        libvlc_video_set_callbacks(m_MediaPlayer, vlc_lock, vlc_unlock, vlc_display, vCtx);
    }

    // Audio: salida NATIVA de VLC (mmdevice/WASAPI en Windows). VLC gestiona el
    // reloj de audio, el remuestreo y la sincronia A/V por si mismo; antes el audio
    // se desviaba por callbacks propios a waveOut (44.1 kHz fijo, buffers a mano),
    // causa de los chasquidos, desfases y cuelgues.
    ApplyAudioState();

    libvlc_event_manager_t* em = libvlc_media_player_event_manager(m_MediaPlayer);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached,       &VLCBasePlayer::OnVlcEvent, this);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &VLCBasePlayer::OnVlcEvent, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPlaying,          &VLCBasePlayer::OnVlcEvent, this);

    ApplyDeinterlace(s_Deinterlace);
}

void VLCBasePlayer::DestroyVLC()
{

    Stop();

    if (m_MediaPlayer)
    {

        libvlc_media_player_release(m_MediaPlayer);
        m_MediaPlayer = nullptr;
    }
    if (m_VideoCtx)
    {
        auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
        delete[] static_cast<uint8_t*>(ctx->frontBuf);
        delete[] static_cast<uint8_t*>(ctx->backBuf);
        delete ctx;
        m_VideoCtx = nullptr;
    }
    if (m_Instance)
    {
        libvlc_release(m_Instance);
        m_Instance = nullptr;
    }
}

void VLCBasePlayer::EnsureTexture(int w, int h)
{
    if (m_TextureID && m_VideoW == w && m_VideoH == h) return;
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);

    glBindTexture(GL_TEXTURE_2D, 0);

    m_VideoW = w;
    m_VideoH = h;
}

void VLCBasePlayer::Play(const std::string& path, bool loop, bool startMuted)
{
    if (!m_Instance || !m_MediaPlayer) return;

    if (m_NativeWindowOutput && !m_NativeWindowHandle)
    {
        std::cerr << "[VLC#" << m_InstanceId << "] Play() omitido: reproductor nativo sin ventana asignada (evita ventana externa de VLC).\n";
        return;
    }

    if (m_PathBlocked && NormalizePathForCompare(m_BlockedPath) == NormalizePathForCompare(path))
    {
        std::cerr << "[VLC] Play() ignorado, ruta bloqueada: " << path << "\n";
        return;
    }

    if (!path.empty() && path == m_CurrentPath)
    {
        std::cerr << "[VLC#" << m_InstanceId << "] Play() ignorado, reentrante para ruta ya activa/cargando: "
                  << path << "\n";
        return;
    }
    m_CurrentPath = path;

    std::cerr << "[VLC#" << m_InstanceId << "] Play() path=" << path
              << " startMuted=" << (startMuted ? "true" : "false")
              << " forceSilent=" << (m_ForceSilent.load(std::memory_order_relaxed) ? "true" : "false")
              << " audioActive=" << (m_AudioActive.load(std::memory_order_relaxed) ? "true" : "false")
              << "\n";

    uint64_t myGen = ++m_LoadGeneration;
    m_HasEverPlayed.store(true, std::memory_order_relaxed);

    if (startMuted)
        SetMute(true);

    LoadAndPlay(path, loop, startMuted, myGen);
}

void VLCBasePlayer::BlockPath(const std::string& path)
{
    m_BlockedPath  = path;
    m_PathBlocked  = true;
}

void VLCBasePlayer::UnblockPath()
{
    m_PathBlocked = false;
    m_BlockedPath.clear();
}

void VLCBasePlayer::LoadAndPlay(const std::string& path, bool loop, bool , uint64_t myGeneration)
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
    {
        return;
    }

    if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        return;

    libvlc_media_t* media = nullptr;
    if (finalPath.rfind("http", 0) == 0 || finalPath.rfind("rtsp", 0) == 0) {
        media = libvlc_media_new_location(m_Instance, finalPath.c_str());
    } else {
        std::string vlcUtf8Path = ResolveLocalPathForVLC(finalPath);
        media = libvlc_media_new_path(m_Instance, vlcUtf8Path.c_str());
    }

    if (!media)
    {
        std::cerr << "[VLC] No se pudo abrir: " << finalPath << "\n";
        return;
    }

    if (loop)
        libvlc_media_add_option(media, "input-repeat=65535");

    if (m_ForceSilent.load(std::memory_order_relaxed))
        libvlc_media_add_option(media, ":no-audio");

    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;

    std::string hw = s_HwDecoder.empty() ? "any" : s_HwDecoder;
    if (!m_UseHardwareDecode || hw == "none") {
        hw = "none";
    }
#ifndef _WIN32
    if (!m_NativeWindowOutput) {
        hw = "none";
    }
#endif
    std::string hwOpt = ":avcodec-hw=" + hw;
    libvlc_media_add_option(media, hwOpt.c_str());

    int effectiveThreads = m_DecodeThreads;
    if (perf.lowMemoryMode) {
        effectiveThreads = (m_DecodeThreads == 1) ? 1 : 2;
        libvlc_media_add_option(media, ":file-caching=120");
        libvlc_media_add_option(media, ":network-caching=400");
        libvlc_media_add_option(media, ":avcodec-fast");
        libvlc_media_add_option(media, ":avcodec-skiploopfilter=nonref");
    } else if (perf.performanceMode) {
        if (effectiveThreads <= 0 || effectiveThreads > 2) effectiveThreads = 2;
        libvlc_media_add_option(media, ":file-caching=180");
        libvlc_media_add_option(media, ":network-caching=600");
        libvlc_media_add_option(media, ":avcodec-fast");
    }

    if (effectiveThreads > 0) {
        std::string thrOpt = ":avcodec-threads=" + std::to_string(effectiveThreads);
        libvlc_media_add_option(media, thrOpt.c_str());
    }

    if (m_NativeWindowOutput && !s_VideoOutput.empty() && s_VideoOutput != "auto" && s_VideoOutput != "any") {
        std::string voutOpt = ":vout=" + s_VideoOutput;
        libvlc_media_add_option(media, voutOpt.c_str());
    }

    if (!s_Deinterlace.empty() && s_Deinterlace != "discard" && s_Deinterlace != "none") {
        libvlc_media_add_option(media, ":deinterlace=1");
        std::string deintMode = ":deinterlace-mode=" + s_Deinterlace;
        libvlc_media_add_option(media, deintMode.c_str());
    } else {
        libvlc_media_add_option(media, ":deinterlace=0");
    }

    {
        std::lock_guard<std::mutex> lock(m_MediaSwapMutex);

        if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        {
            libvlc_media_release(media);
            return;
        }

        libvlc_media_player_stop(m_MediaPlayer);
        m_EndReached.store(false, std::memory_order_relaxed);
        m_HadError.store(false, std::memory_order_relaxed);
        m_LoadHasError.store(false, std::memory_order_relaxed);
        m_VlcIsPlaying.store(false, std::memory_order_relaxed);

        if (m_VideoCtx)
        {
            auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
            std::lock_guard<std::mutex> ctxLock(ctx->mutex);
            ctx->everHadFrame = false;
            ctx->dirty        = false;
        }

        libvlc_media_player_set_media(m_MediaPlayer, media);
        // Mute/volumen ANTES de arrancar: asi un reproductor en espera (Standby,
        // preview, fondo sin audio) nunca deja escapar un instante de sonido.
        ApplyAudioState();
        if (!m_AudioDeviceId.empty() && m_AudioDeviceId != "default" &&
            !std::isdigit(static_cast<unsigned char>(m_AudioDeviceId[0])))
            libvlc_audio_output_device_set(m_MediaPlayer, nullptr, m_AudioDeviceId.c_str());
        libvlc_media_player_play(m_MediaPlayer);
        ApplyDeinterlace(s_Deinterlace);
        m_Paused.store(false, std::memory_order_relaxed);
        m_SilenceEnforced.store(false, std::memory_order_relaxed);
        m_AudioNeedsSync.store(true, std::memory_order_relaxed);
    }

    libvlc_media_release(media);
}

void VLCBasePlayer::Stop()
{
    ++m_LoadGeneration;

    m_EndReached.store(false, std::memory_order_relaxed);
    m_VlcIsPlaying.store(false, std::memory_order_relaxed);
    m_CurrentPath.clear();

    std::lock_guard<std::mutex> lock(m_MediaSwapMutex);
    if (m_MediaPlayer)
    {
        libvlc_media_player_stop(m_MediaPlayer);
        libvlc_media_player_set_media(m_MediaPlayer, nullptr);
    }
}

void VLCBasePlayer::ReleaseIdleResources()
{
    if (m_VlcIsPlaying.load(std::memory_order_relaxed) || !m_CurrentPath.empty())
        return;

    if (m_VideoCtx)
    {
        auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
        std::lock_guard<std::mutex> ctxLock(ctx->mutex);
        delete[] static_cast<uint8_t*>(ctx->frontBuf);
        delete[] static_cast<uint8_t*>(ctx->backBuf);
        ctx->frontBuf     = nullptr;
        ctx->backBuf      = nullptr;
        ctx->width        = 0;
        ctx->height       = 0;
        ctx->pitch        = 0;
        ctx->dirty        = false;
        ctx->everHadFrame = false;
    }

    if (m_TextureID)
    {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
        m_VideoW    = 0;
        m_VideoH    = 0;
    }
}

bool VLCBasePlayer::ConsumeEndReached()
{
    return m_EndReached.exchange(false, std::memory_order_relaxed);
}

bool VLCBasePlayer::ConsumeHadError()
{
    return m_HadError.exchange(false, std::memory_order_relaxed);
}

void VLCBasePlayer::ApplyAudioState()
{
    if (!m_MediaPlayer) return;

    const bool silent = m_ForceSilent.load(std::memory_order_relaxed) ||
                        !m_AudioActive.load(std::memory_order_relaxed) ||
                        m_Muted.load(std::memory_order_relaxed);
    if (silent)
    {
        // Documentación oficial libVLC:
        // "To force silent playback, disable all audio tracks. This is more efficient and reliable than mute."
        // Al poner la pista en -1, VLC desactiva la salida de audio internamente
        // sin llamar a ISimpleAudioVolume::SetMute(TRUE), evitando que Windows
        // mutee la sesión de toda la aplicación en el Mezclador de Windows.
        libvlc_audio_set_track(m_MediaPlayer, -1);
    }
    else
    {
        libvlc_audio_set_mute(m_MediaPlayer, 0);
        if (libvlc_audio_get_track(m_MediaPlayer) == -1)
        {
            libvlc_audio_set_track(m_MediaPlayer, 1);
        }
        int vol = static_cast<int>(m_VolumeMultiplier.load(std::memory_order_relaxed) * 100.0f + 0.5f);
        if (vol < 0) vol = 0;
        libvlc_audio_set_volume(m_MediaPlayer, vol);
#ifdef _WIN32
        ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
    }
}

void VLCBasePlayer::SetMute(bool mute)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    bool effectiveMute = mute || m_ForceSilent.load(std::memory_order_relaxed);
    bool prev = m_Muted.exchange(effectiveMute, std::memory_order_relaxed);
    if (prev == effectiveMute) return;

    std::cerr << "[VLC#" << m_InstanceId << "] SetMute(" << (mute ? "true" : "false")
              << ") -> effectiveMute=" << (effectiveMute ? "true" : "false") << "\n";

    ApplyAudioState();
}

void VLCBasePlayer::SetAudioActive(bool active)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    bool effectiveActive = active && !m_ForceSilent.load(std::memory_order_relaxed);
    bool prev = m_AudioActive.exchange(effectiveActive, std::memory_order_relaxed);
    if (prev == effectiveActive) return;

    std::cerr << "[VLC#" << m_InstanceId << "] SetAudioActive(" << (active ? "true" : "false")
              << ") -> effectiveActive=" << (effectiveActive ? "true" : "false") << "\n";

    ApplyAudioState();
}

void VLCBasePlayer::SetVolume(int volume)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    if (m_ForceSilent.load(std::memory_order_relaxed))
        volume = 0;

    float multiplier = static_cast<float>(volume) / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);

    ApplyAudioState();
}

void VLCBasePlayer::SetSoftwareVolume(float percent)
{
    m_SilenceEnforced.store(false, std::memory_order_relaxed);
    if (m_ForceSilent.load(std::memory_order_relaxed))
        percent = 0.0f;

    float multiplier = percent / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);

    ApplyAudioState();
}

void VLCBasePlayer::EnforceSilenceIfNeeded()
{
    if (!m_MediaPlayer) return;

    // La salida de audio de VLC solo existe una vez que el medio esta en
    // reproduccion (evento Playing): ahi se reaplican mute / volumen / dispositivo.
    // Tambien se reaplica si cambia el estado "silencio" deseado.
    const bool shouldBeSilent = m_ForceSilent.load(std::memory_order_relaxed) ||
                                !m_AudioActive.load(std::memory_order_relaxed);
    const bool needsSync = m_AudioNeedsSync.exchange(false, std::memory_order_relaxed);
    const bool silenceChanged = m_SilenceEnforced.exchange(shouldBeSilent, std::memory_order_relaxed) != shouldBeSilent;

    if (needsSync || silenceChanged)
    {
        ApplyAudioState();
        if (!shouldBeSilent && !m_AudioDeviceId.empty() && m_AudioDeviceId != "default" &&
            !std::isdigit(static_cast<unsigned char>(m_AudioDeviceId[0])))
            libvlc_audio_output_device_set(m_MediaPlayer, nullptr, m_AudioDeviceId.c_str());
    }
}
void VLCBasePlayer::ApplyEqualizer()
{
    if (!m_MediaPlayer) return;

    if (!m_EqEnabled) {
        libvlc_media_player_set_equalizer(m_MediaPlayer, nullptr);
        return;
    }

    libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
    if (!eq) return;
    libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
    for (int b = 0; b < kEqualizerBands; b++)
        libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b], static_cast<unsigned>(b));
    libvlc_media_player_set_equalizer(m_MediaPlayer, eq);
    libvlc_audio_equalizer_release(eq);
}

void VLCBasePlayer::SetEqualizerEnabled(bool enabled)
{
    m_EqEnabled = enabled;
    ApplyEqualizer();
}

void VLCBasePlayer::SetEqualizerPreamp(float preampDb)
{
    m_EqPreamp = preampDb;
    if (m_EqEnabled) ApplyEqualizer();
}

void VLCBasePlayer::SetEqualizerBand(int index, float ampDb)
{
    if (index < 0 || index >= kEqualizerBands) return;
    m_EqBands[index] = ampDb;
    if (m_EqEnabled) ApplyEqualizer();
}

void VLCBasePlayer::SetPause(bool paused)
{
    m_Paused.store(paused, std::memory_order_relaxed);
    if (m_MediaPlayer)
        libvlc_media_player_set_pause(m_MediaPlayer, paused ? 1 : 0);
}

void VLCBasePlayer::SetPosition(float pos)
{
    if (m_MediaPlayer)
        libvlc_media_player_set_position(m_MediaPlayer, pos);
}

float VLCBasePlayer::GetPosition() const
{
    return m_MediaPlayer ? libvlc_media_player_get_position(m_MediaPlayer) : 0.0f;
}

bool VLCBasePlayer::IsPlaying() const
{
    return m_MediaPlayer ? (libvlc_media_player_is_playing(m_MediaPlayer) != 0) : false;
}

int64_t VLCBasePlayer::GetTime() const
{
    return m_MediaPlayer ? libvlc_media_player_get_time(m_MediaPlayer) : 0;
}

int64_t VLCBasePlayer::GetLength() const
{
    return m_MediaPlayer ? libvlc_media_player_get_length(m_MediaPlayer) : -1;
}

void* VLCBasePlayer::GetTextureID()
{
    return m_TextureID
        ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureID))
        : nullptr;
}

void VLCBasePlayer::GetVideoSize(int& width, int& height)
{
    if (m_VideoCtx)
    {
        auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        width  = static_cast<int>(ctx->width);
        height = static_cast<int>(ctx->height);
    }
    else
    {
        width = height = 0;
    }
}

bool VLCBasePlayer::HasVideoFrame() const
{
    if (!m_VideoCtx) return false;
    auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
    std::lock_guard<std::mutex> lock(ctx->mutex);

    return ctx->everHadFrame && ctx->frontBuf != nullptr && ctx->width > 0 && ctx->height > 0;
}

VLCBasePlayer::LoadState VLCBasePlayer::GetLoadState() const
{
    if (!m_HasEverPlayed.load(std::memory_order_relaxed)) return LoadState::Idle;
    if (m_LoadHasError.load(std::memory_order_relaxed))   return LoadState::Error;

    bool ready = m_NativeWindowOutput
        ? m_VlcIsPlaying.load(std::memory_order_relaxed)
        : HasVideoFrame();
    if (ready) return LoadState::Ready;

    if (!m_VlcIsPlaying.load(std::memory_order_relaxed))  return LoadState::Opening;
    return LoadState::Buffering;
}

bool VLCBasePlayer::IsLoading() const
{
    LoadState s = GetLoadState();
    return s == LoadState::Opening || s == LoadState::Buffering;
}

bool VLCBasePlayer::UpdateTexture()
{
    if (!m_MediaPlayer || !m_VideoCtx) return false;
    EnforceSilenceIfNeeded();
    auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);

    std::lock_guard<std::mutex> lock(ctx->mutex);
    if (!ctx->dirty || !ctx->frontBuf || ctx->width == 0 || ctx->height == 0 || ctx->pitch == 0)
        return false;

    const unsigned w     = ctx->width;
    const unsigned h     = ctx->height;
    const unsigned pitch = ctx->pitch;
    ctx->dirty = false;

    EnsureTexture(static_cast<int>(w), static_cast<int>(h));
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(pitch / 4));
#ifdef _WIN32
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ctx->frontBuf);
#else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, ctx->frontBuf);
#endif
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

std::vector<VLCBasePlayer::AudioDevice> VLCBasePlayer::GetAvailableAudioDevices()
{
    std::vector<AudioDevice> devices;
    devices.push_back({ "default", "Dispositivo predeterminado del sistema" });
    if (!m_Instance) return devices;

#ifdef _WIN32
    const char* aout = "mmdevice";
#else
    const char* aout = nullptr;
#endif
    libvlc_audio_output_device_t* devList = aout
        ? libvlc_audio_output_device_list_get(m_Instance, aout)
        : (m_MediaPlayer ? libvlc_audio_output_device_enum(m_MediaPlayer) : nullptr);
    for (auto* p = devList; p != nullptr; p = p->p_next)
    {
        if (p->psz_device && p->psz_description && p->psz_device[0] != '\0')
            devices.push_back({ p->psz_device, p->psz_description });
    }
    if (devList)
        libvlc_audio_output_device_list_release(devList);

    return devices;
}

void VLCBasePlayer::SetAudioDevice(const std::string& deviceId)
{
    m_AudioDeviceId = deviceId;

    std::cerr << "[VLC#" << m_InstanceId << "] SetAudioDevice(\""
              << deviceId << "\")\n";

    // Ajustes antiguos guardaban el indice numerico de waveOut: ya no es valido
    // para VLC, se interpreta como "predeterminado".
    if (!m_MediaPlayer || deviceId.empty() || deviceId == "default" ||
        std::isdigit(static_cast<unsigned char>(deviceId[0])))
        return;
    libvlc_audio_output_device_set(m_MediaPlayer, nullptr, deviceId.c_str());
}

void VLCBasePlayer::GetAudioLevels(float& left, float& right)
{
    left = right = 0.0f;
#ifdef _WIN32
    if (ProyecThor::Settings::SettingsManager::Get().GetSettings().performance.performanceMode)
        return;
    const bool audible = m_MediaPlayer && !m_Paused.load(std::memory_order_relaxed) &&
                         !m_Muted.load(std::memory_order_relaxed) &&
                         m_AudioActive.load(std::memory_order_relaxed) &&
                         !m_ForceSilent.load(std::memory_order_relaxed) &&
                         libvlc_media_player_is_playing(m_MediaPlayer) != 0;
    if (!audible) return;
    ProcessAudioMeter::Get().Read(left, right);
#endif
}
void VLCBasePlayer::AttachNativeWindow(void* nativeHandle)
{
    if (m_NativeWindowHandle == nativeHandle) return;
    m_NativeWindowHandle = nativeHandle;
    if (!m_MediaPlayer || !nativeHandle) return;
#ifdef _WIN32
    libvlc_media_player_set_hwnd(m_MediaPlayer, nativeHandle);
#else
    libvlc_media_player_set_xwindow(m_MediaPlayer,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nativeHandle)));
#endif
}

void VLCBasePlayer::DetachNativeWindow()
{
    m_NativeWindowHandle = nullptr;
    if (!m_MediaPlayer) return;
#ifdef _WIN32
    libvlc_media_player_set_hwnd(m_MediaPlayer, nullptr);
#else
    libvlc_media_player_set_xwindow(m_MediaPlayer, 0);
#endif
}

void VLCBasePlayer::Reinit()
{
    if (m_NativeWindowOutput && !m_NativeWindowHandle) return;

    std::string prevPath = m_CurrentPath;
    bool wasPlaying = m_VlcIsPlaying.load(std::memory_order_relaxed);
    int64_t prevTime = GetTime();
    bool prevMuted = m_Muted.load(std::memory_order_relaxed);
    void* prevHandle = m_NativeWindowHandle;

    DestroyVLC();
    InitVLC();
    CreatePersistentPlayer();

    if (m_NativeWindowOutput && prevHandle) {
        AttachNativeWindow(prevHandle);
    }

    if (!prevPath.empty()) {
        m_CurrentPath.clear();
        if (wasPlaying) {
            Play(prevPath, false, prevMuted);
            if (prevTime > 0 && m_MediaPlayer) {
                libvlc_media_player_set_time(m_MediaPlayer, prevTime);
            }
        }
    }
}

}
