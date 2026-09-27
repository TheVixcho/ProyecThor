#include "StreamEncoder.h"
#include "FfmpegPath.h"
#include <cstdlib>

#if defined(_WIN32)
    #include "HiddenProcess.h"
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace ProyecThor::Core {

StreamEncoder::~StreamEncoder() {
    Stop();
}

bool StreamEncoder::Start(const std::string& rtmpUrl, int width, int height, int fps,
                           int videoBitrateKbps, std::string* errorOut) {
    if (m_Pipe) {
        if (errorOut) *errorOut = "Ya hay un stream activo; llama a Stop() primero.";
        return false;
    }
    if (rtmpUrl.empty() || rtmpUrl.rfind("rtmp://", 0) != 0) {
        if (errorOut) *errorOut = "La URL de destino debe empezar con rtmp://";
        return false;
    }
    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app). "
                                   "Si lo borraste, reinstala ProyecThor o instala ffmpeg y agregalo al PATH.";
        return false;
    }

    // Ancho/alto par: requisito de libx264 con -pix_fmt yuv420p (submuestreo
    // de croma 4:2:0, necesita dimensiones divisibles por 2).
    width  &= ~1;
    height &= ~1;
    if (width <= 0 || height <= 0) {
        if (errorOut) *errorOut = "Resolucion invalida.";
        return false;
    }

    int bufsizeKbps = videoBitrateKbps * 2;
    int gop         = fps * 2;

    // Pista de audio silenciosa (anullsrc): varios servidores RTMP (Twitch/
    // YouTube incluidos) rechazan o se comportan mal con streams sin pista
    // de audio -- mas adelante se puede reemplazar por audio real, por
    // ahora es lo que hace viable "ir en vivo" ya mismo.
    char cmd[1536];
    std::snprintf(cmd, sizeof(cmd),
        "%s -y -loglevel warning "
        "-f lavfi -i anullsrc=channel_layout=stereo:sample_rate=44100 "
        "-f rawvideo -pix_fmt rgba -s %dx%d -r %d -i - "
        "-map 1:v -map 0:a "
        "-c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p "
        "-b:v %dk -maxrate %dk -bufsize %dk -g %d "
        "-c:a aac -b:a 128k -shortest "
        "-f flv \"%s\"",
        ffmpegPath.c_str(), width, height, fps,
        videoBitrateKbps, videoBitrateKbps, bufsizeKbps, gop,
        rtmpUrl.c_str());

#if defined(_WIN32)
    // CreateProcess con CREATE_NO_WINDOW: sin esto, cada vez que se inicia
    // una transmision aparece (y desaparece al terminar) una consola de
    // ffmpeg tapando la app -- ProyecThor es GUI-only, no hereda consola
    // propia, y _popen() no da forma de suprimirla.
    if (!StartHiddenProcess(cmd, /*wantStdinPipe=*/true, &m_Pipe,
                             /*wantOutputCapture=*/false, nullptr, &m_ProcessHandle)) {
        if (errorOut) *errorOut = "No se pudo iniciar el proceso de ffmpeg.";
        return false;
    }
#else
    m_Pipe = PT_POPEN(cmd, "wb");
    if (!m_Pipe) {
        if (errorOut) *errorOut = "No se pudo iniciar el proceso de ffmpeg.";
        return false;
    }
#endif

    m_Width        = width;
    m_Height       = height;
    m_Fps          = fps > 0 ? fps : 30;
    m_LastPushTime = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    return true;
}

void StreamEncoder::Stop() {
    if (!m_Pipe) return;
    std::fflush(m_Pipe);

#if defined(_WIN32)
    std::fclose(m_Pipe); // cierra el pipe de stdin -- ffmpeg ve el EOF y termina solo
    m_Pipe = nullptr;
    if (m_ProcessHandle) {
        WaitHiddenProcess(m_ProcessHandle);
        m_ProcessHandle = nullptr;
    }
#else
    PT_PCLOSE(m_Pipe);
    m_Pipe = nullptr;
#endif
}

void StreamEncoder::PushFrame(const uint8_t* rgba, int width, int height) {
    if (!m_Pipe || !rgba) return;
    if (width != m_Width || height != m_Height) return; // frame de otro tamaño -- se descarta

    auto now         = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(now - m_LastPushTime).count();
    double targetMs  = 1000.0 / (double)m_Fps;
    if (elapsedMs < targetMs) return; // todavia no toca el proximo frame -- se descarta

    m_LastPushTime = now;

    size_t frameBytes = (size_t)width * (size_t)height * 4;
    size_t written     = std::fwrite(rgba, 1, frameBytes, m_Pipe);
    if (written != frameBytes) {
        // El pipe se corto (ffmpeg murio, red caida, etc.) -- cerramos
        // limpio para que IsStreaming() refleje la realidad en el proximo
        // frame de UI en vez de seguir "streameando" a la nada.
        Stop();
    }
}

} // namespace ProyecThor::Core
