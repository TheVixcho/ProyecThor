#include "AudioMixdown.h"
#include "FfmpegPath.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include "HiddenProcess.h"
#endif

namespace ProyecThor::Core {

namespace {

std::string CodecArgsForFormat(AudioExportFormat format) {
    switch (format) {
        case AudioExportFormat::WAV: return " -c:a pcm_s16le";
        case AudioExportFormat::MP3: return " -c:a libmp3lame -b:a 192k";
        case AudioExportFormat::AAC: return " -c:a aac -b:a 192k";
        case AudioExportFormat::OGG: return " -c:a libvorbis -q:a 5";
        default:                    return "";
    }
}

} // namespace (anonimo)

AudioMixdown::~AudioMixdown() {
    if (m_Thread.joinable()) m_Thread.join();
}

bool AudioMixdown::Start(const std::vector<AudioMixdownClip>& clips, const std::string& outputPath,
                          AudioExportFormat format, std::string* errorOut) {
    if (m_Running.load()) {
        if (errorOut) *errorOut = "Ya hay una exportacion en curso.";
        return false;
    }
    if (clips.empty()) {
        if (errorOut) *errorOut = "No hay ningun clip para exportar.";
        return false;
    }
#if !defined(_WIN32)
    if (errorOut) *errorOut = "Exportar todavia no esta disponible en esta plataforma.";
    return false;
#else
    std::string ffmpegPath = FfmpegPath();
    if (!FfmpegAvailable(ffmpegPath)) {
        if (errorOut) *errorOut = "No se encontro ffmpeg (deberia estar empaquetado junto a la app).";
        return false;
    }

    if (m_Thread.joinable()) m_Thread.join();

    m_Running          = true;
    m_CancelRequested  = false;
    m_Progress         = -1.0f;
    {
        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = false;
    }

    m_Thread = std::thread([this, ffmpegPath, clips, outputPath, format]() {
        // Duracion total conocida de antemano (no hace falta probing como en
        // MediaConverter): el clip que termina mas tarde en la linea de
        // tiempo del proyecto.
        double totalDuration = 0.0;
        for (const auto& c : clips)
            totalDuration = std::max(totalDuration, (double)(c.timelinePosMs + c.durationMs) / 1000.0);

        // -i por cada clip + un filtro atrim(recorte real)+adelay(posicion
        // en la linea de tiempo) por clip, mezclados con amix al final.
        // "all=1" en adelay aplica el mismo retraso a todos los canales sin
        // importar si la fuente es mono/estereo (evita tener que adivinar
        // cuantos "|N" separados por canal hacen falta).
        std::string cmd = ffmpegPath + " -y -loglevel error -progress pipe:1 -nostats";
        for (const auto& c : clips)
            cmd += " -i \"" + c.sourcePath + "\"";

        std::string filter;
        for (size_t i = 0; i < clips.size(); i++) {
            const auto& c = clips[i];
            double offS = c.sourceOffsetMs / 1000.0;
            double endS = (c.sourceOffsetMs + c.durationMs) / 1000.0;
            filter += "[" + std::to_string(i) + ":a]atrim=start=" + std::to_string(offS) +
                      ":end=" + std::to_string(endS) +
                      ",asetpts=PTS-STARTPTS,adelay=delays=" + std::to_string(c.timelinePosMs) +
                      ":all=1[a" + std::to_string(i) + "];";
        }
        for (size_t i = 0; i < clips.size(); i++)
            filter += "[a" + std::to_string(i) + "]";
        filter += "amix=inputs=" + std::to_string(clips.size()) + ":duration=longest[out]";

        cmd += " -filter_complex \"" + filter + "\" -map \"[out]\"" +
               CodecArgsForFormat(format) + " \"" + outputPath + "\"";

        bool        success = false;
        bool        cancelled = false;
        std::string lastErrorLine;

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
                    lastErrorLine = line;
                }
                if (m_CancelRequested.load()) break;
            }
            if (pipe) std::fclose(pipe);
            int status = WaitHiddenProcess(proc);
            {
                std::lock_guard<std::mutex> lk(m_ProcMutex);
                m_ProcessHandle = nullptr;
            }
            cancelled = m_CancelRequested.load();
            success   = !cancelled && (status == 0);
        } else {
            lastErrorLine = "No se pudo iniciar ffmpeg.";
        }

        std::lock_guard<std::mutex> lock(m_ResultMutex);
        m_HasPendingResult = true;
        m_LastSuccess       = success;
        m_LastMessage       = cancelled ? std::string("Cancelado.")
                            : success   ? ("Listo: " + outputPath)
                                        : ("La exportacion fallo" + (lastErrorLine.empty() ? std::string(".") : (": " + lastErrorLine)));
        if (success) m_Progress = 1.0f;
        m_Running = false;
    });

    return true;
#endif
}

void AudioMixdown::Cancel() {
    if (!m_Running.load()) return;
    m_CancelRequested = true;
#if defined(_WIN32)
    std::lock_guard<std::mutex> lk(m_ProcMutex);
    if (m_ProcessHandle) TerminateHiddenProcess(m_ProcessHandle);
#endif
}

bool AudioMixdown::PollFinished(bool& outSuccess, std::string& outMessage) {
    std::lock_guard<std::mutex> lock(m_ResultMutex);
    if (!m_HasPendingResult) return false;

    m_HasPendingResult = false;
    outSuccess         = m_LastSuccess;
    outMessage         = m_LastMessage;
    return true;
}

} // namespace ProyecThor::Core
