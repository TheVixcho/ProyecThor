#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ProyecThor::Core {

// ─────────────────────────────────────────────────────────────────────────────
//  StreamEncoder — publica un stream RTMP real invocando "ffmpeg" como
//  subproceso (mismo criterio que ya usa este proyecto para apoyarse en
//  herramientas externas especializadas en vez de reimplementarlas -- ver
//  yt-dlp.exe en extrabuild/). Le mandamos frames RGBA crudos por su stdin
//  (pipe binario) y ffmpeg se encarga de codificar H.264/AAC y empujarlos
//  al servidor RTMP.
//
//  Requiere que "ffmpeg" este en el PATH del sistema (o en extrabuild/ junto
//  al resto de binarios que se copian al build, si se agrega mas adelante).
//  Si no se encuentra, Start() devuelve false con un mensaje claro.
// ─────────────────────────────────────────────────────────────────────────────
class StreamEncoder {
public:
    ~StreamEncoder();

    // rtmpUrl ya viene armada (serverUrl + "/" + streamKey, ver
    // BroadcastPanel). width/height deben ser pares (requisito de libx264
    // con yuv420p). fps y videoBitrateKbps son los que elige el operador en
    // "Iniciar". Devuelve false (y llena errorOut) si no se pudo lanzar
    // ffmpeg -- el motivo mas comun es que no este instalado/en el PATH.
    bool Start(const std::string& rtmpUrl, int width, int height, int fps,
               int videoBitrateKbps, std::string* errorOut = nullptr);

    // Cierra el pipe de forma prolija (fflush + fclose) para que ffmpeg
    // termine de escribir el archivo/stream antes de salir, en vez de
    // matarlo de un balazo.
    void Stop();

    bool IsStreaming() const { return m_Pipe != nullptr; }

    // Manda un frame RGBA (width*height*4 bytes, el mismo tamaño pasado a
    // Start) al encoder. Se throttlea sola a los FPS configurados -- llamar
    // desde Render() cada frame de ImGui sin preocuparse por el timing, los
    // frames de mas simplemente se descartan.
    void PushFrame(const uint8_t* rgba, int width, int height);

private:
    FILE*       m_Pipe          = nullptr;
    void*       m_ProcessHandle = nullptr; // solo Windows (HiddenProcess.h) -- nullptr en el resto
    int         m_Width  = 0;
    int         m_Height = 0;
    int         m_Fps    = 30;
    std::chrono::steady_clock::time_point m_LastPushTime;
};

} // namespace ProyecThor::Core
