#pragma once

#include <string>
#include <functional>
#include <vector>

namespace ProyecThor::Core {

class FileDeletionManager {
public:
    using UsageReleaseHook = std::function<void(const std::string& fullPath)>;

    // Registra un callback de subsistema para ser notificado antes de eliminar un archivo
    static void RegisterUsageReleaseHook(UsageReleaseHook hook);

    // Detiene y libera todos los usos internos (PresentationCore, preview, audio, live background, etc.)
    static void ReleaseAllUsages(const std::string& fullPath);

    // Elimina forzadamente un archivo de disco, liberando bloqueos internos y del sistema operativo
    static bool ForceDeleteFile(const std::string& path);

    // Elimina forzadamente un directorio de disco, liberando bloqueos de su contenido
    static bool ForceDeleteDirectory(const std::string& path);
};

} // namespace ProyecThor::Core

