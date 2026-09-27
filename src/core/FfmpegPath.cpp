#include "FfmpegPath.h"
#include <cstdio>

#if defined(_WIN32)
    #include <windows.h>
    #define PT_POPEN  _popen
    #define PT_PCLOSE _pclose
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace ProyecThor::Core {

std::string FfmpegPath() {
#if defined(_WIN32)
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string path(exePath);
        size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos) {
            std::string candidate = path.substr(0, slash + 1) + "ffmpeg.exe";
            FILE* f = std::fopen(candidate.c_str(), "rb");
            if (f) { std::fclose(f); return "\"" + candidate + "\""; }
        }
    }
    return "ffmpeg"; // fallback: PATH del sistema, por si no esta empaquetado
#else
    return "ffmpeg";
#endif
}

bool FfmpegAvailable(const std::string& ffmpegPath) {
    FILE* probe = PT_POPEN((ffmpegPath + " -version").c_str(), "r");
    if (!probe) return false;

    char buf[256];
    bool found = false;
    if (std::fgets(buf, sizeof(buf), probe) && std::string(buf).find("ffmpeg") != std::string::npos)
        found = true;

    PT_PCLOSE(probe);
    return found;
}

} // namespace ProyecThor::Core
