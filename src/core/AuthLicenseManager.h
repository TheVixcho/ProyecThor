#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace ProyecThor::Core {

struct UserProfile {
    std::string localId;
    std::string email;
    std::string displayName;
    std::string idToken;
    std::string refreshToken;
};

struct LicenseInfo {
    bool        isValid         = false;
    std::string key;
    std::string status;          // "active", "inactive", "expired", etc.
    std::string instanceId;
    std::string instanceName;
    std::string customerName;
    std::string customerEmail;
    std::string productName;
    std::string expiryDate;      // Human-readable date string (e.g., "25/11/2026 18:30")
    int64_t     activatedAt      = 0; // Unix timestamp (seconds) when activated
    int64_t     expiresAt        = 0; // Unix timestamp (seconds) when license expires (1 month)
    int64_t     lastValidatedAt  = 0; // Unix timestamp (seconds) of last online check
};

class AuthLicenseManager {
public:
    static AuthLicenseManager& Get();

    // ── Ciclo de vida y persistencia ─────────────────────────────────────────
    void Initialize();
    void SaveState();
    void LoadState();

    // ── Autenticación Firebase (Email y Contraseña) ───────────────────────────
    bool IsLoggedIn() const;
    const UserProfile& GetUser() const;
    const std::string& GetAuthError() const;
    bool IsAuthBusy() const;

    void LoginWithFirebase(const std::string& email, const std::string& password,
                           std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void SignUpWithFirebase(const std::string& email, const std::string& password,
                            std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void Logout();

    // ── Compra de Suscripción (Lemon Squeezy) ─────────────────────────────────
    // Retorna true si el usuario tiene sesión iniciada y puede proceder a la compra
    bool CanBuySubscription() const;
    bool OpenCheckoutUrl(std::string& outError);

    // ── Licencia Lemon Squeezy (ProyecThor for Professionals) ──────────────────
    bool IsPro() const;
    bool IsLicenseExpired() const;
    int64_t GetDaysRemaining() const;
    const LicenseInfo& GetLicenseInfo() const;
    const std::string& GetLicenseError() const;
    bool IsLicenseBusy() const;

    void ActivateLicense(const std::string& licenseKey, const std::string& instanceName = "",
                         std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void ValidateLicense(std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void DeactivateLicense(std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    // Configuración de API Keys
    void SetFirebaseApiKey(const std::string& key);
    std::string GetFirebaseApiKey() const;

private:
    AuthLicenseManager();
    ~AuthLicenseManager() = default;
    AuthLicenseManager(const AuthLicenseManager&) = delete;
    AuthLicenseManager& operator=(const AuthLicenseManager&) = delete;

    std::string GetConfigFilePath() const;
    std::string GetDefaultInstanceName() const;

    // Métodos internos de red HTTPS nativos (WinHTTP en Windows / fallback en Linux)
    static std::string PerformHttpsPost(const std::string& host, const std::string& path,
                                        const std::string& postData, const std::string& contentType,
                                        int& outStatusCode, std::string& outError);

    mutable std::mutex m_Mutex;

    // Estado de Firebase
    std::string m_FirebaseApiKey;
    bool        m_IsLoggedIn  = false;
    UserProfile m_User;
    std::string m_AuthError;
    std::atomic<bool> m_AuthBusy{false};

    // Estado de Licencia (Lemon Squeezy)
    LicenseInfo m_License;
    std::string m_LicenseError;
    std::atomic<bool> m_LicenseBusy{false};

    const std::string m_CheckoutBaseUrl = "https://vixcho.lemonsqueezy.com/checkout/buy/e609efc0-4393-46fc-b6ed-d535cb9dd2dc";
    const std::string m_LemonHost       = "api.lemonsqueezy.com";
    const std::string m_FirebaseHost    = "identitytoolkit.googleapis.com";
};

} // namespace ProyecThor::Core
