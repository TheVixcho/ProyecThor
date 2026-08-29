#include "Model3DPanel.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/ui/DesignSystem.h"
#include "frontend/ui/AppIcons.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

Model3DPanel::Model3DPanel() {
    m_Renderer     = std::make_unique<Model3DRenderer>();
    m_LiveRenderer = std::make_unique<Model3DRenderer>();

    // Carpeta predeterminada de modelos
    std::string assetsModels = ProyecThor::GetAssetsPath() + "/models";
    if (fs::exists(assetsModels)) {
        m_Folder = assetsModels;
    } else {
        std::error_code ec;
        fs::create_directories(assetsModels, ec);
        m_Folder = assetsModels;
    }

    RefreshFolder();

    // Cargar modelo inicial (Cruz 3D integrada)
    if (!m_Assets.empty()) {
        LoadAsset(m_Assets[0]);
    }
}

Model3DPanel::~Model3DPanel() {
    if (m_CurrentMesh.isGpuLoaded) {
        m_CurrentMesh.ReleaseGpu();
    }
}

void Model3DPanel::RefreshFolder() {
    m_Assets = Model3DLoader::ScanDirectory(m_Folder);
}

void Model3DPanel::LoadAsset(const Model3DAsset& asset) {
    m_CurrentAsset = asset;

    if (m_CurrentMesh.isGpuLoaded) {
        m_CurrentMesh.ReleaseGpu();
    }

    if (asset.isBuiltIn) {
        m_CurrentMesh = Model3DLoader::CreatePrimitive(asset.displayName);
    } else {
        Model3DFormat fmt;
        Model3DLoader::LoadModel(asset.path, m_CurrentMesh, fmt);
    }

    m_CurrentAsset.vertexCount   = m_CurrentMesh.vertices.size();
    m_CurrentAsset.triangleCount = m_CurrentMesh.indices.size() / 3;

    m_Config.ResetCamera();
}

void Model3DPanel::SetProjectingLive(bool live) {
    m_IsProjectingLive = live;
    Core::PresentationCore::Get().SetLive3DModelActive(live);
    if (live) {
        Core::PresentationCore::Get().SetLive3DModelTexture(
            (void*)(intptr_t)GetLiveTextureID());
    } else {
        Core::PresentationCore::Get().SetLive3DModelTexture(nullptr);
    }
}

unsigned int Model3DPanel::GetLiveTextureID() {
    if (!m_LiveRenderer) return 0;
    // Configuración para la proyección: fondo transparente para poder superponer
    Model3DRenderConfig liveConfig = m_Config;
    liveConfig.background = Model3DBackground::Transparent;
    liveConfig.showGrid   = false;

    return m_LiveRenderer->RenderToTexture(m_CurrentMesh, liveConfig, 1920, 1080);
}

void Model3DPanel::RenderTopBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

    // Icono y Título
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        AppIcons::DrawIcon_Cube3D(dl, { p0.x, p0.y + 2.0f }, 22.0f, DS::AccentColor);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::AccentColor);
        ImGui::TextUnformatted("RECURSOS Y MODELOS 3D");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 16.0f);

    // Botón de Proyección en Vivo Principal
    {
        bool isLive = m_IsProjectingLive;
        ImVec4 btnBg  = isLive ? ImVec4(0.85f, 0.15f, 0.20f, 0.35f) : ImVec4(0.12f, 0.65f, 0.35f, 0.25f);
        ImVec4 btnBdr = isLive ? ImVec4(0.95f, 0.25f, 0.30f, 0.90f) : ImVec4(0.20f, 0.85f, 0.45f, 0.80f);

        ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isLive ? ImVec4(0.95f, 0.25f, 0.30f, 0.50f) : ImVec4(0.20f, 0.85f, 0.45f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border, btnBdr);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.2f);

        const char* liveText = isLive ? "[ ⏹ DETENER PROYECCIÓN 3D ]" : "[ 🚀 PROYECTAR 3D A PANTALLA ]";
        if (ImGui::Button(liveText, ImVec2(0.0f, 26.0f))) {
            SetProjectingLive(!isLive);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine();

    // Filtro de búsqueda
    float searchW = 160.0f;
    float availW  = ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), availW - searchW - 10.0f));
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##search3d", "Buscar modelo...", m_SearchBuf, sizeof(m_SearchBuf));

    ImGui::PopStyleVar(2);
}

