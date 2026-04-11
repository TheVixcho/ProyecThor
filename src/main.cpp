#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "PresentationCore.h"
#include "ui/UIManager.h"
#include "ui/panels/LibraryPanel.h"
#include "ui/panels/PreviewPanel.h"
#include "ui/panels/ControlPanel.h"
#include "ui/panels/LayersPanel.h"

static constexpr int   SPLASH_W       = 600;
static constexpr int   SPLASH_H       = 350;
static constexpr int   MAIN_W         = 1280;
static constexpr int   MAIN_H         = 720;
static constexpr float PROGRESS_STEPS = 4.0f;

// =============================================================================
//  Splash Screen
// =============================================================================
static void RenderSplashScreen(GLFWwindow* splashWindow,
                                const std::string& status,
                                float progress)
{
    glfwMakeContextCurrent(splashWindow);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)SPLASH_W, (float)SPLASH_H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoDecoration         |
        ImGuiWindowFlags_NoMove               |
        ImGuiWindowFlags_NoBackground         |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Fondo degradado
    dl->AddRectFilledMultiColor(
        ImVec2(0, 0), ImVec2((float)SPLASH_W, (float)SPLASH_H),
        IM_COL32(25, 25, 28, 255),  IM_COL32(40, 42, 48, 255),
        IM_COL32(15, 15, 18, 255),  IM_COL32(18, 18, 22, 255));

    dl->AddRect(ImVec2(1, 1), ImVec2((float)SPLASH_W - 1, (float)SPLASH_H - 1),
        IM_COL32(60, 60, 70, 255), 0.0f, 0, 1.0f);

    // Icono
    dl->AddRectFilled(ImVec2(40, 40), ImVec2(100, 100), IM_COL32(49, 168, 255, 255), 14.0f);
    dl->AddRectFilled(ImVec2(42, 42), ImVec2(98,  72),  IM_COL32(80, 190, 255,  60), 12.0f);

    ImGui::SetCursorPos(ImVec2(50, 48));
    ImGui::SetWindowFontScale(3.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.04f, 0.12f, 0.22f, 1.0f));
    ImGui::Text("Pr");
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(120, 42));
    ImGui::SetWindowFontScale(1.9f);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "ProyecThor");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(123, 76));
    ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "Professional Presentation Engine");

    dl->AddLine(ImVec2(40, 115), ImVec2(SPLASH_W - 40, 115), IM_COL32(50, 50, 60, 255), 1.0f);

    ImGui::SetCursorPos(ImVec2(40, 278));
    ImGui::TextColored(ImVec4(0.35f, 0.35f, 0.45f, 1.0f), "Version 0.1.0  |  Build 2026");

    ImGui::SetCursorPos(ImVec2(40, 302));
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.85f, 1.0f), "%s", status.c_str());

    // Barra de progreso
    dl->AddRectFilled(ImVec2(0, (float)SPLASH_H - 3),
                       ImVec2((float)SPLASH_W, (float)SPLASH_H),
                       IM_COL32(30, 30, 38, 255));
    float barEnd = (float)SPLASH_W * progress;
    dl->AddRectFilledMultiColor(
        ImVec2(0, (float)SPLASH_H - 3),
        ImVec2(barEnd, (float)SPLASH_H),
        IM_COL32(30, 140, 255, 255), IM_COL32(100, 200, 255, 255),
        IM_COL32(100, 200, 255, 255), IM_COL32(30, 140, 255, 255));
    if (barEnd > 4.0f)
        dl->AddRectFilled(
            ImVec2(barEnd - 3, (float)SPLASH_H - 3),
            ImVec2(barEnd + 1, (float)SPLASH_H),
            IM_COL32(200, 235, 255, 255));

    ImGui::End();
    ImGui::PopStyleVar(2);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(splashWindow);
}

