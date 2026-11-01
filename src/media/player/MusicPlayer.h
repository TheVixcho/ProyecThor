#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct libvlc_instance_t;
struct libvlc_media_player_t;
struct libvlc_event_t;

namespace ProyecThor::Core {

// Reproductor de MUSICA basado en libVLC con la salida de audio NATIVA de VLC
// (mmdevice/WASAPI compartido en Windows). A diferencia de VLCBasePlayer (que
// enruta el audio por callbacks propios a waveOut y esta pensado para video):
//   - Soporta todo lo que VLC soporta (FLAC de 24 bit / 96 kHz, ALAC, OPUS...).
//   - Convive con el resto de sonidos del sistema (modo compartido).
//   - La UI nunca se bloquea: abrir / cambiar / detener una pista se ejecuta en
//     un hilo propio y "gana el ultimo comando" (como cambiar de cancion
//     rapido en Spotify).
class MusicPlayer {
public:
    MusicPlayer();
    ~MusicPlayer();

    MusicPlayer(const MusicPlayer&)            = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    // Abre y reproduce `utf8Path` (reinicia si ya era la misma pista).
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
    // endpoint WASAPI). Sin soporte -> 0.
    void GetAudioLevels(float& left, float& right);

    static constexpr int kEqualizerBands = 10;

private:
    struct Cmd {
        enum Type { None, PlayPath, StopPlayback } type = None;
        std::string path;
        uint64_t    gen = 0;
    };

    void WorkerLoop();
    void ExecutePlay(const Cmd& cmd);
    void ExecuteStop();
    void ApplyVolume();
    void ApplyEqualizer();
    void SubmitCmd(Cmd&& cmd);

    static void OnVlcEvent(const libvlc_event_t* evt, void* userData);

    std::thread             m_Worker;
    std::mutex              m_CmdMutex;
    std::condition_variable m_CmdCv;
    Cmd                     m_PendingCmd;
    bool                    m_Quit = false;

    libvlc_instance_t*                    m_Instance = nullptr;   // solo el worker lo toca
    std::atomic<libvlc_media_player_t*>   m_Mp{nullptr};

    // Generaciones: cada Play/Stop de la UI incrementa m_ReqGen. El worker
    // marca m_LiveGen con la generacion que esta sonando. Un fin de pista solo
    // cuenta si m_LiveGen == m_ReqGen (ignora eventos de pistas ya reemplazadas).
    std::atomic<uint64_t> m_ReqGen{0};
    std::atomic<uint64_t> m_LiveGen{0};

    std::atomic<bool> m_EndReached{false};
    std::atomic<bool> m_HadError{false};
    std::atomic<bool> m_Paused{false};
    std::atomic<bool> m_Muted{false};
    // Estado a reaplicar cuando la pista ya suena (evento Playing)
    mutable std::atomic<bool>  m_ApplyPending{false};
    mutable std::atomic<float> m_PendingSeek{-1.0f};
    std::atomic<int>  m_Volume{100};

    std::mutex m_EqMutex;
    bool  m_EqEnabled = false;
    float m_EqPreamp  = 0.0f;
    float m_EqBands[kEqualizerBands] = { 0.0f };

    // Medidor de picos del sistema (Windows)
    void* m_MeterPtr        = nullptr;  // IAudioMeterInformation*
    double m_MeterRetryAt   = 0.0;
    bool  m_ComInitialized  = false;
    float m_LastL = 0.0f, m_LastR = 0.0f;
};

}
