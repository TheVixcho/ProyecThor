#include "ProImageGeneratorPanel.h"
#include "core/ProAIManager.h"
#include "core/AuthLicenseManager.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/LoadingSpinner.h"
#include <imgui.h>
#include <GL/glew.h>
#include "stb_image.h"
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace ProyecThor::UI {

ProImageGeneratorPanel::~ProImageGeneratorPanel() {
    FreePreviewTexture();
}

void ProImageGeneratorPanel::FreePreviewTexture() {
    if (m_LastTextureId != 0) {
        glDeleteTextures(1, &m_LastTextureId);
        m_LastTextureId = 0;
        m_TexW = 0;
        m_TexH = 0;
    }
}

void ProImageGeneratorPanel::LoadPreviewTexture(const std::string& path) {
    FreePreviewTexture();

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!pixels) return;

    glGenTextures(1, &m_LastTextureId);
    glBindTexture(GL_TEXTURE_2D, m_LastTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    m_TexW = w;
    m_TexH = h;
}

void ProImageGeneratorPanel::Render(bool* pOpen, GlassRenderer& glass) {
    if (!pOpen || !*pOpen) return;

    auto& proMgr  = ProyecThor::Core::ProAIManager::Get();
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 880.0f, 660.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 620.0f, 480.0f }, { 10000.0f, 10000.0f });

    ImGuiWindowClass floatingClass;
    floatingClass.DockingAllowUnclassed = false;
    ImGui::SetNextWindowClass(&floatingClass);

    bool open = DS::BeginGlassPanel("Creador de Imágenes IA Pro (Fondos 16:9)", glass, pOpen,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking,
        ImVec2(18.0f, 16.0f));

    if (open) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float availW = ImGui::GetContentRegionAvail().x;

        // ── Cabecera: Contador de Imágenes Mensuales (Límite 30 imágenes) ────
        ImVec2 topP = ImGui::GetCursorScreenPos();
        float topH  = 44.0f;
        dl->AddRectFilled(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(32, 26, 18, 220), DS::RadiusMedium);
        dl->AddRect(topP, ImVec2(topP.x + availW, topP.y + topH), IM_COL32(251, 191, 36, 140), DS::RadiusMedium);

        ImGui::SetCursorScreenPos(ImVec2(topP.x + 16.0f, topP.y + 12.0f));

        int availImages = proMgr.GetAvailableImages();
        int maxImages   = proMgr.GetMaxMonthlyImages();
        int usedImages  = proMgr.GetUsedImages();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.40f, 1.0f));
        ImGui::Text("Imágenes disponibles este mes: %d / %d", availImages, maxImages);
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 30.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
        ImGui::Text("Generadas: %d | Resolución: 1920x1080 (16:9)", usedImages);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(topP.x, topP.y + topH + 12.0f));

        // ── Píldoras de estilos temáticos ───────────────────────────────────
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "Estilos predefinidos para fondos de proyección:");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        if (DS::GlassButton("Cielo Estrellado", ImVec2(140.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Cielo nocturno profundo con nebulosa galáctica sutil en tonos violeta y azul oscuro, cinematográfico, fondo de alabanza limpio, 4k", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 8.0f);
        if (DS::GlassButton("Montañas Sagradas", ImVec2(150.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Montañas majestuosas cubiertas de niebla al atardecer con rayos de luz dorada celestial, atmósfera de adoración, solemne, 4k", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 8.0f);
        if (DS::GlassButton("Luces de Escenario", ImVec2(150.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Haces de luz volumétrica y niebla cálida en escenario de auditorio contemporáneo, fondo abstracto oscuro para proyección de letras", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 8.0f);
        if (DS::GlassButton("Texturas Minimalistas", ImVec2(165.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Textura abstracta fluida y etérea en tonos azul índigo y dorado suave, minimalista, sin texto, alto contraste para texto blanco", sizeof(m_PromptBuf) - 1);
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // ── Formulario de Prompt ─────────────────────────────────────────────
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Descripción del fondo deseado:");
        ImGui::SetNextItemWidth(availW - 190.0f);
        ImGui::InputTextWithHint("##prompt_input", "Ej: Nubes doradas al amanecer sobre aguas calmas, cinematográfico, 16:9...",
                                 m_PromptBuf, sizeof(m_PromptBuf));

        ImGui::SameLine(0, 10.0f);

        const bool isBusy = proMgr.IsImageBusy();
        const bool canGen = proMgr.CanGenerateImage();

        if (isBusy || !canGen) ImGui::BeginDisabled();

        bool genClicked = ImGui::Button(isBusy ? "Generando..." : "Generar Imagen", ImVec2(170.0f, 32.0f));

        if (isBusy || !canGen) ImGui::EndDisabled();

        if (genClicked && m_PromptBuf[0] != '\0' && !isBusy && canGen) {
            m_StatusFeedback = "Generando imagen con IA (16:9) optimizada para proyección...";
            m_StatusIsSuccess = true;

            std::string promptStr = m_PromptBuf;
            proMgr.GenerateImage(promptStr, [this](bool success, const std::string& path, const std::string& err) {
                if (success) {
                    m_LastGeneratedPath = path;
                    LoadPreviewTexture(path);
                    m_StatusFeedback = "Imagen generada con éxito!";
                    m_StatusIsSuccess = true;
                } else {
                    m_StatusFeedback = "Error: " + err;
                    m_StatusIsSuccess = false;
                }
            });
        }

        if (isBusy) {
            ImGui::SameLine(0, 12.0f);
            DrawLoadingSpinner(dl, ImVec2(ImGui::GetCursorScreenPos().x + 10.0f, ImGui::GetCursorScreenPos().y + 15.0f), 10.0f);
        }

        if (!canGen) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("Has alcanzado el límite mensual de 30 imágenes de tu suscripción Pro. Tu cupo se renovará al finalizar el mes.");
            ImGui::PopStyleColor();
        }

        if (!m_StatusFeedback.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            ImVec4 col = m_StatusIsSuccess ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(m_StatusFeedback.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // ── Vista Previa de la Imagen Generada ────────────────────────────────
        float previewW = availW;
        float previewH = previewW * (9.0f / 16.0f); // Proporción 16:9
        float maxH = ImGui::GetContentRegionAvail().y - 48.0f;
        if (previewH > maxH && maxH > 100.0f) {
            previewH = maxH;
            previewW = previewH * (16.0f / 9.0f);
        }

        if (m_LastTextureId != 0) {
            // Centrar horizontalmente
            float posX = (availW - previewW) * 0.5f;
            if (posX > 0) ImGui::SetCursorPosX(posX);

            ImGui::Image((ImTextureID)(intptr_t)m_LastTextureId, ImVec2(previewW, previewH));

            ImGui::Dummy(ImVec2(0.0f, 10.0f));

            if (DS::GlassButton("Abrir carpeta de imágenes", ImVec2(200.0f, 28.0f))) {
#ifdef _WIN32
                ShellExecuteA(NULL, "open", proMgr.GetAIImagesDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
            }

            ImGui::SameLine(0, 10.0f);
            if (DS::GlassButton("Copiar ruta del archivo", ImVec2(180.0f, 28.0f))) {
                ImGui::SetClipboardText(m_LastGeneratedPath.c_str());
                m_StatusFeedback = "Ruta copiada al portapapeles.";
            }
        } else {
            ImVec2 boxMin = ImGui::GetCursorScreenPos();
            ImVec2 boxMax = ImVec2(boxMin.x + availW, boxMin.y + std::min(previewH, 260.0f));
            dl->AddRectFilled(boxMin, boxMax, IM_COL32(16, 18, 24, 200), DS::RadiusMedium);
            dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 20), DS::RadiusMedium);

            const char* emptyHint = "Escribe una descripción arriba o selecciona un estilo y presiona 'Generar Imagen'.";
            ImVec2 tSz = ImGui::CalcTextSize(emptyHint);
            dl->AddText(ImVec2(boxMin.x + (availW - tSz.x) * 0.5f, boxMin.y + (boxMax.y - boxMin.y - tSz.y) * 0.5f),
                        DS::TextHint, emptyHint);

            ImGui::Dummy(ImVec2(0.0f, boxMax.y - boxMin.y));
        }
    }

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI
