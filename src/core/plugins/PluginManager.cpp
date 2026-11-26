#include "PluginManager.h"
#include "core/AppPaths.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <future>
#include <array>
#include <cstdlib>

#if defined(_WIN32)
    #include <windows.h>
    #include <shellapi.h>
    #include "core/HiddenProcess.h"
    #define PT_POPEN  _popen
    #define PT_PCLOSE _pclose
#else
    #define PT_POPEN  popen
    #define PT_PCLOSE pclose
#endif

namespace fs = std::filesystem;

namespace ProyecThor::Core {

namespace {

void OpenFolderInExplorer(const std::string& path) {
#if defined(_WIN32)
    std::wstring wpath;
    int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (len > 0) {
        wpath.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);
        ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
#elif defined(__APPLE__)
    std::string cmd = "open \"" + path + "\" &";
    std::system(cmd.c_str());
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
    std::system(cmd.c_str());
#endif
}

std::string SanitizeArgument(const std::string& arg) {
    std::string safe = arg;
    // Evitar secuencias peligrosas
    for (char& c : safe) {
        if (c == '"' || c == '`' || c == '$' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return safe;
}

} // namespace

PluginManager& PluginManager::Get() {
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager() = default;
PluginManager::~PluginManager() = default;

void PluginManager::Initialize() {
    if (m_Initialized.exchange(true)) return;

    // Asegurar que la carpeta de plugins primaria exista
    std::string primaryDir = GetPrimaryPluginsDir();
    std::error_code ec;
    fs::create_directories(primaryDir, ec);

    ReloadPlugins();
}

std::string PluginManager::GetPrimaryPluginsDir() const {
    // 1. Probar ./plugins en el directorio de trabajo actual (desarrollo / portable)
    std::error_code ec;
    fs::path localPlugins = fs::current_path(ec) / "plugins";
    if (fs::exists(localPlugins, ec) || !ec) {
        return localPlugins.string();
    }

    // 2. Carpeta de datos de usuario de ProyecThor
    fs::path appDataPlugins = fs::path(ProyecThor::GetAppDataRoot()) / "plugins";
    return appDataPlugins.string();
}

std::vector<std::string> PluginManager::GetSearchPaths() const {
    std::vector<std::string> paths;
    std::error_code ec;

    // 1. plugins/ en el directorio actual
    fs::path p1 = fs::current_path(ec) / "plugins";
    if (!ec) paths.push_back(p1.string());

    // 2. plugins/ en AppData
    fs::path p2 = fs::path(ProyecThor::GetAppDataRoot()) / "plugins";
    paths.push_back(p2.string());

    // Desduplicar usando canonical si existen
    std::vector<std::string> uniquePaths;
    for (const auto& p : paths) {
        bool duplicate = false;
        for (const auto& u : uniquePaths) {
            std::error_code eqEc;
            if (fs::exists(p, eqEc) && fs::exists(u, eqEc) && fs::equivalent(p, u, eqEc)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            uniquePaths.push_back(p);
        }
    }

    return uniquePaths;
}

void PluginManager::OpenPluginsFolder() const {
    std::string p = GetPrimaryPluginsDir();
    std::error_code ec;
    fs::create_directories(p, ec);
    OpenFolderInExplorer(p);
}

void PluginManager::OpenPluginFolder(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& pl : m_Plugins) {
        if (pl->id == pluginId) {
            fs::path path(pl->path);
            if (pl->isFolder) {
                OpenFolderInExplorer(path.string());
            } else {
                OpenFolderInExplorer(path.parent_path().string());
            }
            return;
        }
    }
}

void PluginManager::ReloadPlugins() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Plugins.clear();

    auto searchPaths = GetSearchPaths();
    for (const auto& sp : searchPaths) {
        ScanDirectory(sp);
    }

    std::cout << "[PluginManager] Se han cargado " << m_Plugins.size() << " plugin(s).\n";
}

void PluginManager::ScanDirectory(const std::string& dirPath) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;

        const auto& path = entry.path();
        std::string filename = path.filename().string();
        if (filename.empty() || filename[0] == '.') continue; // Ignorar ocultos

        if (entry.is_directory()) {
            PluginInfo info;
            if (LoadPluginFromFolder(path.string(), info)) {
                // Verificar que no esté ya añadido
                bool exists = false;
                for (const auto& p : m_Plugins) {
                    if (p->id == info.id) { exists = true; break; }
                }
                if (!exists) {
                    m_Plugins.push_back(std::make_shared<PluginInfo>(info));
                }
            }
        } else if (entry.is_regular_file()) {
            std::string ext = path.extension().string();
            // Solo considerar archivos con extensiones ejecutables o de script
            if (ext == ".py" || ext == ".sh" || ext == ".bat" || ext == ".exe") {
                PluginInfo info;
                if (LoadPluginFromFile(path.string(), info)) {
                    bool exists = false;
                    for (const auto& p : m_Plugins) {
                        if (p->id == info.id) { exists = true; break; }
                    }
                    if (!exists) {
                        m_Plugins.push_back(std::make_shared<PluginInfo>(info));
                    }
                }
            }
        }
    }
}

bool PluginManager::LoadPluginFromFolder(const std::string& folderPath, PluginInfo& info) {
    fs::path folder(folderPath);
    fs::path manifestPath = folder / "plugin.json";

    std::error_code ec;
    if (fs::exists(manifestPath, ec)) {
        try {
            std::ifstream f(manifestPath);
            nlohmann::json j = nlohmann::json::parse(f);

            info.id          = j.value("id", folder.filename().string());
            info.name        = j.value("name", info.id);
            info.version     = j.value("version", "1.0.0");
            info.author      = j.value("author", "Comunidad");
            info.description = j.value("description", "");
            info.icon        = j.value("icon", "puzzle");
            info.path        = folder.string();
            info.entrypoint  = j.value("entrypoint", "main.py");
            info.isFolder    = true;
            info.isEnabled   = true;
            info.isBuiltin   = (info.id == "ytdlp");

            if (j.contains("actions") && j["actions"].is_array()) {
                for (const auto& act : j["actions"]) {
                    PluginAction pa;
                    pa.id          = act.value("id", "");
                    pa.name        = act.value("name", pa.id);
                    pa.description = act.value("description", "");
                    pa.command     = act.value("command", "");
                    if (act.contains("args") && act["args"].is_array()) {
                        for (const auto& a : act["args"]) {
                            pa.args.push_back(a.get<std::string>());
                        }
                    }
                    if (!pa.id.empty()) {
                        info.actions.push_back(pa);
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[PluginManager] Error al parsear plugin.json en " << folderPath << ": " << e.what() << "\n";
        }
    }

    // Carpeta sin plugin.json: detectar si tiene algún script principal
    for (const auto& entry : fs::directory_iterator(folder, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string fname = entry.path().filename().string();
        std::string ext   = entry.path().extension().string();
        if (ext == ".py" || ext == ".sh" || ext == ".bat" || ext == ".exe") {
            info.id          = folder.filename().string();
            info.name        = info.id;
            info.version     = "1.0.0";
            info.author      = "Usuario";
            info.description = "Plugin de usuario en carpeta " + info.id;
            info.icon        = "folder";
            info.path        = folder.string();
            info.entrypoint  = fname;
            info.isFolder    = true;
            info.isEnabled   = true;
            info.isBuiltin   = false;

            PluginAction pa;
            pa.id          = "run";
            pa.name        = "Ejecutar Script Principal";
            pa.description = "Ejecuta " + fname;
            info.actions.push_back(pa);
            return true;
        }
    }

    return false;
}

bool PluginManager::LoadPluginFromFile(const std::string& filePath, PluginInfo& info) {
    fs::path p(filePath);
    info.id          = p.stem().string();
    info.name        = p.stem().string();
    info.version     = "1.0.0";
    info.author      = "Usuario";
    info.description = "Script de usuario independiente (" + p.filename().string() + ")";
    info.icon        = "script";
    info.path        = p.string();
    info.entrypoint  = p.filename().string();
    info.isFolder    = false;
    info.isEnabled   = true;
    info.isBuiltin   = false;

    PluginAction pa;
    pa.id          = "run";
    pa.name        = "Ejecutar";
    pa.description = "Ejecuta el archivo directamente";
    info.actions.push_back(pa);
    return true;
}

std::vector<std::shared_ptr<PluginInfo>> PluginManager::GetPlugins() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Plugins;
}

bool PluginManager::HasPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& p : m_Plugins) {
        if (p->id == pluginId && p->isEnabled) return true;
    }
    return false;
}

std::shared_ptr<PluginInfo> PluginManager::GetPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto& p : m_Plugins) {
        if (p->id == pluginId) return p;
    }
    return nullptr;
}

