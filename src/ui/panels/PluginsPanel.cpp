#include "PluginsPanel.h"
#include "core/plugins/PluginManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/AppIcons.h"
#include "ui/framework/FilePicker.h"
#include "ui/framework/UIManager.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <chrono>

namespace DS = ProyecThor::UI::DS;
namespace fs = std::filesystem;

namespace ProyecThor::UI {

namespace {

std::string ToLower(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

PluginsPanel::PluginsPanel() {
    // Inicializar PluginManager si no lo estaba
    Core::PluginManager::Get().Initialize();
    auto plugins = Core::PluginManager::Get().GetPlugins();
    if (!plugins.empty()) {
        m_SelectedPluginId = plugins.front()->id;
    }
}

PluginsPanel::~PluginsPanel() = default;

void PluginsPanel::RefreshPlugins() {
    Core::PluginManager::Get().ReloadPlugins();
    auto plugins = Core::PluginManager::Get().GetPlugins();
    if (!plugins.empty()) {
        bool found = false;
        for (const auto& p : plugins) {
            if (p->id == m_SelectedPluginId) { found = true; break; }
        }
        if (!found) m_SelectedPluginId = plugins.front()->id;
    } else {
        m_SelectedPluginId.clear();
    }
}

void PluginsPanel::Render() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos  = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // Fondo acorde a ProyecThor
    dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                      DS::GlassFillTop);

