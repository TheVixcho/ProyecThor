#include "CanvaPanel.h"
#include "core/CanvaClient.h"
#include "core/AuthLicenseManager.h"
#include "ui/framework/AppIcons.h"
#include "ui/framework/DesignSystem.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace ProyecThor::UI {

CanvaPanel::CanvaPanel() {
}

void CanvaPanel::SetNotification(const std::string& msg, bool success) {
    std::lock_guard<std::mutex> lock(m_NotificationMutex);
    m_NotificationMsg = msg;
    m_NotificationIsSuccess = success;
    m_NotificationTimer = 6.0f; // Visible durante 6 segundos
}

void CanvaPanel::Render(bool* pOpen, GlassRenderer& glass) {
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(960.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(600.0f, 400.0f), ImVec2(1920.0f, 1080.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.13f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.00f, 0.77f, 0.80f, 0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.0f, 14.0f));

    std::string titleStr = "Canva Workspace - ProyecThor Pro";
    bool isOpen = ImGui::Begin(titleStr.c_str(), pOpen, ImGuiWindowFlags_NoCollapse);

    if (isOpen) {
        RenderContent(&glass);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void CanvaPanel::RenderContent(GlassRenderer* /*glass*/) {
    auto& canva = ProyecThor::Core::CanvaClient::Get();
    auto& auth  = ProyecThor::Core::AuthLicenseManager::Get();

    // Procesar texturas descargadas en el hilo OpenGL principal
    canva.ProcessPendingTextures();

    // Actualizar temporizador de notificaciones
    float dt = ImGui::GetIO().DeltaTime;
    std::string curNoteMsg;
    bool curNoteSuccess = false;
    {
        std::lock_guard<std::mutex> lock(m_NotificationMutex);
        if (m_NotificationTimer > 0.0f) {
            m_NotificationTimer -= dt;
            if (m_NotificationTimer <= 0.0f) {
                m_NotificationMsg.clear();
            }
        }
        curNoteMsg = m_NotificationMsg;
        curNoteSuccess = m_NotificationIsSuccess;
    }

    const bool isLoggedIn = auth.IsLoggedIn();
    const bool isPro      = auth.IsPro();
    const float availW    = ImGui::GetContentRegionAvail().x;
    const float availH    = ImGui::GetContentRegionAvail().y;

    // ─────────────────────────────────────────────────────────────────────────
    // 1. BARRERA DE LICENCIA PRO
    // ─────────────────────────────────────────────────────────────────────────
    if (!isLoggedIn || !isPro) {
        float boxW = std::min(availW - 8.0f, 560.0f);
        if (boxW < 240.0f) boxW = availW;
        float boxX = std::max(0.0f, (availW - boxW) * 0.5f);

        if (boxX > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + boxX);
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.18f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.96f, 0.72f, 0.20f, 0.65f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));

        if (ImGui::BeginChild("##proBarrierBox", ImVec2(boxW, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 iconPos = ImGui::GetCursorScreenPos();
            AppIcons::DrawIcon_Canva(dl, iconPos, 32.0f, IM_COL32(0, 196, 204, 255));
            ImGui::Dummy(ImVec2(40.0f, 32.0f));
            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.88f, 1.0f), "CANVA CONNECT API");
            ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "Integración de Diseños y Carpetas (Pro)");
            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "Característica exclusiva de ProyecThor Pro.");
            ImGui::Dummy(ImVec2(0.0f, 4.0f));

            ImGui::TextWrapped(
                "Conecta tu cuenta de Canva para explorar tus proyectos en tiempo real, "
                "visualizar miniaturas e importarlas en alta calidad (PNG) "
                "directamente a tu biblioteca de Fondos o Multimedia con un solo clic."
            );

            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f), "✓ Explorador de proyectos y carpetas de Canva");
            ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f), "✓ Miniaturas de diapositivas en tiempo real");
            ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f), "✓ Descarga directa a Fondos o Multimedia");
            ImGui::TextColored(ImVec4(0.40f, 0.90f, 0.55f, 1.0f), "✓ Licenciamiento oficial Canva Connect API");

            ImGui::Dummy(ImVec2(0.0f, 14.0f));

            if (!isLoggedIn) {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "Inicia sesión con tu cuenta de ProyecThor para usar Canva.");
            } else {
                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f),
                    "Tu cuenta está en Edición Base. Activa Pro con Flow ($9.990 CLP) para desbloquear Canva.");
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

    } else if (!canva.IsConnected()) {
        // ─────────────────────────────────────────────────────────────────────
        // 2. PANTALLA DE VINCULACIÓN OAUTH 2.0 PKCE CON CANVA
        // ─────────────────────────────────────────────────────────────────────
        float boxW = std::min(availW - 8.0f, 580.0f);
        if (boxW < 240.0f) boxW = availW;
        float boxX = std::max(0.0f, (availW - boxW) * 0.5f);

        if (boxX > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + boxX);
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.18f, 0.92f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.00f, 0.77f, 0.80f, 0.55f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));

        if (ImGui::BeginChild("##canvaConnectBox", ImVec2(boxW, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 iconPos = ImGui::GetCursorScreenPos();
            AppIcons::DrawIcon_Canva(dl, iconPos, 34.0f, IM_COL32(0, 196, 204, 255));
            ImGui::Dummy(ImVec2(42.0f, 34.0f));
            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.88f, 1.0f), "VINCULACIÓN OFICIAL CANVA CONNECT");
            ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "Conecta tu cuenta de Canva a ProyecThor Pro");
            ImGui::EndGroup();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            ImGui::TextWrapped(
                "Autoriza a ProyecThor en Canva abriendo tu navegador con OAuth 2.0 PKCE. "
                "Una vez aceptado, tus carpetas y diseños se sincronizarán directamente en esta sección."
            );

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            // Configuración de Redirect URI y Scopes
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.12f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(0.00f, 0.65f, 0.70f, 0.40f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

            if (ImGui::BeginChild("##redirectConfigBox", ImVec2(0.0f, 160.0f), true, ImGuiWindowFlags_NoScrollbar)) {
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.95f, 1.0f), "1. URL de Redirección (Canva Redirect URL):");
                std::string curRedirect = canva.GetRedirectUri();
                bool isLocal = (curRedirect == "http://127.0.0.1:8765/callback");
                bool isWeb   = (curRedirect == "https://proyecthor.web.app/" || curRedirect == "https://proyecthor.web.app");

                if (ImGui::RadioButton("Directo Local (http://127.0.0.1:8765/callback)", isLocal)) {
                    canva.SetRedirectUri("http://127.0.0.1:8765/callback");
                }
                if (ImGui::RadioButton("Web ProyecThor (https://proyecthor.web.app/)", isWeb)) {
                    canva.SetRedirectUri("https://proyecthor.web.app/");
                }

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.95f, 1.0f), "2. Permisos en Canva Developers:");
                std::string curScopes = canva.GetScopes();
                bool isStandardScopes = (curScopes.find("folder:read") != std::string::npos);
                bool isMinimalScopes  = (!isStandardScopes);

                if (ImGui::RadioButton("Estándar (Diseños + Carpetas)", isStandardScopes)) {
                    canva.SetScopes("design:meta:read design:content:read folder:read asset:read");
                }
                ImGui::SameLine(0, 12.0f);
                if (ImGui::RadioButton("Mínimo (Solo Diseños)", isMinimalScopes)) {
                    canva.SetScopes("design:meta:read design:content:read");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ImGui::Dummy(ImVec2(0.0f, 12.0f));

            const bool isWaiting = canva.IsWaitingOAuth();
            float btnW = std::min(boxW - 40.0f, 320.0f);

            if (!isWaiting) {
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.00f, 0.65f, 0.70f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.78f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.00f, 0.55f, 0.60f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

                if (ImGui::Button("Vincular con Canva (Abrir Navegador)", ImVec2(btnW, 38.0f))) {
                    canva.StartOAuthFlow([this](bool success, const std::string& msg) {
                        SetNotification(msg, success);
                    });
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
            } else {
                float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 3.5f);
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.95f, 0.60f + 0.40f * pulse),
                                   "⌛ Esperando autorización en el navegador web...");
                ImGui::Dummy(ImVec2(0.0f, 4.0f));

                if (ImGui::Button("Cancelar intento", ImVec2(140.0f, 30.0f))) {
                    canva.CancelOAuthFlow();
                }
                ImGui::SameLine(0, 8.0f);
                if (ImGui::Button("Copiar enlace", ImVec2(140.0f, 30.0f))) {
                    ImGui::SetClipboardText(canva.GetAuthorizationUrl().c_str());
                    SetNotification("Enlace copiado al portapapeles.", true);
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 12.0f));

            // Entrada manual
            if (ImGui::TreeNodeEx("¿El navegador no se abrió? (Vinculación Manual)", ImGuiTreeNodeFlags_Framed)) {
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                ImGui::TextColored(ImVec4(0.78f, 0.82f, 0.88f, 1.0f), "Pega la URL de retorno o código:");
                ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 100.0f));
                ImGui::InputTextWithHint("##manualCodeInput", "http://127.0.0.1:8765/callback?code=...", m_ManualCodeBuf, sizeof(m_ManualCodeBuf));
                ImGui::SameLine(0, 6.0f);

                if (ImGui::Button("Vincular", ImVec2(90.0f, 0.0f))) {
                    if (strlen(m_ManualCodeBuf) > 0) {
                        canva.CompleteOAuthManual(m_ManualCodeBuf, [this](bool success, const std::string& msg) {
                            SetNotification(msg, success);
                        });
                    }
                }
                ImGui::TreePop();
            }

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextColored(ImVec4(0.50f, 0.55f, 0.65f, 1.0f),
                "Cumplimiento Canva: La conexión utiliza tokens encriptados y permisos oficiales Canva Connect API.");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

    } else {
        // ─────────────────────────────────────────────────────────────────────
        // 3. EXPLORADOR DE PROYECTOS, CARPETAS Y MINIATURAS CANVA
        // ─────────────────────────────────────────────────────────────────────

        // Barra superior de navegación (responsive)
        const bool narrowTopNav = (availW < 460.0f);
        const float navH = narrowTopNav ? 74.0f : 44.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.13f, 0.18f, 0.88f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

        if (ImGui::BeginChild("##canvaTopNav", ImVec2(0.0f, navH), true, ImGuiWindowFlags_NoScrollbar)) {
            // Icono Canva
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 iconPos = ImGui::GetCursorScreenPos();
            AppIcons::DrawIcon_Canva(dl, iconPos, 20.0f, IM_COL32(0, 196, 204, 255));
            ImGui::Dummy(ImVec2(24.0f, 20.0f));
            ImGui::SameLine(0, 6.0f);

            // Migas de pan (Breadcrumbs)
            const auto& crumbs = canva.GetBreadcrumbs();
            for (size_t i = 0; i < crumbs.size(); ++i) {
                if (i > 0) {
                    ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.60f, 1.0f), ">");
                    ImGui::SameLine(0, 4.0f);
                }
                std::string btnLbl = (crumbs[i].id == "root") ? "Proyectos" : crumbs[i].name;
                std::string crumbId = btnLbl + "##crumb_" + std::to_string(i);

                if (i == crumbs.size() - 1) {
                    ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.88f, 1.0f), "%s", btnLbl.c_str());
                } else {
                    if (ImGui::SmallButton(crumbId.c_str())) {
                        while (canva.GetBreadcrumbs().size() > (i + 1)) {
                            canva.NavigateBack();
                        }
                    }
                }
                ImGui::SameLine(0, 4.0f);
            }

            if (narrowTopNav) {
                ImGui::NewLine();
            } else {
                float rightW = 280.0f;
                float rightX = ImGui::GetWindowWidth() - rightW - 10.0f;
                if (rightX > ImGui::GetCursorPosX()) {
                    ImGui::SetCursorPosX(rightX);
                }
            }

            // Filtro de búsqueda
            float searchW = narrowTopNav ? (ImGui::GetContentRegionAvail().x - 165.0f) : 120.0f;
            if (searchW < 70.0f) searchW = 70.0f;
            ImGui::SetNextItemWidth(searchW);
            ImGui::InputTextWithHint("##searchCanva", "Buscar...", m_SearchFilter, sizeof(m_SearchFilter));
            ImGui::SameLine(0, 6.0f);

            if (ImGui::Button("↺ Actualizar")) {
                canva.RefreshCurrentFolder();
            }
            ImGui::SameLine(0, 6.0f);

            if (ImGui::Button("Desconectar")) {
                canva.Disconnect();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        // Notificación de estado (Toast)
        if (!curNoteMsg.empty()) {
            ImVec4 noteCol = curNoteSuccess ? ImVec4(0.40f, 0.90f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::TextColored(noteCol, "%s %s", curNoteSuccess ? "✓ " : "⚠ ", curNoteMsg.c_str());
            if (curNoteSuccess && m_OnOpenMultimedia) {
                ImGui::SameLine(0, 10.0f);
                if (ImGui::SmallButton("Ver en Multimedia ›")) {
                    m_OnOpenMultimedia();
                }
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // Grilla responsiva de Proyectos y Diseños
        float contentH = ImGui::GetContentRegionAvail().y - 28.0f;
        if (contentH < 120.0f) contentH = 120.0f;

        if (ImGui::BeginChild("##canvaGridArea", ImVec2(0.0f, contentH), true)) {
            auto items = canva.GetCurrentItemsCopy();

            std::string filterLower = m_SearchFilter;
            for (char& c : filterLower) c = (char)tolower((unsigned char)c);

            std::vector<ProyecThor::Core::CanvaItem> filtered;
            for (const auto& it : items) {
                if (filterLower.empty()) {
                    filtered.push_back(it);
                } else {
                    std::string titleLower = it.title;
                    for (char& c : titleLower) c = (char)tolower((unsigned char)c);
                    if (titleLower.find(filterLower) != std::string::npos) {
                        filtered.push_back(it);
                    }
                }
            }

            if (filtered.empty()) {
                ImGui::Dummy(ImVec2(0.0f, 30.0f));
                if (canva.IsBusy()) {
                    float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.0f);
                    ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.88f, 0.60f + 0.40f * pulse),
                                       "⌛ Cargando proyectos de Canva...");
                } else {
                    ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "No se encontraron elementos en esta carpeta.");
                }
            } else {
                float availGridW = ImGui::GetContentRegionAvail().x;
                const float minCardW = 155.0f;
                const float spacing  = 10.0f;
                int cols = std::max(1, (int)((availGridW + spacing) / (minCardW + spacing)));
                float cardW = (availGridW - (cols - 1) * spacing) / (float)cols;
                const float cardH = 205.0f;
                const float thumbAreaH = 105.0f;

                for (size_t idx = 0; idx < filtered.size(); ++idx) {
                    const auto& item = filtered[idx];

                    if (idx % cols != 0) {
                        ImGui::SameLine(0, spacing);
                    }

                    std::string cardChildId = "##card_" + item.id + "_" + std::to_string(idx);
                    std::string cardPopupId = "##ctx_card_" + item.id + "_" + std::to_string(idx);
                    bool isFolder = (item.type == "folder");

                    ImVec4 cardBg = isFolder ? ImVec4(0.14f, 0.17f, 0.24f, 0.90f) : ImVec4(0.11f, 0.13f, 0.19f, 0.92f);
                    ImVec4 cardBorder = isFolder ? ImVec4(0.25f, 0.42f, 0.65f, 0.65f) : ImVec4(0.18f, 0.25f, 0.38f, 0.60f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, cardBg);
                    ImGui::PushStyleColor(ImGuiCol_Border,  cardBorder);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

                    bool cardRightClicked = false;
                    bool cardDoubleClicked = false;
                    bool openFolderRequested = false;

                    if (ImGui::BeginChild(cardChildId.c_str(), ImVec2(cardW, cardH), true, ImGuiWindowFlags_NoScrollbar)) {
                        // Detección de clics en la tarjeta
                        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                                cardRightClicked = true;
                            }
                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                cardDoubleClicked = true;
                            }
                        }

                        // 1. Área de miniatura / icono
                        ImVec2 pMin = ImGui::GetCursorScreenPos();
                        float innerW = cardW - 16.0f;
                        ImVec2 pMax = ImVec2(pMin.x + innerW, pMin.y + thumbAreaH);

                        if (isFolder) {
                            ImDrawList* dl = ImGui::GetWindowDrawList();
                            dl->AddRectFilled(pMin, pMax, IM_COL32(26, 38, 54, 200), 6.0f);
                            dl->AddRect(pMin, pMax, IM_COL32(56, 189, 248, 60), 6.0f);

                            ImVec2 iconPos = ImVec2(pMin.x + innerW * 0.5f - 24.0f, pMin.y + thumbAreaH * 0.5f - 18.0f);
                            dl->AddText(iconPos, IM_COL32(251, 191, 36, 255), "[CARPETA]");
                            dl->AddText(ImVec2(pMin.x + 10.0f, pMin.y + thumbAreaH - 18.0f),
                                        IM_COL32(148, 163, 184, 220), "Doble clic para abrir");
                            ImGui::Dummy(ImVec2(innerW, thumbAreaH));
                        } else {
                            if (item.glTextureId != 0) {
                                ImGui::Image((ImTextureID)(intptr_t)item.glTextureId, ImVec2(innerW, thumbAreaH));
                            } else {
                                ImDrawList* dl = ImGui::GetWindowDrawList();
                                dl->AddRectFilled(pMin, pMax, IM_COL32(20, 24, 34, 220), 6.0f);
                                dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 20), 6.0f);
                                float pulse = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.0f);
                                dl->AddText(ImVec2(pMin.x + 12.0f, pMin.y + thumbAreaH * 0.5f - 8.0f),
                                            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.77f, 0.80f, 0.40f + 0.50f * pulse)),
                                            "Cargando vista...");
                                ImGui::Dummy(ImVec2(innerW, thumbAreaH));
                            }
                        }

                        // 2. Título del elemento
                        ImGui::Dummy(ImVec2(0.0f, 3.0f));
                        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + innerW);
                        std::string displayTitle = item.title;
                        if (displayTitle.size() > 30) {
                            displayTitle = displayTitle.substr(0, 27) + "...";
                        }
                        ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.98f, 1.0f), "%s", displayTitle.c_str());
                        ImGui::PopTextWrapPos();

                        // 3. Botones de acción directa
                        ImGui::SetCursorPosY(cardH - 32.0f);
                        if (isFolder) {
                            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.28f, 0.44f, 0.85f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.38f, 0.58f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.22f, 0.36f, 1.0f));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

                            if (ImGui::Button(("Abrir Carpeta##btn_" + item.id).c_str(), ImVec2(innerW, 24.0f))) {
                                openFolderRequested = true;
                            }

                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor(3);
                        } else {
                            float halfBtnW = (innerW - 6.0f) * 0.5f;

                            // Botón "+ Fondos"
                            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.45f, 0.38f, 0.90f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.60f, 0.50f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.06f, 0.35f, 0.30f, 1.0f));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

                            if (ImGui::Button(("+ Fondos##" + item.id).c_str(), ImVec2(halfBtnW, 24.0f))) {
                                canva.ExportAndImportDesign(item.id, item.title, true, [this](bool success, const std::string&, const std::string& msg) {
                                    SetNotification(msg, success);
                                });
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Descargar e importar directamente a Fondos (Backgrounds)");
                            }

                            ImGui::SameLine(0, 6.0f);

                            // Botón "+ Media"
                            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.32f, 0.60f, 0.90f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.44f, 0.78f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.25f, 0.48f, 1.0f));

                            if (ImGui::Button(("+ Media##" + item.id).c_str(), ImVec2(halfBtnW, 24.0f))) {
                                canva.ExportAndImportDesign(item.id, item.title, false, [this](bool success, const std::string&, const std::string& msg) {
                                    SetNotification(msg, success);
                                });
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Descargar e importar a Multimedia (Imágenes)");
                            }

                            ImGui::PopStyleColor(6);
                            ImGui::PopStyleVar();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(2);

                    // Detectar clics en la tarjeta desde la ventana contenedora
                    if (ImGui::IsItemHovered()) {
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            cardRightClicked = true;
                        }
                        if (isFolder && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            openFolderRequested = true;
                        }
                    }

                    if (cardRightClicked) {
                        ImGui::OpenPopup(cardPopupId.c_str());
                    }

                    // Menú contextual con clic derecho
                    if (ImGui::BeginPopup(cardPopupId.c_str())) {
                        if (isFolder) {
                            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.20f, 1.0f), "Carpeta: %s", item.title.c_str());
                            ImGui::Separator();
                            if (ImGui::MenuItem("📂 Abrir Carpeta")) {
                                openFolderRequested = true;
                            }
                        } else {
                            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.88f, 1.0f), "%s", item.title.c_str());
                            ImGui::Separator();

                            if (ImGui::MenuItem("📥 Descargar y Enviar a Fondos")) {
                                canva.ExportAndImportDesign(item.id, item.title, true, [this](bool success, const std::string&, const std::string& msg) {
                                    SetNotification(msg, success);
                                });
                            }

                            if (ImGui::MenuItem("📥 Descargar y Enviar a Multimedia")) {
                                canva.ExportAndImportDesign(item.id, item.title, false, [this](bool success, const std::string&, const std::string& msg) {
                                    SetNotification(msg, success);
                                });
                            }

                            ImGui::Separator();

                            if (ImGui::MenuItem("🔗 Abrir en Canva.com")) {
                                std::string url = "https://www.canva.com/design/" + item.id;
#ifdef _WIN32
                                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
                            }

                            if (ImGui::MenuItem("📋 Copiar ID de diseño")) {
                                ImGui::SetClipboardText(item.id.c_str());
                                SetNotification("ID copiado al portapapeles.", true);
                            }
                        }
                        ImGui::EndPopup();
                    }

                    if (isFolder && (openFolderRequested || cardDoubleClicked)) {
                        canva.NavigateToFolder(item.id, item.title);
                    }
                }
            }
        }
        ImGui::EndChild();

        // Barra inferior de estado
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        std::string st = canva.GetStatusMessage();
        if (st.empty()) st = "Explorando biblioteca de Canva.";
        ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.75f, 1.0f), "%s", st.c_str());

        if (availW > 380.0f) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 240.0f);
            ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "Canva Connect API • Oficial");
        }
    }
}

} // namespace ProyecThor::UI