void PluginManager::SetPluginEnabled(const std::string& pluginId, bool enabled) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& p : m_Plugins) {
        if (p->id == pluginId) {
            p->isEnabled = enabled;
            break;
        }
    }
}

bool PluginManager::DeletePlugin(const std::string& pluginId) {
    std::string pathToRemove;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto& p : m_Plugins) {
            if (p->id == pluginId) {
                pathToRemove = p->path;
                break;
            }
        }
    }

    if (pathToRemove.empty()) return false;

    std::error_code ec;
    fs::remove_all(pathToRemove, ec);
    ReloadPlugins();
    return !ec;
}

bool PluginManager::InstallPluginFromPath(const std::string& sourcePath, std::string* outMsg) {
    std::error_code ec;
    fs::path src(sourcePath);
    if (!fs::exists(src, ec)) {
        if (outMsg) *outMsg = "El archivo o carpeta de origen no existe.";
        return false;
    }

    fs::path destDir(GetPrimaryPluginsDir());
    fs::create_directories(destDir, ec);

    fs::path target = destDir / src.filename();
    try {
        if (fs::is_directory(src, ec)) {
            fs::copy(src, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        } else {
            fs::copy_file(src, target, fs::copy_options::overwrite_existing);
        }
        ReloadPlugins();
        std::string successMsg = "Plugin instalado con éxito: " + src.filename().string();
        SetStatusMessage(successMsg);
        if (outMsg) *outMsg = successMsg;
        return true;
    } catch (const std::exception& e) {
        std::string err = "Error al copiar plugin: " + std::string(e.what());
        SetStatusMessage(err);
        if (outMsg) *outMsg = err;
        return false;
    }
}

std::string PluginManager::BuildCommandLine(const PluginInfo& info, const std::string& actionId,
                                            const std::vector<std::pair<std::string, std::string>>& params,
                                            const std::string& customArgs) const
{
    fs::path entry(info.entrypoint);
    std::string ext = entry.extension().string();

    std::string baseExec;
    if (ext == ".py") {
#if defined(_WIN32)
        baseExec = "python \"" + info.entrypoint + "\"";
#else
        baseExec = "python3 \"" + info.entrypoint + "\"";
#endif
    } else if (ext == ".sh") {
        baseExec = "bash \"" + info.entrypoint + "\"";
    } else if (ext == ".bat") {
        baseExec = "cmd /c \"" + info.entrypoint + "\"";
    } else {
        baseExec = "\"" + info.entrypoint + "\"";
    }

    std::string fullCmd = baseExec;
    if (!actionId.empty()) {
        fullCmd += " --action " + actionId;
    }

    for (const auto& [key, val] : params) {
        if (!key.empty()) {
            fullCmd += " --" + key + " \"" + SanitizeArgument(val) + "\"";
        }
    }

    if (!customArgs.empty()) {
        fullCmd += " " + customArgs;
    }

    return fullCmd;
}

void PluginManager::ExecuteActionAsync(const std::string& pluginId,
                                       const std::string& actionId,
                                       const std::vector<std::pair<std::string, std::string>>& params,
                                       std::function<void(const PluginExecutionResult&)> onComplete,
                                       std::function<void(const std::string&)> onLog)
{
    std::shared_ptr<PluginInfo> plugin = GetPlugin(pluginId);
    if (!plugin) {
        PluginExecutionResult res;
        res.success = false;
        res.error   = "Plugin no encontrado: " + pluginId;
        if (onComplete) onComplete(res);
        return;
    }

    if (!plugin->isEnabled) {
        PluginExecutionResult res;
        res.success = false;
        res.error   = "El plugin está deshabilitado.";
        if (onComplete) onComplete(res);
        return;
    }

    std::string command = BuildCommandLine(*plugin, actionId, params);
    fs::path workDir = plugin->isFolder ? fs::path(plugin->path) : fs::path(plugin->path).parent_path();

    std::thread([this, plugin, command, workDir, onComplete, onLog]() {
        plugin->isRunning = true;
        std::string startMsg = ">> Ejecutando: " + command;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            plugin->consoleLogs.push_back(startMsg);
        }
        if (onLog) onLog(startMsg);

        // Armar comando con cd a su carpeta de trabajo
#if defined(_WIN32)
        std::string fullCmd = "cd /d \"" + workDir.string() + "\" && " + command;
#else
        std::string fullCmd = "cd \"" + workDir.string() + "\" && " + command + " 2>&1";
#endif

#if defined(_WIN32)
        // En Windows usar HiddenProcess con captura de stdout+stderr combinados
        // para no mostrar ventana de consola del plugin.
        FILE* outputPipe = nullptr;
        void* hProc      = nullptr;
        bool started = ProyecThor::Core::StartHiddenProcess(
            fullCmd, false, nullptr, true, &outputPipe, &hProc);

        PluginExecutionResult result;
        if (!started || !outputPipe) {
            if (hProc) ProyecThor::Core::WaitHiddenProcess(hProc);
            result.success = false;
            result.error   = "No se pudo iniciar el proceso del plugin.";
            plugin->isRunning = false;
            if (onComplete) onComplete(result);
            return;
        }

        std::array<char, 1024> buffer;
        std::string combinedOutput;
        while (std::fgets(buffer.data(), buffer.size(), outputPipe) != nullptr) {
            std::string line = buffer.data();
            combinedOutput += line;
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                plugin->consoleLogs.push_back(line);
                if (plugin->consoleLogs.size() > 500)
                    plugin->consoleLogs.erase(plugin->consoleLogs.begin(), plugin->consoleLogs.begin() + 100);
            }
            if (onLog) onLog(line);
        }
        std::fclose(outputPipe);
        int exitCode = ProyecThor::Core::WaitHiddenProcess(hProc);
#else
        FILE* pipe = PT_POPEN(fullCmd.c_str(), "r");
        PluginExecutionResult result;

        if (!pipe) {
            result.success = false;
            result.error   = "No se pudo iniciar el proceso del plugin.";
            plugin->isRunning = false;
            if (onComplete) onComplete(result);
            return;
        }

        std::array<char, 1024> buffer;
        std::string combinedOutput;

        while (std::fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            std::string line = buffer.data();
            combinedOutput += line;
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();

            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                plugin->consoleLogs.push_back(line);
                if (plugin->consoleLogs.size() > 500) {
                    plugin->consoleLogs.erase(plugin->consoleLogs.begin(), plugin->consoleLogs.begin() + 100);
                }
            }
            if (onLog) onLog(line);
        }

        int exitCode = PT_PCLOSE(pipe);
#endif
        result.exitCode = exitCode;
        result.output   = combinedOutput;

        // Intentar parsear JSON de la salida
        try {
            // Buscar inicio '{' y fin '}'
            size_t startIdx = combinedOutput.find('{');
            size_t endIdx   = combinedOutput.rfind('}');
            if (startIdx != std::string::npos && endIdx != std::string::npos && endIdx > startIdx) {
                std::string jsonStr = combinedOutput.substr(startIdx, endIdx - startIdx + 1);
                result.jsonData = nlohmann::json::parse(jsonStr);
                if (result.jsonData.contains("success")) {
                    result.success = result.jsonData["success"].get<bool>();
                } else {
                    result.success = (exitCode == 0);
                }
                if (!result.success && result.jsonData.contains("error")) {
                    result.error = result.jsonData["error"].get<std::string>();
                }
            } else {
                result.success = (exitCode == 0);
                if (!result.success) result.error = combinedOutput;
            }
        } catch (...) {
            result.success = (exitCode == 0);
            if (!result.success) result.error = combinedOutput;
        }

        plugin->isRunning    = false;
        plugin->lastExitCode = exitCode;
        plugin->lastOutput   = result.output;
        plugin->lastError    = result.error;

        std::string endMsg = ">> Finalizado con código de salida: " + std::to_string(exitCode);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            plugin->consoleLogs.push_back(endMsg);
        }
        if (onLog) onLog(endMsg);

        if (onComplete) onComplete(result);
    }).detach();
}