// =============================================================================
//  MAIN
// =============================================================================
int main()
{
    // -------------------------------------------------------------------------
    //  1. GLFW
    // -------------------------------------------------------------------------
    glfwSetErrorCallback([](int code, const char* desc) {
        std::cerr << "[GLFW ERROR " << code << "] " << desc << "\n";
    });

    if (!glfwInit()) {
        std::cerr << "[FATAL] No se pudo inicializar GLFW.\n";
        return -1;
    }

    // -------------------------------------------------------------------------
    //  2. SPLASH SCREEN
    // -------------------------------------------------------------------------
    glfwWindowHint(GLFW_DECORATED,               GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING,                GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE,               GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW,           GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,   3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,   0);

    GLFWwindow* splashWindow = glfwCreateWindow(SPLASH_W, SPLASH_H, "ProyecThor", nullptr, nullptr);
    if (!splashWindow) {
        std::cerr << "[FATAL] No se pudo crear la ventana splash.\n";
        glfwTerminate();
        return -1;
    }

    {
        const GLFWvidmode* vm = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (vm)
            glfwSetWindowPos(splashWindow,
                (vm->width  - SPLASH_W) / 2,
                (vm->height - SPLASH_H) / 2);
    }

    glfwMakeContextCurrent(splashWindow);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "[FATAL] GLEW falló en splash.\n";
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(splashWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();

    struct LoadStep { const char* label; int ms; };
    static const LoadStep steps[] = {
        { "Reading preferences...",           350 },
        { "Loading UI modules...",            450 },
        { "Initializing graphics engine...",  400 },
        { "Starting ProyecThor...",           300 },
    };
    for (int i = 0; i < (int)(sizeof(steps) / sizeof(steps[0])); ++i) {
        RenderSplashScreen(splashWindow, steps[i].label,
            (float)(i + 1) / PROGRESS_STEPS);
        std::this_thread::sleep_for(std::chrono::milliseconds(steps[i].ms));
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(splashWindow);

    // -------------------------------------------------------------------------
    //  3. VENTANA PRINCIPAL
    // -------------------------------------------------------------------------
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* mainWindow = glfwCreateWindow(
        MAIN_W, MAIN_H, "ProyecThor Engine", nullptr, nullptr);
    if (!mainWindow) {
        std::cerr << "[FATAL] No se pudo crear la ventana principal.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(mainWindow);
    glfwSwapInterval(1); // VSync

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "[FATAL] GLEW falló en ventana principal.\n";
        glfwTerminate();
        return -1;
    }

    // -------------------------------------------------------------------------
    //  4. IMGUI
    // -------------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // paneles flotantes
    io.IniFilename  = "proyecthor_ui.ini";

    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    ImGui::StyleColorsDark();

    // -------------------------------------------------------------------------
    //  5. UI MANAGER + PANELES
    // -------------------------------------------------------------------------
    ProyecThor::UI::UIManager uiManager;
    if (!uiManager.Initialize(mainWindow)) {
        std::cerr << "[FATAL] UIManager::Initialize falló.\n";
        return -1;
    }

    uiManager.AddPanel(std::make_shared<ProyecThor::UI::LibraryPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::PreviewPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::LayersPanel>());
    uiManager.AddPanel(std::make_shared<ProyecThor::UI::ControlPanel>(&uiManager));

    std::cout << "[INFO] ProyecThor iniciado. Entrando al bucle de render...\n";

    // -------------------------------------------------------------------------
    //  6. BUCLE PRINCIPAL
    // -------------------------------------------------------------------------
    while (!glfwWindowShouldClose(mainWindow))
    {
        glfwPollEvents();

        // --- Actualizar motor (VLC → GPU upload) ---
        auto& core = ProyecThor::Core::PresentationCore::Get();
        core.Update();
        const auto state = core.GetState();

        // --- Nuevo frame de ImGui ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Viewport y fondo por defecto ---
        int fw, fh;
        glfwGetFramebufferSize(mainWindow, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // =====================================================================
        //  RENDER DE CAPAS DE VIDEO/PRESENTACIÓN
        //
        //  Se renderiza ANTES de la UI de ImGui para que las capas queden
        //  detrás del panel de control.
        //
        //  Orden dentro de RenderProjectorWindow():
        //    CAPA 0 → video de fondo (o color sólido)
        //    CAPA 1 → texto (letras de la presentación)
        //    CAPA 2 → video frontal (encima de las letras)
        //
        //  Si isProjecting == false, se omite el render de capas y solo
        //  se ve el fondo gris oscuro de la UI.
        // =====================================================================
        core.SetProjectorSize(fw, fh);

        if (state.isProjecting) {
            core.RenderProjectorWindow();
        }

        // =====================================================================
        //  RENDER DE LA UI (ImGui) — va encima de todo
        // =====================================================================
        uiManager.RenderAll();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Soporte para viewports flotantes (paneles arrastrados fuera de la ventana)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* ctxBackup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(ctxBackup);
        }

        glfwSwapBuffers(mainWindow);
    }

    // -------------------------------------------------------------------------
    //  7. LIMPIEZA
    // -------------------------------------------------------------------------
    std::cout << "[INFO] Cerrando ProyecThor...\n";

    uiManager.Shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}
