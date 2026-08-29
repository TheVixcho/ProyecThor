#include "VLCBasePlayer.h"

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
#include <future>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif
#pragma comment(lib, "winmm.lib")
#endif

namespace {

std::atomic<int> g_NextVlcInstanceId{0};

// popen()/_popen() bloquean hasta que el proceso hijo termina (o el pipe se
// cierra). yt-dlp puede tardar de mas o colgarse (red caida, proceso
// zombie) — sin limite de tiempo eso congela quien haya llamado a Play()
// (tipicamente el hilo principal de render). Corremos el trabajo bloqueante
// en un hilo aparte y esperamos con timeout (mismo patron que
// NetworkStreamServer::Start(), que ya usa promise/future + wait_for).
// Si expira, devolvemos vacio para que Play() falle de forma controlada en
// vez de colgar la app; el hilo detached simplemente termina en background
// y se descarta cuando yt-dlp finalice por su cuenta.
std::string GetDirectYoutubeURL(const std::string& youtubeURL)
{
    auto runYtDlp = [](const std::string& url) -> std::string {
#ifdef _WIN32
        std::string command = "yt-dlp.exe -f \"best[ext=mp4]/best\" -g --no-playlist \""
                            + url + "\"";
#else
        std::string command = "yt-dlp -f \"best[ext=mp4]/best\" -g --no-playlist \""
                            + url + "\"";
#endif
        std::array<char, 1024> buffer;
        std::string result;
#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(command.c_str(), "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
#endif
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            result += buffer.data();
        if (!result.empty() && result.back() == '\n') result.pop_back();
        if (!result.empty() && result.back() == '\r') result.pop_back();
        return result;
    };

    std::packaged_task<std::string()> task([runYtDlp, youtubeURL] { return runYtDlp(youtubeURL); });
    std::future<std::string> future = task.get_future();
    std::thread(std::move(task)).detach();

    constexpr auto kYtDlpTimeout = std::chrono::seconds(8);
    if (future.wait_for(kYtDlpTimeout) == std::future_status::ready)
        return future.get();

    std::cerr << "[VLC] Timeout resolviendo URL de YouTube (yt-dlp tardo mas de "
              << kYtDlpTimeout.count() << "s): se omite el clip.\n";
    return "";
}

// Normaliza una ruta para comparacion: pasa todo a minusculas y reemplaza
// backslashes por forward slashes. Se usa unicamente para decidir si una
// ruta solicitada en Play() coincide con la ruta bloqueada por BlockPath().
std::string NormalizePathForCompare(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

#ifdef _WIN32
// Actualiza un maximo atomico sin locks. Se usa para los picos de audio,
// leidos cada frame por el VU meter sin competir con el hilo de audio real.
// Solo se usa en Windows: es la unica plataforma donde interceptamos los
// samples crudos.
static inline void AtomicUpdateMax(std::atomic<float>& target, float value)
{
    float current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}
#endif

struct VLCAudioCtx {
    std::atomic<float> peakL{0.0f};
    std::atomic<float> peakR{0.0f};

    std::atomic<float>* volumeMultiplier = nullptr;
    std::atomic<bool>*  muted            = nullptr;
    std::atomic<bool>*  audioActive      = nullptr;
    std::atomic<bool>*  forceSilent      = nullptr;

#ifdef _WIN32
    HWAVEOUT hWaveOut = nullptr;
    static const int NUM_BUFFERS = 8;
    WAVEHDR waveHeaders[NUM_BUFFERS] = {};
    int currentHeader = 0;

    // true una vez que waveOutOpen tuvo exito. El dispositivo se abre al
    // arrancar el audio y se mantiene abierto mientras no cambie de
    // dispositivo (ver SetAudioDevice / OpenWaveOutDeviceLocked /
    // CloseWaveOutDeviceLocked).
    bool deviceInitialized = false;

    // Dispositivo WinMM deseado. WAVE_MAPPER = predeterminado del sistema.
    // Se puede cambiar en caliente via VLCBasePlayer::SetAudioDevice(),
    // que cierra y reabre el HWAVEOUT en el nuevo id.
    UINT_PTR deviceId = WAVE_MAPPER;

    // Protege apertura/cierre/reapertura de hWaveOut contra el callback
    // de audio (vlc_audio_play), que corre en un hilo interno de libVLC.
    std::mutex deviceMutex;
#endif
};

struct VLCVideoCtx {
    std::mutex mutex;
    void*    frontBuf = nullptr; // listo para subir a GL
    void*    backBuf  = nullptr; // lo escribe el decoder de VLC
    unsigned width  = 0;
    unsigned height = 0;
    bool     dirty  = false;

    // Distinto de "dirty": dirty es "hay un frame NUEVO sin subir todavia a
    // GL" y se consume (pasa a false) en cada UpdateTexture(). everHadFrame
    // es pegajoso — una vez que se decodifico el primer frame real, queda
    // en true hasta el proximo vlc_format() (nueva carga). HasVideoFrame()
    // debe reflejar "ya se vio al menos un frame alguna vez", no "hay uno
    // pendiente de subir ESTE instante" — mezclar ambas cosas hacia que
    // GetLoadState()==Ready practicamente nunca se observara true: el
    // mismo Update() que llamaba a UpdateTexture() (consumiendo dirty)
    // chequeaba Ready statement despues, viendo dirty ya en false.
    bool     everHadFrame = false;
};

#ifdef _WIN32

// Abre el dispositivo WinMM indicado con el formato fijo que usa este
// reproductor (PCM 16-bit, 44.1kHz, estereo) y prepara los buffers de
// multiple buffering. Debe llamarse con ctx->deviceMutex tomado.
static bool OpenWaveOutDeviceLocked(VLCAudioCtx* ctx, UINT_PTR deviceId)
{
    WAVEFORMATEX wfx       = {};
    wfx.wFormatTag         = WAVE_FORMAT_PCM;
    wfx.nChannels          = 2;
    wfx.nSamplesPerSec     = 44100;
    wfx.wBitsPerSample     = 16;
    wfx.nBlockAlign        = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec    = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (waveOutOpen(&ctx->hWaveOut, static_cast<UINT>(deviceId), &wfx, 0, 0, CALLBACK_NULL)
        != MMSYSERR_NOERROR)
    {
        ctx->hWaveOut = nullptr;
        return false;
    }

    for (int i = 0; i < VLCAudioCtx::NUM_BUFFERS; ++i)
    {
        ctx->waveHeaders[i] = {};
        ctx->waveHeaders[i].dwBufferLength = 4096 * 8;
        ctx->waveHeaders[i].lpData         = new char[ctx->waveHeaders[i].dwBufferLength];
        waveOutPrepareHeader(ctx->hWaveOut, &ctx->waveHeaders[i], sizeof(WAVEHDR));
    }
    ctx->currentHeader     = 0;
    ctx->deviceId          = deviceId;
    ctx->deviceInitialized = true;
    return true;
}

// Cierra el dispositivo WinMM actualmente abierto (si lo hay). Debe
// llamarse con ctx->deviceMutex tomado.
static void CloseWaveOutDeviceLocked(VLCAudioCtx* ctx)
{
    if (!ctx->hWaveOut)
    {
        ctx->deviceInitialized = false;
        return;
    }

    waveOutReset(ctx->hWaveOut);
    for (int i = 0; i < VLCAudioCtx::NUM_BUFFERS; ++i)
    {
        waveOutUnprepareHeader(ctx->hWaveOut, &ctx->waveHeaders[i], sizeof(WAVEHDR));
        delete[] ctx->waveHeaders[i].lpData;
        ctx->waveHeaders[i].lpData = nullptr;
    }
    waveOutClose(ctx->hWaveOut);
    ctx->hWaveOut          = nullptr;
    ctx->deviceInitialized = false;
}

static int vlc_audio_setup(void** opaque, char* format, unsigned* rate, unsigned* channels)
{
    auto* ctx = static_cast<VLCAudioCtx*>(*opaque);
    std::memcpy(format, "S16N", 4);
    *rate     = 44100;
    *channels = 2;

    std::lock_guard<std::mutex> lock(ctx->deviceMutex);
    if (ctx->deviceInitialized)
        return 0;

    if (!OpenWaveOutDeviceLocked(ctx, ctx->deviceId))
        std::cerr << "[Audio] Error al abrir la salida WinMM (deviceId="
                  << ctx->deviceId << ").\n";

    return 0;
}

static void vlc_audio_cleanup(void* opaque)
{
    auto* ctx = static_cast<VLCAudioCtx*>(opaque);
    std::lock_guard<std::mutex> lock(ctx->deviceMutex);
    if (ctx->hWaveOut)
        waveOutReset(ctx->hWaveOut);
}

static void vlc_audio_destroy_device(VLCAudioCtx* ctx)
{
    std::lock_guard<std::mutex> lock(ctx->deviceMutex);
    CloseWaveOutDeviceLocked(ctx);
}

static void vlc_audio_play(void* opaque, const void* samples, unsigned count, int64_t /*pts*/)
{
    auto* ctx = static_cast<VLCAudioCtx*>(opaque);

    if (ctx->audioActive && !ctx->audioActive->load(std::memory_order_relaxed))
        return;

    if (!ctx->volumeMultiplier || !ctx->muted) return;

    // El dispositivo puede estar cerrado momentaneamente si SetAudioDevice()
    // lo esta reabriendo desde el hilo de UI. En ese caso descartamos este
    // bloque de samples: preferible perder unos milisegundos de audio a
    // bloquear el hilo interno de audio de libVLC esperando el lock.
    std::unique_lock<std::mutex> devLock(ctx->deviceMutex, std::try_to_lock);
    if (!devLock.owns_lock() || !ctx->hWaveOut) return;

    // forceSilent manda por encima de cualquier otro estado: si este
    // player nacio silenciado (preview), el volumen efectivo es siempre 0,
    // sin importar lo que diga m_Muted/m_VolumeMultiplier.
    bool isForceSilent = ctx->forceSilent && ctx->forceSilent->load(std::memory_order_relaxed);
    bool isMuted        = isForceSilent || ctx->muted->load(std::memory_order_relaxed);
    float vol            = isMuted ? 0.0f : ctx->volumeMultiplier->load(std::memory_order_relaxed);

    const int16_t* pIn = static_cast<const int16_t*>(samples);
    float maxL = 0.0f;
    float maxR = 0.0f;

    WAVEHDR& hdr = ctx->waveHeaders[ctx->currentHeader];
    while (hdr.dwFlags & WHDR_INQUEUE)
        Sleep(1);

    int16_t* pOut = reinterpret_cast<int16_t*>(hdr.lpData);

    for (unsigned i = 0; i < count; ++i)
    {
        float sL = pIn[i * 2]     * vol;
        float sR = pIn[i * 2 + 1] * vol;

        if (sL >  32767.0f) sL =  32767.0f;
        else if (sL < -32768.0f) sL = -32768.0f;
        if (sR >  32767.0f) sR =  32767.0f;
        else if (sR < -32768.0f) sR = -32768.0f;

        pOut[i * 2]     = static_cast<int16_t>(sL);
        pOut[i * 2 + 1] = static_cast<int16_t>(sR);

        float nL = std::abs(sL) / 32768.0f;
        float nR = std::abs(sR) / 32768.0f;
        if (nL > maxL) maxL = nL;
        if (nR > maxR) maxR = nR;
    }

    AtomicUpdateMax(ctx->peakL, maxL);
    AtomicUpdateMax(ctx->peakR, maxR);

    hdr.dwBufferLength = count * 2 * sizeof(int16_t);
    waveOutWrite(ctx->hWaveOut, &hdr, sizeof(WAVEHDR));
    ctx->currentHeader = (ctx->currentHeader + 1) % VLCAudioCtx::NUM_BUFFERS;
}
#endif // _WIN32

static unsigned vlc_format(void** opaque, char* chroma, unsigned* width, unsigned* height,
                            unsigned* pitches, unsigned* lines)
{
    auto* ctx = static_cast<VLCVideoCtx*>(*opaque);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    std::memcpy(chroma, "RGBA", 4);
    ctx->width  = *width;
    ctx->height = *height;
    *pitches    = (*width) * 4;
    *lines      = *height;

    size_t sz = static_cast<size_t>(*pitches) * (*lines);
    delete[] static_cast<uint8_t*>(ctx->frontBuf);
    delete[] static_cast<uint8_t*>(ctx->backBuf);
    ctx->frontBuf = new uint8_t[sz];
    ctx->backBuf  = new uint8_t[sz];
    std::memset(ctx->frontBuf, 0, sz);
    std::memset(ctx->backBuf,  0, sz);
    ctx->dirty       = false;
    ctx->everHadFrame = false; // nueva carga: todavia no se decodifico nada
    return 1;
}

static void vlc_cleanup(void* /*opaque*/) {}

static void* vlc_lock(void* opaque, void** planes)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    ctx->mutex.lock();
    *planes = ctx->backBuf;
    return nullptr;
}

// FIX (desincronizacion audio/video en hardware lento): antes, esta
// funcion marcaba dirty=true apenas terminaba de DECODIFICAR un frame —
// segun la doc de libVLC (libvlc_media_player.h: libvlc_video_unlock_cb),
// unlock() se invoca "despues de decodificar, pero ANTES de mostrarse",
// sin ninguna relacion con el reloj de reproduccion. En hardware rapido el
// decode alcanza el ritmo real y "de casualidad" se veia bien, pero en
// hardware lento (Pentium dual-core ~2GHz reportado por usuarios) el
// decode se atrasa y, como no habia ningun mecanismo de correccion, el
// video quedaba cada vez mas atras del audio (que sigue su propio reloj
// real) sin recuperarse nunca. El swap de buffers sigue haciendose aca
// (necesario: deja backBuf libre para el proximo decode sin pisar el
// frame recien terminado) y everHadFrame tambien (lo sigue necesitando
// HasVideoFrame()/GetLoadState() para detectar "ya se decodifico algo",
// sin depender de cuando el reloj decida mostrarlo) — lo unico que se
// saca de aca es el flag "dirty", que ahora se marca en vlc_display().
static void vlc_unlock(void* opaque, void* /*picture*/, void* const* /*planes*/)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    std::swap(ctx->frontBuf, ctx->backBuf);
    ctx->everHadFrame = true;
    ctx->mutex.unlock();
}

// display() SI esta atado al reloj de reproduccion de libVLC (doc:
// libvlc_video_display_cb — "se invoca cuando el frame necesita
// mostrarse, segun lo determine el reloj de reproduccion del medio", que
// usa el audio como maestro cuando hay audio presente). Marcar dirty aca
// en vez de en unlock() es lo que deja que el auto-corrector de drift de
// libVLC (ver --file-caching en InitVLC(), clock-jitter/clock-synchro
// deliberadamente NO forzados a 0) realmente actue: si el decode se
// atrasa, es libVLC quien salta frames internamente para alcanzar de
// nuevo al audio, en vez de que este reproductor muestre "lo ultimo
// decodificado" sin ninguna relacion con el tiempo real.
static void vlc_display(void* opaque, void* /*picture*/)
{
    auto* ctx = static_cast<VLCVideoCtx*>(opaque);
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->dirty = true;
}

} // anonymous namespace

namespace ProyecThor::Core {

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
    std::string threadsArg = "--avcodec-threads=" + std::to_string(m_DecodeThreads);

    // "any": libVLC autodetecta el mejor metodo de aceleracion de hardware
    // disponible segun la plataforma real en la que esta corriendo
    // (D3D11VA/DXVA2 en Windows, VAAPI/VDPAU en Linux), sin necesidad de
    // codificar el valor a mano para cada sistema operativo.
    std::string hwDecodeArg = m_UseHardwareDecode
        ? "--avcodec-hw=any"
        : "--avcodec-hw=none";

