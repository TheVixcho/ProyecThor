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
#include "framework/PatchNotesData.h"

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

void WaitForUserConfirmation(GLFWwindow* window, ImVec2 size,
                             GLuint logoTexture, GLuint bgTexture, const Fonts& fonts,
                             const std::string& creditText, const ProyecThor::Settings::ThemeSettings& theme)
{
    bool confirmed = false;
    double startTime = glfwGetTime();

    while (!confirmed && !glfwWindowShouldClose(window))
    {
        float elapsed = (float)glfwGetTime();

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
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,            1.0f);

        ImGui::Begin("##SplashWait", nullptr,
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

        // --- Botones interactivos limpios: "Iniciar" y "Notas de versión" ---
        const float btnW      = 120.0f * scaleY;
        const float notesBtnW = 150.0f * scaleY;
        const float btnH      = 34.0f * scaleY;
        const float btnX      = padX;
        const float btnY      = footerY + (footerH - btnH) * 0.5f - 2.0f;

        static bool s_ShowPatchNotesModal = false;

        ImGui::SetCursorPos(ImVec2(btnX, btnY));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f * scaleY);

        ImGui::PushStyleColor(ImGuiCol_Button,        ThemeColorVec4(theme.accent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColorVec4(theme.accentLight));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeColorVec4(theme.accent));
        ImGui::PushStyleColor(ImGuiCol_Text,          ThemeColorVec4(theme.textPrimary));

        if (fonts.small) ImGui::PushFont(fonts.small);
        if (ImGui::Button("Iniciar", ImVec2(btnW, btnH))) {
            confirmed = true;
        }

        ImGui::SameLine(0.0f, 10.0f * scaleY);
        ImGui::PushStyleColor(ImGuiCol_Button,        ThemeColorVec4(theme.surface1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColorVec4(theme.surface2));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeColorVec4(theme.surface3));
        ImGui::PushStyleColor(ImGuiCol_Text,          ThemeColorVec4(theme.textPrimary));
        if (ImGui::Button("Notas de versión", ImVec2(notesBtnW, btnH))) {
            s_ShowPatchNotesModal = !s_ShowPatchNotesModal;
        }
        ImGui::PopStyleColor(4);

        if (fonts.small) ImGui::PopFont();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

        // Si el modal de notas está abierto, mostrar el diálogo interactivo
        if (s_ShowPatchNotesModal) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                s_ShowPatchNotesModal = false;
            }

            const float padM    = 14.0f * scaleY;
            const float mWidth  = size.x - padM * 2.0f;
            const float mHeight = size.y - padM * 2.0f;

            ImGui::SetNextWindowPos(ImVec2(padM, padM));
            ImGui::SetNextWindowSize(ImVec2(mWidth, mHeight));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColorVec4(theme.surface0, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_Border,   ThemeColorVec4(theme.accent, 0.75f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * scaleY);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scaleY, 12.0f * scaleY));

            if (ImGui::Begin("##PatchNotesModal", &s_ShowPatchNotesModal,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoTitleBar))
            {
                // Cabecera
                if (fonts.regular) ImGui::PushFont(fonts.regular);
                ImGui::TextColored(ThemeColorVec4(theme.accentLight), "Notas de Versión");
                if (fonts.regular) ImGui::PopFont();

                ImGui::SameLine(0.0f, 8.0f * scaleY);
                if (fonts.small) ImGui::PushFont(fonts.small);
                ImGui::TextColored(ThemeColorVec4(theme.textDim), "— Historial Completo");
                if (fonts.small) ImGui::PopFont();

                const float closeIconSize = 22.0f * scaleY;
                ImGui::SameLine(mWidth - closeIconSize - 32.0f * scaleY);
                ImGui::PushStyleColor(ImGuiCol_Button,        ThemeColorVec4(theme.surface2));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColorVec4(theme.surface3));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeColorVec4(theme.surface1));
                ImGui::PushStyleColor(ImGuiCol_Text,          ThemeColorVec4(theme.textPrimary));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scaleY);
                if (ImGui::Button("X##close_notes_top", ImVec2(closeIconSize, closeIconSize))) {
                    s_ShowPatchNotesModal = false;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);

                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 4.0f * scaleY));

                // Área de notas con scroll vertical hacia abajo (estilo Hub)
                const float footerAreaH = 40.0f * scaleY;
                ImGui::BeginChild("##NotesScroll", ImVec2(0.0f, -footerAreaH), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

                const auto& patchNotes = ProyecThor::UI::GetPatchNotesRegistry();
                for (size_t i = 0; i < patchNotes.size(); ++i) {
                    const auto& info = patchNotes[i];
                    ImGui::PushID(static_cast<int>(i));

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColorVec4(theme.surface1, 0.65f));
                    ImGui::PushStyleColor(ImGuiCol_Border,  info.isBeta ? ThemeColorVec4(theme.borderFaint, 0.40f) : ThemeColorVec4(theme.accent, 0.55f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f * scaleY);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.2f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scaleY, 8.0f * scaleY));

                    char cardId[32];
                    snprintf(cardId, sizeof(cardId), "##card_%d", info.id);
                    if (ImGui::BeginChild(cardId, ImVec2(0.0f, 0.0f),
                            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                    {
                        if (fonts.small) ImGui::PushFont(fonts.small);
                        const char* badge = (info.modalBadge && info.modalBadge[0]) ? info.modalBadge : (info.isBeta ? "ETAPA BETA" : "ACTUALIZACIÓN ESTABLE");
                        ImGui::TextColored(info.isBeta ? ThemeColorVec4(theme.textDim) : ThemeColorVec4(theme.accentLight), "[ %s ]", badge);
                        ImGui::SameLine(0.0f, 10.0f * scaleY);
                        ImGui::TextColored(ThemeColorVec4(theme.textPrimary), "•  Versión v%s", info.version);
                        if (fonts.small) ImGui::PopFont();

                        ImGui::Dummy(ImVec2(0.0f, 2.0f * scaleY));

                        if (fonts.small) ImGui::PushFont(fonts.small);
                        ImGui::PushTextWrapPos(0.0f);
                        ImGui::TextColored(ThemeColorVec4(theme.textDim), "%s", info.summary);
                        ImGui::PopTextWrapPos();
                        if (fonts.small) ImGui::PopFont();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(3);
                    ImGui::PopStyleColor(2);

                    ImGui::Dummy(ImVec2(0.0f, 6.0f * scaleY));
                    ImGui::PopID();
                }

                ImGui::EndChild(); // ##NotesScroll

                // Barra inferior de acciones (Cerrar / Iniciar) limpia y sin cortes
                ImGui::Dummy(ImVec2(0.0f, 4.0f * scaleY));

                const float btnH       = 28.0f * scaleY;
                const float closeBtnW  = 100.0f * scaleY;
                const float launchBtnW = 160.0f * scaleY;

                ImGui::PushStyleColor(ImGuiCol_Button,        ThemeColorVec4(theme.surface2));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColorVec4(theme.surface3));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeColorVec4(theme.surface1));
                ImGui::PushStyleColor(ImGuiCol_Text,          ThemeColorVec4(theme.textPrimary));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * scaleY);

                if (fonts.small) ImGui::PushFont(fonts.small);
                if (ImGui::Button("Cerrar", ImVec2(closeBtnW, btnH))) {
                    s_ShowPatchNotesModal = false;
                }

                ImGui::SameLine(0.0f, 12.0f * scaleY);
                ImGui::PushStyleColor(ImGuiCol_Button,        ThemeColorVec4(theme.accent));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ThemeColorVec4(theme.accentLight));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ThemeColorVec4(theme.accent));
                ImGui::PushStyleColor(ImGuiCol_Text,          ThemeColorVec4(theme.textPrimary));
                if (ImGui::Button("Iniciar ProyecThor", ImVec2(launchBtnW, btnH))) {
                    confirmed = true;
                }
                ImGui::PopStyleColor(4);

                if (fonts.small) ImGui::PopFont();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(4);
            }
            ImGui::End();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        static const std::string versionLine =
            "Version " PROYECTHOR_VERSION_STRING "  |  Build " + std::to_string(PROYECTHOR_BUILD_NUMBER);
        const std::string copyLine = "\xC2\xA9 2026 ProyecThor Team";
        if (fonts.small) ImGui::PushFont(fonts.small);
        const float vW = ImGui::CalcTextSize(versionLine.c_str()).x;
        const float cW = ImGui::CalcTextSize(copyLine.c_str()).x;

        ImGui::SetCursorPos(ImVec2(size.x - vW - padX, footerY + 18.0f * scaleY));
        ImGui::TextColored(ThemeColorVec4(theme.textFaint), "%s", versionLine.c_str());

        ImGui::SetCursorPos(ImVec2(size.x - cW - padX, footerY + 42.0f * scaleY));
        ImGui::TextColored(ThemeColorVec4(theme.textFaint, theme.textFaint[3] * 0.75f), "%s", copyLine.c_str());
        if (fonts.small) ImGui::PopFont();

        // Barra de progreso 100% llena
        const float barH = 4.0f;
        dl->AddRectFilled(ImVec2(0.0f, size.y - barH), size, ThemeColorU32(theme.accent));

        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

} // namespace ProyecThor::Splash
