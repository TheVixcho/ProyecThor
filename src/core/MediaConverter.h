#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace ProyecThor::Core {

// Codec de video a forzar en la conversion -- Auto deja que ffmpeg infiera
// el codec a partir de la extension del archivo de salida (comportamiento
// historico, sin flags), igual que antes de agregar compresion/codec. Los
// demas fuerzan un codec concreto vía "-c:v" + "-crf" (ver MediaConverter.cpp
// para la escala de CRF exacta de cada uno).
enum class VideoCodec { Auto, H264, H265, VP9, AV1 };

// ─────────────────────────────────────────────────────────────────────────────
//  MediaConverter — convierte un archivo de video/audio a otro formato
//  invocando "ffmpeg" (mismo binario empaquetado que usa StreamEncoder, ver
//  extrabuild/ffmpeg.exe) como subproceso, en un hilo de fondo para no
//  trabar la UI mientras dura la conversion.
//
//  Por default (codec=Auto) deja que ffmpeg infiera el codec/contenedor a
//  partir de la extension del archivo de salida, igual que antes. Si se
//  pide un VideoCodec concreto, se agrega compresion real vía "-crf" (0..100
//  en la UI, mapeado a la escala de CRF propia de cada codec) -- solo tiene
//  sentido para conversiones de Video, el llamador no debe pasar un codec
//  distinto de Auto para audio.
//
//  Progreso: antes de la conversion real se hace una pasada rapida (mismo
//  binario ffmpeg, sin ffprobe empaquetado) solo para leer la duracion del
//  archivo de entrada; despues la conversion corre con "-progress pipe:1",
//  que ffmpeg usa para ir emitiendo "out_time_ms=..." -- GetProgress()
//  devuelve out_time / duracion total. Si no se pudo determinar la
//  duracion, GetProgress() se queda en -1 (el llamador puede mostrar una
//  barra indeterminada).
// ─────────────────────────────────────────────────────────────────────────────
class MediaConverter {
public:
    ~MediaConverter();

    // Arranca la conversion en un hilo de fondo. Devuelve false (con
    // errorOut) si ya hay una conversion corriendo o si no se encontro
    // ffmpeg. No bloquea -- consultar IsRunning()/GetProgress()/
    // PollFinished() desde Render() en cada frame. `compression` es 0
    // (mejor calidad, archivo mas pesado) a 100 (mas liviano, menor
    // calidad); se ignora si codec==Auto.
    bool Start(const std::string& inputPath, const std::string& outputPath,
               VideoCodec codec = VideoCodec::Auto, int compression = 40,
               std::string* errorOut = nullptr);

    // Pide cancelar la conversion en curso -- no bloquea ni garantiza que
    // el proceso ya este muerto al volver. El resultado sigue llegando por
    // PollFinished() como de costumbre, con outSuccess=false y un mensaje
    // que dice "Cancelado" (no un error real de ffmpeg). No hace nada si
    // no hay ninguna conversion corriendo.
    void Cancel();

    bool IsRunning() const { return m_Running.load(); }

    // 0..1 segun cuanto del archivo de entrada ya se proceso, o -1 si
    // todavia no se pudo determinar (probing de duracion en curso, o el
    // archivo de entrada no reporta duracion). Valido apenas IsRunning()
    // es true; se resetea a -1 en cada Start().
    float GetProgress() const { return m_Progress.load(); }

    // Devuelve true UNA sola vez, la primera vez que se llama despues de
    // que la conversion en curso termino (exito, error o cancelacion) --
    // llamadas siguientes devuelven false hasta la proxima conversion.
    // outSuccess/outMessage quedan completos solo cuando devuelve true.
    bool PollFinished(bool& outSuccess, std::string& outMessage);

private:
    std::thread       m_Thread;
    std::atomic<bool> m_Running{ false };
    std::atomic<bool> m_CancelRequested{ false };
    std::atomic<float> m_Progress{ -1.0f };

    // Handle/pid del proceso de ffmpeg ACTUALMENTE corriendo -- protegido
    // por mutex porque Cancel() se llama desde el hilo de UI mientras el
    // hilo de fondo todavia lo esta usando/reasignando.
    std::mutex m_ProcMutex;
    void*      m_ProcessHandle = nullptr; // HANDLE de Windows; ver TerminateHiddenProcess
#ifndef _WIN32
    int        m_ChildPid = -1; // ver comentario "echo $$" en MediaConverter.cpp
#endif

    std::mutex  m_ResultMutex;
    bool        m_HasPendingResult = false;
    bool        m_LastSuccess      = false;
    std::string m_LastMessage;
};

} // namespace ProyecThor::Core
