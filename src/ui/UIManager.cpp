#include <GL/glew.h> 
#include "UIManager.h"
#include "../../PresentationCore.h"
#include "../toolbar/ConfigPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <imgui_internal.h> 
#include "../external/tools/OpenURL.h"

namespace ProyecThor::UI {

    static bool g_ShowAbout = false;

    UIManager::UIManager() : m_Window(nullptr), m_ShowConfig(false) {}
    UIManager::~UIManager() { Shutdown(); }

    bool UIManager::Initialize(GLFWwindow* window) {
        m_Window = window;
        if (!m_Window) return false;
        ImGuiIO& io = ImGui::GetIO();
        
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // Inicializamos el tema dinámico
        m_SettingsPanel.InitializeTheme();
        
        return true;
    }

    void UIManager::AddPanel(std::shared_ptr<IPanel> panel) {
        if (panel) m_Panels.push_back(std::move(panel));
    }

    void UIManager::RenderAll() {
        BeginDockspace();

        for (auto& panel : m_Panels)
            panel->Render();

        // ── Proyector Live ────────────────────────────────────────────────────
        auto state = Core::PresentationCore::Get().GetState();
        if (state.isProjecting) {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            
            if (monitors && monitorCount > 0 &&
                state.targetMonitorIndex >= 0 && state.targetMonitorIndex < monitorCount) {

                const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
                
                if (mode && mode->width > 0 && mode->height > 0) {
                    int mx, my;
                    glfwGetMonitorPos(monitors[state.targetMonitorIndex], &mx, &my);

                    ImGui::SetNextWindowPos(ImVec2(mx, my));
                    ImGui::SetNextWindowSize(ImVec2(mode->width, mode->height));
                    
                    ImGuiWindowFlags flags =
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

                    ImGui::Begin("ProjectorLive", nullptr, flags);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    
                    if (state.bgType == Core::PresentationState::BackgroundType::Video) {
                        void* texID = Core::PresentationCore::Get().GetBackgroundTexture();
                        if (texID) drawList->AddImage(texID, ImVec2(mx, my), ImVec2(mx + mode->width, my + mode->height));
                        else drawList->AddRectFilled(ImVec2(mx, my), ImVec2(mx + mode->width, my + mode->height), IM_COL32(0, 0, 0, 255));
                    } else {
                        drawList->AddRectFilled(ImVec2(mx, my), ImVec2(mx + mode->width, my + mode->height),
                            IM_COL32(state.bgColor[0]*255, state.bgColor[1]*255, state.bgColor[2]*255, 255));
                    }

                    if (state.showText && !state.currentText.empty()) {
                        float screenScale = mode->width / 1920.0f;
                        float marginL = state.margins[0] * screenScale;
                        float marginT = state.margins[1] * screenScale;
                        float marginR = state.margins[2] * screenScale;
                        float marginB = state.margins[3] * screenScale;

                        float boxX = mx + marginL;
                        float boxY = my + marginT;
                        float boxW = std::max(10.0f, mode->width  - marginL - marginR);
                        float boxH = std::max(10.0f, mode->height - marginT - marginB);

                        float targetFontSize = state.textSize * screenScale;
                        
                        if (state.autoScale) {
                            while (targetFontSize > 10.0f) {
                                ImVec2 tSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, state.currentText.c_str());
                                if (tSize.y <= boxH) break;
                                targetFontSize -= 1.0f;
                            }
                        }

                        ImVec2 finalBlockSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, state.currentText.c_str());
                        float textX = boxX;
                        if (state.textAlignment == 1) textX += (boxW - finalBlockSize.x) * 0.5f;
                        else if (state.textAlignment == 2) textX += (boxW - finalBlockSize.x);
                        float textY = boxY + (boxH - finalBlockSize.y) * 0.5f;
                        ImVec2 textPos = ImVec2(textX, textY);
                        
                        ImU32 col = ImGui::ColorConvertFloat4ToU32(
                            ImVec4(state.textColor[0], state.textColor[1], state.textColor[2], state.textColor[3]));
                        ImU32 shadowCol = IM_COL32(0, 0, 0, 220);

                        drawList->AddText(ImGui::GetFont(), targetFontSize, ImVec2(textPos.x + 3, textPos.y + 3), shadowCol, state.currentText.c_str(), NULL, boxW);
                        drawList->AddText(ImGui::GetFont(), targetFontSize, textPos, col, state.currentText.c_str(), NULL, boxW);
                    }

                    ImGui::End();
                }
            }
        }

        // ── Ventanas flotantes ────────────────────────────────────────────────
        if (m_ShowConfig) {
            m_SettingsPanel.Render(&m_ShowConfig); // Llama al nuevo panel
        }

        if (g_ShowAbout)
            ImGui::OpenPopup("Acerca de ProyecThor");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // About modal — styled with gold accent header
        ImGui::PushStyleColor(ImGuiCol_PopupBg,      ImVec4(0.060f, 0.065f, 0.088f, 0.99f));
        ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0.250f, 0.210f, 0.090f, 0.80f)); // gold border
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(24.0f, 20.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  8.0f);

        if (ImGui::BeginPopupModal("Acerca de ProyecThor", &g_ShowAbout,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {

            // App name in gold
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
            ImGui::SetWindowFontScale(1.20f);
            ImGui::Text("ProyecThor");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.420f, 0.420f, 0.420f, 1.0f));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
            ImGui::Text("v0.1.0");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.250f, 0.210f, 0.090f, 0.60f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.780f, 0.770f, 0.740f, 1.0f));
            ImGui::Text("Software profesional para gestion de proyecciones.");
            ImGui::Spacing();
            ImGui::TextWrapped("Sin fines de lucro. Funcionamos mediante donaciones\ndel equipo de desarrollo y la comunidad.");
            ImGui::Spacing();
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.500f, 0.500f, 0.490f, 1.0f));
            ImGui::Text("Creado por TheVixcho");
            ImGui::Spacing();
            ImGui::TextDisabled("© 2026  Todos los derechos reservados.");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.180f, 0.185f, 0.230f, 1.0f));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Close button — gold style
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.240f, 0.200f, 0.085f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.500f, 0.415f, 0.180f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.650f, 0.530f, 0.220f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.920f, 0.820f, 0.560f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16.0f, 6.0f));
            if (ImGui::Button("Cerrar")) {
                ImGui::CloseCurrentPopup();
                g_ShowAbout = false;
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(4);
            ImGui::SetItemDefaultFocus();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        EndDockspace();
    }

    void UIManager::BeginDockspace() {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |=
            ImGuiWindowFlags_NoTitleBar      | ImGuiWindowFlags_NoCollapse    |
            ImGuiWindowFlags_NoResize        | ImGuiWindowFlags_NoMove        |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("ProyecThorWorkspace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        static bool s_ResetLayout = true;

        // ── Menu Bar ─────────────────────────────────────────────────────────
        // Slightly taller menu bar with gold separator line at bottom
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 7.0f));

        if (ImGui::BeginMenuBar()) {
            // App logo / title in gold (acts as a brand mark)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.788f, 0.659f, 0.298f, 1.0f));
            ImGui::Text("ProyecThor");
            ImGui::PopStyleColor();

            // Thin vertical separator
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.240f, 0.250f, 0.320f, 1.0f));
            ImGui::SameLine(0, 14);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 14);
            ImGui::PopStyleColor();

            // ── Archivo ──
            if (ImGui::BeginMenu("Archivo")) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.58f, 1.0f));
                if (ImGui::MenuItem("Nuevo Proyecto",   "Ctrl+N")) {}
                if (ImGui::MenuItem("Abrir Proyecto...", "Ctrl+O")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Guardar",          "Ctrl+S")) {}
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.45f, 0.45f, 1.0f));
                if (ImGui::MenuItem("Salir", "Alt+F4")) { glfwSetWindowShouldClose(m_Window, true); }
                ImGui::PopStyleColor();
                ImGui::EndMenu();
            }

            // ── Editar ──
            if (ImGui::BeginMenu("Editar")) {
                if (ImGui::MenuItem("Preferencias...", "Ctrl+P")) { m_ShowConfig = true; }
                ImGui::EndMenu();
            }

            // ── Vista ──
            if (ImGui::BeginMenu("Vista")) {
                if (ImGui::MenuItem("Restablecer Entorno")) { s_ResetLayout = true; }
                ImGui::EndMenu();
            }

            // ── Ayuda ──
            if (ImGui::BeginMenu("Ayuda")) {
                if (ImGui::MenuItem("Documentación", "F1")) {
                    ProyecThor::External::OpenURL("https://tu-sitio-web.com/docs");
                }
                // Gold donation item
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));
                if (ImGui::MenuItem("Donaciones")) {
                    ProyecThor::External::OpenURL("https://ko-fi.com/vixcho");
                }
                ImGui::PopStyleColor();
                ImGui::Separator();
                if (ImGui::MenuItem("Acerca de ProyecThor")) { g_ShowAbout = true; }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        ImGui::PopStyleVar(); // FramePadding

        // ── Dockspace ─────────────────────────────────────────────────────────
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (s_ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id)) {
            s_ResetLayout = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main  = dockspace_id;
            ImGuiID dock_left  = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left,  0.22f, nullptr, &dock_main);
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.35f, nullptr, &dock_main);

            ImGuiID dock_right_top, dock_right_bottom;
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.60f, &dock_right_top, &dock_right_bottom);

            ImGui::DockBuilderDockWindow("Biblioteca",                   dock_left);
            ImGui::DockBuilderDockWindow("Preview",                      dock_main);
            ImGui::DockBuilderDockWindow("Inspector de Capas y Fondos",  dock_right_top);
            ImGui::DockBuilderDockWindow("Control",                      dock_right_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    void UIManager::EndDockspace() {
        ImGui::End();
    }

    void UIManager::Shutdown() {
        m_Panels.clear();
    }

} // namespace ProyecThor::UI