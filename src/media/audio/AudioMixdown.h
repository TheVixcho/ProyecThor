#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ProyecThor::Core {

enum class AudioExportFormat { WAV, MP3, AAC, OGG };

// Un clip a mezclar: <sourceOffsetMs>..<sourceOffsetMs+durationMs> del
// archivo <sourcePath>, ubicado en <timelinePosMs> de la linea de tiempo del
// proyecto (pueden superponerse entre si, incluso de distintas pistas).
struct AudioMixdownClip {
    std::string sourcePath;
    int64_t     sourceOffsetMs = 0;
    int64_t     durationMs     = 0;
    int64_t     timelinePosMs  = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  AudioMixdown — mezcla varios clips (con su propio recorte y posicion en
//  la linea de tiempo) en un solo archivo de audio, invocando "ffmpeg" con
//  un filtro adelay+amix por clip. Mismo patron de hilo de fondo +
//  PollFinished() que MediaConverter (ver ese archivo para el porque de
//  cada decision de diseño, no repetido aca).
// ─────────────────────────────────────────────────────────────────────────────
class AudioMixdown {
public:
    ~AudioMixdown();

    bool Start(const std::vector<AudioMixdownClip>& clips, const std::string& outputPath,
               AudioExportFormat format, std::string* errorOut = nullptr);
    void Cancel();

    bool  IsRunning() const { return m_Running.load(); }
    // 0..1, o -1 si no se pudo estimar la duracion total todavia.
    float GetProgress() const { return m_Progress.load(); }

    bool PollFinished(bool& outSuccess, std::string& outMessage);

private:
    std::thread        m_Thread;
    std::atomic<bool>  m_Running{ false };
    std::atomic<bool>  m_CancelRequested{ false };
    std::atomic<float> m_Progress{ -1.0f };

    std::mutex m_ProcMutex;
    void*      m_ProcessHandle = nullptr;

    std::mutex  m_ResultMutex;
    bool        m_HasPendingResult = false;
    bool        m_LastSuccess      = false;
    std::string m_LastMessage;
};

} // namespace ProyecThor::Core
