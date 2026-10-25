#include "ProAIChatPanel.h"
#include "core/ProAIManager.h"
#include "core/AuthLicenseManager.h"
#include "core/AppPaths.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/LoadingSpinner.h"
#include "ui/framework/FilePicker.h"
#include "ui/panels/biblio/LibrarySongs.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace ProyecThor::UI {

// Helper para formatear números con separador de miles
static std::string FormatNumberWithCommas(uint64_t value) {
    std::string s = std::to_string(value);
    int n = (int)s.length() - 3;
    while (n > 0) {
        s.insert(n, ",");
        n -= 3;
    }
    return s;
}

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
        size_t start = line.find_first_not_of(" #*-\t\r\n");
        if (start != std::string::npos) {
            std::string candidate = line.substr(start);
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

    // Asegurar que Gemini sea siempre el proveedor activo
    proMgr.SetCurrentProvider(ProyecThor::Core::AIProvider::Gemini);

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 880.0f, 650.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 600.0f, 450.0f }, { 10000.0f, 10000.0f });

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Asistente IA Pro (Google Gemini)", glass, pOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(18.0f, 16.0f));

    if (open) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float availW = ImGui::GetContentRegionAvail().x;

        // ── Cabecera: Monitoreo de Tokens Totales ────────────────────────────
        ImVec2 topP = ImGui::GetCursorScreenPos();
        float topH  = 40.0f;
        dl->AddRectFilled(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(20, 28, 45, 230), DS::RadiusMedium);
        dl->AddRect(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(56, 189, 248, 140), DS::RadiusMedium);

        ImGui::SetCursorScreenPos(ImVec2(topP.x + 16.0f, topP.y + 11.0f));

        uint64_t availTokens = proMgr.GetAvailableTokens();
        std::string tokensStr = FormatNumberWithCommas(availTokens);

        // Mostrar únicamente tokens totales
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.90f, 1.0f, 1.0f));
        ImGui::Text("Tokens totales: %s", tokensStr.c_str());
        ImGui::PopStyleColor();

        // Botón Limpiar Chat
        ImGui::SetCursorScreenPos(ImVec2(topP.x + availW - 125.0f, topP.y + 7.0f));
        if (DS::GlassButton("Limpiar chat", ImVec2(110.0f, 26.0f))) {
            m_Messages.clear();
            m_StatusFeedback.clear();
            m_AttachedSongName.clear();
            m_AttachedSongContent.clear();
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
        float inputAreaH = (!m_AttachedSongName.empty()) ? 104.0f : 68.0f;
        float chatH = ImGui::GetContentRegionAvail().y - inputAreaH - 10.0f;

        ImGui::BeginChild("##chat_scroll", ImVec2(-1.0f, chatH), true);

        if (m_Messages.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 30.0f));
            ImGui::SetCursorPosX(availW * 0.15f);
            ImGui::PushTextWrapPos(availW * 0.85f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextWrapped("Hola! Soy tu asistente de ProyecThor for Professionals potenciado por Google Gemini. "
                               "Puedes pedirme que componga canciones completas, busque letras, organice estrofas "
                               "o adjuntes tus canciones en formato .txt con el botón '+' para analizarlas o mejorarlas.");
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        } else {
            for (size_t i = 0; i < m_Messages.size(); ++i) {
                const auto& msg = m_Messages[i];
                bool isUser = (msg.role == "user");

                ImGui::Dummy(ImVec2(0.0f, 8.0f));

                if (isUser) {
                    // Burbuja de Usuario
                    float bubbleW = std::min(availW * 0.80f, ImGui::CalcTextSize(msg.content.c_str()).x + 36.0f);
                    ImGui::SetCursorPosX(availW - bubbleW - 16.0f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(28, 44, 68, 230));
                    ImGui::BeginChild(("##usr_b_" + std::to_string(i)).c_str(), ImVec2(bubbleW, 0.0f),
                                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.94f, 1.0f, 1.0f));
                    ImGui::TextWrapped("%s", msg.content.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                } else {
                    // Burbuja de Asistente Gemini
                    float bubbleW = availW * 0.90f;
                    ImGui::SetCursorPosX(10.0f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 22, 32, 240));
                    ImGui::BeginChild(("##ast_b_" + std::to_string(i)).c_str(), ImVec2(bubbleW, 0.0f),
                                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

                    // Etiqueta del modelo
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
                    ImGui::TextUnformatted("Google Gemini AI");
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));

                    // Contenido de la respuesta
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.94f, 0.98f, 1.0f));
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

        // ── Píldora de Canción Adjunta (si hay una seleccionada) ──────────────
        if (!m_AttachedSongName.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImVec2 chipP = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(chipP, ImVec2(chipP.x + availW, chipP.y + 28.0f), IM_COL32(24, 40, 62, 220), 4.0f);
            dl->AddRect(chipP, ImVec2(chipP.x + availW, chipP.y + 28.0f), IM_COL32(56, 189, 248, 140), 4.0f);

            ImGui::SetCursorScreenPos(ImVec2(chipP.x + 10.0f, chipP.y + 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 1.0f, 1.0f));
            ImGui::Text("Canción adjunta (.txt):");
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 6.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", m_AttachedSongName.c_str());

            ImGui::SameLine(0, 14.0f);
            if (ImGui::SmallButton("X Descartar")) {
                m_AttachedSongName.clear();
                m_AttachedSongContent.clear();
            }

            ImGui::SetCursorScreenPos(ImVec2(chipP.x, chipP.y + 32.0f));
        } else {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // ── Área de Entrada con Botón '+' para Adjuntar Canción ───────────────
        const bool isBusy = proMgr.IsChatBusy();
        const bool canSend = proMgr.CanSendMessage();

        if (!canSend) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("Has alcanzado el límite mensual de tokens de tu suscripción. Tu cuota se renovará al finalizar el mes o al ampliar tu suscripción a una de mayor capacidad (próximamente).");
            ImGui::PopStyleColor();
        } else {
            if (isBusy) ImGui::BeginDisabled();

            // Botón '+' para adjuntar canciones en formato TXT
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.35f, 0.60f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.45f, 0.75f, 1.0f));
            if (ImGui::Button("+##attach_song_btn", ImVec2(34.0f, 32.0f))) {
                ImGui::OpenPopup("##attach_song_menu");
            }
            ImGui::PopStyleColor(2);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Adjuntar canción en formato .txt desde tu PC o biblioteca");
            }

            if (ImGui::BeginPopup("##attach_song_menu")) {
                if (ImGui::MenuItem("Elegir archivo .txt de la PC...")) {
                    std::string path = ProyecThor::UI::PickFile("Seleccionar archivo de canción (.txt)", { { "Canciones y Texto (*.txt)", { "*.txt" } } });
                    if (!path.empty()) {
                        std::ifstream f(path);
                        if (f.is_open()) {
                            std::stringstream ss;
                            ss << f.rdbuf();
                            m_AttachedSongContent = ss.str();
                            m_AttachedSongName = std::filesystem::path(path).filename().string();
                        }
                    }
                }
                if (ImGui::MenuItem("Elegir de la Biblioteca de Canciones...")) {
                    m_ShowLocalLibraryModal = true;
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine(0, 8.0f);

            // Campo de entrada de texto
            ImGui::SetNextItemWidth(availW - 165.0f);
            bool enterPressed = ImGui::InputTextWithHint("##chat_user_input",
                                                         "Escribe tu mensaje, pregunta o instrucción para la canción... (Enter para enviar)",
                                                         m_InputBuf, sizeof(m_InputBuf),
                                                         ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine(0, 8.0f);

            bool sendClicked = ImGui::Button(isBusy ? "Pensando..." : "Enviar", ImVec2(105.0f, 32.0f));

            if ((enterPressed || sendClicked) && !isBusy) {
                std::string userPrompt = m_InputBuf;
                std::string fullMsg;

                if (!m_AttachedSongContent.empty()) {
                    if (userPrompt.empty()) {
                        fullMsg = "Por favor analiza, revisa la estructura (estrofas y coro) y sugiere mejoras para esta canción:\n\n[Canción adjunta: " + m_AttachedSongName + "]\n\n" + m_AttachedSongContent;
                    } else {
                        fullMsg = userPrompt + "\n\n--- Letra de la canción adjunta [" + m_AttachedSongName + "] ---\n" + m_AttachedSongContent;
                    }
                } else if (!userPrompt.empty()) {
                    fullMsg = userPrompt;
                }

                if (!fullMsg.empty()) {
                    m_Messages.push_back({ "user", fullMsg });
                    m_InputBuf[0] = '\0';
                    m_AttachedSongName.clear();
                    m_AttachedSongContent.clear();
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
            }

            if (isBusy) {
                ImGui::EndDisabled();
                ImGui::SameLine(0, 8.0f);
                DrawLoadingSpinner(dl, ImVec2(ImGui::GetCursorScreenPos().x + 8.0f, ImGui::GetCursorScreenPos().y + 15.0f), 8.0f);
            }
        }

        // ── Modal: Selector de la Biblioteca de Canciones ─────────────────────
        if (m_ShowLocalLibraryModal) {
            ImGui::OpenPopup("Seleccionar Canción de la Biblioteca##modalSongPickChat");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500.0f, 420.0f));

        if (ImGui::BeginPopupModal("Seleccionar Canción de la Biblioteca##modalSongPickChat", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("Selecciona una canción para adjuntarla al chat:");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            ImGui::BeginChild("##chat_local_songs_scroll", ImVec2(-1.0f, 310.0f), true);

            std::string songsDir = ProyecThor::SongsPath();
            if (std::filesystem::exists(songsDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(songsDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                        std::string stem = entry.path().stem().string();
                        if (ImGui::Selectable(stem.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 26.0f))) {
                            std::ifstream f(entry.path());
                            if (f.is_open()) {
                                std::stringstream ss;
                                ss << f.rdbuf();
                                m_AttachedSongContent = ss.str();
                                m_AttachedSongName = stem + ".txt";
                            }
                            m_ShowLocalLibraryModal = false;
                            ImGui::CloseCurrentPopup();
                            break;
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No se encontró la carpeta de canciones.");
            }

            ImGui::EndChild();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (ImGui::Button("Cerrar", ImVec2(100.0f, 28.0f))) {
                m_ShowLocalLibraryModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI
