#include "SplashScreen.h"

#include <GL/glew.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include "Version.h"

std::string GetAppDataFilePath(const std::string& filename);

namespace ProyecThor::Splash {

namespace {

const std::vector<Art> kRegistry = {
    {"splash_bg1.jpg", "Fabiola Fernandez"},
    {"splash_bg5.jpg", "TheVixcho"},
    {"splash_bg2.jpg", "TheVixcho"},
    {"splash_bg7.jpg", "TheVixcho"},
};

float EaseOutQuad(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

ImU32 ThemeColorU32(const float c[4], float alphaOverride = -1.0f) {
    auto toByte = [](float v) -> int {
        v = std::clamp(v, 0.0f, 1.0f);
        return (int)(v * 255.0f + 0.5f);
    };
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return IM_COL32(toByte(c[0]), toByte(c[1]), toByte(c[2]), toByte(a));
}

ImVec4 ThemeColorVec4(const float c[4], float alphaOverride = -1.0f) {
    const float a = (alphaOverride >= 0.0f) ? alphaOverride : c[3];
    return ImVec4(c[0], c[1], c[2], a);
}

} // namespace

Art PickArt() {
    const std::string stateFile = GetAppDataFilePath("splash_state.txt");
    int index = 0;

    std::ifstream inFile(stateFile);
    if (inFile.is_open()) {
        if (inFile >> index) {
            if (index < 0) index = 0;
            index = (index + 1) % (int)kRegistry.size();
        }
        inFile.close();
    }
    if (index < 0 || index >= (int)kRegistry.size())
        index = 0;

    std::ofstream outFile(stateFile);
    if (outFile.is_open()) {
        outFile << index;
        outFile.close();
    }

    return kRegistry[index];
}

void Render(GLFWwindow* window, ImVec2 size, const std::string& status, float progress,
            GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
            const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme,
            const std::string& detail)
{
    progress = std::clamp(progress, 0.0f, 1.0f);
    static double s_FirstRenderTime = -1.0;
    const double now = glfwGetTime();
    if (s_FirstRenderTime < 0.0)
        s_FirstRenderTime = now;
    const float appear = EaseOutQuad(std::min(1.0f, (float)((now - s_FirstRenderTime) / 0.22) + 0.35f));

    if (fonts.ctx)
        ImGui::SetCurrentContext(fonts.ctx);
    glfwMakeContextCurrent(window);
    glClearColor(theme.base[0], theme.base[1], theme.base[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,            appear);

    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float scaleY = size.y / 380.0f;

    if (bgTexture != 0)
        dl->AddImage((void*)(intptr_t)bgTexture, ImVec2(0.0f, 0.0f), size);
    else
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), size, ThemeColorU32(theme.surface0));

