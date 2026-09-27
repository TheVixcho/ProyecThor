#include "MediaConverter.h"
#include "FfmpegPath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
    #include "HiddenProcess.h"
#else
    #include <csignal>
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace ProyecThor::Core {

namespace {

// Arma los flags de "-c:v ... -crf ..." para el codec/compresion elegidos.
// Cada codec tiene su propia escala de CRF (valores tipicos recomendados
// por ffmpeg para uso general, no los extremos de cada escala) -- se
// interpola linealmente el 0..100 de la UI dentro de ese rango. VP9/AV1 en
// modo CRF puro necesitan "-b:v 0" explicito, si no ffmpeg los trata como
// bitrate-constreñidos en vez de calidad constante.
std::string BuildVideoCodecArgs(VideoCodec codec, int compression) {
    compression = std::clamp(compression, 0, 100);
    float t = compression / 100.0f;

    switch (codec) {
        case VideoCodec::H264: {
            int crf = (int)std::lround(18 + t * (32 - 18));
            return " -c:v libx264 -crf " + std::to_string(crf) + " -pix_fmt yuv420p";
        }
        case VideoCodec::H265: {
            int crf = (int)std::lround(20 + t * (34 - 20));
            return " -c:v libx265 -crf " + std::to_string(crf) + " -pix_fmt yuv420p";
        }
        case VideoCodec::VP9: {
            int crf = (int)std::lround(24 + t * (45 - 24));
            return " -c:v libvpx-vp9 -crf " + std::to_string(crf) + " -b:v 0";
        }
        case VideoCodec::AV1: {
            // libaom-av1 es MUY lento en su velocidad default -- "-cpu-used 6"
            // es el punto medio que recomienda la propia documentacion de
            // ffmpeg para uso general (mas alto = mas rapido, peor
            // compresion; mas bajo = mas lento, mejor compresion).
            int crf = (int)std::lround(24 + t * (45 - 24));
            return " -c:v libaom-av1 -crf " + std::to_string(crf) + " -b:v 0 -cpu-used 6 -row-mt 1";
        }
        case VideoCodec::Auto:
        default:
            return "";
    }
}

// Busca "Duration: HH:MM:SS.cc" en cualquier parte de la linea (asi la
// imprime ffmpeg al abrir el archivo de entrada) -- devuelve -1 si esta
// linea no la tiene.
double ParseDurationLine(const std::string& line) {
    auto pos = line.find("Duration:");
    if (pos == std::string::npos) return -1.0;
    int h = 0, m = 0;
    double s = 0.0;
    if (std::sscanf(line.c_str() + pos, "Duration: %d:%d:%lf", &h, &m, &s) == 3)
        return h * 3600.0 + m * 60.0 + s;
    return -1.0;
}

// Duracion del archivo de entrada, en segundos -- no hay ffprobe
// empaquetado (solo ffmpeg.exe, ver extrabuild/), asi que se aprovecha que
// ffmpeg imprime "Duration: ..." al abrir el archivo INCLUSO sin pedirle
// ninguna salida (termina en error "At least one output file must be
// specified", que se ignora a proposito: solo interesa esa linea). Devuelve
// -1 si no se pudo determinar (el llamador muestra progreso indeterminado).
double ProbeDurationSeconds(const std::string& ffmpegPath, const std::string& inputPath) {
    std::string cmd = ffmpegPath + " -hide_banner -i \"" + inputPath + "\"";
    double duration = -1.0;

#if defined(_WIN32)
    FILE* pipe = nullptr;
    void* proc = nullptr;
    if (StartHiddenProcess(cmd, /*wantStdinPipe=*/false, nullptr,
                            /*wantOutputCapture=*/true, &pipe, &proc)) {
        char buf[512];
        while (pipe && std::fgets(buf, sizeof(buf), pipe)) {
            double d = ParseDurationLine(buf);
            if (d > 0.0) duration = d;
        }
        if (pipe) std::fclose(pipe);
        WaitHiddenProcess(proc);
    }
#else
    FILE* pipe = PT_POPEN((cmd + " 2>&1").c_str(), "r");
    if (pipe) {
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe)) {
            double d = ParseDurationLine(buf);
            if (d > 0.0) duration = d;
        }
        PT_PCLOSE(pipe);
    }
#endif
    return duration;
}

} // namespace (anonimo)

MediaConverter::~MediaConverter() {
    if (m_Thread.joinable()) m_Thread.join();
}

