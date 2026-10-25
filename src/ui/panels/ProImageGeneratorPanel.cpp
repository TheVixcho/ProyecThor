#include "ProImageGeneratorPanel.h"
#include "core/ProAIManager.h"
#include "core/AuthLicenseManager.h"
#include "core/AppPaths.h"
#include "core/PresentationCore.h"
#include "core/ThumbnailWorker.h"
#include "ui/framework/DesignSystem.h"
#include "ui/framework/LoadingSpinner.h"
#include "ui/framework/FilePicker.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include <imgui.h>
#include <GL/glew.h>
#include "stb_image.h"
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <chrono>

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
    FreeThumbnails();
}

void ProImageGeneratorPanel::FreePreviewTexture() {
    if (m_LastTextureId != 0) {
        glDeleteTextures(1, &m_LastTextureId);
        m_LastTextureId = 0;
        m_TexW = 0;
        m_TexH = 0;
    }
}

void ProImageGeneratorPanel::FreeThumbnails() {
    for (auto& item : m_CreatedImages) {
        if (item.thumbTexId != 0) {
            GLuint tex = static_cast<GLuint>(item.thumbTexId);
            glDeleteTextures(1, &tex);
            item.thumbTexId = 0;
        }
    }
    m_CreatedImages.clear();
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

void ProImageGeneratorPanel::EnsureThumbnailLoaded(CreatedImageItem& item) {
    if (item.thumbTexId != 0) return;

    std::vector<uint8_t> pixels;
    unsigned w = 0, h = 0;
    if (!Core::ThumbnailWorker::LoadThumbnailPixelsFromDisk(item.fullPath, pixels, w, h, 320, 180)) {
        return;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    item.thumbTexId = tex;
    item.thumbW = (int)w;
    item.thumbH = (int)h;
}

void ProImageGeneratorPanel::ScanCreatedImages() {
    FreeThumbnails();

    auto& proMgr = ProyecThor::Core::ProAIManager::Get();
    std::string aiDir = proMgr.GetAIImagesDir();
    std::error_code ec;

    if (!std::filesystem::exists(aiDir, ec)) {
        m_NeedScanImages = false;
        return;
    }

    struct TempEntry {
        std::filesystem::path path;
        std::filesystem::file_time_type writeTime;
        uint64_t sz = 0;
    };
    std::vector<TempEntry> entries;

    for (const auto& entry : std::filesystem::directory_iterator(aiDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
            auto wt = entry.last_write_time(ec);
            uint64_t sz = entry.file_size(ec);
            entries.push_back({ entry.path(), wt, sz });
        }
    }

    // Ordenar de más reciente a más antigua
    std::sort(entries.begin(), entries.end(), [](const TempEntry& a, const TempEntry& b) {
        return a.writeTime > b.writeTime;
    });

    std::string assetsImages = ProyecThor::ImagesPath();

    for (const auto& e : entries) {
        CreatedImageItem item;
        item.filename = e.path.filename().string();
        item.fullPath = e.path.string();
        item.fileSize = e.sz;

        // Formato de fecha
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            e.writeTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", &tm);
        item.timeStr = timeBuf;

        // Verificar si ya está en multimedia/fondos
        std::filesystem::path dest = std::filesystem::path(assetsImages) / item.filename;
        std::error_code checkEc;
        item.isImported = std::filesystem::exists(dest, checkEc);

        m_CreatedImages.push_back(std::move(item));
    }

    // Si no hay imagen en preview pero existen imágenes creadas, precargar la primera
    if (m_LastTextureId == 0 && !m_CreatedImages.empty()) {
        m_LastGeneratedPath = m_CreatedImages[0].fullPath;
        LoadPreviewTexture(m_LastGeneratedPath);
    }

    m_NeedScanImages = false;
}

bool ProImageGeneratorPanel::ImportToMultimedia(const std::string& srcPath, std::string& outMsg) {
    if (srcPath.empty()) {
        outMsg = "No hay ninguna imagen seleccionada para importar.";
        return false;
    }

    try {
        std::string destDir = ProyecThor::ImagesPath();
        std::error_code ec;
        std::filesystem::create_directories(destDir, ec);

        std::string filename = std::filesystem::path(srcPath).filename().string();
        std::filesystem::path destPath = std::filesystem::path(destDir) / filename;

        std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            outMsg = "Error al copiar a Multimedia: " + ec.message();
            return false;
        }

        ProyecThor::Library::RefreshMultimediaLists();
        outMsg = "Imagen agregada a Fondos y Multimedia (" + filename + ")";

        for (auto& item : m_CreatedImages) {
            if (item.fullPath == srcPath) {
                item.isImported = true;
                break;
            }
        }
        return true;
    } catch (const std::exception& e) {
        outMsg = std::string("Error al importar: ") + e.what();
        return false;
    }
}

void ProImageGeneratorPanel::SetAsLiveBackground(const std::string& fullPath) {
    if (fullPath.empty()) return;

    auto& core = ProyecThor::Core::PresentationCore::Get();
    core.SetBackgroundMedia(fullPath, false);
    core.SetProjecting(true);

    m_StatusFeedback = "Fondo proyectado en vivo en la pantalla de salida.";
    m_StatusIsSuccess = true;
}

void ProImageGeneratorPanel::Render(bool* pOpen, GlassRenderer& glass) {
    if (!pOpen || !*pOpen) return;

    auto& proMgr  = ProyecThor::Core::ProAIManager::Get();
    auto& authMgr = ProyecThor::Core::AuthLicenseManager::Get();

    // Cargar o refrescar lista de imágenes creadas si es necesario
    if (m_NeedScanImages) {
        ScanCreatedImages();
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f },
                             ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 1080.0f, 700.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 820.0f, 520.0f }, { 10000.0f, 10000.0f });

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

        // ── Distribución en 2 Columnas: Izquierda (Generador y Preview) | Derecha (Galería Creadas)
        float rightColW = (availW > 860.0f) ? 340.0f : 280.0f;
        float leftColW  = availW - rightColW - 14.0f;
        float contentH  = ImGui::GetContentRegionAvail().y;

        // ── COLUMNA IZQUIERDA: Generación y Vista Previa Principal ───────────
        ImGui::BeginChild("##left_gen_col", ImVec2(leftColW, contentH), false);

        // Estilos temáticos predefinidos
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint), "Estilos predefinidos para fondos de proyección:");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        if (DS::GlassButton("Cielo Estrellado", ImVec2(130.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Cielo nocturno profundo con nebulosa galáctica sutil en tonos violeta y azul oscuro, cinematográfico, fondo de alabanza limpio, 4k", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 6.0f);
        if (DS::GlassButton("Montañas Sagradas", ImVec2(140.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Montañas majestuosas cubiertas de niebla al atardecer con rayos de luz dorada celestial, atmósfera de adoración, solemne, 4k", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 6.0f);
        if (DS::GlassButton("Luces de Escenario", ImVec2(140.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Haces de luz volumétrica y niebla cálida en escenario de auditorio contemporáneo, fondo abstracto oscuro para proyección de letras", sizeof(m_PromptBuf) - 1);
        }
        ImGui::SameLine(0, 6.0f);
        if (DS::GlassButton("Textura Fluida", ImVec2(120.0f, 26.0f))) {
            strncpy(m_PromptBuf, "Textura abstracta fluida y etérea en tonos azul índigo y dorado suave, minimalista, sin texto, alto contraste para texto blanco", sizeof(m_PromptBuf) - 1);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Chip de imagen de referencia adjunta si existe
        if (!m_AttachedImageName.empty()) {
            ImVec2 chipP = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(chipP, ImVec2(chipP.x + leftColW, chipP.y + 26.0f), IM_COL32(40, 32, 20, 220), 4.0f);
            dl->AddRect(chipP, ImVec2(chipP.x + leftColW, chipP.y + 26.0f), IM_COL32(251, 191, 36, 140), 4.0f);

            ImGui::SetCursorScreenPos(ImVec2(chipP.x + 8.0f, chipP.y + 5.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.40f, 1.0f));
            ImGui::Text("Referencia:");
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 6.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", m_AttachedImageName.c_str());

            ImGui::SameLine(0, 10.0f);
            if (ImGui::SmallButton("X Descartar##discard_img")) {
                m_AttachedImagePath.clear();
                m_AttachedImageName.clear();
            }

            ImGui::SetCursorScreenPos(ImVec2(chipP.x, chipP.y + 32.0f));
        }

        // Formulario de prompt con botón '+'
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary), "Descripción del fondo deseado:");

        const bool isBusy = proMgr.IsImageBusy();
        const bool canGen = proMgr.CanGenerateImage();

        if (isBusy || !canGen) ImGui::BeginDisabled();

        // Botón '+' para adjuntar
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.30f, 0.15f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.40f, 0.20f, 1.0f));
        if (ImGui::Button("+##attach_img_btn", ImVec2(32.0f, 32.0f))) {
            ImGui::OpenPopup("##attach_img_menu");
        }
        ImGui::PopStyleColor(2);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Adjuntar imagen de referencia desde tu PC o fondos guardados");
        }

        if (ImGui::BeginPopup("##attach_img_menu")) {
            if (ImGui::MenuItem("Elegir imagen de la PC...")) {
                std::string path = ProyecThor::UI::PickImageFile();
                if (!path.empty()) {
                    m_AttachedImagePath = path;
                    m_AttachedImageName = std::filesystem::path(path).filename().string();
                }
            }
            if (ImGui::MenuItem("Elegir de Fondos guardados...")) {
                m_ShowLocalBgModal = true;
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(0, 6.0f);

        float inputW = leftColW - 190.0f;
        if (inputW < 140.0f) inputW = 140.0f;
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputTextWithHint("##prompt_input", "Ej: Nubes doradas al amanecer sobre aguas calmas, cinematográfico, 16:9...",
                                 m_PromptBuf, sizeof(m_PromptBuf));

        ImGui::SameLine(0, 6.0f);

        bool genClicked = ImGui::Button(isBusy ? "Generando..." : "Generar Imagen", ImVec2(145.0f, 32.0f));

        if (isBusy || !canGen) ImGui::EndDisabled();

        if (genClicked && (m_PromptBuf[0] != '\0' || !m_AttachedImageName.empty()) && !isBusy && canGen) {
            m_StatusFeedback = "Generando imagen con IA (16:9) optimizada para proyección...";
            m_StatusIsSuccess = true;

            std::string promptStr = m_PromptBuf;
            if (!m_AttachedImageName.empty()) {
                if (promptStr.empty()) {
                    promptStr = "Fondo cinematográfico de adoración 16:9 inspirado en el estilo y composición de: " + m_AttachedImageName;
                } else {
                    promptStr += " (Inspirado en la paleta y composición de " + m_AttachedImageName + ")";
                }
            }

            proMgr.GenerateImage(promptStr, [this](bool success, const std::string& path, const std::string& err) {
                if (success) {
                    m_LastGeneratedPath = path;
                    LoadPreviewTexture(path);
                    m_NeedScanImages = true; // Actualiza el cuadro de imágenes de la derecha
                    m_StatusFeedback = "Imagen generada con éxito!";
                    m_StatusIsSuccess = true;
                } else {
                    m_StatusFeedback = "Error: " + err;
                    m_StatusIsSuccess = false;
                }
            });
        }

        if (isBusy) {
            ImGui::SameLine(0, 8.0f);
            DrawLoadingSpinner(dl, ImVec2(ImGui::GetCursorScreenPos().x + 8.0f, ImGui::GetCursorScreenPos().y + 16.0f), 9.0f);
        }

        if (!canGen) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextWrapped("Has alcanzado el límite mensual de 30 imágenes de tu suscripción Pro.");
            ImGui::PopStyleColor();
        }

        if (!m_StatusFeedback.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImVec4 col = m_StatusIsSuccess ? ImVec4(0.40f, 0.85f, 0.55f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(m_StatusFeedback.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Vista Previa de Imagen Principal (16:9)
        float previewW = leftColW;
        float previewH = previewW * (9.0f / 16.0f);
        float maxH = ImGui::GetContentRegionAvail().y - 48.0f;
        if (previewH > maxH && maxH > 100.0f) {
            previewH = maxH;
            previewW = previewH * (16.0f / 9.0f);
        }

        if (m_LastTextureId != 0) {
            float posX = (leftColW - previewW) * 0.5f;
            if (posX > 0) ImGui::SetCursorPosX(posX);

            ImGui::Image((ImTextureID)(intptr_t)m_LastTextureId, ImVec2(previewW, previewH));

            ImGui::Dummy(ImVec2(0.0f, 8.0f));

            // Botones de acción directa para la imagen previsualizada
            bool isImported = false;
            if (!m_LastGeneratedPath.empty()) {
                std::string fname = std::filesystem::path(m_LastGeneratedPath).filename().string();
                std::error_code ec;
                isImported = std::filesystem::exists(std::filesystem::path(ProyecThor::ImagesPath()) / fname, ec);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.28f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.70f, 0.35f, 1.0f));
            if (ImGui::Button("Proyectar como Fondo en Vivo", ImVec2(205.0f, 28.0f))) {
                SetAsLiveBackground(m_LastGeneratedPath);
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, isImported ? ImVec4(0.25f, 0.35f, 0.45f, 0.85f) : ImVec4(0.20f, 0.45f, 0.75f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.55f, 0.88f, 1.0f));
            const char* impBtnText = isImported ? "✓ En Multimedia / Re-importar" : "Importar a Multimedia / Fondos";
            if (ImGui::Button(impBtnText, ImVec2(225.0f, 28.0f))) {
                std::string msg;
                if (ImportToMultimedia(m_LastGeneratedPath, msg)) {
                    m_StatusFeedback = msg;
                    m_StatusIsSuccess = true;
                } else {
                    m_StatusFeedback = msg;
                    m_StatusIsSuccess = false;
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0, 8.0f);
            if (DS::GlassButton("Copiar ruta", ImVec2(100.0f, 28.0f))) {
                ImGui::SetClipboardText(m_LastGeneratedPath.c_str());
                m_StatusFeedback = "Ruta copiada al portapapeles.";
                m_StatusIsSuccess = true;
            }
        } else {
            ImVec2 boxMin = ImGui::GetCursorScreenPos();
            ImVec2 boxMax = ImVec2(boxMin.x + leftColW, boxMin.y + std::min(previewH, 240.0f));
            dl->AddRectFilled(boxMin, boxMax, IM_COL32(16, 18, 24, 200), DS::RadiusMedium);
            dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 20), DS::RadiusMedium);

            const char* emptyHint = "Escribe una descripción o selecciona una imagen de la derecha.";
            ImVec2 tSz = ImGui::CalcTextSize(emptyHint);
            dl->AddText(ImVec2(boxMin.x + (leftColW - tSz.x) * 0.5f, boxMin.y + (boxMax.y - boxMin.y - tSz.y) * 0.5f),
                        DS::TextHint, emptyHint);

            ImGui::Dummy(ImVec2(0.0f, boxMax.y - boxMin.y));
        }

        ImGui::EndChild();

        ImGui::SameLine(0, 14.0f);

        // ── COLUMNA DERECHA: Cuadro de Imágenes Creadas y Acciones de Importación ───
        ImGui::BeginChild("##right_gallery_col", ImVec2(rightColW, contentH), true);

        // Cabecera del cuadro derecho
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.40f, 1.0f));
        ImGui::Text("Imágenes Creadas (%zu)", m_CreatedImages.size());
        ImGui::PopStyleColor();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 68.0f);
        if (ImGui::SmallButton("Actualizar##ref_gal")) {
            ScanCreatedImages();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        // Área scrolleable de tarjetas de imágenes creadas
        float footerH = 34.0f;
        float listH = ImGui::GetContentRegionAvail().y - footerH;

        ImGui::BeginChild("##gallery_items_scroll", ImVec2(-1.0f, listH), false);

        if (m_CreatedImages.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 30.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
            ImGui::TextWrapped("Aún no has generado imágenes.\nLas imágenes que crees aparecerán aquí automáticamente.");
            ImGui::PopStyleColor();
        } else {
            for (size_t i = 0; i < m_CreatedImages.size(); ++i) {
                auto& item = m_CreatedImages[i];
                ImGui::PushID(static_cast<int>(i));

                bool isSelected = (item.fullPath == m_LastGeneratedPath);

                // Marco de tarjeta
                ImVec2 cardMin = ImGui::GetCursorScreenPos();
                float cardW = ImGui::GetContentRegionAvail().x;
                float thumbW = cardW - 16.0f;
                float thumbH = thumbW * (9.0f / 16.0f);
                float cardH = thumbH + 72.0f;
                ImVec2 cardMax = ImVec2(cardMin.x + cardW, cardMin.y + cardH);

                ImU32 bgCol = isSelected ? IM_COL32(36, 48, 64, 230) : IM_COL32(22, 26, 34, 200);
                ImU32 borderCol = isSelected ? IM_COL32(56, 189, 248, 220) : IM_COL32(255, 255, 255, 25);
                dl->AddRectFilled(cardMin, cardMax, bgCol, 6.0f);
                dl->AddRect(cardMin, cardMax, borderCol, 6.0f, 0, isSelected ? 1.5f : 1.0f);

                // Carga de miniatura on-demand
                EnsureThumbnailLoaded(item);

                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + 8.0f, cardMin.y + 8.0f));

                // Click en miniatura para seleccionarla en el visor principal
                if (item.thumbTexId != 0) {
                    if (ImGui::ImageButton("##thumb_btn", (ImTextureID)(intptr_t)item.thumbTexId,
                                           ImVec2(thumbW, thumbH), ImVec2(0,0), ImVec2(1,1),
                                           ImVec4(0,0,0,0), isSelected ? ImVec4(0.3f,0.8f,1.0f,1.0f) : ImVec4(1,1,1,0.8f))) {
                        m_LastGeneratedPath = item.fullPath;
                        LoadPreviewTexture(item.fullPath);
                    }
                } else {
                    ImGui::Dummy(ImVec2(thumbW, thumbH));
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click para ver en tamaño grande");
                }

                // Fecha y hora
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + 10.0f, cardMin.y + thumbH + 12.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextHint));
                ImGui::Text("%s", item.timeStr.c_str());
                ImGui::PopStyleColor();

                // Botones rápidos: Fondo | Importar a Multimedia
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + 10.0f, cardMin.y + thumbH + 34.0f));

                float btnW1 = (cardW - 26.0f) * 0.44f;
                float btnW2 = (cardW - 26.0f) * 0.56f;

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.28f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.68f, 0.35f, 1.0f));
                if (ImGui::Button("Fondo##card_bg", ImVec2(btnW1, 24.0f))) {
                    m_LastGeneratedPath = item.fullPath;
                    LoadPreviewTexture(item.fullPath);
                    SetAsLiveBackground(item.fullPath);
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Proyectar inmediatamente como fondo en vivo");
                }

                ImGui::SameLine(0, 6.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, item.isImported ? ImVec4(0.22f, 0.30f, 0.40f, 0.80f) : ImVec4(0.20f, 0.42f, 0.70f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.52f, 0.85f, 1.0f));
                const char* impLabel = item.isImported ? "✓ Importado" : "Importar";
                if (ImGui::Button(impLabel, ImVec2(btnW2, 24.0f))) {
                    std::string msg;
                    if (ImportToMultimedia(item.fullPath, msg)) {
                        m_StatusFeedback = msg;
                        m_StatusIsSuccess = true;
                    } else {
                        m_StatusFeedback = msg;
                        m_StatusIsSuccess = false;
                    }
                }
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Copiar a la carpeta de fondos y multimedia del programa");
                }

                ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardMax.y + 8.0f));
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        // Pie del cuadro derecho: acceso a carpeta
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        if (DS::GlassButton("Abrir carpeta IA", ImVec2(rightColW - 16.0f, 26.0f))) {
#ifdef _WIN32
            ShellExecuteA(NULL, "open", proMgr.GetAIImagesDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
        }

        ImGui::EndChild();

        // ── Modal: Selector de Fondos Guardados para Referencia ─────────────
        if (m_ShowLocalBgModal) {
            ImGui::OpenPopup("Fondos Guardados##modalBgPick");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500.0f, 420.0f));

        if (ImGui::BeginPopupModal("Fondos Guardados##modalBgPick", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("Selecciona una imagen de la galería de fondos:");
            ImGui::Dummy(ImVec2(0.0f, 6.0f));

            ImGui::BeginChild("##bg_local_scroll", ImVec2(-1.0f, 310.0f), true);

            std::string imgDir = ProyecThor::ImagesPath();
            if (std::filesystem::exists(imgDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(imgDir)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
                            std::string stem = entry.path().filename().string();
                            if (ImGui::Selectable(stem.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 26.0f))) {
                                m_AttachedImagePath = entry.path().string();
                                m_AttachedImageName = stem;
                                m_ShowLocalBgModal = false;
                                ImGui::CloseCurrentPopup();
                                break;
                            }
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("No se encontró la carpeta de imágenes.");
            }

            ImGui::EndChild();

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            if (ImGui::Button("Cerrar", ImVec2(100.0f, 28.0f))) {
                m_ShowLocalBgModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    DS::EndGlassPanel();
}

} // namespace ProyecThor::UI