    const char* args[] = {
        "--no-xlib",
        "--quiet",
        "--no-osd",
        "--no-video-title-show",
        hwDecodeArg.c_str(),
        threadsArg.c_str(),
        // Cache mas generoso (antes 300ms) para dar margen en disco/CPU
        // lentos. clock-jitter/clock-synchro NO se fuerzan a 0: eso
        // desactivaba el auto-corrector de drift audio/video de VLC, que es
        // justo el mecanismo que hace falta en hardware limitado.
        "--file-caching=1000",
    };
    m_Instance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
    if (!m_Instance)
        std::cerr << "[VLC] Error al crear instancia libVLC.\n";
}

void VLCBasePlayer::OnVlcEvent(const libvlc_event_t* evt, void* userData)
{
    // Corre en un hilo interno de libVLC: solo tocar atomicos.
    auto* self = static_cast<VLCBasePlayer*>(userData);
    if (evt->type == libvlc_MediaPlayerEndReached ||
        evt->type == libvlc_MediaPlayerEncounteredError)
    {
        if (evt->type == libvlc_MediaPlayerEncounteredError) {
            self->m_HadError.store(true, std::memory_order_relaxed);
            self->m_LoadHasError.store(true, std::memory_order_relaxed);
        }
        self->m_EndReached.store(true, std::memory_order_relaxed);
    }
    else if (evt->type == libvlc_MediaPlayerPlaying)
    {
        self->m_VlcIsPlaying.store(true, std::memory_order_relaxed);
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

    // nativeWindowOutput: NO se registran los callbacks vmem — este
    // player se adjunta a una ventana nativa via AttachNativeWindow() y
    // deja que libVLC dibuje ahi con su propio renderer acelerado. Los
    // callbacks vmem y la salida por ventana nativa son mutuamente
    // excluyentes; m_VideoCtx queda nullptr (GetVideoSize/HasVideoFrame/
    // UpdateTexture ya toleran eso, ver sus chequeos existentes).
    if (!m_NativeWindowOutput)
    {
        auto* vCtx = new VLCVideoCtx();
        m_VideoCtx = vCtx;
        libvlc_video_set_format_callbacks(m_MediaPlayer, vlc_format, vlc_cleanup);
        libvlc_video_set_callbacks(m_MediaPlayer, vlc_lock, vlc_unlock, vlc_display, vCtx);
    }

    auto* aCtx = new VLCAudioCtx();
    aCtx->volumeMultiplier = &m_VolumeMultiplier;
    aCtx->muted            = &m_Muted;
    aCtx->audioActive      = &m_AudioActive;
    aCtx->forceSilent      = &m_ForceSilent;
    m_AudioCtx = aCtx;

#ifdef _WIN32
    // Solo en Windows interceptamos los samples crudos para mandarlos a
    // WinMM manualmente (necesario para el VU meter con picos reales y
    // para poder elegir el dispositivo de salida explicitamente).
    libvlc_audio_set_format_callbacks(m_MediaPlayer, vlc_audio_setup, vlc_audio_cleanup);
    libvlc_audio_set_callbacks(m_MediaPlayer, vlc_audio_play,
                               nullptr, nullptr, nullptr, nullptr, aCtx);
#else
    // En Linux NO registramos callbacks de audio: dejamos que libVLC use
    // su salida nativa (PulseAudio/ALSA autodetectado), que es la unica
    // que realmente reproduce sonido en esta plataforma. Volumen/mute/
    // dispositivo se controlan via libvlc_audio_set_volume()/
    // libvlc_audio_set_mute()/libvlc_audio_output_device_set() (ver
    // SetVolume/SetMute/SetAudioDevice mas abajo). Si el player es
    // forceSilent, el estado inicial ya queda mudo y en volumen 0.
    bool initialMute = m_Muted.load(std::memory_order_relaxed) ||
                        m_ForceSilent.load(std::memory_order_relaxed);
    libvlc_audio_set_mute(m_MediaPlayer, initialMute ? 1 : 0);

    int initialVolume = m_ForceSilent.load(std::memory_order_relaxed)
        ? 0
        : static_cast<int>(m_VolumeMultiplier.load(std::memory_order_relaxed) * 100.0f);
    libvlc_audio_set_volume(m_MediaPlayer, initialVolume);
#endif

    libvlc_event_manager_t* em = libvlc_media_player_event_manager(m_MediaPlayer);
    libvlc_event_attach(em, libvlc_MediaPlayerEndReached,       &VLCBasePlayer::OnVlcEvent, this);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &VLCBasePlayer::OnVlcEvent, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPlaying,          &VLCBasePlayer::OnVlcEvent, this);
}

