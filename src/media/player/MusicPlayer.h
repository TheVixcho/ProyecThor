#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct mpv_handle;

namespace ProyecThor::Core {

// Reproductor de MUSICA basado en libmpv con salida de audio nativa
// (WASAPI compartido en Windows, PipeWire/PulseAudio/ALSA en Linux).
//   - Soporta todos los codecs de FFmpeg/mpv (FLAC 24-bit/96kHz, ALAC, OPUS, MP3...).
//   - La UI nunca se bloquea: todos los comandos son asincronos y los eventos
//     corren en un hilo ligero dedicado.
class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();

    MusicPlayer(const MusicPlayer&)            = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    void Play(const std::string& utf8Path);
    void Stop();

    void SetPause(bool paused);
    bool IsPaused() const { return m_Paused.load(std::memory_order_relaxed); }

    void    SetPosition(float normalized);
    int64_t GetTime() const;
    int64_t GetLength() const;

    // 0..200 (100 = volumen nativo, >100 amplifica)
    void SetVolume(int percent);
    void SetMute(bool mute);

    void SetEqualizerEnabled(bool enabled);
    void SetEqualizerPreamp(float preampDb);
    void SetEqualizerBand(int index, float ampDb);

    // true una sola vez cuando la pista termina sola (no por Stop/cambio).
    bool ConsumeEndReached();
    bool ConsumeHadError();

    // Picos 0..1 de la salida de audio del sistema (Windows: medidor del
    // endpoint WASAPI). Sin soporte -> animacion suave.
    void GetAudioLevels(float& left, float& right);

    static constexpr int kEqualizerBands = 10;

private:
    void EventLoop();
    void ApplyVolume();
    void ApplyEqualizer();

    mpv_handle*       m_Mpv = nullptr;
    std::thread       m_EventThread;
    std::atomic<bool> m_Quit{false};

    std::atomic<uint64_t> m_ReqGen{0};
    std::atomic<bool>     m_Playing{false};
    std::atomic<bool>     m_EndReached{false};
    std::atomic<bool>     m_HadError{false};
    std::atomic<bool>     m_Paused{false};
    std::atomic<bool>     m_Muted{false};
    std::atomic<int>      m_Volume{100};
    std::atomic<int64_t>  m_TimeMs{0};
    std::atomic<int64_t>  m_LengthMs{0};

    std::mutex m_EqMutex;
    bool  m_EqEnabled = false;
    float m_EqPreamp  = 0.0f;
    float m_EqBands[kEqualizerBands] = { 0.0f };

    // Medidor de picos del sistema (Windows)
    void*  m_MeterPtr       = nullptr;  // IAudioMeterInformation*
    double m_MeterRetryAt   = 0.0;
    bool   m_ComInitialized = false;
    float  m_LastL = 0.0f, m_LastR = 0.0f;
};

}
