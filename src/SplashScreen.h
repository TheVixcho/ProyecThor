#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include "SettingsManager.h"

namespace ProyecThor::Splash {

struct Art {
    std::string filename;
    std::string author;
};

#ifdef small
#undef small
#endif

struct Fonts {
    ImFont* title   = nullptr;
    ImFont* regular = nullptr;
    ImFont* small   = nullptr;
    ImGuiContext* ctx = nullptr;
};

using ProgressReporter = std::function<void(float subProgress, const std::string& detail)>;

struct Step {
    std::string                           msg;
    float                                 weight = 1.0f;
    std::function<void()>                 task;
    std::function<void(ProgressReporter)> taskWithProgress;
};

Art PickArt();

void Render(GLFWwindow* window, ImVec2 size, const std::string& status, float progress,
            GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
            const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme,
            const std::string& detail = "");

void RunStep(const Step& step, float startProgress, float endProgress, GLFWwindow* window, ImVec2 size,
             GLuint logoTex, GLuint bgTex, const Fonts& fonts,
             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme);

void RunSteps(const std::vector<Step>& steps, GLFWwindow* window, ImVec2 size,
              GLuint logoTex, GLuint bgTex, const Fonts& fonts,
              const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::Splash