bool MediaConverter::Start(const std::string& inputPath, const std::string& outputPath,
                            VideoCodec codec, int compression, std::string* errorOut) {
    if (m_Running.load()) {
        if (errorOut) *errorOut = "Ya hay una conversion en curso.";
        return false;
    }

    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app). "
                                   "Si lo borraste, reinstala ProyecThor o instala ffmpeg y agregalo al PATH.";
        return false;
    }

    if (m_Thread.joinable()) m_Thread.join(); // conversion anterior ya terminada, solo falta unir el hilo

    m_Running          = true;
    m_CancelRequested  = false;
    m_Progress         = -1.0f;
    {
        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = false;
    }

    m_Thread = std::thread([this, ffmpegPath, inputPath, outputPath, codec, compression]() {
        double totalDuration = ProbeDurationSeconds(ffmpegPath, inputPath);

        // -y: sobreescribe si por algun motivo ya existe el destino (el
        // llamador ya se encarga de elegir un nombre libre, ver
        // RenderConverterSection en LibraryPanel.cpp). Sin -hwaccel.
        // codec==Auto: sin flags de codec, ffmpeg elige un codec/contenedor
        // razonable a partir de la extension de salida (comportamiento
        // historico). Otro codec: se agrega "-c:v ... -crf ..." (ver
        // BuildVideoCodecArgs). "-progress pipe:1 -nostats": en vez de las
        // lineas humanas "frame=... time=..." (que -nostats apaga), ffmpeg
        // imprime pares "clave=valor" faciles de parsear -- "out_time_us"
        // da el avance real, comparado contra totalDuration.
        std::string codecArgs = BuildVideoCodecArgs(codec, compression);
        std::string cmd = ffmpegPath + " -y -loglevel error -progress pipe:1 -nostats -i \"" +
                           inputPath + "\"" + codecArgs + " \"" + outputPath + "\"";

        bool        success   = false;
        bool        cancelled = false;
        std::string lastErrorLine;

#if defined(_WIN32)
        // CREATE_NO_WINDOW (via HiddenProcess): sin esto aparece una
        // consola de ffmpeg tapando la app por cada conversion. Tambien
        // captura stdout+stderr combinados -- antes con _popen(cmd,"r") se
        // perdia stderr (donde ffmpeg reporta los errores de verdad),
        // quedaba solo un generico "fallo" sin decir por que.
        FILE* pipe = nullptr;
        void* proc = nullptr;
        if (StartHiddenProcess(cmd, /*wantStdinPipe=*/false, nullptr,
                                /*wantOutputCapture=*/true, &pipe, &proc)) {
            {
                std::lock_guard<std::mutex> lk(m_ProcMutex);
                m_ProcessHandle = proc;
            }
            char buf[512];
            while (pipe && std::fgets(buf, sizeof(buf), pipe)) {
                std::string line(buf);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (line.rfind("out_time_us=", 0) == 0 && totalDuration > 0.0) {
                    long long us = std::atoll(line.c_str() + 12);
                    m_Progress = (float)std::clamp((double)us / 1e6 / totalDuration, 0.0, 1.0);
                } else if (!line.empty() && line.find('=') == std::string::npos) {
                    // Las lineas "clave=valor" de -progress no son errores;
                    // lo que SI viene suelto (warnings/errores de ffmpeg,
                    // -loglevel error) es candidato a mensaje real.
                    lastErrorLine = line;
                }
                if (m_CancelRequested.load()) break; // el proceso ya deberia estar muerto, ver Cancel()
            }
            if (pipe) std::fclose(pipe);
            int status = WaitHiddenProcess(proc);
            {
                std::lock_guard<std::mutex> lk(m_ProcMutex);
                m_ProcessHandle = nullptr;
            }
            cancelled = m_CancelRequested.load();
            success   = !cancelled && (status == 0);
        }
#else
        // Trick para poder matar el proceso desde Cancel(): popen() ya
        // envuelve el comando en "sh -c", asi que "echo $$" imprime el pid
        // de ESE shell -- y como "exec" reemplaza ese mismo proceso (sin
        // fork) por ffmpeg, ese pid termina siendo el de ffmpeg. Se imprime
        // ANTES que nada mas, asi que siempre es la primera linea leida.
        std::string wrappedCmd = "echo $$; exec " + cmd + " 2>&1";
        FILE* pipe = PT_POPEN(wrappedCmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            bool first = true;
            while (std::fgets(buf, sizeof(buf), pipe)) {
                std::string line(buf);
                if (!line.empty() && line.back() == '\n') line.pop_back();
                if (first) {
                    first  = false;
                    int pid = std::atoi(line.c_str());
                    if (pid > 0) {
                        std::lock_guard<std::mutex> lk(m_ProcMutex);
                        m_ChildPid = pid;
                    }
                    continue;
                }
                if (line.rfind("out_time_us=", 0) == 0 && totalDuration > 0.0) {
                    long long us = std::atoll(line.c_str() + 12);
                    m_Progress = (float)std::clamp((double)us / 1e6 / totalDuration, 0.0, 1.0);
                } else if (!line.empty() && line.find('=') == std::string::npos) {
                    lastErrorLine = line;
                }
                if (m_CancelRequested.load()) break;
            }
            int status = PT_PCLOSE(pipe);
            {
                std::lock_guard<std::mutex> lk(m_ProcMutex);
                m_ChildPid = -1;
            }
            cancelled = m_CancelRequested.load();
            success   = !cancelled && (status == 0);
        }
#endif

        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = true;
        m_LastSuccess       = success;
        m_LastMessage       = cancelled ? std::string("Cancelado.")
                            : success   ? ("Listo: " + outputPath)
                                        : ("La conversion fallo" + (lastErrorLine.empty() ? std::string(".") : (": " + lastErrorLine)));
        if (success) m_Progress = 1.0f;
        m_Running = false;
    });

    return true;
}

void MediaConverter::Cancel() {
    if (!m_Running.load()) return;
    m_CancelRequested = true;

    // Se mata ya mismo, no solo se pone la bandera -- si el hilo de fondo
    // esta bloqueado en fgets() esperando la proxima linea, matar el
    // proceso cierra el pipe y lo desbloquea solo (si no, se quedaria
    // esperando hasta la siguiente linea de progreso para notar la
    // bandera).
    std::lock_guard<std::mutex> lk(m_ProcMutex);
#if defined(_WIN32)
    if (m_ProcessHandle) TerminateHiddenProcess(m_ProcessHandle);
#else
    if (m_ChildPid > 0) kill(m_ChildPid, SIGTERM);
#endif
}

bool MediaConverter::PollFinished(bool& outSuccess, std::string& outMessage) {
    std::lock_guard<std::mutex> lock(m_ResultMutex);
    if (!m_HasPendingResult) return false;

    m_HasPendingResult = false;
    outSuccess         = m_LastSuccess;
    outMessage         = m_LastMessage;
    return true;
}

} // namespace ProyecThor::Core
