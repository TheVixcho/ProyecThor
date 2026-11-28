#include "SlintApp.h"
#include <clocale>
#include <cstdlib>
#include <cstring>

#ifdef __APPLE__
#include <filesystem>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>
#include <vector>
#endif

namespace {

void SetEnvIfUnset(const char* key, const char* value, bool overwrite = false) {
#ifdef _WIN32
    if (!overwrite) {
        const char* cur = std::getenv(key);
        if (cur && *cur) return;
    }
    _putenv_s(key, value);
#else
    setenv(key, value, overwrite ? 1 : 0);
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    // Requerido por libmpv en sistemas con locale decimal con coma (es_CL, es_MX, es_ES)
    std::setlocale(LC_NUMERIC, "C");

    // Escape seguro de renderizado por software (--software-renderer / --safe-mode / env)
    // para máquinas virtuales o dispositivos ARM sin drivers gráficos completos.
    bool forceSoftware = false;
    if (const char* swEnv = std::getenv("PROYECTHOR_SOFTWARE_RENDERER")) {
        if (std::strcmp(swEnv, "1") == 0 || std::strcmp(swEnv, "true") == 0) {
            forceSoftware = true;
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && (std::strcmp(argv[i], "--software-renderer") == 0 ||
                        std::strcmp(argv[i], "--safe-mode") == 0)) {
            forceSoftware = true;
        }
    }

    if (forceSoftware) {
        SetEnvIfUnset("SLINT_BACKEND", "winit-software", true);
    } else {
#if defined(__APPLE__) || (defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__)))
        // En macOS (Apple Silicon & Intel) y Windows ARM64, seleccionar el backend 'winit'
        // prioriza automáticamente WinitSkiaRenderer (Metal CAMetalLayer nativo en macOS y
        // Direct3D 12/DXGI en Windows ARM64) manteniendo fallback automático a FemtoVG/Software.
        SetEnvIfUnset("SLINT_BACKEND", "winit", false);
#endif
    }

#ifdef __APPLE__
    // Al iniciar desde ProyecThor.app/Contents/MacOS/ProyecThor:
    // 1) Añadir Contents/MacOS (ffmpeg empaquetado) y rutas Homebrew al PATH.
    // 2) Cambiar directorio de trabajo a Contents/Resources si contiene assets/.
    uint32_t exeSize = 0;
    _NSGetExecutablePath(nullptr, &exeSize);
    if (exeSize > 0) {
        std::vector<char> exeBuf(exeSize + 1, '\0');
        if (_NSGetExecutablePath(exeBuf.data(), &exeSize) == 0) {
            std::error_code ec;
            std::filesystem::path exePath = std::filesystem::weakly_canonical(exeBuf.data(), ec);
            std::filesystem::path exeDir  = exePath.parent_path();

            const char* oldPath = std::getenv("PATH");
            std::string newPath = exeDir.string() + ":/opt/homebrew/bin:/usr/local/bin";
            if (oldPath && *oldPath) {
                newPath += ":";
                newPath += oldPath;
            }
            setenv("PATH", newPath.c_str(), 1);

            std::filesystem::path resDir = exeDir.parent_path() / "Resources";
            if (std::filesystem::exists(resDir, ec) && std::filesystem::is_directory(resDir, ec)) {
                (void)chdir(resDir.c_str());
            } else if (std::filesystem::exists(exeDir, ec)) {
                (void)chdir(exeDir.c_str());
            }
        }
    }
#endif

    ProyecThor::SlintUI::SlintApp app;
    return app.Run();
}