void Model3DPanel::RenderModelGallery(float w, float h) {
    ImGui::BeginChild("##modelGallery", ImVec2(w, h), true, ImGuiWindowFlags_None);

    // Barra de carpeta y recarga
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("↺ Recargar##3d", ImVec2(75.0f, 22.0f))) {
        RefreshFolder();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%d modelos", (int)m_Assets.size());
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::Separator();
    ImGui::Spacing();

    std::string filter = m_SearchBuf;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    for (int i = 0; i < (int)m_Assets.size(); ++i) {
        const auto& asset = m_Assets[i];

        if (!filter.empty()) {
            std::string nameLower = asset.displayName;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(filter) == std::string::npos) continue;
        }

        bool isSelected = (m_SelectedAssetIndex == i);

        ImGui::PushID(i);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float cardH = 46.0f;
        float cardW = ImGui::GetContentRegionAvail().x;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = isSelected ? IM_COL32(94, 107, 255, 55) : IM_COL32(255, 255, 255, 10);
        ImU32 border = isSelected ? IM_COL32(110, 140, 255, 200) : IM_COL32(255, 255, 255, 20);

        dl->AddRectFilled(pos, { pos.x + cardW, pos.y + cardH }, bg, 6.0f);
        dl->AddRect(pos, { pos.x + cardW, pos.y + cardH }, border, 6.0f, 0, isSelected ? 1.5f : 1.0f);

        // Icono de cubo 3D
        AppIcons::DrawIcon_Cube3D(dl, { pos.x + 8.0f, pos.y + 11.0f }, 24.0f, isSelected ? DS::AccentColor : DS::TextSecondary);

        // Nombre del modelo
        std::string dName = asset.displayName;
        if (dName.length() > 22) dName = dName.substr(0, 19) + "...";
        dl->AddText({ pos.x + 38.0f, pos.y + 6.0f }, isSelected ? IM_COL32_WHITE : DS::TextPrimary, dName.c_str());

        // Badge de formato y subtítulo
        const char* fmtTag = (asset.format == Model3DFormat::OBJ)  ? "OBJ" :
                             (asset.format == Model3DFormat::STL)  ? "STL" :
                             (asset.format == Model3DFormat::PLY)  ? "PLY" :
                             (asset.format == Model3DFormat::GLTF) ? "GLTF" :
                             (asset.format == Model3DFormat::GLB)  ? "GLB" : "3D";
        float tagW = (asset.format == Model3DFormat::GLTF) ? 36.0f : 28.0f;
        dl->AddRectFilled({ pos.x + 38.0f, pos.y + 24.0f }, { pos.x + 38.0f + tagW, pos.y + 38.0f }, IM_COL32(0, 0, 0, 120), 3.0f);
        dl->AddText({ pos.x + 42.0f, pos.y + 24.0f }, isSelected ? DS::AccentLight : DS::TextSecondary, fmtTag);

        if (asset.isBuiltIn) {
            dl->AddText({ pos.x + 44.0f + tagW, pos.y + 24.0f }, DS::TextHint, "Integrado");
        } else {
            dl->AddText({ pos.x + 44.0f + tagW, pos.y + 24.0f }, DS::TextHint, "Modelo 3D");
        }

        if (ImGui::InvisibleButton("##cardBtn", ImVec2(cardW, cardH))) {
            m_SelectedAssetIndex = i;
            LoadAsset(asset);
        }

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void Model3DPanel::RenderViewportControls(ImVec2 vpMin, ImVec2 vpMax) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Badge superior izquierdo con información del modelo
    {
        char stats[128];
        snprintf(stats, sizeof(stats), "%s | %zu Triángulos | %zu Vértices",
                 m_CurrentAsset.displayName.c_str(),
                 m_CurrentAsset.triangleCount,
                 m_CurrentAsset.vertexCount);

        ImVec2 sSz = ImGui::CalcTextSize(stats);
        ImVec2 b0  = { vpMin.x + 10.0f, vpMin.y + 10.0f };
        ImVec2 b1  = { b0.x + sSz.x + 16.0f, b0.y + sSz.y + 8.0f };

        dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 180), 5.0f);
        dl->AddRect(b0, b1, IM_COL32(255, 255, 255, 30), 5.0f);
        dl->AddText({ b0.x + 8.0f, b0.y + 4.0f }, IM_COL32_WHITE, stats);
    }

    // 2. Toolbar flotante inferior para controles 3D interactivos
    {
        float tbH = 34.0f;
        float tbW = std::min(vpMax.x - vpMin.x - 20.0f, 540.0f);
        ImVec2 tb0 = { vpMin.x + ((vpMax.x - vpMin.x) - tbW) * 0.5f, vpMax.y - tbH - 10.0f };
        ImVec2 tb1 = { tb0.x + tbW, tb0.y + tbH };

        dl->AddRectFilled(tb0, tb1, IM_COL32(18, 20, 26, 225), 7.0f);
        dl->AddRect(tb0, tb1, IM_COL32(255, 255, 255, 35), 7.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos({ tb0.x + 8.0f, tb0.y + 4.0f });

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));

        // Centrar cámara
        if (ImGui::Button("⊙ Centrar", ImVec2(0, 24.0f))) {
            m_Config.ResetCamera();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restablecer orientación y zoom de cámara");

        ImGui::SameLine();

        // Auto-rotar (Turntable)
        bool ar = m_Config.autoRotate;
        if (ar) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.45f, 0.95f, 0.60f));
        }
        if (ImGui::Button(ar ? "⟳ Rotando" : "⟳ Auto-rotar", ImVec2(0, 24.0f))) {
            m_Config.autoRotate = !m_Config.autoRotate;
        }
        if (ar) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Activar giro continuo automático 360°");

        ImGui::SameLine();

        // Modo de sombreado
        const char* shadLabels[] = { "Sombreado", "Malla (Wireframe)", "Sombreado + Malla" };
        int curShad = (m_Config.shading == Model3DShading::SmoothLit) ? 0 :
                      (m_Config.shading == Model3DShading::Wireframe) ? 1 : 2;

        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::Combo("##shadingMode", &curShad, shadLabels, 3)) {
            m_Config.shading = (curShad == 0) ? Model3DShading::SmoothLit :
                               (curShad == 1) ? Model3DShading::Wireframe : Model3DShading::ShadedWithEdges;
        }

        ImGui::SameLine();

        // Selector de Color
        if (ImGui::ColorButton("##3dColorBtn", m_Config.modelColor, ImGuiColorEditFlags_NoAlpha, ImVec2(24.0f, 24.0f))) {
            ImGui::OpenPopup("##3DColorPickerPopup");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color y material del modelo");

        if (ImGui::BeginPopup("##3DColorPickerPopup")) {
            ImGui::TextUnformatted("Color del Modelo 3D");
            ImGui::Separator();
            ImGui::ColorPicker4("##picker3d", (float*)&m_Config.modelColor, ImGuiColorEditFlags_NoAlpha);
            ImGui::Spacing();
            ImGui::TextUnformatted("Preajustes:");
            auto PresetBtn = [&](const char* name, ImVec4 col) {
                if (ImGui::ColorButton(name, col, ImGuiColorEditFlags_NoAlpha, ImVec2(20.0f, 20.0f)))
                    m_Config.modelColor = col;
                ImGui::SameLine();
            };
            PresetBtn("Blanco", { 0.95f, 0.95f, 0.98f, 1.0f });
            PresetBtn("Oro",    { 0.95f, 0.78f, 0.22f, 1.0f });
            PresetBtn("Plata",  { 0.75f, 0.80f, 0.88f, 1.0f });
            PresetBtn("Bronce", { 0.80f, 0.50f, 0.25f, 1.0f });
            PresetBtn("Azul",   { 0.25f, 0.65f, 1.00f, 1.0f });
            PresetBtn("Rojo",   { 0.90f, 0.25f, 0.25f, 1.0f });
            ImGui::NewLine();
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // Fondo transparente / Estudio
        bool isTrans = (m_Config.background == Model3DBackground::Transparent);
        if (ImGui::Button(isTrans ? "🏁 Transparente" : "⬛ Estudio", ImVec2(0, 24.0f))) {
            m_Config.background = isTrans ? Model3DBackground::DarkStudio : Model3DBackground::Transparent;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Alternar fondo transparente para superposición");

        ImGui::PopStyleVar(2);
    }
}

void Model3DPanel::Render3DViewport(float w, float h) {
    ImVec2 vpMin = ImGui::GetCursorScreenPos();
    ImVec2 vpMax = { vpMin.x + w, vpMin.y + h };

    // Actualizar auto-rotación
    m_Renderer->Update(ImGui::GetIO().DeltaTime, m_Config);

    // Renderizar escena 3D en textura OpenGL
    int texW = (int)w;
    int texH = (int)h;
    unsigned int texID = m_Renderer->RenderToTexture(m_CurrentMesh, m_Config, texW, texH);

    // Mostrar textura en ImGui
    if (texID != 0) {
        ImGui::Image((ImTextureID)(intptr_t)texID, ImVec2(w, h), ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::Dummy(ImVec2(w, h));
    }

    // Procesar eventos de interacción del ratón en el visor
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        bool leftDrag  = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        bool rightDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle);
        float wheel    = ImGui::GetIO().MouseWheel;

        m_Renderer->ProcessMouseInput(m_Config, delta, leftDrag, rightDrag, wheel);
    }

    // Renderizar controles y estadísticas flotantes encima del visor
    RenderViewportControls(vpMin, vpMax);

    // Si la proyección en vivo está activa, actualizar el renderizado en vivo
    if (m_IsProjectingLive) {
        Core::PresentationCore::Get().SetLive3DModelTexture(
            (void*)(intptr_t)GetLiveTextureID());
    }
}

void Model3DPanel::Render() {
    RenderTopBar();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;

    float galleryW = std::clamp(totalW * m_GalleryWidthFrac, 180.0f, 320.0f);
    float viewportW = totalW - galleryW - 10.0f;

    // Columna Izquierda: Galería de modelos 3D
    RenderModelGallery(galleryW, totalH);

    ImGui::SameLine(0.0f, 10.0f);

    // Columna Derecha: Visor 3D Interactivo
    Render3DViewport(viewportW, totalH);
}

} // namespace ProyecThor::UI

