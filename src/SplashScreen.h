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

struct Fonts {
    ImFont* title   = nullptr;
    ImFont* regular = nullptr;
    ImFont* small   = nullptr;
};

struct Step {
    std::string           msg;
    std::function<void()> task;
};

Art PickArt();

void Render(GLFWwindow* window, ImVec2 size, const std::string& status, float progress,
            GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
            const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme);

void RunStep(const Step& step, int idx, int total, GLFWwindow* window, ImVec2 size,
             GLuint logoTex, GLuint bgTex, const Fonts& fonts,
             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::Splash
