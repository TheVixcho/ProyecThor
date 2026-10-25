#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace ProyecThor::Core {

struct ProChatMessage {
    std::string role;    // "user" o "assistant"
    std::string content;
};

struct CloudSong {
    std::string id;
    std::string title;
    std::string content;
    std::string updatedAt;
};

enum class AIProvider {
    Gemini = 0,
    DeepSeek = 1
};

class ProAIManager {
public:
    static ProAIManager& Get();

    void Initialize();
    void SaveQuota();
    void LoadQuota();

    // ── Proveedor Activo (Gemini por defecto / DeepSeek) ─────────────────────
    AIProvider GetCurrentProvider() const;
    void SetCurrentProvider(AIProvider p);

    // ── Seguridad de API Key ─────────────────────────────────────────────────
    // Retorna las claves descifradas en memoria únicamente al invocar la API
    std::string GetDecryptedGeminiKey() const;
    std::string GetDecryptedDeepSeekKey() const;
    std::string GetDecryptedApiKey() const; // Retorna la del proveedor activo
    void SetRemoteApiKey(const std::string& key);

    // ── Control de Cuotas y Tokens (Límites Pro) ──────────────────────────────
    // Control de límite de $5 USD en tokens DeepSeek y 30 imágenes mensuales
    void CheckMonthlyReset();

    bool     CanSendMessage() const;
    uint64_t GetAvailableTokens() const;
    uint64_t GetMaxMonthlyTokens() const;
    uint64_t GetUsedTokens() const;

    bool     CanGenerateImage() const;
    int      GetAvailableImages() const;
    int      GetMaxMonthlyImages() const;
    int      GetUsedImages() const;

    // ── Servicio de Chat IA (DeepSeek / Gemini) ──────────────────────────────
    bool IsChatBusy() const;
    void SendChatMessage(const std::vector<ProChatMessage>& history,
                         std::function<void(bool success, const std::string& reply, uint32_t tokensUsed, const std::string& error)> callback);

    // ── Corrección y Edición de Letras con IA ─────────────────────────────
    void EditLyricsWithAI(const std::string& currentLyrics,
                          const std::string& userPrompt,
                          std::function<void(bool success, const std::string& modifiedLyrics, uint32_t tokensUsed, const std::string& error)> callback);

    // ── Servicio de Generación de Imágenes ───────────────────────────────────
    bool IsImageBusy() const;
    void GenerateImage(const std::string& prompt,
                       std::function<void(bool success, const std::string& savedPath, const std::string& error)> callback);

    // ── Servicio de Nube de Canciones (Firebase) ─────────────────────────────
    bool IsCloudBusy() const;
    const std::vector<CloudSong>& GetCloudSongs() const;
    void RefreshCloudSongs(std::function<void(bool success, const std::string& msg)> callback = nullptr);
    void UploadCloudSong(const std::string& title, const std::string& content,
                         std::function<void(bool success, const std::string& msg)> callback = nullptr);
    void DeleteCloudSong(const std::string& songId,
                         std::function<void(bool success, const std::string& msg)> callback = nullptr);

    std::string GetAIImagesDir() const;

private:
    ProAIManager();
    ~ProAIManager() = default;
    ProAIManager(const ProAIManager&) = delete;
    ProAIManager& operator=(const ProAIManager&) = delete;

    std::string GetQuotaFilePath() const;
    std::string GetCloudSongsCachePath() const;
    std::string GetCurrentMonthKey() const;

    // Helper nativo HTTPS en Windows
    static std::string PerformHttpsPost(const std::string& host, const std::string& path,
                                        const std::string& postData, const std::string& bearerToken,
                                        int& outStatusCode, std::string& outError);

    static bool DownloadFileHttps(const std::string& urlStr, const std::string& localDestPath, std::string& outError);

    mutable std::mutex m_Mutex;

    AIProvider m_CurrentProvider = AIProvider::Gemini;

    // Tokens y cuotas mensuales
    // ~$5.00 USD en DeepSeek V3 equivalen a ~15,000,000 de tokens
    const uint64_t m_MaxMonthlyTokens = 15000000;
    const int      m_MaxMonthlyImages = 30;

    std::string m_CurrentMonthCycle;
    uint64_t    m_TokensUsedThisMonth = 0;
    int         m_ImagesUsedThisMonth = 0;

    std::string m_RemoteApiKeyOverride;

    std::atomic<bool> m_ChatBusy{false};
    std::atomic<bool> m_ImageBusy{false};
    std::atomic<bool> m_CloudBusy{false};

    std::vector<CloudSong> m_CloudSongs;
};

} // namespace ProyecThor::Core
