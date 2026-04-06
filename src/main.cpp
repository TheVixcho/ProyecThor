#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <thread>      
#include <chrono>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "PresentationCore.h"
#include "ui/UIManager.h"
#include "ui/panels/LibraryPanel.h"
#include "ui/panels/PreviewPanel.h"
#include "ui/panels/ControlPanel.h"
#include "ui/panels/LayersPanel.h"

void RenderSplashScreen(GLFWwindow* splashWindow, const std::string& status, float progress) {
    glfwMakeContextCurrent(splashWindow);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({600, 350});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("Splash", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    draw_list->AddRectFilledMultiColor(ImVec2(0,0), ImVec2(600, 350), 
        IM_COL32(25, 25, 28, 255), IM_COL32(40, 42, 48, 255), 
        IM_COL32(15, 15, 18, 255), IM_COL32(18, 18, 22, 255));

    draw_list->AddRectFilled(ImVec2(40, 40), ImVec2(100, 100), IM_COL32(49, 168, 255, 255), 14.0f);
    
    ImGui::SetCursorPos({ 52, 50 });
    ImGui::SetWindowFontScale(3.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.15f, 0.25f, 1.0f));
    ImGui::Text("Pr");
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos({ 125, 45 });
    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "ProyecThor");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos({ 128, 75 });
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Professional Engine");

    ImGui::SetCursorPos({ 40, 280 });
    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Version 0.0.1");

    ImGui::SetCursorPos({ 40, 305 });
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), status.c_str());

    draw_list->AddRectFilled(ImVec2(0, 348), ImVec2(600 * progress, 350), IM_COL32(49, 168, 255, 255));

    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(splashWindow);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Error al inicializar GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); 
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);   
    
    GLFWwindow* splashWindow = glfwCreateWindow(600, 350, "Cargando ProyecThor...", NULL, NULL);
    if (!splashWindow) {
        glfwTerminate();
        return -1;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    if (mode) {
        glfwSetWindowPos(splashWindow, (mode->width - 600) / 2, (mode->height - 350) / 2);
    }

    glfwMakeContextCurrent(splashWindow);
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Error al inicializar GLEW para el Splash" << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    const char* steps[] = { 
        "Reading preferences...", 
        "Loading UI modules...", 
        "Initializing graphics engine...", 
        "Starting ProyecThor..." 
    };

    for (int i = 0; i < 4; i++) {
        RenderSplashScreen(splashWindow, steps[i], (i + 1) * 0.25f);
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(splashWindow);
    glfwDefaultWindowHints(); 
    
    GLFWwindow* mainWindow = glfwCreateWindow(1280, 720, "ProyecThor Engine", NULL, NULL);
    if (!mainWindow) {
        std::cerr << "[ERROR] Fallo al crear la ventana principal de GLFW." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(mainWindow);
    glfwSwapInterval(1); 
    
    glewExperimental = GL_TRUE; 
    if (glewInit() != GLEW_OK) {
        std::cerr << "[ERROR] Fallo al re-inicializar GLEW para la ventana principal." << std::endl;
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
  

    ProyecThor::UI::UIManager uiManager;
    if (!uiManager.Initialize(mainWindow)) {
        std::cerr << "[ERROR] UIManager::Initialize falló y cerró la app." << std::endl;
        return -1;
    }

    std::cout << "[INFO] Motor iniciado correctamente. Entrando al bucle de render..." << std::endl;

    uiManager.AddPanel(std::make_shared<ProyecThor::UI::LibraryPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::PreviewPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::LayersPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::ControlPanel>(&uiManager));

    while (!glfwWindowShouldClose(mainWindow)) {
        glfwPollEvents();

        ProyecThor::Core::PresentationCore::Get().Update();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int dw, dh;
        glfwGetFramebufferSize(mainWindow, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        uiManager.RenderAll(); 
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(mainWindow);
    }

    uiManager.Shutdown(); 
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}
//mingw32-make