#include "MusicPlayer.h"
#include "media/audio/WindowsAudioSession.h"

#include <vlc/vlc.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

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
    // Todo el arranque de libVLC (carga de plugins, ~cientos de ms) ocurre en
    // el hilo de trabajo para no retrasar el inicio de la aplicacion.
    m_Worker = std::thread([this]() { WorkerLoop(); });
}

MusicPlayer::~MusicPlayer()
{
    {
        std::lock_guard<std::mutex> lk(m_CmdMutex);
        m_Quit = true;
    }
    m_CmdCv.notify_all();
    if (m_Worker.joinable())
        m_Worker.join();

#ifdef _WIN32
    if (m_MeterPtr) {
        static_cast<IAudioMeterInformation*>(m_MeterPtr)->Release();
        m_MeterPtr = nullptr;
    }
    if (m_ComInitialized)
        CoUninitialize();
#endif
}

void MusicPlayer::WorkerLoop()
{
    const char* args[] = {
        "--no-video",
        "--no-xlib",
        "--quiet",
        "--no-osd",
        "--no-video-title-show",
        "--file-caching=300",
        "--app-name=ProyecThor",
        "--app-id=ProyecThor",
        "--no-mmdevice-volume-save",
    };
    m_Instance = libvlc_new(static_cast<int>(sizeof(args) / sizeof(args[0])), args);
    if (!m_Instance)
        m_Instance = libvlc_new(0, nullptr);
    if (!m_Instance) {
        std::cerr << "[MusicPlayer] No se pudo crear la instancia de libVLC.\n";
    } else {
        libvlc_media_player_t* mp = libvlc_media_player_new(m_Instance);
        if (mp) {
            libvlc_media_player_set_role(mp, libvlc_role_Music);
            libvlc_event_manager_t* em = libvlc_media_player_event_manager(mp);
            libvlc_event_attach(em, libvlc_MediaPlayerEndReached,       &MusicPlayer::OnVlcEvent, this);
            libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &MusicPlayer::OnVlcEvent, this);
            libvlc_event_attach(em, libvlc_MediaPlayerPlaying,          &MusicPlayer::OnVlcEvent, this);
            m_Mp.store(mp);
            ApplyVolume();
            ApplyEqualizer();
        }
    }

    std::unique_lock<std::mutex> lk(m_CmdMutex);
    while (true) {
        m_CmdCv.wait(lk, [this]() { return m_Quit || m_PendingCmd.type != Cmd::None; });
        if (m_Quit) break;

        Cmd cmd = std::move(m_PendingCmd);
        m_PendingCmd = Cmd{};
        lk.unlock();

        if (cmd.type == Cmd::PlayPath)          ExecutePlay(cmd);
        else if (cmd.type == Cmd::StopPlayback) ExecuteStop();

        lk.lock();
    }
    lk.unlock();

    if (libvlc_media_player_t* mp = m_Mp.exchange(nullptr)) {
        libvlc_media_player_stop(mp);
        libvlc_media_player_release(mp);
    }
    if (m_Instance) {
        libvlc_release(m_Instance);
        m_Instance = nullptr;
    }
}

void MusicPlayer::SubmitCmd(Cmd&& cmd)
{
    cmd.gen = ++m_ReqGen;
    {
        std::lock_guard<std::mutex> lk(m_CmdMutex);
        m_PendingCmd = std::move(cmd);   // el ultimo comando reemplaza al pendiente
    }
    m_CmdCv.notify_one();
}

void MusicPlayer::Play(const std::string& utf8Path)
{
    if (utf8Path.empty()) return;
    m_EndReached.store(false, std::memory_order_relaxed);
    m_HadError.store(false, std::memory_order_relaxed);
    m_Paused.store(false, std::memory_order_relaxed);
    m_PendingSeek.store(-1.0f, std::memory_order_relaxed);

    Cmd c;
    c.type = Cmd::PlayPath;
    c.path = utf8Path;
    SubmitCmd(std::move(c));
}

