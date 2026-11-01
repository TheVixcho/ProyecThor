#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <atomic>

namespace ProyecThor::Core {

enum class MuninResourceType {
    All,
    Video,
    Image,
    Bible,
    Song
};

struct MuninResource {
    std::string id;
    MuninResourceType type = MuninResourceType::Video;
    std::string typeStr; // "video", "image", "bible", "song"
    std::string title;
    std::string author;
    std::string sizeStr;
    std::string downloadUrl;
    std::string thumbnailUrl;
    std::string filename;
    std::vector<std::string> tags;
    std::string description;
};

class MuninCloudManager {
public:
    static MuninCloudManager& Get();

    void Initialize();

    // ── Catálogo ──
    void RefreshCatalog(std::function<void(bool success, const std::string& msg)> onDone = nullptr);
    bool IsBusy() const { return m_IsBusy.load(); }
    std::string GetStatusMessage() const;

    std::vector<MuninResource> GetResources(MuninResourceType filterType = MuninResourceType::All,
                                            const std::string& search = "") const;

    // ── Instalación / Descarga ──
    bool IsResourceInstalled(const MuninResource& res) const;
    void DownloadResource(const MuninResource& res,
                          std::function<void(bool success, const std::string& msg)> onDone = nullptr);
    std::string ResolveTargetPath(const MuninResource& res) const;

    float GetDownloadProgress(const std::string& resId) const;
    bool IsDownloading(const std::string& resId) const;

    // ── Descarga silenciosa sin ventanas de consola (WinHTTP / No Window) ──
    static bool DownloadFileSilent(const std::string& url,
                                   const std::string& destPath,
                                   std::function<void(float progress)> onProgress = nullptr);

    // ── Configuración de URL Cloudflare R2 ──
    std::string GetCatalogUrl() const;
    void SetCatalogUrl(const std::string& url);

private:
    MuninCloudManager();
    ~MuninCloudManager() = default;

    void LoadCachedCatalog();
    void SaveCachedCatalog(const std::string& rawJson);
    void PopulateDefaultFallbackCatalog();

    mutable std::mutex m_Mutex;
    std::vector<MuninResource> m_Resources;
    std::unordered_map<std::string, float> m_DownloadProgress;
    std::unordered_map<std::string, bool> m_ActiveDownloads;

    std::atomic<bool> m_IsBusy{false};
    std::string m_StatusMessage;
    std::string m_CatalogUrl;
};

} // namespace ProyecThor::Core