    dl->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), size,
        ThemeColorU32(theme.base, 245.0f / 255.0f), ThemeColorU32(theme.base, 130.0f / 255.0f),
        ThemeColorU32(theme.base, 130.0f / 255.0f), ThemeColorU32(theme.base, 245.0f / 255.0f));

    const float padX     = 50.0f;
    const float logoSize = 88.0f * scaleY;
    const float logoY    = 58.0f * scaleY;

    if (logoTexture != 0)
        dl->AddImage((void*)(intptr_t)logoTexture, ImVec2(padX, logoY), ImVec2(padX + logoSize, logoY + logoSize));

    dl->AddLine(ImVec2(padX + logoSize + 18.0f, logoY + 6.0f), ImVec2(padX + logoSize + 18.0f, logoY + logoSize - 6.0f),
        ThemeColorU32(theme.accent, 170.0f / 255.0f), 1.8f);

    ImGui::SetCursorPos(ImVec2(padX + logoSize + 32.0f, logoY + 10.0f));
    if (fonts.title) ImGui::PushFont(fonts.title);
    ImGui::TextColored(ThemeColorVec4(theme.textPrimary), "ProyecThor");
    if (fonts.title) ImGui::PopFont();

    ImGui::SetCursorPos(ImVec2(padX + logoSize + 34.0f, logoY + 60.0f));
    if (fonts.regular) ImGui::PushFont(fonts.regular);
    ImGui::TextColored(ThemeColorVec4(theme.accentLight), "Professional Presentation Engine");
    if (fonts.regular) ImGui::PopFont();

    const float footerH = 82.0f * scaleY;
    const float footerY = size.y - footerH;

    if (fonts.small) ImGui::PushFont(fonts.small);
    const float creditW = ImGui::CalcTextSize(creditText.c_str()).x;
    const float creditH = ImGui::CalcTextSize(creditText.c_str()).y;
    const float badgeY  = footerY - creditH - 16.0f;

    dl->AddRectFilled(ImVec2(padX - 10.0f, badgeY), ImVec2(padX + creditW + 10.0f, footerY - 6.0f),
        ThemeColorU32(theme.base, 200.0f / 255.0f), 6.0f);

    ImGui::SetCursorPos(ImVec2(padX, badgeY + 5.0f));
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", creditText.c_str());
    if (fonts.small) ImGui::PopFont();

    dl->AddRectFilled(ImVec2(0.0f, footerY), size, ThemeColorU32(theme.base, 218.0f / 255.0f));
    dl->AddLine(ImVec2(0.0f, footerY), ImVec2(size.x, footerY), ThemeColorU32(theme.borderFaint, 18.0f / 255.0f), 1.0f);

    if (fonts.small) ImGui::PushFont(fonts.small);

    static const std::string versionLine =
        "Version " PROYECTHOR_VERSION_STRING "  |  Build " + std::to_string(PROYECTHOR_BUILD_NUMBER);
    const std::string copyLine = "\xC2\xA9 2026 ProyecThor Team";
    const float vW = ImGui::CalcTextSize(versionLine.c_str()).x;
    const float cW = ImGui::CalcTextSize(copyLine.c_str()).x;
    const float rightColX = size.x - std::max(vW, cW) - padX;
    const float maxStatusW = std::max(140.0f, rightColX - padX - 16.0f);

    // Porcentaje + mensaje principal de estado y sub-detalle real del recurso actual
    const int pct = std::clamp((int)(progress * 100.0f + 0.5f), 0, 100);
    const std::string pctText = std::to_string(pct) + "%";
    const float pctW = ImGui::CalcTextSize(pctText.c_str()).x;

    const float statusY = detail.empty() ? (footerY + 28.0f * scaleY) : (footerY + 17.0f * scaleY);
    ImGui::SetCursorPos(ImVec2(padX, statusY));
    ImGui::TextColored(ThemeColorVec4(theme.accentLight), "%s", pctText.c_str());

    ImGui::SameLine(0.0f, 8.0f * scaleY);
    dl->PushClipRect(ImVec2(padX, footerY), ImVec2(padX + maxStatusW, size.y), true);
    ImGui::TextColored(ThemeColorVec4(theme.textPrimary, 0.92f), "%s", status.c_str());
    dl->PopClipRect();

    if (!detail.empty()) {
        ImGui::SetCursorPos(ImVec2(padX + pctW + 8.0f * scaleY, footerY + 41.0f * scaleY));
        dl->PushClipRect(ImVec2(padX, footerY), ImVec2(padX + maxStatusW, size.y), true);
        ImGui::TextColored(ThemeColorVec4(theme.textDim, 0.85f), "%s", detail.c_str());
        dl->PopClipRect();
    }

    ImGui::SetCursorPos(ImVec2(size.x - vW - padX, footerY + 18.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint), "%s", versionLine.c_str());

    ImGui::SetCursorPos(ImVec2(size.x - cW - padX, footerY + 42.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint, theme.textFaint[3] * 0.75f), "%s", copyLine.c_str());
    if (fonts.small) ImGui::PopFont();

    const float barH   = 4.5f * scaleY;
    const float barEnd = size.x * progress;

    dl->AddRectFilled(ImVec2(0.0f, size.y - barH), size, ThemeColorU32(theme.surface0));

    if (barEnd > 2.0f) {
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH - 6.0f), ImVec2(barEnd, size.y),
            ThemeColorU32(theme.accent, 35.0f / 255.0f));
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH - 2.0f), ImVec2(barEnd, size.y),
            ThemeColorU32(theme.accent, 70.0f / 255.0f));
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH), ImVec2(barEnd, size.y), ThemeColorU32(theme.accent));

        if (barEnd > 10.0f) {
            dl->AddRectFilled(ImVec2(barEnd - 10.0f, size.y - barH), ImVec2(barEnd, size.y),
                ThemeColorU32(theme.accentLight));
        }
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
    glfwPollEvents();
}

void RunStep(const Step& step, float startProgress, float endProgress, GLFWwindow* window, ImVec2 size,
             GLuint logoTex, GLuint bgTex, const Fonts& fonts,
             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    // Mostrar el mensaje de este paso ANTES de ejecutar su tarea pesada
    Render(window, size, step.msg, startProgress, logoTex, bgTex, fonts, creditText, theme);
    double lastRenderTime = glfwGetTime();
    std::string lastDetail;

    auto report = [&](float subProgress, const std::string& detail) {
        subProgress = std::clamp(subProgress, 0.0f, 1.0f);
        lastDetail  = detail;

        const double now = glfwGetTime();
        // Limitar redibujado intermedio a ~65 FPS para que bucles rapidos de
        // recursos cacheados no pierdan tiempo en SwapBuffers innecesarios.
        if (subProgress < 1.0f && (now - lastRenderTime) < 0.015)
            return;
        lastRenderTime = now;

        GLFWwindow*   prevWin = glfwGetCurrentContext();
        ImGuiContext* prevCtx = ImGui::GetCurrentContext();

        const float p = startProgress + (endProgress - startProgress) * subProgress;
        Render(window, size, step.msg, p, logoTex, bgTex, fonts, creditText, theme, detail);

        if (prevWin && prevWin != window)
            glfwMakeContextCurrent(prevWin);
        if (prevCtx && prevCtx != fonts.ctx)
            ImGui::SetCurrentContext(prevCtx);
    };

    if (step.taskWithProgress)
        step.taskWithProgress(report);
    else if (step.task)
        step.task();

    Render(window, size, step.msg, endProgress, logoTex, bgTex, fonts, creditText, theme, lastDetail);
}

void RunSteps(const std::vector<Step>& steps, GLFWwindow* window, ImVec2 size,
              GLuint logoTex, GLuint bgTex, const Fonts& fonts,
              const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    if (steps.empty()) return;

    float totalWeight = 0.0f;
    for (const auto& s : steps)
        totalWeight += std::max(0.05f, s.weight);

    constexpr float kInitialProgress = 0.03f;
    const float span = 1.0f - kInitialProgress;
    float accum = 0.0f;

    for (const auto& s : steps) {
        const float w  = std::max(0.05f, s.weight);
        const float p0 = kInitialProgress + span * (accum / totalWeight);
        accum += w;
        const float p1 = kInitialProgress + span * (accum / totalWeight);

        RunStep(s, p0, p1, window, size, logoTex, bgTex, fonts, creditText, theme);
        if (glfwWindowShouldClose(window))
            break;
    }
}

} // namespace ProyecThor::Splash