void MusicPlayer::Stop()
{
    m_EndReached.store(false, std::memory_order_relaxed);
    m_Paused.store(false, std::memory_order_relaxed);

    Cmd c;
    c.type = Cmd::StopPlayback;
    SubmitCmd(std::move(c));
}

void MusicPlayer::ExecutePlay(const Cmd& cmd)
{
    libvlc_media_player_t* mp = m_Mp.load();
    if (!mp || !m_Instance) {
        m_HadError.store(true, std::memory_order_relaxed);
        m_EndReached.store(true, std::memory_order_relaxed);
        return;
    }
    if (cmd.gen != m_ReqGen.load()) return;   // ya hay una orden mas nueva

    m_LiveGen.store(0);
    m_ApplyPending.store(false, std::memory_order_relaxed);
    libvlc_media_player_stop(mp);             // bloquea solo a este hilo

    libvlc_media_t* media = nullptr;
    if (cmd.path.rfind("http", 0) == 0 || cmd.path.rfind("rtsp", 0) == 0)
        media = libvlc_media_new_location(m_Instance, cmd.path.c_str());
    else
        media = libvlc_media_new_path(m_Instance, ResolveLocalPath(cmd.path).c_str());

    if (!media) {
        std::cerr << "[MusicPlayer] No se pudo abrir: " << cmd.path << "\n";
        m_LiveGen.store(cmd.gen);
        m_HadError.store(true, std::memory_order_relaxed);
        m_EndReached.store(true, std::memory_order_relaxed);
        return;
    }

    libvlc_media_add_option(media, ":no-video");
    libvlc_media_add_option(media, ":avcodec-hw=none");
    // Una pausa pedida antes de que la pista arranque no se pierde.
    if (m_Paused.load(std::memory_order_relaxed))
        libvlc_media_add_option(media, ":start-paused");

    m_EndReached.store(false, std::memory_order_relaxed);
    m_HadError.store(false, std::memory_order_relaxed);

    // Se marca antes de play(): si el archivo falla de inmediato el evento de
    // error ya cuenta como de esta pista.
    m_LiveGen.store(cmd.gen);

    libvlc_media_player_set_media(mp, media);
    libvlc_media_release(media);
    libvlc_media_player_play(mp);
    ApplyVolume();
#ifdef _WIN32
    ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
}

void MusicPlayer::ExecuteStop()
{
    libvlc_media_player_t* mp = m_Mp.load();
    m_LiveGen.store(0);
    if (!mp) return;
    libvlc_media_player_stop(mp);
    libvlc_media_player_set_media(mp, nullptr);
}

void MusicPlayer::OnVlcEvent(const libvlc_event_t* evt, void* userData)
{
    auto* self = static_cast<MusicPlayer*>(userData);
    if (!self) return;

    switch (evt->type) {
    case libvlc_MediaPlayerEndReached:
    case libvlc_MediaPlayerEncounteredError:
    {
        if (self->m_LiveGen.load() == 0 || self->m_LiveGen.load() != self->m_ReqGen.load())
            return;   // evento de una pista que ya fue reemplazada
        if (evt->type == libvlc_MediaPlayerEncounteredError) {
            const char* msg = libvlc_errmsg();
            std::cerr << "[MusicPlayer] Error de libVLC: " << (msg ? msg : "desconocido") << "\n";
            self->m_HadError.store(true, std::memory_order_relaxed);
        }
        self->m_EndReached.store(true, std::memory_order_relaxed);
        break;
    }
    case libvlc_MediaPlayerPlaying:
        // La salida de audio recien existe aqui. No se llama a libVLC desde su
        // propio hilo de eventos (riesgo de bloqueo): se marca y el estado
        // (volumen / mute / pausa / seek pendientes) se reaplica en el siguiente
        // sondeo de la UI (GetTime).
        self->m_ApplyPending.store(true, std::memory_order_release);
        break;
    default:
        break;
    }
}

void MusicPlayer::SetPause(bool paused)
{
    m_Paused.store(paused, std::memory_order_relaxed);
    if (libvlc_media_player_t* mp = m_Mp.load())
        libvlc_media_player_set_pause(mp, paused ? 1 : 0);
}