    // ── Header Bar ───────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 6.0f));
    ImGui::Spacing();

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::AccentColor));
    ImGui::TextUnformatted("PLUGINS & EXTENSIONES");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextDisabled("| Gestor modular de scripts, herramientas y utilidades externas");

    // Botones de acción en la cabecera
    float headerBtnW = 140.0f;
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(ImGui::GetWindowWidth() - (headerBtnW * 3.0f + 36.0f));

    if (ImGui::Button("Abrir Carpeta", ImVec2(headerBtnW, 26.0f))) {
        Core::PluginManager::Get().OpenPluginsFolder();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Abre la carpeta plugins/ en el explorador de archivos para arrastrar o editar plugins.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Instalar Plugin...", ImVec2(headerBtnW, 26.0f))) {
        std::vector<FileFilterItem> filters = {
            { "Scripts y Plugins", { "*.py", "*.sh", "*.bat", "*.exe", "*.*" } }
        };
        std::string picked = PickFile("Seleccionar archivo de plugin", filters);
        if (!picked.empty()) {
            std::string msg;
            if (Core::PluginManager::Get().InstallPluginFromPath(picked, &msg)) {
                m_FeedbackMessage = msg;
                m_FeedbackTimer   = 4.0f;
                RefreshPlugins();
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Elige un archivo de script o ejecutable para instalarlo en la carpeta plugins/.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Recargar", ImVec2(headerBtnW, 26.0f))) {
        RefreshPlugins();
        m_FeedbackMessage = "Lista de plugins recargada.";
        m_FeedbackTimer   = 2.5f;
    }

    // Mensaje de feedback temporal
    if (m_FeedbackTimer > 0.0f) {
        m_FeedbackTimer -= ImGui::GetIO().DeltaTime;
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.85f, 0.4f, 1.0f));
        ImGui::Text("  [OK] %s", m_FeedbackMessage.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::PopStyleVar();

    // ── Layout de dos columnas (Sidebar izquierda + Detalles derecha) ─────────
    float totalH    = ImGui::GetContentRegionAvail().y - 8.0f;
    float totalW    = ImGui::GetContentRegionAvail().x;
    float sidebarW  = std::clamp(totalW * 0.28f, 240.0f, 320.0f);
    float contentW  = totalW - sidebarW - 12.0f;

    RenderSidebar(sidebarW, totalH);
    ImGui::SameLine();
    RenderPluginDetails(contentW, totalH);
}

void PluginsPanel::RenderSidebar(float sidebarW, float contentH) {
    ImGui::BeginChild("##pluginsSidebar", ImVec2(sidebarW, contentH), true);

    // Barra de búsqueda
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##pluginSearch", "Buscar plugin...", m_SearchFilter, sizeof(m_SearchFilter));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto plugins = Core::PluginManager::Get().GetPlugins();
    std::string q = ToLower(m_SearchFilter);

    if (plugins.empty()) {
        ImGui::TextWrapped("No hay plugins instalados.\n\nArrastra archivos o carpetas a plugins/ para comenzar.");
    } else {
        for (const auto& pl : plugins) {
            if (!q.empty()) {
                std::string n = ToLower(pl->name);
                std::string d = ToLower(pl->description);
                if (n.find(q) == std::string::npos && d.find(q) == std::string::npos) {
                    continue;
                }
            }

            bool isSelected = (pl->id == m_SelectedPluginId);
            ImGui::PushID(pl->id.c_str());

            // Fondo de selección
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.45f, 0.85f, 0.25f));
            }

            std::string label = pl->name + "##item";
            if (ImGui::Selectable(label.c_str(), isSelected, 0, ImVec2(0.0f, 38.0f))) {
                m_SelectedPluginId = pl->id;
            }

            if (isSelected) {
                ImGui::PopStyleColor();
            }

            // Indicador de estado (punto verde/gris)
            ImVec2 pMin = ImGui::GetItemRectMin();
            ImVec2 pMax = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            ImU32 dotCol = pl->isEnabled ? IM_COL32(34, 197, 94, 255) : IM_COL32(156, 163, 175, 255);
            dl->AddCircleFilled(ImVec2(pMin.x + 8.0f, (pMin.y + pMax.y) * 0.5f), 4.0f, dotCol);

            // Versión / insignia
            std::string verBadge = "v" + pl->version;
            ImVec2 verSz = ImGui::CalcTextSize(verBadge.c_str());
            dl->AddText(ImVec2(pMax.x - verSz.x - 8.0f, pMin.y + 6.0f),
                        IM_COL32(160, 165, 175, 200), verBadge.c_str());

            if (pl->isRunning) {
                dl->AddText(ImVec2(pMax.x - 70.0f, pMin.y + 20.0f),
                            IM_COL32(234, 179, 8, 255), "Ejecutando...");
            }

            ImGui::PopID();
        }
    }

    // Tarjeta informativa de Drag & Drop al pie del sidebar
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
    ImGui::TextWrapped("Tip: Puedes arrastrar archivos (.py, .sh, ejecutables) directamente a la carpeta plugins/ de ProyecThor.");
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void PluginsPanel::RenderPluginDetails(float contentW, float contentH) {
    ImGui::BeginChild("##pluginDetails", ImVec2(contentW, contentH), true);

    auto plugin = Core::PluginManager::Get().GetPlugin(m_SelectedPluginId);
    if (!plugin) {
        ImGui::TextDisabled("Selecciona un plugin del panel izquierdo para ver sus opciones y ejecutarlo.");
        ImGui::EndChild();
        return;
    }

    // ── Tarjeta de información principal ──────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.16f, 0.6f));
    ImGui::BeginChild("##pluginHeaderCard", ImVec2(0.0f, 95.0f), true);

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("%s", plugin->name.c_str());
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "v%s", plugin->version.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("por %s", plugin->author.c_str());

    // Botones a la derecha de la tarjeta
    ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
    bool enabled = plugin->isEnabled;
    if (ImGui::Checkbox("Habilitado", &enabled)) {
        Core::PluginManager::Get().SetPluginEnabled(plugin->id, enabled);
    }

    ImGui::SameLine();
    if (ImGui::Button("Carpeta")) {
        Core::PluginManager::Get().OpenPluginFolder(plugin->id);
    }

    if (!plugin->isBuiltin) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
        if (ImGui::Button("Eliminar")) {
            ImGui::OpenPopup("##confirmDeletePopup");
        }
        ImGui::PopStyleColor();

        if (ImGui::BeginPopupModal("##confirmDeletePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("¿Deseas eliminar permanentemente el plugin '%s'?", plugin->name.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Sí, eliminar", ImVec2(120.0f, 0.0f))) {
                Core::PluginManager::Get().DeletePlugin(plugin->id);
                RefreshPlugins();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", plugin->description.c_str());
    ImGui::TextDisabled("Ruta: %s", plugin->path.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Interfaz Específica según Plugin ──────────────────────────────────────
    float availableH = ImGui::GetContentRegionAvail().y;
    float upperH     = std::max(220.0f, availableH * 0.50f);
    float consoleH   = availableH - upperH - 12.0f;

    ImGui::BeginChild("##pluginControlsArea", ImVec2(0.0f, upperH), false);

    if (plugin->id == "ytdlp") {
        RenderYtDlpSpecificUI();
    } else {
        RenderGenericPluginUI();
    }

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Consola de Registro / Terminal en Vivo ────────────────────────────────
    RenderTerminalConsole(0.0f, consoleH);

    ImGui::EndChild();
}

void PluginsPanel::RenderYtDlpSpecificUI() {
    auto plugin = Core::PluginManager::Get().GetPlugin("ytdlp");
    if (!plugin) return;

    // Sección 1: Estado y Actualización de yt-dlp
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "ESTADO Y MANTENIMIENTO DEL MOTOR YT-DLP");
    ImGui::Spacing();

    if (ImGui::Button("Comprobar Instalación", ImVec2(180.0f, 28.0f))) {
        Core::PluginManager::Get().ExecuteActionAsync("ytdlp", "status", {}, [this](const Core::PluginExecutionResult& res) {
            if (res.success && res.jsonData.is_object()) {
                m_YtDlpInstalled = res.jsonData.value("installed", false);
                std::string ver  = res.jsonData.value("version", "Desconocida");
                std::string p    = res.jsonData.value("path", "");
                m_YtDlpStatusText = "yt-dlp detectado: " + p + " (" + ver + ")";
            } else {
                m_YtDlpInstalled  = false;
                m_YtDlpStatusText = !res.error.empty() ? res.error : "yt-dlp no encontrado.";
            }
            m_YtDlpStatusChecked = true;
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Actualizar yt-dlp (-U)", ImVec2(180.0f, 28.0f))) {
        Core::PluginManager::Get().ExecuteActionAsync("ytdlp", "update", {}, [this](const Core::PluginExecutionResult& res) {
            m_YtDlpStatusText = res.success ? "Actualización completada." : "Fallo al actualizar: " + res.error;
            m_YtDlpStatusChecked = true;
        });
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Ejecuta la actualización oficial de yt-dlp sin requerir recompilar ProyecThor.");
    }

    if (m_YtDlpStatusChecked) {
        ImGui::SameLine();
        ImVec4 col = m_YtDlpInstalled ? ImVec4(0.3f, 0.85f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f);
        ImGui::TextColored(col, "%s", m_YtDlpStatusText.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Sección 2: Resolver URL de Video
    ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "RESOLVER ENLACE DE STREAMING (VLC)");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160.0f);
    ImGui::InputTextWithHint("##ytdlpStreamInput", "Pega URL de YouTube o video...", m_YtDlpStreamUrlInput, sizeof(m_YtDlpStreamUrlInput));
    ImGui::SameLine();

    if (ImGui::Button("Resolver Stream", ImVec2(150.0f, 0.0f))) {
        if (m_YtDlpStreamUrlInput[0] != '\0') {
            std::string url = m_YtDlpStreamUrlInput;
            Core::PluginManager::Get().ExecuteActionAsync("ytdlp", "resolve", {{"url", url}}, [this](const Core::PluginExecutionResult& res) {
                if (res.success && res.jsonData.is_object() && res.jsonData.contains("url")) {
                    m_ResolvedStreamUrl = res.jsonData["url"].get<std::string>();
                } else {
                    m_ResolvedStreamUrl = "Error: " + res.error;
                }
            });
        }
    }

    if (!m_ResolvedStreamUrl.empty()) {
        ImGui::Spacing();
        ImGui::Text("Resultado:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", m_ResolvedStreamUrl.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::SmallButton("Copiar URL")) {
            ImGui::SetClipboardText(m_ResolvedStreamUrl.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Sección 3: Descargar Subtítulos / Letra
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "EXTRAER SUBTÍTULOS COMO LETRA DE CANCIÓN");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160.0f);
    ImGui::InputTextWithHint("##ytdlpLyricsInput", "Pega URL para extraer letra...", m_YtDlpLyricsUrlInput, sizeof(m_YtDlpLyricsUrlInput));
    ImGui::SameLine();

    if (ImGui::Button("Descargar Letra", ImVec2(150.0f, 0.0f))) {
        if (m_YtDlpLyricsUrlInput[0] != '\0') {
            std::string url = m_YtDlpLyricsUrlInput;
            Core::PluginManager::Get().ExecuteActionAsync("ytdlp", "lyrics", {{"url", url}}, [this](const Core::PluginExecutionResult& res) {
                if (res.success && res.jsonData.is_object()) {
                    m_DownloadedLyricsTitle   = res.jsonData.value("title", "Canción");
                    m_DownloadedLyricsContent = res.jsonData.value("lyrics", "");
                } else {
                    m_DownloadedLyricsTitle   = "Error";
                    m_DownloadedLyricsContent = res.error;
                }
            });
        }
    }

    if (!m_DownloadedLyricsContent.empty()) {
        ImGui::Spacing();
        ImGui::Text("Título: %s", m_DownloadedLyricsTitle.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copiar Letra")) {
            ImGui::SetClipboardText(m_DownloadedLyricsContent.c_str());
        }

        ImGui::InputTextMultiline("##lyricsPreviewBox",
                                  const_cast<char*>(m_DownloadedLyricsContent.c_str()),
                                  m_DownloadedLyricsContent.size() + 1,
                                  ImVec2(-1.0f, 85.0f),
                                  ImGuiInputTextFlags_ReadOnly);
    }
}

void PluginsPanel::RenderGenericPluginUI() {
    auto plugin = Core::PluginManager::Get().GetPlugin(m_SelectedPluginId);
    if (!plugin) return;

    ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "ACCIONES DISPONIBLES");
    ImGui::Spacing();

    if (!plugin->actions.empty()) {
        for (const auto& act : plugin->actions) {
            ImGui::PushID(act.id.c_str());
            std::string btnLabel = "Ejecutar: " + act.name;
            if (ImGui::Button(btnLabel.c_str(), ImVec2(200.0f, 28.0f))) {
                Core::PluginManager::Get().ExecuteActionAsync(plugin->id, act.id, {});
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", act.description.c_str());
            ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Ejecución con argumentos personalizados
    ImGui::Text("Argumentos personalizados:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
    ImGui::InputTextWithHint("##genericCustomArgs", "--opcion valor ...", m_GenericArgsInput, sizeof(m_GenericArgsInput));
    ImGui::SameLine();
    if (ImGui::Button("Ejecutar", ImVec2(130.0f, 0.0f))) {
        Core::PluginManager::Get().ExecuteCustomCommandAsync(plugin->id, m_GenericArgsInput);
    }
}

void PluginsPanel::RenderTerminalConsole(float w, float h) {
    auto plugin = Core::PluginManager::Get().GetPlugin(m_SelectedPluginId);
    if (!plugin) return;

    ImGui::TextUnformatted("REGISTRO DE EJECUCIÓN (TERMINAL)");
    ImGui::SameLine(ImGui::GetWindowWidth() - 280.0f);

    ImGui::Checkbox("Auto-scroll", &m_AutoScrollConsole);
    ImGui::SameLine();
    if (ImGui::SmallButton("Limpiar")) {
        Core::PluginManager::Get().ClearLogs(plugin->id);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Copiar")) {
        std::string allLogs;
        for (const auto& l : plugin->consoleLogs) {
            allLogs += l + "\n";
        }
        ImGui::SetClipboardText(allLogs.c_str());
    }

    // Ventana oscura tipo consola
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 15, 18, 255));
    ImGui::BeginChild("##consoleBox", ImVec2(w, h), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& logLine : plugin->consoleLogs) {
        if (logLine.rfind(">>", 0) == 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.4f, 1.0f), "%s", logLine.c_str());
        } else if (logLine.rfind("ERROR", 0) == 0 || logLine.rfind("Error", 0) == 0) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", logLine.c_str());
        } else {
            ImGui::TextUnformatted(logLine.c_str());
        }
    }

    if (m_AutoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI
