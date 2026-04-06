#include <GL/glew.h> 
#include "UIManager.h"
#include "../../PresentationCore.h"
#include "../toolbar/ConfigPanel.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm> // Añadido para std::max
#include <imgui_internal.h> 
#include "../external/tools/OpenURL.h"

namespace ProyecThor::UI {

    // Variable global estática para controlar la ventana "Acerca de" sin modificar el .h
    static bool g_ShowAbout = false;

    UIManager::UIManager() : m_Window(nullptr), m_ShowConfig(false) {}
    
    UIManager::~UIManager() { Shutdown(); }

    void UIManager::ApplyProfessionalTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.11f, 0.96f);
        colors[ImGuiCol_Border]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.70f, 0.20f, 1.00f); 
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.20f, 0.70f, 0.20f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.30f, 0.80f, 0.30f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        style.WindowPadding     = ImVec2(8.0f, 8.0f);
        style.FramePadding      = ImVec2(6.0f, 4.0f);
        style.CellPadding       = ImVec2(6.0f, 4.0f);
        style.ItemSpacing       = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
        style.ScrollbarSize     = 14.0f;
        style.GrabMinSize       = 10.0f;

        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 1.0f;
        style.PopupBorderSize   = 1.0f;
        style.FrameBorderSize   = 0.0f;

        style.WindowRounding    = 4.0f;
        style.ChildRounding     = 4.0f;
        style.FrameRounding     = 3.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding      = 3.0f;
        style.TabRounding       = 4.0f;
    }

    bool UIManager::Initialize(GLFWwindow* window) {
        m_Window = window;
        if (!m_Window) return false;
        ImGuiIO& io = ImGui::GetIO();
        
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ApplyProfessionalTheme(); 

        return true;
    }

    void UIManager::AddPanel(std::shared_ptr<IPanel> panel) {
        if (panel) m_Panels.push_back(std::move(panel));
    }

    void UIManager::RenderAll() {
        BeginDockspace();

        // 1. Renderizar paneles
        for (auto& panel : m_Panels) {
            panel->Render();
        }

        // 2. Proyector Live
        auto state = Core::PresentationCore::Get().GetState();
        if (state.isProjecting) {
            int monitorCount = 0;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            
            // Protección vital: asegurar que los monitores existan
            if (monitors && monitorCount > 0 && state.targetMonitorIndex >= 0 && state.targetMonitorIndex < monitorCount) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitors[state.targetMonitorIndex]);
                
                if (mode && mode->width > 0 && mode->height > 0) { // Protección de tamaño cero
                    int mx, my;
                    glfwGetMonitorPos(monitors[state.targetMonitorIndex], &mx, &my);

                    ImGui::SetNextWindowPos(ImVec2(mx, my));
                    ImGui::SetNextWindowSize(ImVec2(mode->width, mode->height));
                    
                    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | 
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
                        float boxW = std::max(10.0f, mode->width - marginL - marginR); // Evita anchos negativos
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
                        
                        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(state.textColor[0], state.textColor[1], state.textColor[2], state.textColor[3]));
                        ImU32 shadowCol = IM_COL32(0, 0, 0, 220);

                        drawList->AddText(ImGui::GetFont(), targetFontSize, ImVec2(textPos.x + 3, textPos.y + 3), shadowCol, state.currentText.c_str(), NULL, boxW);
                        drawList->AddText(ImGui::GetFont(), targetFontSize, textPos, col, state.currentText.c_str(), NULL, boxW);
                    }

                    ImGui::End();
                }
            }
        }

        // ---------------------------------------------------------
        // 3. Renderizar Ventanas Flotantes desde la Toolbar
        // ---------------------------------------------------------
        
        // Panel de Preferencias
        if (m_ShowConfig) {
            static ConfigPanel configPanel; 
            configPanel.RenderConfig(&m_ShowConfig);
        }

        // Ventana Modal "Acerca de"
        if (g_ShowAbout) {
            ImGui::OpenPopup("Acerca de ProyecThor");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Acerca de ProyecThor", &g_ShowAbout, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextColored(ImVec4(0.20f, 0.70f, 0.20f, 1.0f), "ProyecThor v0.0.1");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Software profesional para gestion de proyecciones.");
            ImGui::Text("Software sin fines de lucros. Solo funcionamos mediante donaciones al equipo de desarrollo");
            ImGui::Text("Creado por TheVixcho");
            ImGui::Spacing();
            ImGui::TextDisabled("© 2026 Todos los derechos reservados.");
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Cerrar", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                g_ShowAbout = false;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        EndDockspace();
    }

    void UIManager::BeginDockspace() {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None; 
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        
        // ¡SOLUCIÓN CRÍTICA!: Hace que el lienzo no tape tus paneles.
        window_flags |= ImGuiWindowFlags_NoBackground; 
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("ProyecThorWorkspace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

static bool s_ResetLayout = true; 

        if (ImGui::BeginMenuBar()) {
            // --- Menú Archivo ---
            if (ImGui::BeginMenu("Archivo")) {
                if (ImGui::MenuItem("Nuevo Proyecto", "Ctrl+N")) { /* Lógica */ }
                if (ImGui::MenuItem("Abrir Proyecto...", "Ctrl+O")) { /* Lógica */ }
                ImGui::Separator();
                if (ImGui::MenuItem("Guardar", "Ctrl+S")) { /* Lógica */ }
                if (ImGui::MenuItem("Salir", "Alt+F4")) { glfwSetWindowShouldClose(m_Window, true); }
                ImGui::EndMenu();
            }

            // --- Menú Editar ---
            if (ImGui::BeginMenu("Editar")) {
                if (ImGui::MenuItem("Preferencias...", "Ctrl+P")) { m_ShowConfig = true; } 
                ImGui::EndMenu();
            }

            // --- Menú Vista ---
            if (ImGui::BeginMenu("Vista")) {
                if (ImGui::MenuItem("Restablecer Entorno Original")) { s_ResetLayout = true; }
                ImGui::EndMenu();
            }

            // --- Menú Ayuda (CORREGIDO) ---
            if (ImGui::BeginMenu("Ayuda")) {
                if (ImGui::MenuItem("Documentación", "F1")) { 
                    ProyecThor::External::OpenURL("https://tu-sitio-web.com/docs"); 
                }
                
                // Color dorado para donaciones
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.84f, 0.0f, 1.0f));
                if (ImGui::MenuItem("Donaciones", "♥")) {
                    ProyecThor::External::OpenURL("https://ko-fi.com/vixcho");
                }
                ImGui::PopStyleColor();

                ImGui::Separator();
                
                if (ImGui::MenuItem("Acerca de ProyecThor")) { 
                    g_ShowAbout = true; 
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (s_ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id)) {
            s_ResetLayout = false;
            ImGui::DockBuilderRemoveNode(dockspace_id); 
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main = dockspace_id;
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.22f, nullptr, &dock_main);
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.35f, nullptr, &dock_main);
            
            ImGuiID dock_right_top, dock_right_bottom;
            ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.60f, &dock_right_top, &dock_right_bottom);

            ImGui::DockBuilderDockWindow("Biblioteca", dock_left); 
            ImGui::DockBuilderDockWindow("Preview", dock_main); 
            ImGui::DockBuilderDockWindow("Inspector de Capas y Fondos", dock_right_top); 
            ImGui::DockBuilderDockWindow("Control", dock_right_bottom); 
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    void UIManager::EndDockspace() {
        ImGui::End();
    }

    void UIManager::Shutdown() {
        m_Panels.clear();
    }
}