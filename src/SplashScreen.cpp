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
#include "Version.h"

std::string GetAppDataFilePath(const std::string& filename);

namespace ProyecThor::Splash {

namespace {

const std::vector<Art> kRegistry = {
    {"splash_bg1.jpg", "Fabiola Fernandez"},
    {"splash_bg5.jpg", "TheVixcho"},
    {"splash_bg2.jpg", "TheVixcho"},
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
        if (inFile >> index)
            index = (index + 1) % (int)kRegistry.size();
        inFile.close();
    }

    std::ofstream outFile(stateFile);
    if (outFile.is_open()) {
        outFile << index;
        outFile.close();
    }

    return kRegistry[index];
}

void Render(GLFWwindow* window, ImVec2 size, const std::string& status, float progress,
            GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
            const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    const float elapsed = (float)glfwGetTime();
    const float appear  = EaseOutQuad(std::max(progress, 0.34f));

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
    const ImVec2 logoCenter(padX + logoSize * 0.5f, logoY + logoSize * 0.5f);

    const float pulse = 0.5f + 0.5f * sinf(elapsed * 1.8f);
    dl->AddCircleFilled(logoCenter, logoSize * 0.72f + pulse * 5.0f,
        ThemeColorU32(theme.accent, (0.05f + pulse * 0.05f) * appear), 40);

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

    ImGui::SetCursorPos(ImVec2(padX, footerY + 28.0f * scaleY));
    if (fonts.small) ImGui::PushFont(fonts.small);
    ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", status.c_str());

    static const std::string versionLine =
        "Version " PROYECTHOR_VERSION_STRING "  |  Build " + std::to_string(PROYECTHOR_BUILD_NUMBER);
    const std::string copyLine = "\xC2\xA9 2026 ProyecThor Team";
    const float vW = ImGui::CalcTextSize(versionLine.c_str()).x;
    const float cW = ImGui::CalcTextSize(copyLine.c_str()).x;

    ImGui::SetCursorPos(ImVec2(size.x - vW - padX, footerY + 18.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint), "%s", versionLine.c_str());

    ImGui::SetCursorPos(ImVec2(size.x - cW - padX, footerY + 42.0f * scaleY));
    ImGui::TextColored(ThemeColorVec4(theme.textFaint, theme.textFaint[3] * 0.75f), "%s", copyLine.c_str());
    if (fonts.small) ImGui::PopFont();

    const float barH   = 4.0f;
    const float barEnd = size.x * progress;

    dl->AddRectFilled(ImVec2(0.0f, size.y - barH), size, ThemeColorU32(theme.surface0));

    if (barEnd > 2.0f) {
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH - 6.0f), ImVec2(barEnd, size.y),
            ThemeColorU32(theme.accent, 35.0f / 255.0f));
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH - 2.0f), ImVec2(barEnd, size.y),
            ThemeColorU32(theme.accent, 70.0f / 255.0f));
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH), ImVec2(barEnd, size.y), ThemeColorU32(theme.accent));

        if (barEnd > 8.0f) {
            dl->AddRectFilled(ImVec2(barEnd - 8.0f, size.y - barH), ImVec2(barEnd, size.y),
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

void RunStep(const Step& step, int idx, int total, GLFWwindow* window, ImVec2 size,
             GLuint logoTex, GLuint bgTex, const Fonts& fonts,
             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    step.task();

    const float progress = (float)(idx + 1) / (float)total;
    glfwMakeContextCurrent(window);
    Render(window, size, step.msg, progress, logoTex, bgTex, fonts, creditText, theme);
}

} // namespace ProyecThor::Splash
