#include "SettingsPanel.h"
#include "core/settings/SettingsManager.h"
#include "core/PresentationCore.h"
#include "ui/framework/DesignSystem.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Categoría "Pantallas"
//  1. Ventana de Proyección (Salida Pública): selección de pantalla/monitor,
//     resolución personalizada o nativa, y pantalla completa vs modo ventana
//     (especialmente crítico para Linux Wayland / X11).
//  2. Monitor de Confianza (Stage Display): configuración de salida al escenario.
// ─────────────────────────────────────────────────────────────────────────────

namespace ProyecThor::UI::Settings {

void SettingsPanel::RenderCategoryStage() {
    auto& mgr = ProyecThor::Settings::SettingsManager::Get();
    auto& p = mgr.GetSettings().projection;
    auto& core = Core::PresentationCore::Get();
    bool changed = false;

    // ── 1. VENTANA DE PROYECCIÓN (SALIDA PÚBLICA) ──────────────────────────
    if (SectionTitle("Ventana de Proyección (Público)")) {
        ImGui::TextDisabled("Ajustes de la ventana que ve el público en pantallas o proyectores externos.");
        ImGui::Spacing();

        // ── Tarjeta de Estado & Acciones Rápidas ────────────────────────────
        bool isWindowActive = core.IsProjectorWindowActive() || core.IsProjecting();
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.18f, 0.60f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

        if (ImGui::BeginChild("##ProjStatusCard", ImVec2(0, 78.0f), true)) {
            ImGui::AlignTextToFramePadding();
            if (isWindowActive) {
                ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.45f, 1.0f), "● Ventana de Proyección ACTIVA");
                ImGui::SameLine();
                int currentMon = (p.targetMonitor >= 0 && p.targetMonitor < monitorCount) ? p.targetMonitor : 0;
                ImGui::TextDisabled("| Pantalla %d (%s)", currentMon + 1, p.windowFullscreen ? "Pantalla Completa" : "Ventana");
            } else {
                ImGui::TextColored(ImVec4(0.65f, 0.68f, 0.75f, 1.0f), "○ Ventana de Proyección INACTIVA");
                ImGui::SameLine();
                ImGui::TextDisabled("| Cerrada");
            }

            ImGui::Spacing();
            if (isWindowActive) {
                if (ImGui::Button("Reabrir / Aplicar Cambios", ImVec2(190.0f, 28.0f))) {
                    #ifndef _WIN32
                    core.DestroyProjectorWindow();
                    core.CreateProjectorWindow(p.targetMonitor, p.windowFullscreen ? 1 : 0, 
                        p.windowAutoDetectRes ? 0 : p.windowWidth, 
                        p.windowAutoDetectRes ? 0 : p.windowHeight);
                    #endif
                    core.SetProjecting(true);
                }
                ImGui::SameLine();
                if (ImGui::Button("Cerrar Ventana", ImVec2(120.0f, 28.0f))) {
                    #ifndef _WIN32
                    core.DestroyProjectorWindow();
                    #endif
                    core.SetProjecting(false);
                }
            } else {
                if (ImGui::Button("Abrir Ventana de Prueba", ImVec2(190.0f, 28.0f))) {
                    #ifndef _WIN32
                    core.CreateProjectorWindow(p.targetMonitor, p.windowFullscreen ? 1 : 0,
                        p.windowAutoDetectRes ? 0 : p.windowWidth,
                        p.windowAutoDetectRes ? 0 : p.windowHeight);
                    #endif
                    core.SetProjecting(true);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // ── 1. Elección de Pantalla ─────────────────────────────────────────
        ImGui::TextUnformatted("Pantalla de Salida (Monitor)");
        if (monitors && monitorCount > 0) {
            std::vector<std::string> itemLabels;
            std::vector<const char*> names;
            for (int i = 0; i < monitorCount; i++) {
                const char* monName = glfwGetMonitorName(monitors[i]);
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
                std::string label = "Pantalla " + std::to_string(i + 1);
                if (monName && monName[0] != '\0') label += ": " + std::string(monName);
                if (mode) {
                    label += " (" + std::to_string(mode->width) + "x" + std::to_string(mode->height) +
                             " @" + std::to_string(mode->refreshRate) + "Hz)";
                }
                if (i == 0) label += " [Principal]";
                itemLabels.push_back(label);
            }
            for (const auto& l : itemLabels) names.push_back(l.c_str());

            int sel = std::clamp(p.targetMonitor < 0 ? (monitorCount > 1 ? 1 : 0) : p.targetMonitor, 0, monitorCount - 1);
            ImGui::SetNextItemWidth(450.0f);
            if (ImGui::Combo("##monSel", &sel, names.data(), (int)names.size())) {
                p.targetMonitor = sel;
                core.SetTargetMonitor(sel);
                changed = true;
            }
            HelpTooltip("Elige en qué monitor físico se abrirá la ventana de proyección.\n"
                        "Se recomienda seleccionar la pantalla secundaria conectada al proyector o TV.");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No se detectaron pantallas adicionales.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── 2. Modo de Visualización (Pantalla Completa vs Modo Ventana) ───
        ImGui::TextUnformatted("Modo de Visualización");
        ImGui::Spacing();

        bool isFullscreen = p.windowFullscreen;
        float availW = ImGui::GetContentRegionAvail().x;
        float btnW = std::min(220.0f, (availW - 10.0f) * 0.5f);

        auto DrawModeButton = [&](const char* id, const char* label, bool active) {
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.55f, 0.95f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.60f, 1.0f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.50f, 0.90f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.20f, 0.26f, 0.60f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.27f, 0.35f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.17f, 0.22f, 0.90f));
            }
            bool clicked = ImGui::Button(label, ImVec2(btnW, 36.0f));
            ImGui::PopStyleColor(3);
            return clicked;
        };

        if (DrawModeButton("##btnFS", "Pantalla Completa (Fullscreen)", isFullscreen)) {
            p.windowFullscreen = true;
            changed = true;
        }
        ImGui::SameLine();
        if (DrawModeButton("##btnWin", "Modo Ventana (Windowed)", !isFullscreen)) {
            p.windowFullscreen = false;
            changed = true;
        }

        if (p.windowFullscreen) {
            ImGui::TextDisabled("La ventana cubre la totalidad de la pantalla seleccionada sin bordes.");
        } else {
            ImGui::TextDisabled("La proyección se abre como una ventana normal con bordes. Útil en Linux para moverla,\n"
                                "redimensionarla libremente o capturarla en OBS Studio sin necesidad de proyector físico.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── 3. Resolución de la Ventana ─────────────────────────────────────
        ImGui::TextUnformatted("Resolución de la Ventana de Proyección");
        ImGui::Spacing();

        const char* resOptions[] = {
            "Nativa / Automática del monitor",
            "1920 × 1080 (Full HD 16:9)",
            "1280 × 720 (HD 16:9)",
            "2560 × 1440 (2K QHD 16:9)",
            "3840 × 2160 (4K UHD 16:9)",
            "Personalizada..."
        };
        int currentResIndex = 0;
        if (p.windowAutoDetectRes) {
            currentResIndex = 0;
        } else if (p.windowWidth == 1920 && p.windowHeight == 1080) {
            currentResIndex = 1;
        } else if (p.windowWidth == 1280 && p.windowHeight == 720) {
            currentResIndex = 2;
        } else if (p.windowWidth == 2560 && p.windowHeight == 1440) {
            currentResIndex = 3;
        } else if (p.windowWidth == 3840 && p.windowHeight == 2160) {
            currentResIndex = 4;
        } else {
            currentResIndex = 5;
        }

        ImGui::SetNextItemWidth(350.0f);
        if (ImGui::Combo("Resolución##winRes", &currentResIndex, resOptions, IM_ARRAYSIZE(resOptions))) {
            switch (currentResIndex) {
                case 0: // Nativa
                    p.windowAutoDetectRes = true;
                    break;
                case 1: // 1080p
                    p.windowAutoDetectRes = false;
                    p.windowWidth = 1920; p.windowHeight = 1080;
                    break;
                case 2: // 720p
                    p.windowAutoDetectRes = false;
                    p.windowWidth = 1280; p.windowHeight = 720;
                    break;
                case 3: // 1440p
                    p.windowAutoDetectRes = false;
                    p.windowWidth = 2560; p.windowHeight = 1440;
                    break;
                case 4: // 4K
                    p.windowAutoDetectRes = false;
                    p.windowWidth = 3840; p.windowHeight = 2160;
                    break;
                case 5: // Personalizada
                    p.windowAutoDetectRes = false;
                    if (p.windowWidth <= 0) p.windowWidth = 1920;
                    if (p.windowHeight <= 0) p.windowHeight = 1080;
                    break;
            }
            changed = true;
        }
        HelpTooltip("Define los píxeles de ancho y alto de la ventana de proyección.\n"
                    "En modo ventana, determina su tamaño inicial. En fullscreen, define la resolución deseada.");

        if (!p.windowAutoDetectRes || currentResIndex == 5) {
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Ancho (px):");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt("##winW", &p.windowWidth, 10, 100)) {
                p.windowWidth = std::clamp(p.windowWidth, 320, 7680);
                changed = true;
            }

            ImGui::SameLine(0, 20.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Alto (px):");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt("##winH", &p.windowHeight, 10, 100)) {
                p.windowHeight = std::clamp(p.windowHeight, 240, 4320);
                changed = true;
            }
        }

        // Monitores adicionales
        if (monitorCount > 1) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("Enviar también a estas pantallas en simultáneo (opcional):");
            int sel = std::clamp(p.targetMonitor < 0 ? 1 : p.targetMonitor, 0, monitorCount - 1);
            for (int i = 0; i < monitorCount; i++) {
                if (i == sel) continue;
                bool isExtra = std::find(p.extraMonitors.begin(), p.extraMonitors.end(), i) != p.extraMonitors.end();
                std::string label = "Pantalla " + std::to_string(i + 1) + "##extramonStage" + std::to_string(i);
                if (ImGui::Checkbox(label.c_str(), &isExtra)) {
                    if (isExtra) {
                        p.extraMonitors.push_back(i);
                    } else {
                        p.extraMonitors.erase(std::remove(p.extraMonitors.begin(), p.extraMonitors.end(), i), p.extraMonitors.end());
                    }
                    changed = true;
                }
            }
        }

        if (changed) {
            mgr.SaveSettings();
        }
    }

    // ── 2. MONITOR DE CONFIANZA (STAGE DISPLAY) ────────────────────────────
    if (SectionTitle("Monitor de Confianza (Stage Display)")) {
        m_StageDisplay.RenderContent();
    }
}

} // namespace ProyecThor::UI::Settings
