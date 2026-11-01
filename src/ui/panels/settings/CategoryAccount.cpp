#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "core/AuthLicenseManager.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace ProyecThor::UI::Settings {

static inline ImVec4 ThemeCol(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

// Progreso animado (0..1) interactivo para elevación, iluminación y resplandor
static float AnimT(ImGuiID baseId, ImU32 salt, bool target, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(baseId ^ salt, target ? 1.0f : 0.0f);
    float dst = target ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// Renderizado profesional de tarjeta de ventaja técnica con micro-animaciones y alto contraste
static void RenderFeatureCard(const char* id, const char* tag, const char* title, const char* desc,
                              ImVec4 tagCol, float width, float height, float timeVal, int idx) {
    ImGui::PushID(id);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);

    ImGui::InvisibleButton("##featHitbox", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    ImGuiID imId = ImGui::GetID("##featHitbox");
    float hoverT = AnimT(imId, 0x821Fu, hovered, 12.0f);

    float lift = hoverT * 2.5f;
    ImVec2 cP0 = ImVec2(p0.x, p0.y - lift);
    ImVec2 cP1 = ImVec2(p1.x, p1.y - lift);
    const float rounding = 10.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Sombra suave en capas
    for (int s = 3; s >= 1; --s) {
        float st = (float)s / 3.0f;
        float offY = (3.0f + hoverT * 5.0f) * st;
        float exp = (hoverT * 4.0f) * st;
        int a = (int)((16.0f + hoverT * 32.0f) * (1.0f - st * 0.4f));
        dl->AddRectFilled(
            ImVec2(cP0.x - exp, cP0.y + offY),
            ImVec2(cP1.x + exp, cP1.y + offY + exp),
            IM_COL32(0, 0, 0, a),
            rounding + exp
        );
    }

    // Resplandor de identidad en hover
    if (hoverT > 0.01f) {
        ImU32 glowCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(tagCol.x, tagCol.y, tagCol.z, 0.16f * hoverT)
        );
        dl->AddRect(
            ImVec2(cP0.x - 1.5f, cP0.y - 1.5f),
            ImVec2(cP1.x + 1.5f, cP1.y + 1.5f),
            glowCol, rounding + 1.0f, 0, 2.0f
        );
    }

    // Fondo oscuro con leve iluminación al pasar el cursor
    float sheen = hoverT * 8.0f;
    ImU32 bgCol = IM_COL32((int)(20 + sheen), (int)(24 + sheen), (int)(32 + sheen), 248);
    dl->AddRectFilled(cP0, cP1, bgCol, rounding);

    // Borde fino con transición dinámica hacia el color del feature
    ImVec4 brdCol(
        0.18f + (tagCol.x - 0.18f) * hoverT,
        0.22f + (tagCol.y - 0.22f) * hoverT,
        0.28f + (tagCol.z - 0.28f) * hoverT,
        0.26f + 0.58f * hoverT
    );
    dl->AddRect(cP0, cP1, ImGui::ColorConvertFloat4ToU32(brdCol), rounding, 0, 1.0f + 0.5f * hoverT);

    // Línea de acento superior viva
    ImU32 topAccentCol = ImGui::ColorConvertFloat4ToU32(
        ImVec4(tagCol.x, tagCol.y, tagCol.z, 0.35f + 0.55f * hoverT)
    );
    dl->AddLine(
        ImVec2(cP0.x + 16.0f, cP0.y),
        ImVec2(cP1.x - 16.0f, cP0.y),
        topAccentCol, 1.8f
    );

    // Pill Badge moderno para la etiqueta (sin corchetes [], con micro-LED pulsante)
    ImVec2 tagSz = ImGui::CalcTextSize(tag);
    const float pillH = 22.0f;
    const float dotR = 2.4f;
    const float padLeft = 8.0f;
    const float dotToText = 7.0f;
    const float padRight = 9.0f;
    const float pillW = padLeft + (dotR * 2.0f) + dotToText + tagSz.x + padRight;

    ImVec2 pillP0(cP0.x + 14.0f, cP0.y + 14.0f);
    ImVec2 pillP1(pillP0.x + pillW, pillP0.y + pillH);

    ImVec4 pillBg(tagCol.x, tagCol.y, tagCol.z, 0.12f + 0.12f * hoverT);
    dl->AddRectFilled(pillP0, pillP1, ImGui::ColorConvertFloat4ToU32(pillBg), pillH * 0.5f);

    ImVec4 pillBrd(tagCol.x, tagCol.y, tagCol.z, 0.35f + 0.40f * hoverT);
    dl->AddRect(pillP0, pillP1, ImGui::ColorConvertFloat4ToU32(pillBrd), pillH * 0.5f, 0, 1.0f);

    // LED indicador pulsante
    float dotPulse = 0.65f + 0.35f * sinf(timeVal * 3.5f + (float)idx * 1.3f);
    ImVec2 dotCenter(pillP0.x + padLeft + dotR, pillP0.y + pillH * 0.5f);
    ImU32 auraCol = ImGui::ColorConvertFloat4ToU32(ImVec4(tagCol.x, tagCol.y, tagCol.z, 0.25f * dotPulse));
    dl->AddCircleFilled(dotCenter, dotR + 2.0f, auraCol);
    ImU32 dotCol = ImGui::ColorConvertFloat4ToU32(ImVec4(tagCol.x, tagCol.y, tagCol.z, 0.85f + 0.15f * dotPulse));
    dl->AddCircleFilled(dotCenter, dotR, dotCol);

    // Texto de la etiqueta
    ImVec2 tagTextPos(dotCenter.x + dotR + dotToText, pillP0.y + (pillH - tagSz.y) * 0.5f);
    dl->AddText(tagTextPos, ImGui::ColorConvertFloat4ToU32(ImVec4(tagCol.x, tagCol.y, tagCol.z, 1.0f)), tag);

    // Título en blanco de alto contraste
    ImVec2 titlePos(cP0.x + 14.0f, cP0.y + 44.0f);
    dl->AddText(titlePos, IM_COL32(248, 250, 254, 255), title);

    // Descripción en texto platino/gris luminoso legible (alto contraste respecto al fondo)
    ImVec2 descPos(cP0.x + 14.0f, cP0.y + 68.0f);
    float wrapW = width - 28.0f;
    dl->AddText(nullptr, 0.0f, descPos, IM_COL32(198, 206, 220, 255), desc, nullptr, wrapW);

    ImGui::PopID();
}

// Renderizado profesional de tarjeta de términos con barra de identidad, hover y alto contraste
static void RenderTermsCard(const char* id, const char* title, const char* text,
                            ImVec4 accentCol, float contentW, float cardH,
                            const char* linkLabel = nullptr, const char* linkUrl = nullptr,
                            const char* linkLabel2 = nullptr, const char* linkUrl2 = nullptr) {
    ImGui::PushID(id);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + contentW, p0.y + cardH);

    ImGui::Dummy(ImVec2(contentW, cardH));
    ImVec2 afterCardPos = ImGui::GetCursorScreenPos();

    bool hovered = ImGui::IsMouseHoveringRect(p0, p1);
    ImGuiID imId = ImGui::GetID("##cardAnim");
    float hoverT = AnimT(imId, 0x486Eu, hovered, 12.0f);

    float lift = hoverT * 1.5f;
    ImVec2 cP0 = ImVec2(p0.x, p0.y - lift);
    ImVec2 cP1 = ImVec2(p1.x, p1.y - lift);
    const float rounding = 8.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (hoverT > 0.01f) {
        dl->AddRectFilled(
            ImVec2(cP0.x, cP0.y + 3.0f * hoverT),
            ImVec2(cP1.x, cP1.y + 5.0f * hoverT),
            IM_COL32(0, 0, 0, (int)(25 * hoverT)),
            rounding + 2.0f
        );
    }

    float sheen = hoverT * 6.0f;
    ImU32 bgCol = IM_COL32((int)(19 + sheen), (int)(23 + sheen), (int)(32 + sheen), 245);
    dl->AddRectFilled(cP0, cP1, bgCol, rounding);

    ImVec4 brdCol(
        0.18f + (accentCol.x - 0.18f) * hoverT,
        0.22f + (accentCol.y - 0.22f) * hoverT,
        0.28f + (accentCol.z - 0.28f) * hoverT,
        0.25f + 0.45f * hoverT
    );
    dl->AddRect(cP0, cP1, ImGui::ColorConvertFloat4ToU32(brdCol), rounding, 0, 1.0f);

    // Barra de acento vertical izquierda
    ImU32 barCol = ImGui::ColorConvertFloat4ToU32(accentCol);
    dl->AddRectFilled(
        ImVec2(cP0.x + 3.0f, cP0.y + 8.0f),
        ImVec2(cP0.x + 6.5f, cP1.y - 8.0f),
        barCol, 2.0f
    );

    // Título limpio (sin corchetes [], coloreado según identidad)
    ImVec2 titlePos(cP0.x + 18.0f, cP0.y + 12.0f);
    dl->AddText(titlePos, barCol, title);

    // Contenido en texto de alto contraste
    ImVec2 bodyPos(cP0.x + 18.0f, cP0.y + 36.0f);
    dl->AddText(nullptr, 0.0f, bodyPos, IM_COL32(198, 206, 220, 255), text, nullptr, contentW - 32.0f);

    // Botones de enlace externo si están definidos
    if (linkLabel && linkUrl) {
        ImGui::SetCursorScreenPos(ImVec2(cP0.x + 18.0f, cP1.y - 36.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accentCol.x * 0.22f, accentCol.y * 0.22f, accentCol.z * 0.22f, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accentCol.x * 0.42f, accentCol.y * 0.42f, accentCol.z * 0.42f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(accentCol.x * 0.58f, accentCol.y * 0.58f, accentCol.z * 0.58f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));

        if (ImGui::Button(linkLabel)) {
#ifdef _WIN32
            ShellExecuteA(NULL, "open", linkUrl, NULL, NULL, SW_SHOWNORMAL);
#endif
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("Abrir en el navegador: %s", linkUrl);
        }

        if (linkLabel2 && linkUrl2) {
            ImGui::SameLine(0.0f, 8.0f);
            if (ImGui::Button(linkLabel2)) {
#ifdef _WIN32
                ShellExecuteA(NULL, "open", linkUrl2, NULL, NULL, SW_SHOWNORMAL);
#endif
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("Abrir en el navegador: %s", linkUrl2);
            }
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::SetCursorScreenPos(afterCardPos);
    }

    ImGui::PopID();
}

// Buffers estáticos para entradas de texto
static char s_EmailBuf[128]           = "";
static char s_PasswordBuf[128]        = "";
static char s_ConfirmPasswordBuf[128] = "";
static char s_NewDisplayNameBuf[128]  = "";
static char s_NewPasswordBuf[128]     = "";
static bool s_BuffersInitialized      = false;

// Estado interno de la interfaz
static bool s_ShowRegisterForm        = false;
static bool s_ShowDeleteModal         = false;

static std::mutex  s_FeedbackMutex;
static std::string s_AccountActionMsg       = "";
static bool        s_AccountActionIsSuccess = false;
static std::string s_LicenseFeedbackMsg     = "";
static bool        s_LicenseFeedbackIsSuccess = false;

static void SetAccountActionMsg(const std::string& msg, bool success) {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    s_AccountActionMsg = msg;
    s_AccountActionIsSuccess = success;
}

static void SetLicenseFeedbackMsg(const std::string& msg, bool success) {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    s_LicenseFeedbackMsg = msg;
    s_LicenseFeedbackIsSuccess = success;
}

static void GetAccountActionMsg(std::string& outMsg, bool& outSuccess) {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    outMsg = s_AccountActionMsg;
    outSuccess = s_AccountActionIsSuccess;
}

static void GetLicenseFeedbackMsg(std::string& outMsg, bool& outSuccess) {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    outMsg = s_LicenseFeedbackMsg;
    outSuccess = s_LicenseFeedbackIsSuccess;
}

static void ClearAccountActionMsg() {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    s_AccountActionMsg.clear();
    s_AccountActionIsSuccess = false;
}

static void ClearLicenseFeedbackMsg() {
    std::lock_guard<std::mutex> lock(s_FeedbackMutex);
    s_LicenseFeedbackMsg.clear();
    s_LicenseFeedbackIsSuccess = false;
}

void SettingsPanel::RenderCategoryAccount() {
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();
    auto& mgr     = ProyecThor::Settings::SettingsManager::Get();
    const auto& theme = mgr.GetSettings().theme;

    const bool isLoggedIn = authMgr.IsLoggedIn();
    const bool isPro      = authMgr.IsPro();
    ProyecThor::Core::UserProfile user;
    if (isLoggedIn) {
        user = authMgr.GetUser();
    }

    if (!s_BuffersInitialized) {
        s_BuffersInitialized = true;
        if (!user.displayName.empty()) {
            strncpy(s_NewDisplayNameBuf, user.displayName.c_str(), sizeof(s_NewDisplayNameBuf) - 1);
        }
    }

    float contentW = ImGui::GetContentRegionAvail().x;
    float timeVal  = (float)ImGui::GetTime();
    float pulse    = 0.5f + 0.5f * sinf(timeVal * 2.5f);

    // ─────────────────────────────────────────────────────────────────────────
    //  SINCRONIZACIÓN DE SUBCATEGORÍAS CON EL SIDEBAR DE SETTINGSPANEL
    // ─────────────────────────────────────────────────────────────────────────
    const char* subSections[] = { "Mi Cuenta", "Suscripción Pro", "Términos y Privacidad" };
    for (const char* sub : subSections) {
        bool alreadyIn = false;
        for (const auto& a : m_SectionAnchors) {
            if (a == sub) { alreadyIn = true; break; }
        }
        if (!alreadyIn) {
            m_SectionAnchors.emplace_back(sub);
        }
    }

    if (m_SelectedSubsection.empty()) {
        m_SelectedSubsection = subSections[0];
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  BARRA DE NAVEGACIÓN SUPERIOR (Pills sincronizadas e interactivas)
    // ─────────────────────────────────────────────────────────────────────────
    float tabBtnW = std::clamp((contentW - 24.0f) / 3.0f, 140.0f, 220.0f);

    for (int i = 0; i < 3; ++i) {
        bool isActive = (m_SelectedSubsection == subSections[i]);
        ImGui::PushID(subSections[i]);

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = ImVec2(p0.x + tabBtnW, p0.y + 36.0f);

        ImGui::InvisibleButton("##tabBtn", ImVec2(tabBtnW, 36.0f));
        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();

        ImGuiID tId = ImGui::GetID("##tabBtn");
        float hoverT  = AnimT(tId, 0x7AB1u, hovered, 14.0f);
        float activeT = AnimT(tId, 0x7AB2u, isActive, 14.0f);

        if (clicked) {
            m_SelectedSubsection = subSections[i];
            ClearAccountActionMsg();
            ClearLicenseFeedbackMsg();
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec4 accentF4 = ThemeCol(theme.accent, 1.0f);
        ImVec4 idleBg(0.12f, 0.14f, 0.20f, 0.85f);
        ImVec4 activeBg(accentF4.x, accentF4.y, accentF4.z, 0.92f);
        ImVec4 curBg(
            idleBg.x + (activeBg.x - idleBg.x) * activeT + hoverT * 0.06f,
            idleBg.y + (activeBg.y - idleBg.y) * activeT + hoverT * 0.06f,
            idleBg.z + (activeBg.z - idleBg.z) * activeT + hoverT * 0.06f,
            0.85f + 0.15f * activeT
        );
        dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(curBg), 10.0f);

        ImVec4 borderCol(
            0.22f + (accentF4.x - 0.22f) * activeT + 0.20f * hoverT,
            0.26f + (accentF4.y - 0.26f) * activeT + 0.20f * hoverT,
            0.32f + (accentF4.z - 0.32f) * activeT + 0.20f * hoverT,
            0.30f + 0.50f * activeT + 0.30f * hoverT
        );
        dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(borderCol), 10.0f, 0, 1.0f + 0.5f * activeT);

        ImVec2 textSz = ImGui::CalcTextSize(subSections[i]);
        ImVec2 textPos(p0.x + (tabBtnW - textSz.x) * 0.5f, p0.y + (36.0f - textSz.y) * 0.5f);
        ImU32 textCol = (isActive || activeT > 0.5f)
            ? IM_COL32(12, 16, 24, 255)
            : ImGui::ColorConvertFloat4ToU32(ImVec4(0.82f + 0.18f * hoverT, 0.86f + 0.14f * hoverT, 0.92f + 0.08f * hoverT, 1.0f));
        dl->AddText(textPos, textCol, subSections[i]);

        ImGui::PopID();
        if (i < 2) ImGui::SameLine(0, 8.0f);
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 0: MI CUENTA
    // ─────────────────────────────────────────────────────────────────────────
    if (m_SelectedSubsection == "Mi Cuenta") {
        if (!isLoggedIn) {
            // Tarjeta de Login / Registro
            float cardH = s_ShowRegisterForm ? 350.0f : 280.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.15f, 0.88f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.24f, 0.30f, 0.42f, 0.70f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

            if (ImGui::BeginChild("##authLoginBox", ImVec2(0.0f, cardH), true, ImGuiWindowFlags_NoScrollbar)) {
                // Selector Login / Registro
                ImGui::PushStyleColor(ImGuiCol_Button, s_ShowRegisterForm ? ImVec4(0.14f, 0.16f, 0.22f, 0.8f) : ThemeCol(theme.accent, 0.88f));
                ImGui::PushStyleColor(ImGuiCol_Text, s_ShowRegisterForm ? ImVec4(0.75f, 0.80f, 0.88f, 1.0f) : ImVec4(0.04f, 0.06f, 0.08f, 1.0f));
                if (ImGui::Button("Iniciar Sesión", ImVec2(140.0f, 32.0f))) {
                    s_ShowRegisterForm = false;
                    ClearAccountActionMsg();
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(0, 10.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, s_ShowRegisterForm ? ThemeCol(theme.accent, 0.88f) : ImVec4(0.14f, 0.16f, 0.22f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, s_ShowRegisterForm ? ImVec4(0.04f, 0.06f, 0.08f, 1.0f) : ImVec4(0.75f, 0.80f, 0.88f, 1.0f));
                if (ImGui::Button("Registrarse", ImVec2(140.0f, 32.0f))) {
                    s_ShowRegisterForm = true;
                    ClearAccountActionMsg();
                }
                ImGui::PopStyleColor(2);

                ImGui::Dummy(ImVec2(0.0f, 12.0f));

                float inputW = std::clamp(contentW * 0.65f, 280.0f, 420.0f);

                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f), "Correo electrónico:");
                ImGui::SetNextItemWidth(inputW);
                ImGui::InputText("##auth_email_in", s_EmailBuf, sizeof(s_EmailBuf));

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f), "Contraseña:");
                ImGui::SetNextItemWidth(inputW);
                ImGui::InputText("##auth_pwd_in", s_PasswordBuf, sizeof(s_PasswordBuf), ImGuiInputTextFlags_Password);

                if (s_ShowRegisterForm) {
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f), "Confirmar contraseña:");
                    ImGui::SetNextItemWidth(inputW);
                    ImGui::InputText("##auth_pwd_confirm_in", s_ConfirmPasswordBuf, sizeof(s_ConfirmPasswordBuf), ImGuiInputTextFlags_Password);

                    bool passMatch = (strlen(s_PasswordBuf) > 0 && strcmp(s_PasswordBuf, s_ConfirmPasswordBuf) == 0);
                    bool passLenOk = (strlen(s_PasswordBuf) >= 6);

                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    if (strlen(s_ConfirmPasswordBuf) > 0) {
                        if (!passMatch) {
                            ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.40f, 1.0f), "● Las contraseñas no coinciden");
                        } else if (!passLenOk) {
                            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f), "● Mínimo 6 caracteres");
                        } else {
                            ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.55f, 1.0f), "● Contraseñas válidas");
                        }
                    }
                }

                ImGui::Dummy(ImVec2(0.0f, 12.0f));

                const bool isAuthBusy = authMgr.IsAuthBusy();
                if (isAuthBusy) ImGui::BeginDisabled();

                if (!s_ShowRegisterForm) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ThemeCol(theme.accent));
                    ImGui::PushStyleColor(ImGuiCol_Text,   ImVec4(0.04f, 0.06f, 0.08f, 1.0f));
                    if (ImGui::Button("Entrar a ProyecThor", ImVec2(180.0f, 36.0f))) {
                        ClearAccountActionMsg();
                        authMgr.LoginWithFirebase(s_EmailBuf, s_PasswordBuf, [](bool success, const std::string& msg) {
                            SetAccountActionMsg(msg, success);
                        });
                    }
                    ImGui::PopStyleColor(2);
                } else {
                    bool canSubmit = (strlen(s_PasswordBuf) >= 6 && strcmp(s_PasswordBuf, s_ConfirmPasswordBuf) == 0 && strlen(s_EmailBuf) > 3);
                    if (!canSubmit) ImGui::BeginDisabled();

                    ImGui::PushStyleColor(ImGuiCol_Button, ThemeCol(theme.accent));
                    ImGui::PushStyleColor(ImGuiCol_Text,   ImVec4(0.04f, 0.06f, 0.08f, 1.0f));
                    if (ImGui::Button("Crear Cuenta e Iniciar", ImVec2(200.0f, 36.0f))) {
                        ClearAccountActionMsg();
                        authMgr.SignUpWithFirebase(s_EmailBuf, s_PasswordBuf, [](bool success, const std::string& msg) {
                            SetAccountActionMsg(msg, success);
                        });
                    }
                    ImGui::PopStyleColor(2);

                    if (!canSubmit) ImGui::EndDisabled();
                }

                if (isAuthBusy) {
                    ImGui::EndDisabled();
                    ImGui::SameLine(0, 14.0f);
                    SpinnerWidget(9.0f, 2.2f, ThemeCol(theme.accent));
                    ImGui::SameLine(0, 8.0f);
                    ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f), "Conectando con Firebase...");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        } else {
            // Usuario Autenticado: Tarjeta de Perfil de alto nivel
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.16f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.22f, 0.45f, 0.78f, 0.65f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));

            const float cardH = 96.0f;
            if (ImGui::BeginChild("##userProfileCard", ImVec2(0.0f, cardH), true, ImGuiWindowFlags_NoScrollbar)) {
                const float innerW = ImGui::GetWindowWidth();
                const float badgeW = 125.0f;
                const float badgeH = 28.0f;

                // Columna izquierda: Información del usuario
                ImGui::BeginGroup();
                std::string dName = user.displayName.empty() ? "Usuario ProyecThor" : user.displayName;
                ImGui::TextColored(ImVec4(0.97f, 0.98f, 1.0f, 1.0f), "%s", dName.c_str());
                ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f), "Correo: %s", user.email.c_str());
                ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "UID: %s", user.localId.c_str());
                ImGui::SameLine(0, 10.0f);
                if (ImGui::SmallButton("Copiar UID")) {
                    ImGui::SetClipboardText(user.localId.c_str());
                }
                ImGui::EndGroup();

                // Columna derecha: Insignia Pro / Base centrada verticalmente en la tarjeta
                const float badgeX = innerW - badgeW - 20.0f;
                const float badgeY = (cardH - badgeH) * 0.5f;
                ImGui::SetCursorPos(ImVec2(badgeX, badgeY));

                if (isPro) {
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.48f, 0.26f, 0.92f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.58f, 0.32f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
                    ImGui::Button("PRO ACTIVO", ImVec2(badgeW, badgeH));
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.16f, 0.18f, 0.24f, 0.90f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.25f, 0.32f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.82f, 0.86f, 0.92f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
                    ImGui::Button("EDICIÓN BASE", ImVec2(badgeW, badgeH));
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ImGui::Dummy(ImVec2(0.0f, 14.0f));

            // Modificar Nickname
            ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "Nombre de Usuario (Nickname):");
            ImGui::TextColored(ImVec4(0.78f, 0.82f, 0.88f, 1.0f), "Nombre visible en ProyecThor y la nube de Firebase:");
            ImGui::SetNextItemWidth(std::clamp(contentW * 0.55f, 260.0f, 380.0f));
            ImGui::InputText("##new_display_name", s_NewDisplayNameBuf, sizeof(s_NewDisplayNameBuf));
            ImGui::SameLine(0, 10.0f);

            if (ImGui::Button("Guardar Nombre", ImVec2(130.0f, 0.0f))) {
                if (strlen(s_NewDisplayNameBuf) > 0) {
                    authMgr.UpdateDisplayName(s_NewDisplayNameBuf, [](bool success, const std::string& msg) {
                        SetAccountActionMsg(msg, success);
                    });
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 14.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // Seguridad
            ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "Seguridad de la Cuenta:");
            ImGui::TextColored(ImVec4(0.78f, 0.82f, 0.88f, 1.0f), "Cambiar contraseña de acceso:");
            ImGui::SetNextItemWidth(std::clamp(contentW * 0.55f, 260.0f, 380.0f));
            ImGui::InputText("##change_pwd_input", s_NewPasswordBuf, sizeof(s_NewPasswordBuf), ImGuiInputTextFlags_Password);
            ImGui::SameLine(0, 10.0f);

            if (ImGui::Button("Actualizar Contraseña", ImVec2(160.0f, 0.0f))) {
                if (strlen(s_NewPasswordBuf) >= 6) {
                    authMgr.ChangePassword(s_NewPasswordBuf, [](bool success, const std::string& msg) {
                        SetAccountActionMsg(msg, success);
                        if (success) s_NewPasswordBuf[0] = '\0';
                    });
                } else {
                    SetAccountActionMsg("La nueva contraseña debe tener al menos 6 caracteres.", false);
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (ImGui::Button("Enviar correo de verificación de cuenta", ImVec2(280.0f, 30.0f))) {
                authMgr.SendEmailVerification([](bool success, const std::string& msg) {
                    SetAccountActionMsg(msg, success);
                });
            }

            ImGui::Dummy(ImVec2(0.0f, 14.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // Cierre de Sesión y Eliminación
            if (ImGui::Button("Cerrar Sesión", ImVec2(140.0f, 32.0f))) {
                try {
                    authMgr.Logout();
                } catch (...) {}
                s_EmailBuf[0] = '\0';
                s_PasswordBuf[0] = '\0';
                s_ConfirmPasswordBuf[0] = '\0';
                s_NewDisplayNameBuf[0] = '\0';
                s_NewPasswordBuf[0] = '\0';
                s_BuffersInitialized = false;
                s_ShowRegisterForm = false;
                s_ShowDeleteModal = false;
                ClearLicenseFeedbackMsg();
                SetAccountActionMsg("Has cerrado sesión correctamente.", true);
                return;
            }

            ImGui::SameLine(0, 20.0f);

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.45f, 0.12f, 0.12f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 0.90f, 0.90f, 1.0f));

            if (ImGui::Button("Eliminar cuenta de forma definitiva...", ImVec2(240.0f, 32.0f))) {
                s_ShowDeleteModal = true;
            }
            ImGui::PopStyleColor(4);

            // Modal de confirmación
            if (s_ShowDeleteModal) {
                ImGui::OpenPopup("¿Eliminar tu cuenta permanentemente?##deleteConfirmModal");
            }

            ImVec2 mCenter = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(mCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("¿Eliminar tu cuenta permanentemente?##deleteConfirmModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushTextWrapPos(420.0f);
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "ADVERTENCIA");
                ImGui::Text(
                    "Esta acción eliminará de forma irreversible tu cuenta en los servidores de Firebase, "
                    "tus datos de usuario y cualquier configuración asociada a tu UID.\n\n"
                    "¿Estás completamente seguro de que deseas proceder?");
                ImGui::PopTextWrapPos();

                ImGui::Dummy(ImVec2(0.0f, 12.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 10.0f));

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 1.0f));
                if (ImGui::Button("Sí, eliminar mi cuenta", ImVec2(180.0f, 34.0f))) {
                    authMgr.DeleteAccount([](bool success, const std::string& msg) {
                        SetAccountActionMsg(msg, success);
                    });
                    s_ShowDeleteModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor();

                ImGui::SameLine(0, 14.0f);
                if (ImGui::Button("Cancelar", ImVec2(120.0f, 34.0f))) {
                    s_ShowDeleteModal = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        std::string actMsg;
        bool actSuccess = false;
        GetAccountActionMsg(actMsg, actSuccess);
        if (!actMsg.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImVec4 col = actSuccess ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextWrapped("%s %s", actSuccess ? "✓ " : "⚠ ", actMsg.c_str());
            ImGui::PopStyleColor();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 1: SUSCRIPCIÓN PRO
    // ─────────────────────────────────────────────────────────────────────────
    else if (m_SelectedSubsection == "Suscripción Pro") {
        // Hero Banner con estética profesional y de alto contraste
        ImVec4 neonBorderCol(0.0f, 0.75f, 0.95f, 0.45f + 0.40f * pulse);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.09f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Border,  neonBorderCol);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 18.0f));

        if (ImGui::BeginChild("##heroBannerPro", ImVec2(0.0f, 104.0f), true, ImGuiWindowFlags_NoScrollbar)) {
            // Línea superior neón
            ImVec2 heroPos = ImGui::GetWindowPos();
            ImVec2 heroSz  = ImGui::GetWindowSize();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(heroPos.x + 18.0f, heroPos.y),
                ImVec2(heroPos.x + heroSz.x - 18.0f, heroPos.y),
                IM_COL32(0, 195, 255, 180), 2.0f
            );

            float priceBadgeW = 155.0f;
            float textW = contentW - priceBadgeW - 60.0f;

            // Columna izquierda: Título y descripción con alto contraste
            ImGui::BeginGroup();
            ImGui::TextColored(ImVec4(0.40f, 0.88f, 1.0f, 1.0f), "ProyecThor for Professionals");
            ImGui::SameLine(0, 8.0f);

            // Badge PRO micro
            {
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 badgeSz(38.0f, 18.0f);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0, ImVec2(p0.x + badgeSz.x, p0.y + badgeSz.y), IM_COL32(0, 180, 255, 40), 9.0f);
                dl->AddRect(p0, ImVec2(p0.x + badgeSz.x, p0.y + badgeSz.y), IM_COL32(0, 200, 255, 160), 9.0f, 0, 1.0f);
                dl->AddText(ImVec2(p0.x + 7.0f, p0.y + 1.0f), IM_COL32(255, 255, 255, 255), "PRO");
                ImGui::Dummy(badgeSz);
            }

            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + textW);
            ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f),
                "Potencia tu auditorio con Inteligencia Artificial avanzada, retorno Stage Display sin latencia y sincronización distribuida en la nube.");
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();

            // Columna derecha: Tarjeta de Precio destacada
            ImGui::SameLine(contentW - priceBadgeW - 35.0f);
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "$9.990 CLP");
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Suscripción mensual");
            ImGui::EndGroup();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // 6 Tarjetas de Ventajas Técnicas (con badges pill, animaciones interactivas y alto contraste)
        struct FeatureItem { const char* tag; const char* title; const char* desc; ImVec4 tagCol; };
        FeatureItem items[6] = {
            { "STAGE DISPLAY", "Retorno Multi-Pantalla", "Monitor de retorno con reloj de cuenta regresiva, estrofa actual, próxima y mensajes para el escenario.", ImVec4(0.0f, 0.80f, 1.0f, 1.0f) },
            { "IA GEMINI", "Inteligencia Artificial Avanzada", "Corrección métrica, estructuración de versos y búsqueda bíblica impulsada por Google Gemini.", ImVec4(0.82f, 0.45f, 1.0f, 1.0f) },
            { "CANVA & APIS", "APIs de Terceros y Canva Connect", "Conexión directa con Canva API para explorar tus diseños e importar diapositivas en alta resolución a Fondos y Multimedia.", ImVec4(0.0f, 0.85f, 0.88f, 1.0f) },
            { "CLOUD SYNC", "Cloud Storage 5,000+ Canciones", "Respaldo y sincronización en Google Firebase con redundancia automática ante fallos de hardware.", ImVec4(0.28f, 0.90f, 0.55f, 1.0f) },
            { "MULTI-SALA", "Sincronización Multi-Sala", "Edita en tu equipo principal y propaga al instante a salas cuna, auditorios auxiliares y sedes filiales.", ImVec4(1.0f, 0.68f, 0.22f, 1.0f) },
            { "INTEGRACIONES", "Ecosistema en Expansión", "Acceso continuo e ilimitado a nuevas integraciones API en la nube y servicios profesionales para tu iglesia.", ImVec4(0.95f, 0.45f, 0.75f, 1.0f) }
        };

        float halfW = (contentW - 14.0f) * 0.5f;
        const float cardH = 136.0f;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 2; ++col) {
                int idx = row * 2 + col;
                std::string childId = "##featCard_" + std::to_string(idx);

                RenderFeatureCard(childId.c_str(), items[idx].tag, items[idx].title, items[idx].desc,
                                  items[idx].tagCol, halfW, cardH, timeVal, idx);

                if (col == 0) ImGui::SameLine(0, 10.0f);
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // Tarjeta de Estado y Pasarela Flow
        const bool isPolling = authMgr.IsPollingPayment();
        ImVec4 statusBg = isPro ? ImVec4(0.06f, 0.16f, 0.10f, 0.90f) :
                          (isPolling ? ImVec4(0.06f, 0.12f, 0.18f, 0.92f) : ImVec4(0.08f, 0.10f, 0.15f, 0.85f));
        ImVec4 statusBorder = isPro ? ImVec4(0.22f, 0.78f, 0.40f, 0.85f) :
                              (isPolling ? ImVec4(0.0f, 0.75f, 0.95f, 0.55f + 0.40f * pulse) : ImVec4(0.25f, 0.32f, 0.45f, 0.70f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, statusBg);
        ImGui::PushStyleColor(ImGuiCol_Border,  statusBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));

        float statusCardH = isPro ? 104.0f : (isPolling ? 142.0f : 132.0f);
        if (ImGui::BeginChild("##proStatusBox", ImVec2(0.0f, statusCardH), true, ImGuiWindowFlags_NoScrollbar)) {
            if (isPro) {
                ImGui::TextColored(ImVec4(0.40f, 0.98f, 0.60f, 1.0f), "PRO ACTIVO EN TU CUENTA");
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                const auto& lic = authMgr.GetLicenseInfo();
                if (!lic.expiryDate.empty()) {
                    int64_t daysLeft = authMgr.GetDaysRemaining();
                    ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.88f, 1.0f),
                        "Suscripción mensual activa verificada en la nube. Vigente hasta: %s (%lld días restantes)",
                        lic.expiryDate.c_str(), (long long)daysLeft);
                } else {
                    ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.88f, 1.0f),
                        "Suscripción mensual activa verificada en tiempo real en los servidores de Firebase.");
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
                if (ImGui::Button("Comprobar estado ahora", ImVec2(210.0f, 32.0f))) {
                    authMgr.RefreshProStatus([](bool success, const std::string& msg) {
                        SetLicenseFeedbackMsg(msg, success);
                    });
                }
            } else if (isPolling) {
                // Estado interactivo de espera de confirmación de pago Flow
                ImGui::TextColored(ImVec4(0.10f, 0.82f, 1.0f, 1.0f), "Esperando confirmación de pago en Flow / Webpay...");
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f),
                    "Se ha abierto Webpay en tu navegador. ProyecThor detectará tu pago y activará Pro en cuanto finalices.");
                ImGui::Dummy(ImVec2(0.0f, 8.0f));

                if (ImGui::Button("Comprobar pago ahora", ImVec2(200.0f, 34.0f))) {
                    authMgr.RefreshProStatus([](bool success, const std::string& msg) {
                        SetLicenseFeedbackMsg(msg, success);
                    });
                }
                ImGui::SameLine(0, 12.0f);
                if (ImGui::Button("Cancelar espera", ImVec2(140.0f, 34.0f))) {
                    authMgr.CancelPaymentPolling();
                    SetLicenseFeedbackMsg("Espera de confirmación cancelada.", false);
                }
            } else {
                const auto& lic = authMgr.GetLicenseInfo();
                const bool isExpired = (lic.status == "expired" || lic.status == "morose" || authMgr.IsLicenseExpired()) && (lic.expiresAt > 0);

                if (lic.status == "unpaid") {
                    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "Suscripción con Pago Pendiente");
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f),
                        "Tu suscripción en Flow tiene una factura pendiente en Webpay. Completa el pago para activar Pro de inmediato.");
                } else if (isExpired) {
                    ImGui::TextColored(ImVec4(0.98f, 0.40f, 0.35f, 1.0f), "Suscripción Vencida (Edición Base)");
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    if (lic.status == "morose") {
                        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                            "El cobro automático de Flow falló (tarjeta rechazada o fondos insuficientes). Actualiza tu medio de pago para reactivar Pro.");
                    } else {
                        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f),
                            "Tu período de ProyecThor for Professionals ha finalizado. Renueva con Flow ($9.990 CLP) para reactivar Pro de inmediato.");
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "Edición Base Gratuita");
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f),
                        "Al pagar con Flow ($9.990 CLP/mes), el programa detectará automáticamente tu pago y activará Pro en segundos.");
                }
                ImGui::Dummy(ImVec2(0.0f, 8.0f));

                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.50f, 0.92f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.62f, 0.98f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.08f, 0.42f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                const char* payBtnLabel = (lic.status == "unpaid") ? "Completar Pago en Flow ($9.990 CLP)" :
                                          (isExpired ? "Renovar con Flow ($9.990 CLP)" : "Pagar y Activar con Flow ($9.990 CLP)");
                if (ImGui::Button(payBtnLabel, ImVec2(290.0f, 34.0f))) {
                    std::string redirectUrl, err;
                    if (authMgr.CreateFlowPaymentOrder(redirectUrl, err, [](bool success, const std::string& msg) {
                        SetLicenseFeedbackMsg(msg, success);
                    })) {
                        SetLicenseFeedbackMsg("Pasarela Flow abierta en el navegador. ProyecThor está a la espera de tu pago...", true);
                    } else {
                        SetLicenseFeedbackMsg(err, false);
                    }
                }
                ImGui::PopStyleColor(4);

                ImGui::SameLine(0, 12.0f);
                if (ImGui::Button("Comprobar suscripción ahora", ImVec2(220.0f, 34.0f))) {
                    authMgr.RefreshProStatus([](bool success, const std::string& msg) {
                        SetLicenseFeedbackMsg(msg, success);
                    });
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Aviso de Activación y Soporte
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.09f, 0.14f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.20f, 0.32f, 0.48f, 0.70f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));

        if (ImGui::BeginChild("##proNoticeBox", ImVec2(0.0f, 114.0f), true, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Condiciones de Activación y Soporte");
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4(0.82f, 0.86f, 0.92f, 1.0f),
                "• Plazo de Activación: La suscripción puede demorarse hasta 24 horas hábiles tras el pago.\n"
                "• Atención Urgente y Soporte: Si requieres tu licencia urgente o ante cualquier problema, escribe a theproyecthor@gmail.com.\n"
                "• Conexión a Internet: La licencia Pro opera exclusivamente con conexión activa a internet para verificación continua.");
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        std::string licMsg;
        bool licSuccess = false;
        GetLicenseFeedbackMsg(licMsg, licSuccess);
        if (!licMsg.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImVec4 col = licSuccess ? ImVec4(0.40f, 0.90f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextWrapped("%s %s", licSuccess ? "✓ " : "⚠ ", licMsg.c_str());
            ImGui::PopStyleColor();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  SECCIÓN 2: TÉRMINOS Y PRIVACIDAD
    // ─────────────────────────────────────────────────────────────────────────
    else if (m_SelectedSubsection == "Términos y Privacidad") {
        // 4 Tarjetas profesionales con barra de identidad, hover suave y alto contraste
        RenderTermsCard("##termsSubCard",
            "Términos y Condiciones de la Suscripción Pro",
            "• Plazo de Activación: La activación de la suscripción puede demorarse hasta 24 horas tras el pago en Flow.\n"
            "• Soporte y Activación Urgente: Si requieres tu licencia de urgencia o experimentas problemas, escribe a theproyecthor@gmail.com.\n"
            "• Conexión Obligatoria a Internet: La licencia Pro opera exclusivamente con conexión activa a internet para verificar continuamente el estado de la cuenta en Firebase y habilitar servicios en la nube (IA Gemini y Sync).\n"
            "• Suspensión o Eliminación por Incumplimiento: La cuenta y el acceso Pro podrán ser revocados o eliminados de forma permanente si se incumplen las reglas de uso (abuso de APIs, reventa no autorizada o conductas ilícitas).",
            ImVec4(0.0f, 0.80f, 1.0f, 1.0f), contentW, 160.0f);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        RenderTermsCard("##termsFirebaseCard",
            "Alojamiento Seguro y Protección en Firebase",
            "Todos los registros de cuenta, credenciales cifradas y sincronización de estado de ProyecThor se alojan de forma segura en la infraestructura oficial de Google Firebase (Google LLC).\n"
            "ProyecThor NO vende, NO arrienda y NO distribuye tus datos a terceros bajo ninguna circunstancia.",
            ImVec4(0.25f, 0.55f, 0.95f, 1.0f), contentW, 105.0f);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        RenderTermsCard("##termsGeminiCard",
            "Directrices y Privacidad de Google Gemini API",
            "• Entrenamiento de Modelos: Las funciones de IA de ProyecThor operan mediante la API oficial de Google Gemini. De acuerdo con los Términos de la API de Gemini, Google procesa las consultas y contenidos enviados a la IA y puede utilizarlos para entrenar y mejorar sus modelos.\n"
            "• Privacidad de Archivos Locales: ÚNICAMENTE se transmiten los datos que envías de forma explícita al usar las herramientas de IA. Tus canciones privadas, notas de sermón, presentaciones y archivos que no envíes a la IA permanecen 100% locales en tu equipo y jamás son transmitidos.",
            ImVec4(0.80f, 0.45f, 1.0f, 1.0f), contentW, 165.0f,
            "Términos de Google Gemini API", "https://ai.google.dev/gemini-api/terms",
            "Políticas de Privacidad de Google", "https://policies.google.com/privacy");

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        RenderTermsCard("##termsCanvaCard",
            "Integración Canva Connect y APIs de Terceros",
            "• Canva Connect API: ProyecThor Pro se conecta oficialmente con Canva mediante OAuth 2.0 PKCE para explorar tus proyectos e importar diapositivas en alta resolución directamente a Fondos o Multimedia.\n"
            "• Seguridad de Credenciales: ProyecThor almacena las credenciales de sesión cifradas localmente y jamás comparte tus tokens ni tus diseños de Canva con terceras partes no autorizadas.\n"
            "• Propiedad Intelectual: Los diseños creados o importados desde Canva permanecen bajo la titularidad y los términos de servicio de Canva Pty Ltd.",
            ImVec4(0.0f, 0.85f, 0.88f, 1.0f), contentW, 160.0f,
            "Términos de Canva", "https://www.canva.com/policies/terms-of-use/",
            "Privacidad de Canva", "https://www.canva.com/policies/privacy-policy/");

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        RenderTermsCard("##termsOfflineCard",
            "Soberanía Local y Funcionamiento Offline",
            "Tus archivos multimedia, colecciones bíblicas XML y librerías de culto se conservan directamente en el disco duro de tu computadora para operación sin conexión en Edición Base.\n"
            "ProyecThor garantiza pleno respeto al derecho a la supresión definitiva de tus datos.",
            ImVec4(0.28f, 0.85f, 0.55f, 1.0f), contentW, 105.0f);

        ImGui::Dummy(ImVec2(0.0f, 20.0f));
    }
}

} // namespace ProyecThor::UI::Settings
