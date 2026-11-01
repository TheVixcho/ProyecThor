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

    // ── Gestión de Perfil de Usuario en Firebase ─────────────────────────────
    void UpdateDisplayName(const std::string& newDisplayName,
                           std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void ChangePassword(const std::string& newPassword,
                        std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void DeleteAccount(std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void SendEmailVerification(std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    // ── Compra de Suscripción (Flow Chile / Lemon Squeezy Internacional) ──────
    // Retorna true si el usuario tiene sesión iniciada y puede proceder a la compra
    bool CanBuySubscription() const;
    bool OpenCheckoutUrl(std::string& outError);
    void SetFlowCheckoutUrl(const std::string& url);
    std::string GetFlowCheckoutUrl() const;

    // Pasarela de Pago directa e Integración Flow API con detección automática
    void SetFlowCredentials(const std::string& apiKey, const std::string& secretKey);
    bool CreateFlowPaymentOrder(std::string& outRedirectUrl, std::string& outError,
                                std::function<void(bool success, const std::string& msg)> onComplete = nullptr);
    void StartPaymentPolling(const std::string& token,
                             std::function<void(bool success, const std::string& msg)> onComplete = nullptr);
    void CancelPaymentPolling();
    bool IsPollingPayment() const;
    std::string GetPendingPaymentToken() const;

    // Comprueba suscripciones activas en Flow para el correo del usuario
    void CheckFlowSubscriptionActive(std::function<void(bool isPro, const std::string& msg)> onComplete = nullptr);

    // ── Licencia ProyecThor (ProyecThor for Professionals) ─────────────────────
    bool IsPro() const;
    bool IsLicenseExpired() const;
    int64_t GetDaysRemaining() const;
    const LicenseInfo& GetLicenseInfo() const;
    const std::string& GetLicenseError() const;
    bool IsLicenseBusy() const;

    // Sincronización directa con Firebase Firestore (Servidor)
    void RefreshProStatus(std::function<void(bool isPro, const std::string& msg)> onComplete = nullptr);

    void ActivateLicense(const std::string& licenseKey, const std::string& instanceName = "",
                         std::function<void(bool success, const std::string& msg)> onComplete = nullptr);

    void ValidateLicense(std::function<void(bool success, const std::string& msg)> onComplete = nullptr);
    void ValidateOnAppLoaded();

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
    static std::string PerformHttpsRequest(const std::string& method, const std::string& host, const std::string& path,
                                           const std::string& data, const std::string& contentType,
                                           const std::string& bearerToken,
                                           int& outStatusCode, std::string& outError);

    static std::string PerformHttpsPost(const std::string& host, const std::string& path,
                                        const std::string& postData, const std::string& contentType,
                                        int& outStatusCode, std::string& outError);

    bool RefreshFirebaseIdToken(std::string& outIdToken, std::string& outError);

    void CheckFirestoreUserPro(const std::string& localId,
                               std::function<void(bool isPro, const std::string& msg)> onComplete);

    void UpdateFirestoreProStatus(const std::string& localId, bool isPro,
                                  const std::string& flowOrder, int64_t expiresAt);

    mutable std::recursive_mutex m_Mutex;

    // Estado de Firebase
    std::string m_FirebaseApiKey;
    bool        m_IsLoggedIn  = false;
    UserProfile m_User;
    std::string m_AuthError;
    std::atomic<bool> m_AuthBusy{false};

    // Estado de Licencia (ProyecThor / Lemon Squeezy / Flow)
    LicenseInfo m_License;
    std::string m_LicenseError;
    std::atomic<bool> m_LicenseBusy{false};

    // Pasarelas de Pago:
    // Flow Chile (Plan PROPROYECTHOR - $9.990 CLP):
    std::string m_FlowCheckoutBaseUrl = "https://www.flow.cl/suscripcion/plan/PROPROYECTHOR";

    // Credenciales oficiales de Flow (Producción)
    std::string m_FlowApiKey    = "3A5F02B4-7BB9-4260-81C4-76ED039L26C1";
    std::string m_FlowSecretKey = "d5966133f9115e9d37067e4984aa4a32389929f2";
    std::string m_FlowPlanId    = "PROPROYECTHOR";

    // Seguimiento de orden de pago en progreso Flow
    std::string m_PendingFlowToken;
    std::string m_PendingCommerceOrder;
    std::atomic<bool> m_IsPollingPayment{false};
    std::atomic<bool> m_StopPollingRequested{false};

    // Lemon Squeezy (guardado para venta internacional futura):
    // const std::string m_LemonCheckoutBaseUrl = "https://vixcho.lemonsqueezy.com/checkout/buy/e609efc0-4393-46fc-b6ed-d535cb9dd2dc";
    const std::string m_LemonHost       = "api.lemonsqueezy.com";
    const std::string m_FirebaseHost    = "identitytoolkit.googleapis.com";
};

} // namespace ProyecThor::Core
