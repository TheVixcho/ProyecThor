#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

namespace ProyecThor::Core {

struct PluginAction {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> args;
    std::string command;
};

struct PluginInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string icon;
    std::string path;          // Ruta absoluta a la carpeta o archivo
    std::string entrypoint;    // Nombre de archivo o comando de entrada
    bool isFolder = true;
    bool isEnabled = true;
    bool isBuiltin = false;
    std::vector<PluginAction> actions;

    // Estado en ejecución
    bool isRunning = false;
    int lastExitCode = 0;
    std::string lastOutput;
    std::string lastError;
    std::vector<std::string> consoleLogs;
};

struct PluginExecutionResult {
    bool success = false;
    int exitCode = -1;
    std::string output;
    std::string error;
    nlohmann::json jsonData;
};

class PluginManager {
public:
    static PluginManager& Get();

    void Initialize();
    void ReloadPlugins();

    // Rutas de búsqueda y carpetas
    std::vector<std::string> GetSearchPaths() const;
    std::string GetPrimaryPluginsDir() const;
    void OpenPluginsFolder() const;
    void OpenPluginFolder(const std::string& pluginId) const;

    // Lista y consulta de plugins
    std::vector<std::shared_ptr<PluginInfo>> GetPlugins();
    bool HasPlugin(const std::string& pluginId);
    std::shared_ptr<PluginInfo> GetPlugin(const std::string& pluginId);
    void SetPluginEnabled(const std::string& pluginId, bool enabled);
    bool DeletePlugin(const std::string& pluginId);

    // Instalación (desde drag & drop o selector de archivos)
    bool InstallPluginFromPath(const std::string& sourcePath, std::string* outMsg = nullptr);

    // Ejecución síncrona y asíncrona
    PluginExecutionResult ExecuteActionSync(const std::string& pluginId,
                                            const std::string& actionId,
                                            const std::vector<std::pair<std::string, std::string>>& params = {},
                                            int timeoutSeconds = 15);

    void ExecuteActionAsync(const std::string& pluginId,
                            const std::string& actionId,
                            const std::vector<std::pair<std::string, std::string>>& params,
                            std::function<void(const PluginExecutionResult&)> onComplete = nullptr,
                            std::function<void(const std::string&)> onLog = nullptr);

    void ExecuteCustomCommandAsync(const std::string& pluginId,
                                   const std::string& customArgs,
                                   std::function<void(const PluginExecutionResult&)> onComplete = nullptr,
                                   std::function<void(const std::string&)> onLog = nullptr);

    // Métodos de alto nivel desacoplados del núcleo:
    // 1. Resolución de URL de video directo (reemplazo de yt-dlp en MPVBasePlayer)
    std::string ResolveStreamUrl(const std::string& url);

    // 2. Extracción de letra/subtítulos (reemplazo de yt-dlp en SubtitleImporter)
    struct LyricsResult {
        bool success = false;
        std::string title;
        std::string lyrics;
        std::string error;
    };
    LyricsResult FetchLyricsFromUrl(const std::string& url);

    // Logs y mensajes de estado para la UI
    void ClearLogs(const std::string& pluginId);
    std::string GetStatusMessage() const;
    void SetStatusMessage(const std::string& msg);

private:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void ScanDirectory(const std::string& dirPath);
    bool LoadPluginFromFolder(const std::string& folderPath, PluginInfo& info);
    bool LoadPluginFromFile(const std::string& filePath, PluginInfo& info);
    std::string BuildCommandLine(const PluginInfo& info, const std::string& actionId,
                                 const std::vector<std::pair<std::string, std::string>>& params,
                                 const std::string& customArgs = "") const;

    mutable std::mutex m_Mutex;
    std::vector<std::shared_ptr<PluginInfo>> m_Plugins;
    std::string m_StatusMessage;
    std::atomic<bool> m_Initialized{false};
};

} // namespace ProyecThor::Core