void PluginManager::ExecuteCustomCommandAsync(const std::string& pluginId,
                                              const std::string& customArgs,
                                              std::function<void(const PluginExecutionResult&)> onComplete,
                                              std::function<void(const std::string&)> onLog)
{
    ExecuteActionAsync(pluginId, "", {}, onComplete, onLog);
}

PluginExecutionResult PluginManager::ExecuteActionSync(const std::string& pluginId,
                                                       const std::string& actionId,
                                                       const std::vector<std::pair<std::string, std::string>>& params,
                                                       int timeoutSeconds)
{
    auto promise = std::make_shared<std::promise<PluginExecutionResult>>();
    auto future  = promise->get_future();

    ExecuteActionAsync(pluginId, actionId, params, [promise](const PluginExecutionResult& res) {
        promise->set_value(res);
    });

    if (future.wait_for(std::chrono::seconds(timeoutSeconds)) == std::future_status::ready) {
        return future.get();
    }

    PluginExecutionResult timeoutRes;
    timeoutRes.success = false;
    timeoutRes.error   = "Tiempo de espera agotado al ejecutar el plugin (" + std::to_string(timeoutSeconds) + "s).";
    return timeoutRes;
}

std::string PluginManager::ResolveStreamUrl(const std::string& url) {
    if (!HasPlugin("ytdlp")) {
        std::cerr << "[PluginManager] Plugin 'ytdlp' no disponible para resolver la URL.\n";
        return "";
    }

    auto res = ExecuteActionSync("ytdlp", "resolve", {{"url", url}}, 14);
    if (res.success && res.jsonData.is_object() && res.jsonData.contains("url")) {
        return res.jsonData["url"].get<std::string>();
    }

    if (!res.error.empty()) {
        std::cerr << "[PluginManager] Error al resolver stream: " << res.error << "\n";
    }
    return "";
}

PluginManager::LyricsResult PluginManager::FetchLyricsFromUrl(const std::string& url) {
    LyricsResult result;
    if (!HasPlugin("ytdlp")) {
        result.success = false;
        result.error = "El plugin 'yt-dlp' no está instalado en la carpeta plugins/. "
                       "Ve a Herramientas > Plugins para instalarlo o configurarlo.";
        return result;
    }

    auto res = ExecuteActionSync("ytdlp", "lyrics", {{"url", url}}, 30);
    if (res.success && res.jsonData.is_object()) {
        result.success = true;
        result.title   = res.jsonData.value("title", "Canción importada");
        result.lyrics  = res.jsonData.value("lyrics", "");
        return result;
    }

    result.success = false;
    result.error   = !res.error.empty() ? res.error : "No se pudieron obtener subtítulos para este video.";
    return result;
}

void PluginManager::ClearLogs(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& p : m_Plugins) {
        if (p->id == pluginId) {
            p->consoleLogs.clear();
            break;
        }
    }
}

std::string PluginManager::GetStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_StatusMessage;
}

void PluginManager::SetStatusMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_StatusMessage = msg;
}

} // namespace ProyecThor::Core