void VLCBasePlayer::DestroyVLC()
{
    // Sin hilo de trabajo propio: no hay nada que apagar/join-ear antes de
    // liberar recursos de libVLC. Stop() detiene sincronicamente.
    Stop();

    if (m_MediaPlayer)
    {
        // release() garantiza que los callbacks de audio/video terminaron
        // antes de retornar — solo entonces es seguro borrar los ctx.
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
    if (m_AudioCtx)
    {
        auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
#ifdef _WIN32
        vlc_audio_destroy_device(ctx);
#endif
        delete ctx;
        m_AudioCtx = nullptr;
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
    glBindTexture(GL_TEXTURE_2D, 0);

    m_VideoW = w;
    m_VideoH = h;
}

void VLCBasePlayer::Play(const std::string& path, bool loop, bool startMuted)
{
    if (!m_Instance || !m_MediaPlayer) return;

    if (m_PathBlocked && NormalizePathForCompare(m_BlockedPath) == NormalizePathForCompare(path))
    {
        std::cerr << "[VLC] Play() ignorado, ruta bloqueada: " << path << "\n";
        return;
    }

    // FIX (freeze en clicks repetidos sobre el mismo video): si esta MISMA
    // instancia ya tiene esta MISMA ruta como contenido actual (cargando o
    // ya activo), un Play() reentrante para ella es un reintento espurio,
    // no un pedido nuevo real — LoadAndPlay() haria un
    // stop()+set_media()+play() COMPLETO de nuevo, sincronico, por cada
    // click extra (N clicks = N ciclos serializados en el hilo de UI).
    // m_CurrentPath se limpia en Stop(), asi que un replay LEGITIMO de la
    // misma ruta despues de que el contenido termino/se detuvo de verdad
    // sigue funcionando normalmente.
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

void VLCBasePlayer::LoadAndPlay(const std::string& path, bool loop, bool /*startMuted*/, uint64_t myGeneration)
{
    std::string finalPath = path;
    if (finalPath.find("youtube.com") != std::string::npos ||
        finalPath.find("youtu.be")    != std::string::npos)
    {
        std::string direct = GetDirectYoutubeURL(finalPath);
        if (!direct.empty())
            finalPath = direct;
        else
            std::cerr << "[yt-dlp] Fallo al resolver la URL.\n";
    }

    if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        return;

    libvlc_media_t* media = nullptr;
    if (finalPath.rfind("http", 0) == 0 || finalPath.rfind("rtsp", 0) == 0)
        media = libvlc_media_new_location(m_Instance, finalPath.c_str());
    else
        media = libvlc_media_new_path(m_Instance, finalPath.c_str());

    if (!media)
    {
        std::cerr << "[VLC] No se pudo abrir: " << finalPath << "\n";
        return;
    }

    if (loop)
        libvlc_media_add_option(media, "input-repeat=65535");

    {
        std::lock_guard<std::mutex> lock(m_MediaSwapMutex);

        if (m_LoadGeneration.load(std::memory_order_relaxed) != myGeneration)
        {
            libvlc_media_release(media);
            return;
        }

        // Detener primero, de forma sincrona, ANTES de cargar lo nuevo:
        // evita correr dos pipelines de decode en paralelo.
        libvlc_media_player_stop(m_MediaPlayer);
        m_EndReached.store(false, std::memory_order_relaxed);
        m_HadError.store(false, std::memory_order_relaxed);
        m_LoadHasError.store(false, std::memory_order_relaxed);
        m_VlcIsPlaying.store(false, std::memory_order_relaxed);

        // FIX: everHadFrame (ver HasVideoFrame()) se reseteaba SOLO dentro
        // de vlc_format(), pero libVLC no vuelve a llamar ese callback si el
        // video nuevo tiene la MISMA resolucion/chroma que el anterior —
        // reutiliza el buffer tal cual esta. Si eso pasaba, everHadFrame
        // seguia en true desde la carga ANTERIOR, asi que HasVideoFrame()
        // (y por lo tanto el crossfade en BackgroundLayer) daba por listo
        // este Play() ANTES de que hubiera decodificado un solo frame real
        // — el publico veia el crossfade animar hacia el frame VIEJO que
        // seguia en el buffer, hasta que el frame nuevo de verdad lo pisaba
        // (ahi "se corregia" solo, de golpe). Resetear aca, en el punto
        // donde SABEMOS que arranca una carga nueva (sin depender de que
        // vlc_format() se dispare o no), lo hace correcto siempre.
        if (m_VideoCtx)
        {
            auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);
            std::lock_guard<std::mutex> ctxLock(ctx->mutex);
            ctx->everHadFrame = false;
            ctx->dirty        = false;
        }

        libvlc_media_player_set_media(m_MediaPlayer, media);
        libvlc_media_player_play(m_MediaPlayer);
        m_Paused.store(false, std::memory_order_relaxed);
    }

    libvlc_media_release(media); // el player ya tomo su propia referencia

#ifndef _WIN32
    // En Linux, cada Play()/set_media reinicia el modulo de salida de
    // audio (aout) de libVLC, lo que puede perder la seleccion de
    // dispositivo hecha con SetAudioDevice(). La reaplicamos aca para que
    // el dispositivo elegido por el usuario persista entre clips.
    if (!m_AudioDeviceId.empty() && m_AudioDeviceId != "default")
        libvlc_audio_output_device_set(m_MediaPlayer, nullptr, m_AudioDeviceId.c_str());
#endif
}

void VLCBasePlayer::Stop()
{
    ++m_LoadGeneration;

    m_EndReached.store(false, std::memory_order_relaxed);
    m_VlcIsPlaying.store(false, std::memory_order_relaxed);
    m_CurrentPath.clear(); // libera el guard de reentrancia de Play()

    std::lock_guard<std::mutex> lock(m_MediaSwapMutex);
    if (m_MediaPlayer)
    {
        libvlc_media_player_stop(m_MediaPlayer);
        libvlc_media_player_set_media(m_MediaPlayer, nullptr);
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

void VLCBasePlayer::SetMute(bool mute)
{
    bool effectiveMute = mute || m_ForceSilent.load(std::memory_order_relaxed);

    std::cerr << "[VLC#" << m_InstanceId << "] SetMute(" << (mute ? "true" : "false")
              << ") -> effectiveMute=" << (effectiveMute ? "true" : "false") << "\n";

    m_Muted.store(effectiveMute, std::memory_order_relaxed);
#ifdef _WIN32
    if (effectiveMute && m_AudioCtx)
    {
        auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
        std::lock_guard<std::mutex> lock(ctx->deviceMutex);
        if (ctx->hWaveOut) {
            waveOutReset(ctx->hWaveOut);
        }
    }
#else
    if (m_MediaPlayer)
    {
        libvlc_audio_set_mute(m_MediaPlayer, effectiveMute ? 1 : 0);

        if (effectiveMute)
            libvlc_audio_set_volume(m_MediaPlayer, 0);
        else if (m_AudioActive.load(std::memory_order_relaxed))
            libvlc_audio_set_volume(m_MediaPlayer,
                static_cast<int>(m_VolumeMultiplier.load(std::memory_order_relaxed) * 100.0f));
    }
#endif
}

void VLCBasePlayer::SetAudioActive(bool active)
{
    bool effectiveActive = active && !m_ForceSilent.load(std::memory_order_relaxed);

    std::cerr << "[VLC#" << m_InstanceId << "] SetAudioActive(" << (active ? "true" : "false")
              << ") -> effectiveActive=" << (effectiveActive ? "true" : "false") << "\n";

    m_AudioActive.store(effectiveActive, std::memory_order_relaxed);
#ifdef _WIN32
    if (!effectiveActive && m_AudioCtx)
    {
        auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
        std::lock_guard<std::mutex> lock(ctx->deviceMutex);
        if (ctx->hWaveOut) {
            waveOutReset(ctx->hWaveOut);
        }
    }
#else
    if (m_MediaPlayer)
    {
        bool shouldMute = !effectiveActive || m_Muted.load(std::memory_order_relaxed);
        libvlc_audio_set_mute(m_MediaPlayer, shouldMute ? 1 : 0);

        if (!effectiveActive)
            libvlc_audio_set_volume(m_MediaPlayer, 0);
        else if (!m_Muted.load(std::memory_order_relaxed))
            libvlc_audio_set_volume(m_MediaPlayer,
                static_cast<int>(m_VolumeMultiplier.load(std::memory_order_relaxed) * 100.0f));
    }
#endif
}

void VLCBasePlayer::SetVolume(int volume)
{
    if (m_ForceSilent.load(std::memory_order_relaxed))
        volume = 0;

    float multiplier = static_cast<float>(volume) / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);
#ifndef _WIN32
    // En Linux el volumen real lo aplica libVLC sobre su salida nativa.
    if (m_MediaPlayer)
        libvlc_audio_set_volume(m_MediaPlayer, volume);
#endif
    // En Windows lo aplica vlc_audio_play() multiplicando los samples.
}

void VLCBasePlayer::SetSoftwareVolume(float percent)
{
    if (m_ForceSilent.load(std::memory_order_relaxed))
        percent = 0.0f;

    float multiplier = percent / 100.0f;
    if (multiplier < 0.0f) multiplier = 0.0f;
    m_VolumeMultiplier.store(multiplier, std::memory_order_relaxed);
#ifndef _WIN32
    if (m_MediaPlayer)
        libvlc_audio_set_volume(m_MediaPlayer, static_cast<int>(percent));
#endif
}

void VLCBasePlayer::EnforceSilenceIfNeeded()
{
#ifndef _WIN32
    bool shouldBeSilent = m_ForceSilent.load(std::memory_order_relaxed) ||
                           !m_AudioActive.load(std::memory_order_relaxed);
    if (!shouldBeSilent || !m_MediaPlayer)
        return;

    libvlc_audio_set_mute(m_MediaPlayer, 1);
    libvlc_audio_set_volume(m_MediaPlayer, 0);
#endif
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
    // FIX: antes chequeaba ctx->dirty (= "hay un frame NUEVO sin subir a GL
    // todavia"), que UpdateTexture() consume (pone en false) cada vez que
    // sube algo. Como BackgroundLayer::Update() llama a UpdateTexture() y
    // JUSTO DESPUES pregunta GetLoadState()==Ready (que depende de esto),
    // el frame recien decodificado ya aparecia consumido para cuando se
    // hacia esa pregunta — Ready practicamente nunca se observaba true, y
    // el prefetch de la cola quedaba reproduciendo sin pausar hasta el
    // timeout de 3s de BackgroundLayer::Update(), avanzando de mas antes
    // del corte (el video "salia a la mitad"). everHadFrame es pegajoso
    // (no se consume): refleja "ya se decodifico al menos un frame real",
    // que es la pregunta que este metodo siempre quiso responder.
    return ctx->everHadFrame && ctx->frontBuf != nullptr && ctx->width > 0 && ctx->height > 0;
}

// Estado derivado (no un evento propio): Idle antes del primer Play(),
// Error si el load actual encontro libvlc_MediaPlayerEncounteredError,
// Ready en cuanto HasVideoFrame() es true (SOLO esa condicion — igual que
// el chequeo que ya funcionaba antes de que existiera este LoadState),
// Opening/Buffering mientras tanto (distincion informativa via el evento
// libvlc_MediaPlayerPlaying, para el spinner/ETA — NUNCA condiciona
// Ready). FIX: la primera version de esto exigia ADEMAS que
// libvlc_MediaPlayerPlaying hubiera disparado para reportar Ready — ese
// evento no siempre llega a tiempo (o de forma confiable) en todos los
// codecs/containers, asi que la mayoria de los swaps terminaban cayendo
// al timeout de 3s de BackgroundLayer::Update() en vez de swapear en
// cuanto el primer frame estaba listo (que es lo que hacia, bien, el
// codigo anterior a este LoadState). Eso se sentia como "todo tarda /
// esta desfasado" — este fix restaura el gate real a HasVideoFrame() solo.
VLCBasePlayer::LoadState VLCBasePlayer::GetLoadState() const
{
    if (!m_HasEverPlayed.load(std::memory_order_relaxed)) return LoadState::Idle;
    if (m_LoadHasError.load(std::memory_order_relaxed))   return LoadState::Error;

    // Un player nativeWindowOutput no tiene VLCVideoCtx (sin callbacks
    // vmem, ver CreatePersistentPlayer), asi que HasVideoFrame() siempre
    // seria false — el evento libvlc_MediaPlayerPlaying es la unica señal
    // de "listo" disponible en este modo.
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
    auto* ctx = static_cast<VLCVideoCtx*>(m_VideoCtx);

    void*    pixelsToUpload = nullptr;
    unsigned w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        if (!ctx->dirty || !ctx->frontBuf || ctx->width == 0 || ctx->height == 0) return false;
        pixelsToUpload = ctx->frontBuf;
        w = ctx->width;
        h = ctx->height;
        ctx->dirty = false;
    } // lock liberado antes de tocar GL: la subida a GL nunca bloquea al decoder

    EnsureTexture(static_cast<int>(w), static_cast<int>(h));
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixelsToUpload);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

std::vector<VLCBasePlayer::AudioDevice> VLCBasePlayer::GetAvailableAudioDevices()
{
    std::vector<AudioDevice> devices;

#ifdef _WIN32
    // En Windows este player no usa el modulo de audio nativo de libVLC
    // (usamos callbacks WinMM propios, ver CreatePersistentPlayer), asi
    // que libvlc_audio_output_device_enum() NO reflejaria los
    // dispositivos reales del sistema. Enumeramos directamente via WinMM.
    devices.push_back({ "default", "Dispositivo predeterminado del sistema" });

    UINT numDevs = waveOutGetNumDevs();
    for (UINT i = 0; i < numDevs; ++i)
    {
        WAVEOUTCAPSA caps{};
        if (waveOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            devices.push_back({ std::to_string(i), caps.szPname });
    }
#else
    if (!m_MediaPlayer) return devices;

    libvlc_audio_output_device_t* devList = libvlc_audio_output_device_enum(m_MediaPlayer);
    for (auto* p = devList; p != nullptr; p = p->p_next)
    {
        if (p->psz_device && p->psz_description)
            devices.push_back({ p->psz_device, p->psz_description });
    }
    if (devList)
        libvlc_audio_output_device_list_release(devList);
#endif

    return devices;
}

void VLCBasePlayer::SetAudioDevice(const std::string& deviceId)
{
    // Se recuerda siempre, incluso si todavia no hay nada reproduciendose:
    // asi, cuando arranque el audio (Windows: vlc_audio_setup / Linux:
    // proximo Play()), se abre directamente en el dispositivo correcto.
    m_AudioDeviceId = deviceId;

    std::cerr << "[VLC#" << m_InstanceId << "] SetAudioDevice(\""
              << deviceId << "\")\n";

#ifdef _WIN32
    if (!m_AudioCtx) return;
    auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);

    UINT_PTR targetId = WAVE_MAPPER;
    if (!deviceId.empty() && deviceId != "default")
    {
        try { targetId = static_cast<UINT_PTR>(std::stoul(deviceId)); }
        catch (...) { targetId = WAVE_MAPPER; }
    }

    std::lock_guard<std::mutex> lock(ctx->deviceMutex);

    if (ctx->deviceInitialized && ctx->deviceId == targetId)
        return; // ya esta en ese dispositivo, nada que hacer

    bool wasInitialized = ctx->deviceInitialized;
    if (wasInitialized)
        CloseWaveOutDeviceLocked(ctx);

    if (wasInitialized)
    {
        // Habia audio en curso: reabrimos de inmediato en el nuevo
        // dispositivo para no interrumpir la reproduccion.
        if (!OpenWaveOutDeviceLocked(ctx, targetId))
            std::cerr << "[Audio] No se pudo cambiar al dispositivo " << targetId << ".\n";
    }
    else
    {
        // Todavia no se abrio ningun dispositivo: solo dejamos el id
        // pedido guardado, y vlc_audio_setup() lo abrira cuando arranque
        // el audio.
        ctx->deviceId = targetId;
    }
#else
    if (m_MediaPlayer && !deviceId.empty() && deviceId != "default")
        libvlc_audio_output_device_set(m_MediaPlayer, nullptr, deviceId.c_str());
#endif
}

void VLCBasePlayer::GetAudioLevels(float& left, float& right)
{
#ifdef _WIN32
    if (!m_AudioCtx)
    {
        left = right = 0.0f;
        return;
    }
    auto* ctx = static_cast<VLCAudioCtx*>(m_AudioCtx);
    // Lock-free: lee el pico acumulado y lo resetea a 0 en la misma
    // operacion atomica, sin bloquear ni competir con el hilo de audio.
    left  = ctx->peakL.exchange(0.0f, std::memory_order_relaxed);
    right = ctx->peakR.exchange(0.0f, std::memory_order_relaxed);
#else
    // En Linux no interceptamos los samples crudos (libVLC usa su salida
    // nativa), asi que no hay picos reales que reportar. Se devuelve 0.0f
    // para no romper a quien consuma el VU meter.
    left = right = 0.0f;
#endif
}

void VLCBasePlayer::AttachNativeWindow(void* nativeHandle)
{
    if (!m_MediaPlayer) return;
#ifdef _WIN32
    libvlc_media_player_set_hwnd(m_MediaPlayer, nativeHandle);
#else
    libvlc_media_player_set_xwindow(m_MediaPlayer,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nativeHandle)));
#endif
}

void VLCBasePlayer::DetachNativeWindow()
{
    if (!m_MediaPlayer) return;
#ifdef _WIN32
    libvlc_media_player_set_hwnd(m_MediaPlayer, nullptr);
#else
    libvlc_media_player_set_xwindow(m_MediaPlayer, 0);
#endif
}

} // namespace ProyecThor::Core