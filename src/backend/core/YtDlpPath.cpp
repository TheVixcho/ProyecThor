#include "YtDlpPath.h"
#include <cstdio>

#if defined(_WIN32)
    #include <windows.h>
#endif

namespace ProyecThor::Core {

std::string YtDlpPath() {
#if defined(_WIN32)
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string path(exePath);
        size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos) {
            std::string candidate = path.substr(0, slash + 1) + "yt-dlp.exe";
            FILE* f = std::fopen(candidate.c_str(), "rb");
            if (f) { std::fclose(f); return "\"" + candidate + "\""; }
        }
    }
    return "yt-dlp"; // fallback: PATH del sistema, por si no esta empaquetado
#else
    return "yt-dlp";
#endif
}

std::string BundledToolsDir() {
#if defined(_WIN32)
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
        std::string path(exePath);
        size_t slash = path.find_last_of("\\/");
        if (slash != std::string::npos)
            return path.substr(0, slash);
    }
#endif
    return "";
}

} // namespace ProyecThor::Core
