#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "core/AuthLicenseManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cstring>
#include <vector>

namespace ProyecThor::UI::Settings {

static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

// Buffers estáticos para entradas de texto
static char s_EmailBuf[128]    = "";
static char s_PasswordBuf[128] = "";
static char s_LicenseKeyBuf[128] = "";
static bool s_BuffersInitialized = false;

static std::string s_AuthFeedbackMsg = "";
static bool        s_AuthFeedbackIsSuccess = false;
static std::string s_LicenseFeedbackMsg = "";
static bool        s_LicenseFeedbackIsSuccess = false;

static bool s_ShowDeactivateConfirmModal = false;

void SettingsPanel::RenderCategoryAccount() {
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();
    auto& mgr     = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    if (!s_BuffersInitialized) {
        s_BuffersInitialized = true;
        const auto& lic = authMgr.GetLicenseInfo();
        if (!lic.key.empty()) {
            strncpy(s_LicenseKeyBuf, lic.key.c_str(), sizeof(s_LicenseKeyBuf) - 1);
        }
    }

    const bool isLoggedIn = authMgr.IsLoggedIn();
    const bool isPro      = authMgr.IsPro();
    const auto& user      = authMgr.GetUser();
    const auto& license   = authMgr.GetLicenseInfo();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float contentW = ImGui::GetContentRegionAvail().x;

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 1: SESIÓN DE USUARIO (Firebase Auth)
    // ─────────────────────────────────────────────────────────────────────────
    if (SectionTitle("Sesión de Usuario", "Cuenta")) {
        // Tarjeta resumen de estado de sesión
        ImVec2 cardP = ImGui::GetCursorScreenPos();
        float cardH  = isLoggedIn ? 140.0f : 180.0f;
        ImU32 bgCol  = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.70f));
        ImU32 borderCol = isLoggedIn
            ? ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.45f))
            : ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.border, 0.60f));

        dl->AddRectFilled(cardP, ImVec2(cardP.x + contentW, cardP.y + cardH), bgCol, theme.frameRounding);
        dl->AddRect(cardP, ImVec2(cardP.x + contentW, cardP.y + cardH), borderCol, theme.frameRounding, 0, 1.2f);

        ImGui::SetCursorScreenPos(ImVec2(cardP.x + 20.0f, cardP.y + 16.0f));

        if (isLoggedIn) {
            // Usuario con sesión iniciada
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.55f, 1.0f));
            ImGui::SetWindowFontScale(1.1f);
            ImGui::TextUnformatted("Sesión iniciada con Firebase");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextColored(ThemeCol(theme.textDim), "Correo electrónico:");
            ImGui::SameLine();
            ImGui::TextColored(ThemeCol(theme.textPrimary), "%s", user.email.c_str());

            ImGui::TextColored(ThemeCol(theme.textDim), "Identificador de usuario (UID):");
            ImGui::SameLine();
            ImGui::TextColored(ThemeCol(theme.textFaint), "%s", user.localId.c_str());

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.15f, 0.15f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.12f, 0.12f, 1.0f));
            if (ImGui::Button("Cerrar sesión", ImVec2(140.0f, 30.0f))) {
                authMgr.Logout();
                s_AuthFeedbackMsg = "Has cerrado sesión correctamente.";
                s_AuthFeedbackIsSuccess = true;
            }
            ImGui::PopStyleColor(3);

            ImGui::SetCursorScreenPos(ImVec2(cardP.x, cardP.y + cardH + 16.0f));
        } else {
            // Usuario sin sesión iniciada
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.65f, 0.25f, 1.0f));
            ImGui::SetWindowFontScale(1.05f);
            ImGui::TextUnformatted("Sin sesión activa en ProyecThor");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushTextWrapPos(cardP.x + contentW - 20.0f);
            ImGui::TextColored(ThemeCol(theme.textDim),
                "Inicia sesión con tu correo y contraseña o crea una cuenta nueva para sincronizar tus canciones, "
                "vincular tu suscripción Pro y acceder a funciones avanzadas.");
            ImGui::PopTextWrapPos();

            ImGui::SetCursorScreenPos(ImVec2(cardP.x, cardP.y + cardH + 20.0f));

            // Formulario de autenticación
            ImGui::TextColored(ThemeCol(theme.textDim), "Correo electrónico");
            ImGui::SetNextItemWidth(contentW * 0.60f);
            ImGui::InputText("##auth_email", s_EmailBuf, sizeof(s_EmailBuf));

            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::TextColored(ThemeCol(theme.textDim), "Contraseña");
            ImGui::SetNextItemWidth(contentW * 0.60f);
            ImGui::InputText("##auth_password", s_PasswordBuf, sizeof(s_PasswordBuf), ImGuiInputTextFlags_Password);

            ImGui::Dummy(ImVec2(0.0f, 12.0f));

            const bool isAuthBusy = authMgr.IsAuthBusy();
            if (isAuthBusy) ImGui::BeginDisabled();

            // Botón Iniciar Sesión
            ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.accent, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.accentLight));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.accentDim));
            ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));
            if (ImGui::Button("Iniciar sesión", ImVec2(160.0f, 34.0f))) {
                s_AuthFeedbackMsg.clear();
                authMgr.LoginWithFirebase(s_EmailBuf, s_PasswordBuf, [](bool success, const std::string& msg) {
                    s_AuthFeedbackMsg = msg;
                    s_AuthFeedbackIsSuccess = success;
                });
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 12.0f);

            // Botón Crear Cuenta
            ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.surface2, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.surface2, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.surface1, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ThemeCol(theme.textPrimary));
            if (ImGui::Button("Crear cuenta", ImVec2(150.0f, 34.0f))) {
                s_AuthFeedbackMsg.clear();
                authMgr.SignUpWithFirebase(s_EmailBuf, s_PasswordBuf, [](bool success, const std::string& msg) {
                    s_AuthFeedbackMsg = msg;
                    s_AuthFeedbackIsSuccess = success;
                });
            }
            ImGui::PopStyleColor(4);

            if (isAuthBusy) {
                ImGui::EndDisabled();
                ImGui::SameLine(0, 14.0f);
                SpinnerWidget(8.0f, 2.0f, ThemeCol(theme.accent));
                ImGui::SameLine(0, 8.0f);
                ImGui::TextColored(ThemeCol(theme.textDim), "Conectando con Firebase...");
            }
        }

        // Mensaje de estado / error de autenticación
        const std::string& authErr = authMgr.GetAuthError();
        if (!authErr.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("%s", authErr.c_str());
            ImGui::PopStyleColor();
        } else if (!s_AuthFeedbackMsg.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImVec4 col = s_AuthFeedbackIsSuccess
                ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f)
                : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextWrapped("%s", s_AuthFeedbackMsg.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 2: SUSCRIPCIÓN PROYECTHOR FOR PROFESSIONALS
    // ─────────────────────────────────────────────────────────────────────────
    if (SectionTitle("Suscripción Pro", "Suscripción")) {
        // Tarjeta informativa de la suscripción Pro
        ImVec2 subP = ImGui::GetCursorScreenPos();
        float subH  = 220.0f;
        ImU32 subBg = ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.surface0, 0.85f));
        ImU32 subBorder = isPro
            ? IM_COL32(235, 175, 50, 200)
            : ImGui::ColorConvertFloat4ToU32(ThemeCol(theme.accent, 0.50f));

        dl->AddRectFilled(subP, ImVec2(subP.x + contentW, subP.y + subH), subBg, theme.frameRounding);
        dl->AddRect(subP, ImVec2(subP.x + contentW, subP.y + subH), subBorder, theme.frameRounding, 0, 1.5f);

        ImGui::SetCursorScreenPos(ImVec2(subP.x + 22.0f, subP.y + 18.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, isPro ? ImVec4(1.0f, 0.82f, 0.35f, 1.0f) : ThemeCol(theme.accent));
        ImGui::SetWindowFontScale(1.15f);
        ImGui::TextUnformatted(isPro ? "ProyecThor for Professionals (Activo)" : "ProyecThor for Professionals");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushTextWrapPos(subP.x + contentW - 24.0f);
        ImGui::TextColored(ThemeCol(theme.textDim),
            "Desbloquea la suite técnica avanzada para auditorios, templos y recintos de alta exigencia:");
        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Bullet(); ImGui::TextColored(ThemeCol(theme.textPrimary), "Almacenamiento en la nube para más de 5,000 canciones sincronizadas.");
        ImGui::Bullet(); ImGui::TextColored(ThemeCol(theme.textPrimary), "Escritura y estructuración de letras potenciada por Inteligencia Artificial.");
        ImGui::Bullet(); ImGui::TextColored(ThemeCol(theme.textPrimary), "Chat y comandos de voz interactivos potenciados por IA para el operador.");
        ImGui::Bullet(); ImGui::TextColored(ThemeCol(theme.textPrimary), "Sincronización multi-sala sin retraso y canal prioritario de asistencia.");

        ImGui::SetCursorScreenPos(ImVec2(subP.x, subP.y + subH + 20.0f));

        // Requisito: el usuario DEBE estar logueado para comprar la suscripción
        if (!isLoggedIn) {
            ImVec2 warnP = ImGui::GetCursorScreenPos();
            float warnH  = 44.0f;
            dl->AddRectFilled(warnP, ImVec2(warnP.x + contentW, warnP.y + warnH), IM_COL32(80, 55, 20, 120), 6.0f);
            dl->AddRect(warnP, ImVec2(warnP.x + contentW, warnP.y + warnH), IM_COL32(230, 160, 40, 160), 6.0f);

            ImGui::SetCursorScreenPos(ImVec2(warnP.x + 14.0f, warnP.y + 12.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.40f, 1.0f));
            ImGui::TextUnformatted("Atención: Debes iniciar sesión con tu correo en la sección 'Sesión de Usuario' antes de comprar la suscripción.");
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(warnP.x, warnP.y + warnH + 16.0f));

            ImGui::BeginDisabled();
            ImGui::PushStyleColor(ImGuiCol_Button, ThemeCol(theme.surface2));
            ImGui::Button("Comprar suscripción (Requiere iniciar sesión)", ImVec2(330.0f, 38.0f));
            ImGui::PopStyleColor();
            ImGui::EndDisabled();
            HelpTooltip("Inicia sesión o crea una cuenta en la sección superior para habilitar el botón de compra.");
        } else {
            ImGui::TextColored(ThemeCol(theme.textDim), "Tu cuenta (%s) está lista para vincular la suscripción.", user.email.c_str());
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.55f, 0.85f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.65f, 0.95f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            if (ImGui::Button("Comprar suscripción Pro", ImVec2(240.0f, 38.0f))) {
                std::string err;
                if (!authMgr.OpenCheckoutUrl(err)) {
                    s_LicenseFeedbackMsg = err;
                    s_LicenseFeedbackIsSuccess = false;
                }
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine(0, 14.0f);
            ImGui::PushTextWrapPos(contentW - 20.0f);
            ImGui::TextColored(ThemeCol(theme.textFaint),
                "Se abrirá la pasarela segura de Lemon Squeezy en tu navegador web con tu correo precargado.");
            ImGui::PopTextWrapPos();
        }

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 3: ACTIVACIÓN DE LICENCIA (Lemon Squeezy License API)
    // ─────────────────────────────────────────────────────────────────────────
    if (SectionTitle("Activar Licencia", "Licencia")) {
        const bool isLicenseBusy = authMgr.IsLicenseBusy();

        if (isPro) {
            // Licencia actualmente activa
            ImVec2 licCardP = ImGui::GetCursorScreenPos();
            float licCardH  = 200.0f;
            dl->AddRectFilled(licCardP, ImVec2(licCardP.x + contentW, licCardP.y + licCardH),
                              IM_COL32(35, 45, 30, 180), theme.frameRounding);
            dl->AddRect(licCardP, ImVec2(licCardP.x + contentW, licCardP.y + licCardH),
                        IM_COL32(80, 200, 120, 200), theme.frameRounding, 0, 1.4f);

            ImGui::SetCursorScreenPos(ImVec2(licCardP.x + 20.0f, licCardP.y + 16.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.90f, 0.60f, 1.0f));
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextUnformatted("Licencia Pro Activada Correctamente");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            ImGui::TextColored(ThemeCol(theme.textDim), "Clave de licencia:");
            ImGui::SameLine();
            ImGui::TextColored(ThemeCol(theme.textPrimary), "%s", license.key.c_str());

            ImGui::TextColored(ThemeCol(theme.textDim), "Estado:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "%s", license.status.c_str());

            if (!license.customerName.empty() || !license.customerEmail.empty()) {
                ImGui::TextColored(ThemeCol(theme.textDim), "Titular:");
                ImGui::SameLine();
                ImGui::TextColored(ThemeCol(theme.textPrimary), "%s (%s)", license.customerName.c_str(), license.customerEmail.c_str());
            }

            if (!license.instanceName.empty()) {
                ImGui::TextColored(ThemeCol(theme.textDim), "Equipo asignado:");
                ImGui::SameLine();
                ImGui::TextColored(ThemeCol(theme.textPrimary), "%s", license.instanceName.c_str());
            }

            if (!license.expiryDate.empty()) {
                ImGui::TextColored(ThemeCol(theme.textDim), "Vencimiento:");
                ImGui::SameLine();
                int64_t daysLeft = authMgr.GetDaysRemaining();
                if (daysLeft > 0) {
                    ImGui::TextColored(ThemeCol(theme.textPrimary), "%s (%lld días restantes)",
                                       license.expiryDate.c_str(), (long long)daysLeft);
                } else {
                    ImGui::TextColored(ThemeCol(theme.textPrimary), "%s (último día del período)",
                                       license.expiryDate.c_str());
                }
            }

            ImGui::SetCursorScreenPos(ImVec2(licCardP.x, licCardP.y + licCardH + 16.0f));

            if (isLicenseBusy) ImGui::BeginDisabled();

            // Botón Validar Licencia
            ImGui::PushStyleColor(ImGuiCol_Button,        ThemeCol(theme.surface2, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeCol(theme.surface2, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeCol(theme.surface1, 1.0f));
            if (ImGui::Button("Verificar validez", ImVec2(160.0f, 32.0f))) {
                authMgr.ValidateLicense([](bool success, const std::string& msg) {
                    s_LicenseFeedbackMsg = msg;
                    s_LicenseFeedbackIsSuccess = success;
                });
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, 12.0f);

            // Botón Desactivar
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.40f, 0.15f, 0.15f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.30f, 0.10f, 0.10f, 1.0f));
            if (ImGui::Button("Desactivar en este equipo", ImVec2(210.0f, 32.0f))) {
                s_ShowDeactivateConfirmModal = true;
            }
            ImGui::PopStyleColor(3);

            if (isLicenseBusy) {
                ImGui::EndDisabled();
                ImGui::SameLine(0, 12.0f);
                SpinnerWidget(8.0f, 2.0f, ThemeCol(theme.accent));
            }

            // Modal de confirmación para desactivar licencia
            if (s_ShowDeactivateConfirmModal) {
                ImGui::OpenPopup("¿Desactivar licencia Pro?##confirmDeactivate");
            }

            ImVec2 mcenter = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(mcenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("¿Desactivar licencia Pro?##confirmDeactivate", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextWrapped(
                    "Esta acción desvinculará este equipo en los servidores de Lemon Squeezy, liberando el cupo "
                    "para que puedas activarla en otra computadora.\n\n¿Deseas continuar?");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Desactivar ahora", ImVec2(140.0f, 32.0f))) {
                    authMgr.DeactivateLicense([](bool success, const std::string& msg) {
                        s_LicenseFeedbackMsg = msg;
                        s_LicenseFeedbackIsSuccess = success;
                    });
                    s_ShowDeactivateConfirmModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar", ImVec2(100.0f, 32.0f))) {
                    s_ShowDeactivateConfirmModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } else {
            // Sin licencia activa: formulario de activación moderno y estilizado
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeCol(theme.surface0, 0.70f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ThemeCol(theme.border, 0.55f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, theme.frameRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

            if (ImGui::BeginChild("##lic_activation_card", ImVec2(contentW, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
                // Badge [ PRO ] y Título
                ImGui::PushStyleColor(ImGuiCol_Text, ThemeCol(theme.accent));
                ImGui::SetWindowFontScale(1.15f);
                ImGui::TextUnformatted("[ PRO ]");
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 10.0f);
                ImGui::TextUnformatted("Activar ProyecThor for Professionals");
                ImGui::SetWindowFontScale(1.0f);

                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                ImGui::PushTextWrapPos(contentW - 44.0f);
                ImGui::TextColored(ThemeCol(theme.textDim),
                    "Ingresa tu clave de licencia para desbloquear todas las herramientas avanzadas en esta computadora: "
                    "Stage Display, Asistente de IA en canciones, reloj y futuras características Pro.");
                ImGui::PopTextWrapPos();

                ImGui::Dummy(ImVec2(0.0f, 14.0f));
                ImGui::TextColored(ThemeCol(theme.textPrimary), "Clave de Licencia (License Key):");

                // Input para la clave de licencia
                float inputW = std::min(contentW * 0.72f, 440.0f);
                ImGui::SetNextItemWidth(inputW);
                ImGui::InputTextWithHint("##lic_key_input", "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX",
                                         s_LicenseKeyBuf, sizeof(s_LicenseKeyBuf));

                ImGui::SameLine(0, 8.0f);
                if (ImGui::Button("Pegar", ImVec2(75.0f, 0.0f))) {
                    const char* clip = ImGui::GetClipboardText();
                    if (clip && strlen(clip) > 0) {
                        std::string s = clip;
                        while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) s.pop_back();
                        while (!s.empty() && (s.front() == ' ' || s.front() == '\r' || s.front() == '\n' || s.front() == '\t')) s.erase(s.begin());
                        strncpy(s_LicenseKeyBuf, s.c_str(), sizeof(s_LicenseKeyBuf) - 1);
                        s_LicenseKeyBuf[sizeof(s_LicenseKeyBuf) - 1] = '\0';
                    }
                }
                HelpTooltip("Pega la clave copiada desde tu correo de compra.");

                if (strlen(s_LicenseKeyBuf) > 0) {
                    ImGui::SameLine(0, 6.0f);
                    if (ImGui::Button("Limpiar", ImVec2(75.0f, 0.0f))) {
                        s_LicenseKeyBuf[0] = '\0';
                        s_LicenseFeedbackMsg.clear();
                    }
                }

                ImGui::Dummy(ImVec2(0.0f, 16.0f));

                if (isLicenseBusy) ImGui::BeginDisabled();

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.62f, 0.36f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.44f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.50f, 0.28f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

                if (ImGui::Button("Activar Licencia Pro", ImVec2(220.0f, 38.0f))) {
                    s_LicenseFeedbackMsg.clear();
                    std::string key = s_LicenseKeyBuf;
                    while (!key.empty() && (key.back() == ' ' || key.back() == '\r' || key.back() == '\n' || key.back() == '\t')) key.pop_back();
                    while (!key.empty() && (key.front() == ' ' || key.front() == '\r' || key.front() == '\n' || key.front() == '\t')) key.erase(key.begin());

                    if (key.empty()) {
                        s_LicenseFeedbackMsg = "Por favor ingresa una clave de licencia válida.";
                        s_LicenseFeedbackIsSuccess = false;
                    } else {
                        strncpy(s_LicenseKeyBuf, key.c_str(), sizeof(s_LicenseKeyBuf) - 1);
                        authMgr.ActivateLicense(key, "", [](bool success, const std::string& msg) {
                            s_LicenseFeedbackMsg = msg;
                            s_LicenseFeedbackIsSuccess = success;
                        });
                    }
                }
                ImGui::PopStyleColor(4);

                if (isLicenseBusy) {
                    ImGui::EndDisabled();
                    ImGui::SameLine(0, 14.0f);
                    SpinnerWidget(9.0f, 2.2f, ThemeCol(theme.accent));
                    ImGui::SameLine(0, 8.0f);
                    ImGui::TextColored(ThemeCol(theme.textDim), "Validando con los servidores de Lemon Squeezy...");
                }

                // Banner de estado / error / éxito
                const std::string& licErr = authMgr.GetLicenseError();
                const bool hasError = !licErr.empty() || (!s_LicenseFeedbackMsg.empty() && !s_LicenseFeedbackIsSuccess);
                const bool hasSuccess = s_LicenseFeedbackIsSuccess && !s_LicenseFeedbackMsg.empty();
                const std::string activeMsg = !licErr.empty() ? licErr : s_LicenseFeedbackMsg;

                if (hasError || hasSuccess) {
                    ImGui::Dummy(ImVec2(0.0f, 14.0f));
                    ImVec4 alertCol = hasSuccess ? ImVec4(0.40f, 0.90f, 0.55f, 1.0f) : ImVec4(0.98f, 0.40f, 0.40f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, alertCol);
                    ImGui::PushTextWrapPos(contentW - 44.0f);
                    ImGui::TextWrapped("%s %s", hasSuccess ? "[OK]" : "[!]", activeMsg.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                }

                ImGui::Dummy(ImVec2(0.0f, 14.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 8.0f));

                // Notas y detalles sobre la suscripción
                ImGui::TextColored(ThemeCol(theme.textDim), "Detalles de la suscripción:");
                ImGui::PushTextWrapPos(contentW - 44.0f);
                ImGui::TextColored(ThemeCol(theme.textFaint),
                    "• Cada suscripción cubre 1 mes completo de uso sin interrupciones.\n"
                    "• La licencia se asocia de forma automática a este equipo sin requerir nombres manuales.\n"
                    "• Al concluir el mes, la aplicación contactará el servidor al iniciar para validar la renovación activa.");
                ImGui::PopTextWrapPos();

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::EndChild();
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        }

        // Mensaje de estado cuando isPro es true (para verificar o desactivar)
        if (isPro) {
            const std::string& licErr = authMgr.GetLicenseError();
            if (!licErr.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 12.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
                ImGui::TextWrapped("%s", licErr.c_str());
                ImGui::PopStyleColor();
            } else if (!s_LicenseFeedbackMsg.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 12.0f));
                ImVec4 col = s_LicenseFeedbackIsSuccess
                    ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f)
                    : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, col);
                ImGui::TextWrapped("%s", s_LicenseFeedbackMsg.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
    }
}

} // namespace ProyecThor::UI::Settings
