#include "ProAIChatPanel.h"
#include "core/ProAIManager.h"
#include "core/AuthLicenseManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/LoadingSpinner.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>

namespace ProyecThor::UI {

// Helper para detectar si un texto parece contener la letra de una canción estructurada
static bool LooksLikeSongLyrics(const std::string& text) {
    if (text.size() < 60) return false;
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower.find("estrofa") != std::string::npos ||
            lower.find("coro") != std::string::npos ||
            lower.find("puente") != std::string::npos ||
            lower.find("verso") != std::string::npos ||
            lower.find("\n\n") != std::string::npos);
}

// Extrae el título sugerido de la primera línea de la respuesta de IA
static std::string ExtractSuggestedTitle(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Quitar caracteres Markdown como # o *
        size_t start = line.find_first_not_of(" #*-\t\r\n");
        if (start != std::string::npos) {
            std::string candidate = line.substr(start);
            // Si no es un encabezado genérico
            if (candidate.find("Estrofa") == std::string::npos &&
                candidate.find("Coro") == std::string::npos &&
                candidate.size() > 2 && candidate.size() < 50) {
                return candidate;
            }
        }
    }
    return "Cancion IA Pro";
}

void ProAIChatPanel::Render(bool* pOpen, GlassRenderer& glass) {
    if (!pOpen || !*pOpen) return;

    auto& proMgr  = ProyecThor::Core::ProAIManager::Get();
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 880.0f, 650.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 600.0f, 450.0f }, { 10000.0f, 10000.0f });

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Asistente y Chat IA Pro (Gemini / DeepSeek)", glass, pOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(18.0f, 16.0f));

    if (open) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float availW = ImGui::GetContentRegionAvail().x;

        // ── Cabecera: Monitoreo de Tokens Disponibles (Límite mensual) ──────
        ImVec2 topP = ImGui::GetCursorScreenPos();
        float topH  = 48.0f;
        dl->AddRectFilled(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(28, 22, 40, 220), DS::RadiusMedium);
        dl->AddRect(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(192, 132, 252, 120), DS::RadiusMedium);

        ImGui::SetCursorScreenPos(ImVec2(topP.x + 16.0f, topP.y + 10.0f));

        uint64_t availTokens = proMgr.GetAvailableTokens();
        uint64_t maxTokens   = proMgr.GetMaxMonthlyTokens();
        float fraction = (maxTokens > 0) ? (float)availTokens / (float)maxTokens : 0.0f;

        // Mostrar únicamente tokens disponibles sin mencionar dinero
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.70f, 1.0f, 1.0f));
        ImGui::Text("Tokens disponibles este mes: %llu / %llu",
                    (unsigned long long)availTokens, (unsigned long long)maxTokens);
        ImGui::PopStyleColor();

        // Barra de progreso de tokens
        ImGui::SetCursorScreenPos(ImVec2(topP.x + 16.0f, topP.y + 30.0f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.70f, 0.40f, 0.95f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.15f, 0.12f, 0.22f, 1.0f));
        ImGui::ProgressBar(fraction, ImVec2(availW - 170.0f, 8.0f), "");
        ImGui::PopStyleColor(2);

        // Selector de Proveedor IA (Gemini / DeepSeek)
        auto curProvider = proMgr.GetCurrentProvider();
        bool isGemini = (curProvider == ProyecThor::Core::AIProvider::Gemini);

        ImGui::SetCursorScreenPos(ImVec2(topP.x + availW - 390.0f, topP.y + 10.0f));
        if (isGemini) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.90f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.28f, 0.7f));
        }
        if (ImGui::Button("Gemini Flash (Gratis)##btnGem", ImVec2(150.0f, 28.0f))) {
            proMgr.SetCurrentProvider(ProyecThor::Core::AIProvider::Gemini);
        }
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 6.0f);
        if (!isGemini) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.25f, 0.85f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.28f, 0.7f));
        }
        if (ImGui::Button("DeepSeek V3##btnDeep", ImVec2(110.0f, 28.0f))) {
            proMgr.SetCurrentProvider(ProyecThor::Core::AIProvider::DeepSeek);
        }
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 6.0f);
        if (DS::GlassButton("Limpiar##btnClean", ImVec2(100.0f, 28.0f))) {
            m_Messages.clear();
            m_StatusFeedback.clear();
        }

        ImGui::SetCursorScreenPos(ImVec2(topP.x, topP.y + topH + 10.0f));

        // ── Píldoras de sugerencias rápidas ─────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::TextUnformatted("Sugerencias:");
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 8.0f);
        if (DS::GlassButton("Componer alabanza de adoración", ImVec2(220.0f, 24.0f))) {
            strncpy(m_InputBuf, "Escribe una canción de adoración sobre la fidelidad y amor de Dios con 2 estrofas y coro.", sizeof(m_InputBuf) - 1);
        }

        ImGui::SameLine(0, 8.0f);
        if (DS::GlassButton("Estructurar en formato TXT", ImVec2(185.0f, 24.0f))) {
            strncpy(m_InputBuf, "Escribe una canción cristiana estructurada en Estrofa 1, Estrofa 2, Coro y Puente, separadas por líneas en blanco.", sizeof(m_InputBuf) - 1);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Feedback de acciones (ej: canción guardada)
        if (!m_StatusFeedback.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.55f, 1.0f));
            ImGui::TextUnformatted(m_StatusFeedback.c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // ── Historial de Mensajes (Área de Scroll) ──────────────────────────
        float inputAreaH = 68.0f;
        float chatH = ImGui::GetContentRegionAvail().y - inputAreaH - 10.0f;

        ImGui::BeginChild("##chat_scroll", ImVec2(-1.0f, chatH), true);

        if (m_Messages.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 30.0f));
            ImGui::SetCursorPosX(availW * 0.15f);
            ImGui::PushTextWrapPos(availW * 0.85f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextWrapped("Hola! Soy tu asistente de ProyecThor for Professionals potenciado por DeepSeek. "
                               "Puedes pedirme que componga canciones completas, busque letras, organice estrofas "
                               "o cree diapositivas en formato TXT listas para proyectar.");
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        } else {
            for (size_t i = 0; i < m_Messages.size(); ++i) {
                const auto& msg = m_Messages[i];
                bool isUser = (msg.role == "user");

                ImGui::Dummy(ImVec2(0.0f, 8.0f));

                if (isUser) {
                    // Burbuja de Usuario (alineada a la derecha)
                    float bubbleW = std::min(availW * 0.75f, ImGui::CalcTextSize(msg.content.c_str()).x + 36.0f);
                    ImGui::SetCursorPosX(availW - bubbleW - 16.0f);

                    ImVec2 bP = ImGui::GetCursorScreenPos();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(40, 30, 65, 230));
                    ImGui::BeginChild(("##usr_b_" + std::to_string(i)).c_str(), ImVec2(bubbleW, 0.0f),
                                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.85f, 1.0f, 1.0f));
                    ImGui::TextWrapped("%s", msg.content.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                } else {
                    // Burbuja de Asistente (alineada a la izquierda)
                    float bubbleW = availW * 0.88f;
                    ImGui::SetCursorPosX(10.0f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 20, 28, 235));
                    ImGui::BeginChild(("##ast_b_" + std::to_string(i)).c_str(), ImVec2(bubbleW, 0.0f),
                                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

                    // Etiqueta del modelo
                    bool isGem = (proMgr.GetCurrentProvider() == ProyecThor::Core::AIProvider::Gemini);
                    ImGui::PushStyleColor(ImGuiCol_Text, isGem ? ImVec4(0.40f, 0.75f, 1.0f, 1.0f) : ImVec4(0.75f, 0.50f, 1.0f, 1.0f));
                    ImGui::TextUnformatted(isGem ? "Google Gemini AI (Gratis)" : "DeepSeek AI");
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));

                    // Contenido de la respuesta
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.92f, 0.96f, 1.0f));
                    ImGui::TextWrapped("%s", msg.content.c_str());
                    ImGui::PopStyleColor();

                    // Acciones si parece una canción estructurada
                    if (LooksLikeSongLyrics(msg.content)) {
                        ImGui::Dummy(ImVec2(0.0f, 8.0f));
                        ImGui::Separator();
                        ImGui::Dummy(ImVec2(0.0f, 4.0f));

                        if (DS::GlassButton(("Copiar TXT##cp_" + std::to_string(i)).c_str(), ImVec2(100.0f, 26.0f))) {
                            ImGui::SetClipboardText(msg.content.c_str());
                            m_StatusFeedback = "Letra copiada al portapapeles.";
                        }

                        ImGui::SameLine(0, 8.0f);
                        if (DS::GlassButton(("Crear en Biblioteca##lib_" + std::to_string(i)).c_str(), ImVec2(160.0f, 26.0f))) {
                            std::string title = ExtractSuggestedTitle(msg.content);
                            ProyecThor::Library::CreateNewSongFromText(title, msg.content);
                            m_StatusFeedback = "Canción '" + title + "' agregada a tu biblioteca local.";
                        }

                        ImGui::SameLine(0, 8.0f);
                        if (DS::GlassButton(("Guardar en Nube Pro##cld_" + std::to_string(i)).c_str(), ImVec2(160.0f, 26.0f))) {
                            std::string title = ExtractSuggestedTitle(msg.content);
                            proMgr.UploadCloudSong(title, msg.content, [this, title](bool ok, const std::string& msg) {
                                m_StatusFeedback = ok ? ("Guardada en la nube: " + title) : msg;
                            });
                        }
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                }
            }
        }

        if (m_ScrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            m_ScrollToBottom = false;
        }

        ImGui::EndChild();

        // ── Área de Entrada de Mensaje ───────────────────────────────────────
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const bool isBusy = proMgr.IsChatBusy();
        const bool canSend = proMgr.CanSendMessage();

        if (!canSend) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("Has alcanzado el límite mensual de tokens de tu suscripción. Tu cuota se renovará al finalizar el mes o al ampliar tu suscripción a una de mayor capacidad (próximamente).");
            ImGui::PopStyleColor();
        } else {
            if (isBusy) ImGui::BeginDisabled();

            ImGui::SetNextItemWidth(availW - 130.0f);
            bool enterPressed = ImGui::InputTextWithHint("##chat_user_input",
                                                         "Escribe tu mensaje o pide una canción... (Presiona Enter)",
                                                         m_InputBuf, sizeof(m_InputBuf),
                                                         ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine(0, 10.0f);

            bool sendClicked = ImGui::Button(isBusy ? "Pensando..." : "Enviar", ImVec2(110.0f, 32.0f));

            if ((enterPressed || sendClicked) && m_InputBuf[0] != '\0' && !isBusy) {
                std::string userPrompt = m_InputBuf;
                m_Messages.push_back({ "user", userPrompt });
                m_InputBuf[0] = '\0';
                m_ScrollToBottom = true;
                m_StatusFeedback.clear();

                proMgr.SendChatMessage(m_Messages, [this](bool success, const std::string& reply, uint32_t tokens, const std::string& err) {
                    if (success) {
                        m_Messages.push_back({ "assistant", reply });
                    } else {
                        m_Messages.push_back({ "assistant", "Error: " + err });
                    }
                    m_ScrollToBottom = true;
                });
            }

            if (isBusy) {
                ImGui::EndDisabled();
                ImGui::SameLine(0, 10.0f);
                DrawLoadingSpinner(dl, ImVec2(ImGui::GetCursorScreenPos().x + 10.0f, ImGui::GetCursorScreenPos().y + 15.0f), 8.0f);
            }
        }
    }

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI
