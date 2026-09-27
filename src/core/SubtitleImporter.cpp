#include "SubtitleImporter.h"
#include "YtDlpPath.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
    #include "HiddenProcess.h"
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace ProyecThor::Core {

// ── Seguridad: la URL viaja embebida en un string de linea de comandos que
// en Linux/macOS termina pasando por "sh -c" (popen). Sin validar, una URL
// con backticks/`$(...)`/comillas podria inyectar comandos arbitrarios --
// se rechaza cualquier cosa que no sea un http(s) URL "de verdad", sin los
// caracteres que permitirian escapar de las comillas que lo envuelven.
static bool IsSafeUrl(const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
        return false;
    for (char c : url) {
        if (c == '"' || c == '`' || c == '$' || c == '\\' || c == '\n' || c == '\r' || c == '\'')
            return false;
    }
    return true;
}

// Corre <cmd> hasta que termina y devuelve TODO stdout+stderr combinado como
// un solo string. Sincronica a proposito -- ver comentario en el header.
static bool RunProcessCaptureAll(const std::string& cmd, std::string& outText) {
    outText.clear();
#if defined(_WIN32)
    FILE* pipe = nullptr;
    void* proc = nullptr;
    if (!StartHiddenProcess(cmd, /*wantStdinPipe=*/false, nullptr,
                             /*wantOutputCapture=*/true, &pipe, &proc))
        return false;

    char buf[1024];
    while (pipe && std::fgets(buf, sizeof(buf), pipe))
        outText += buf;
    if (pipe) std::fclose(pipe);
    int status = WaitHiddenProcess(proc);
    return status == 0;
#else
    FILE* pipe = PT_POPEN((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return false;

    char buf[1024];
    while (std::fgets(buf, sizeof(buf), pipe))
        outText += buf;
    int status = PT_PCLOSE(pipe);
    return status == 0;
#endif
}

static std::string TrimCopy(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Saca tags tipo <i>, </i>, <c.colorXXX>, etc. -- YouTube mete estos en
// subtitulos generados automaticamente para resaltar la palabra que se esta
// "hablando" en ese instante; no aportan nada como letra de cancion.
static std::string StripTags(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool inTag = false;
    for (char c : s) {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; continue; }
        if (!inTag) out += c;
    }
    return out;
}

// "00:00:01,000" -> milisegundos. Devuelve -1 si el formato no matchea.
static long long ParseSrtTimestampMs(const std::string& ts) {
    int h = 0, m = 0, s = 0, ms = 0;
    if (std::sscanf(ts.c_str(), "%d:%d:%d,%d", &h, &m, &s, &ms) != 4)
        return -1;
    return ((long long)h * 3600 + (long long)m * 60 + s) * 1000 + ms;
}

// Convierte el contenido de un .srt en texto plano apto como letra: sin
// numeros de cue ni timestamps, con tags de formato removidos, sin lineas
// duplicadas consecutivas (comunes en subtitulos auto-generados de
// YouTube, donde un cue repite la cola del anterior), y con una linea en
// blanco insertada cuando hay un salto largo entre cues (heuristica simple
// para separar en algo parecido a estrofas en vez de un bloque unico).
static std::string SrtToLyrics(const std::string& srtContent) {
    std::istringstream in(srtContent);
    std::string line;

    enum class State { ExpectIndexOrBlank, ExpectTimestamp, ReadingText };
    State state = State::ExpectIndexOrBlank;

    std::string      result;
    std::string      lastCueText;
    long long        lastCueEndMs = -1;
    long long        curStartMs = -1, curEndMs = -1;
    std::vector<std::string> curTextLines;

    auto flushCue = [&]() {
        if (curTextLines.empty()) return;
        std::string text;
        for (size_t i = 0; i < curTextLines.size(); ++i) {
            if (i > 0) text += ' ';
            text += curTextLines[i];
        }
        text = TrimCopy(StripTags(text));
        curTextLines.clear();
        if (text.empty()) return;

        // Deduplicado: subtitulos auto-generados suelen repetir la misma
        // linea (o una que empieza igual) en el cue siguiente mientras
        // "arrastra" texto en pantalla -- se salta si es identica a la
        // ultima que ya quedo en el resultado.
        if (text == lastCueText) {
            lastCueEndMs = curEndMs;
            return;
        }

        // Salto largo (>1.8s) entre el fin del cue anterior y el arranque
        // de este -- probable pausa musical/cambio de estrofa.
        if (lastCueEndMs >= 0 && curStartMs >= 0 && (curStartMs - lastCueEndMs) > 1800 && !result.empty())
            result += '\n';

        result += text;
        result += '\n';
        lastCueText  = text;
        lastCueEndMs = curEndMs;
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (state == State::ExpectIndexOrBlank) {
            if (line.empty()) continue;
            // Numero de cue (o cualquier otra cosa) -- no importa el
            // contenido exacto, solo que la linea SIGUIENTE sea el timestamp.
            state = State::ExpectTimestamp;
            continue;
        }
        if (state == State::ExpectTimestamp) {
            const size_t arrow = line.find("-->");
            if (arrow == std::string::npos) {
                // Formato inesperado -- se resetea en vez de colgarse leyendo mal.
                state = State::ExpectIndexOrBlank;
                continue;
            }
            curStartMs = ParseSrtTimestampMs(TrimCopy(line.substr(0, arrow)));
            curEndMs   = ParseSrtTimestampMs(TrimCopy(line.substr(arrow + 3)));
            state = State::ReadingText;
            continue;
        }
        // State::ReadingText
        if (line.empty()) {
            flushCue();
            state = State::ExpectIndexOrBlank;
            continue;
        }
        curTextLines.push_back(line);
    }
    flushCue(); // por si el archivo no termina con una linea en blanco

    return TrimCopy(result);
}

// Busca el primer archivo "sub.<lang>.srt" bajo <dir> con preferencia
// es > en > cualquier otro (el orden en que se pidieron en --sub-langs).
static std::string FindDownloadedSrt(const fs::path& dir) {
    std::vector<fs::path> found;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".srt")
            found.push_back(entry.path());
    }
    if (found.empty()) return "";

    auto pick = [&](const char* needle) -> std::string {
        for (const auto& p : found)
            if (p.filename().string().find(needle) != std::string::npos)
                return p.string();
        return "";
    };
    std::string es = pick(".es.");
    if (!es.empty()) return es;
    std::string en = pick(".en.");
    if (!en.empty()) return en;
    return found.front().string();
}

SubtitleFetchResult FetchSubtitlesAsLyrics(const std::string& url) {
    SubtitleFetchResult result;

    const std::string trimmedUrl = TrimCopy(url);
    if (!IsSafeUrl(trimmedUrl)) {
        result.error = "URL invalida: debe ser un link http(s) sin comillas ni caracteres especiales.";
        return result;
    }

    const std::string ytdlp = YtDlpPath();

    // ── Titulo del video (nombre sugerido para la cancion) ──────────────
    {
        std::string titleCmd = ytdlp + " --skip-download --no-playlist --print \"%(title)s\" \"" +
                                trimmedUrl + "\"";
        std::string out;
        RunProcessCaptureAll(titleCmd, out);
        std::istringstream in(out);
        std::string firstLine;
        std::getline(in, firstLine);
        firstLine = TrimCopy(firstLine);
        if (!firstLine.empty() && firstLine.rfind("ERROR", 0) != 0)
            result.title = firstLine;
    }
    if (result.title.empty())
        result.title = "Cancion importada";

    // ── Carpeta temporal propia para esta descarga ───────────────────────
    std::error_code ec;
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    fs::path tempDir = fs::temp_directory_path(ec) / ("proyecthor_sub_" + std::to_string(stamp));
    fs::create_directories(tempDir, ec);
    if (ec) {
        result.error = "No se pudo crear una carpeta temporal para la descarga.";
        return result;
    }

    // ── Descarga de subtitulos -- mismo criterio que subtitle-grabber: pide
    // manuales Y auto-generados, yt-dlp prioriza el manual si ambos existen
    // para el mismo idioma. --restrict-filenames + nombre fijo "sub" para
    // no tener que lidiar con caracteres raros del titulo real al buscar
    // el archivo despues. --ffmpeg-location apunta al ffmpeg empaquetado
    // (necesario para que yt-dlp pueda convertir a .srt si el origen no lo
    // entrega directo en ese formato).
    std::string outTemplate = (tempDir / "sub.%(ext)s").string();
    std::string dlCmd = ytdlp +
        " --write-subs --write-auto-subs --sub-langs \"es,en\" --sub-format srt"
        " --skip-download --no-playlist --restrict-filenames -o \"" + outTemplate + "\"";

    const std::string bundledDir = BundledToolsDir();
    if (!bundledDir.empty())
        dlCmd += " --ffmpeg-location \"" + bundledDir + "\"";

    dlCmd += " \"" + trimmedUrl + "\"";

    std::string dlOutput;
    RunProcessCaptureAll(dlCmd, dlOutput);

    const std::string srtPath = FindDownloadedSrt(tempDir);
    if (srtPath.empty()) {
        result.error = "No se encontraron subtitulos (ni manuales ni automaticos) en español o "
                        "ingles para ese video.";
        fs::remove_all(tempDir, ec);
        return result;
    }

    std::ifstream f(srtPath, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    f.close();

    result.lyrics  = SrtToLyrics(ss.str());
    result.success = !result.lyrics.empty();
    if (!result.success)
        result.error = "El archivo de subtitulos se descargo pero quedo vacio despues de procesarlo.";

    fs::remove_all(tempDir, ec);
    return result;
}

} // namespace ProyecThor::Core
