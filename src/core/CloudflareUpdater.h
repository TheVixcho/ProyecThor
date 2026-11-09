#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>

namespace ProyecThor::Core {

struct CloudflareProbeResult {
    bool        success         = false;
    int         statusCode      = 0;
    float       fileSizeMB      = 0.0f;
    int         latencyMs       = 0;
    std::string detectedUrl;
    std::string detectedVersion;
    std::string errorMsg;
    std::string serverHeader;
    std::string contentType;
};

struct CloudflareDownloadState {
    std::atomic<bool>  isDownloading{false};
    std::atomic<float> progress{0.0f};      // 0.0 a 1.0
    std::atomic<float> speedMBs{0.0f};      // MB/s
    std::atomic<float> downloadedMB{0.0f};  // MB
    std::atomic<float> totalMB{0.0f};       // MB
    std::atomic<bool>  cancelRequested{false};
    std::string        currentFilename;
    std::string        errorMsg;
    std::string        savedFilePath;
};

class CloudflareUpdater {
public:
    static CloudflareUpdater& Get();

    // Normaliza una URL (remueve barras finales, extrae la base si termina en .exe)
    static std::string NormalizeBaseUrl(const std::string& inputUrl);

    // Valida si la URL parece ser del panel interno de Cloudflare (dash.cloudflare.com)
    static bool IsDashboardUrl(const std::string& url);

    // Genera el nombre del ejecutable segun la version (ej. "ProyecThor_Setup_1.1.3.exe")
    static std::string GetExeFilename(const std::string& version);

    // Genera la version siguiente incrementando el parche (ej. "1.1.3" -> "1.1.4")
    static std::string GetNextPatchVersion(const std::string& currentVersion);

    // Compara dos versiones en formato SemVer; retorna true si 'remoteVersion' es mayor que 'localVersion'
    static bool IsVersionNewer(const std::string& localVersion, const std::string& remoteVersion);

    // Genera una lista de versiones candidatas futuras a sondear (parches, minors y majors)
    static std::vector<std::string> GetCandidateVersions(const std::string& currentVersion);

    // Sondeo sincrónico y ligero de un ejecutable en Cloudflare R2 (HEAD request)
    static CloudflareProbeResult ProbeExecutableSync(const std::string& baseUrl, const std::string& version);

    // Prueba si el ejecutable está disponible en Cloudflare R2 / CDN (asíncrono)
    void ProbeExecutableAsync(const std::string& baseUrl, const std::string& version,
                              std::function<void(const CloudflareProbeResult& result)> onComplete);

    // Sondea candidatos concurrentemente buscando la versión más alta disponible en Cloudflare
    void FindHighestAvailableUpdateAsync(const std::string& baseUrl, const std::string& currentVersion,
                                         std::function<void(bool found, const std::string& highestVer,
                                                            const std::string& downloadUrl, float sizeMB,
                                                            const std::vector<std::string>& allFound)> onComplete);

    // Comprueba si hay una nueva versión en Cloudflare buscando el archivo o version.json (asíncrono)
    void CheckForUpdateAsync(const std::string& baseUrl, const std::string& currentVersion,
                             std::function<void(bool updateAvailable, const std::string& newVer,
                                                const std::string& downloadUrl, const std::string& msg)> onComplete);

    // Inicia la descarga del instalador; si launchInstaller es true, lanza el .exe y sale al completar
    void StartDownload(const std::string& url, const std::string& targetVersion, bool launchInstaller,
                       std::function<void(bool success, const std::string& pathOrErr)> onFinished = nullptr);

    void CancelDownload();

    CloudflareDownloadState& GetDownloadState() { return m_DownloadState; }
    const CloudflareDownloadState& GetDownloadState() const { return m_DownloadState; }

    // Ejecuta el instalador con privilegios normales y sale de la aplicación
    static bool LaunchInstallerAndExit(const std::string& installerPath);

private:
    CloudflareUpdater() = default;
    ~CloudflareUpdater() = default;
    CloudflareUpdater(const CloudflareUpdater&) = delete;
    CloudflareUpdater& operator=(const CloudflareUpdater&) = delete;

    CloudflareDownloadState m_DownloadState;
};

} // namespace ProyecThor::Core
