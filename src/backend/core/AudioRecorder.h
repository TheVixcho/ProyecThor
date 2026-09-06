#pragma once
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ProyecThor::Core {

// Un dispositivo de entrada de audio (microfono) detectado via ffmpeg/dshow.
struct AudioInputDevice { std::string name; };

// Lista los dispositivos de audio de entrada disponibles -- bloqueante
// (invoca ffmpeg y espera ~1s), llamar UNA vez al abrir el panel de
// grabacion, nunca por frame. Windows-only (dshow); en otras plataformas
// devuelve una lista vacia.
std::vector<AudioInputDevice> ListAudioInputDevices();

// ─────────────────────────────────────────────────────────────────────────────
//  AudioRecorder — graba el microfono elegido a un archivo WAV invocando
//  "ffmpeg" (mismo binario/patron de subproceso que MediaConverter/
//  StreamEncoder, ver HiddenProcess.h) con entrada DirectShow, en un hilo de
//  fondo para no trabar la UI mientras dura la grabacion.
// ─────────────────────────────────────────────────────────────────────────────
class AudioRecorder {
public:
    ~AudioRecorder();

    // Arranca a grabar en un hilo de fondo. Devuelve false (con errorOut) si
    // ya hay una grabacion en curso o no se encontro ffmpeg. No bloquea.
    bool Start(const std::string& deviceName, const std::string& outputWavPath,
               std::string* errorOut = nullptr);

    // Pide parar -- le manda 'q' a ffmpeg por stdin para que cierre el WAV
    // de forma prolija (headers de duracion correctos), en vez de matar el
    // proceso de golpe. No bloquea; el resultado llega por PollFinished().
    void Stop();

    bool   IsRecording() const { return m_Recording.load(); }
    double GetElapsedSeconds() const { return m_Elapsed.load(); }

    // Devuelve true UNA sola vez, la primera vez que se llama despues de que
    // la grabacion en curso termino (exito o error). outSuccess/outMessage
    // quedan completos solo cuando devuelve true.
    bool PollFinished(bool& outSuccess, std::string& outMessage);

private:
    std::thread        m_Thread;
    std::atomic<bool>  m_Recording{ false };
    std::atomic<bool>  m_StopRequested{ false };
    std::atomic<double> m_Elapsed{ 0.0 };

    std::mutex m_StdinMutex;
    FILE*      m_Stdin = nullptr; // stdin de ffmpeg, para poder mandarle 'q' desde Stop()

    std::mutex  m_ResultMutex;
    bool        m_HasPendingResult = false;
    bool        m_LastSuccess      = false;
    std::string m_LastMessage;
};

} // namespace ProyecThor::Core
