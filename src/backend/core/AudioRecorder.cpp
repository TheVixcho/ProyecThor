#include "AudioRecorder.h"
#include "FfmpegPath.h"
#include <chrono>

#if defined(_WIN32)
#include "HiddenProcess.h"
#endif

namespace ProyecThor::Core {

std::vector<AudioInputDevice> ListAudioInputDevices()
{
    std::vector<AudioInputDevice> devices;
#if defined(_WIN32)
    std::string ffmpegPath = FfmpegPath();
    // "-i dummy" nunca abre nada de verdad -- list_devices hace que ffmpeg
    // imprima los dispositivos disponibles por stderr y despues termine con
    // error (esperado, se ignora el codigo de salida a proposito).
    std::string cmd = ffmpegPath + " -hide_banner -list_devices true -f dshow -i dummy";

    FILE* pipe = nullptr;
    void* proc = nullptr;
    if (StartHiddenProcess(cmd, /*wantStdinPipe=*/false, nullptr,
                            /*wantOutputCapture=*/true, &pipe, &proc)) {
        bool inAudioSection = false;
        char buf[512];
        while (pipe && std::fgets(buf, sizeof(buf), pipe)) {
            std::string line(buf);
            if (line.find("DirectShow audio devices") != std::string::npos) { inAudioSection = true;  continue; }
            if (line.find("DirectShow video devices") != std::string::npos) { inAudioSection = false; continue; }
            // Cada dispositivo real imprime dos lineas: el nombre entre
            // comillas, y despues "Alternative name" con el @device_cm_...
            // -- solo interesa la primera.
            if (inAudioSection && line.find("Alternative name") == std::string::npos) {
                auto first = line.find('"');
                auto last  = line.rfind('"');
                if (first != std::string::npos && last != std::string::npos && last > first)
                    devices.push_back({ line.substr(first + 1, last - first - 1) });
            }
        }
        if (pipe) std::fclose(pipe);
        WaitHiddenProcess(proc);
    }
#endif
    return devices;
}

AudioRecorder::~AudioRecorder() {
    if (m_Recording.load()) Stop();
    if (m_Thread.joinable()) m_Thread.join();
}

bool AudioRecorder::Start(const std::string& deviceName, const std::string& outputWavPath, std::string* errorOut)
{
    if (m_Recording.load()) {
        if (errorOut) *errorOut = "Ya se esta grabando.";
        return false;
    }
#if !defined(_WIN32)
    if (errorOut) *errorOut = "Grabar audio solo esta disponible en Windows por ahora.";
    return false;
#else
    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app).";
        return false;
    }
    if (deviceName.empty()) {
        if (errorOut) *errorOut = "Elegi un microfono primero.";
        return false;
    }

    if (m_Thread.joinable()) m_Thread.join();

    m_Recording      = true;
    m_StopRequested  = false;
    m_Elapsed        = 0.0;
    {
        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = false;
    }

    m_Thread = std::thread([this, ffmpegPath, deviceName, outputWavPath]() {
        std::string cmd = ffmpegPath + " -y -loglevel error -f dshow -i audio=\"" + deviceName +
                           "\" -ar 48000 -ac 2 \"" + outputWavPath + "\"";

        bool        success = false;
        std::string lastErrorLine;

        FILE* stdinPipe = nullptr;
        void* proc      = nullptr;
        if (StartHiddenProcess(cmd, /*wantStdinPipe=*/true, &stdinPipe,
                                /*wantOutputCapture=*/false, nullptr, &proc)) {
            {
                std::lock_guard<std::mutex> lk(m_StdinMutex);
                m_Stdin = stdinPipe;
            }

            auto t0 = std::chrono::steady_clock::now();
            while (!m_StopRequested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                m_Elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            }

            // 'q' le pide a ffmpeg que cierre prolijo (headers de WAV
            // correctos) en vez de matarlo -- WaitHiddenProcess de abajo
            // espera a que salga solo.
            {
                std::lock_guard<std::mutex> lk(m_StdinMutex);
                if (m_Stdin) { std::fputs("q\n", m_Stdin); std::fflush(m_Stdin); }
            }

            int status = WaitHiddenProcess(proc);

            {
                std::lock_guard<std::mutex> lk(m_StdinMutex);
                if (m_Stdin) { std::fclose(m_Stdin); m_Stdin = nullptr; }
            }

            success = (status == 0);
        } else {
            lastErrorLine = "No se pudo iniciar ffmpeg (revisa que el microfono elegido siga conectado).";
        }

        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = true;
        m_LastSuccess       = success;
        m_LastMessage       = success ? ("Grabacion guardada: " + outputWavPath)
                                       : ("La grabacion fallo" + (lastErrorLine.empty() ? std::string(".") : (": " + lastErrorLine)));
        m_Recording = false;
    });

    return true;
#endif
}

void AudioRecorder::Stop()
{
    m_StopRequested = true;
}

bool AudioRecorder::PollFinished(bool& outSuccess, std::string& outMessage)
{
    std::lock_guard<std::mutex> lock(m_ResultMutex);
    if (!m_HasPendingResult) return false;

    m_HasPendingResult = false;
    outSuccess         = m_LastSuccess;
    outMessage         = m_LastMessage;
    return true;
}

} // namespace ProyecThor::Core