void MusicPlayer::SetPosition(float normalized)
{
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    libvlc_media_player_t* mp = m_Mp.load();
    if (!mp) return;
    const libvlc_state_t st = libvlc_media_player_get_state(mp);
    if (st == libvlc_Playing || st == libvlc_Paused) {
        libvlc_media_player_set_position(mp, normalized);
    } else {
        // Aun abriendo: libVLC ignoraria el seek. Se aplica al empezar a sonar.
        m_PendingSeek.store(normalized, std::memory_order_relaxed);
        m_ApplyPending.store(true, std::memory_order_release);
    }
}

int64_t MusicPlayer::GetTime() const
{
    libvlc_media_player_t* mp = m_Mp.load();
    if (!mp) return 0;
    if (m_ApplyPending.exchange(false, std::memory_order_acq_rel)) {
        const_cast<MusicPlayer*>(this)->ApplyVolume();
        const float seek = m_PendingSeek.exchange(-1.0f, std::memory_order_relaxed);
        if (seek >= 0.0f)
            libvlc_media_player_set_position(mp, seek);
        if (m_Paused.load(std::memory_order_relaxed) && libvlc_media_player_is_playing(mp))
            libvlc_media_player_set_pause(mp, 1);
    }
    return std::max<int64_t>(0, libvlc_media_player_get_time(mp));
}

int64_t MusicPlayer::GetLength() const
{
    libvlc_media_player_t* mp = m_Mp.load();
    return mp ? std::max<int64_t>(0, libvlc_media_player_get_length(mp)) : 0;
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
    libvlc_media_player_t* mp = m_Mp.load();
    if (!mp) return;

    const bool muted = m_Muted.load(std::memory_order_relaxed);
    if (muted)
    {
        // Silenciar a nivel interno de VLC sin mutear la sesión de Windows
        libvlc_audio_set_track(mp, -1);
    }
    else
    {
        libvlc_audio_set_mute(mp, 0);
        if (libvlc_audio_get_track(mp) == -1)
        {
            libvlc_audio_set_track(mp, 1);
        }
        libvlc_audio_set_volume(mp, m_Volume.load(std::memory_order_relaxed));
#ifdef _WIN32
        ProyecThor::Audio::EnsureProcessAudioUnmutedAndMaxVolume();
#endif
    }
}

void MusicPlayer::ApplyEqualizer()
{
    libvlc_media_player_t* mp = m_Mp.load();
    if (!mp) return;

    std::lock_guard<std::mutex> lk(m_EqMutex);
    if (!m_EqEnabled) {
        libvlc_media_player_set_equalizer(mp, nullptr);
        return;
    }
    libvlc_equalizer_t* eq = libvlc_audio_equalizer_new();
    if (!eq) return;
    libvlc_audio_equalizer_set_preamp(eq, m_EqPreamp);
    for (int b = 0; b < kEqualizerBands; ++b)
        libvlc_audio_equalizer_set_amp_at_index(eq, m_EqBands[b], static_cast<unsigned>(b));
    libvlc_media_player_set_equalizer(mp, eq);
    libvlc_audio_equalizer_release(eq);
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

    libvlc_media_player_t* mp = m_Mp.load();
    const bool active = mp && !m_Paused.load(std::memory_order_relaxed) &&
                        libvlc_media_player_is_playing(mp) != 0 &&
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
        // El dispositivo cambio / dejo de ser valido: reacquirir en el proximo ciclo.
        meter->Release();
        m_MeterPtr = nullptr;
    }
#endif

    // Sin medidor: ligera animacion para que el visualizador no quede muerto.
    const float t = static_cast<float>(NowSeconds());
    const float v = 0.35f + 0.25f * std::sin(t * 7.3f) + 0.15f * std::sin(t * 13.1f + 1.7f);
    m_LastL = m_LastR = std::clamp(v, 0.05f, 1.0f);
    left = right = m_LastL;
}

}
