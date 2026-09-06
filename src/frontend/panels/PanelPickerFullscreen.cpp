#include "PanelPickerFullscreen.h"
#include "LibraryPanel.h"
#include "frontend/ui/UIManager.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/panels/biblio/LibraryIcons.h"
#include "backend/core/PresentationCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <imgui_internal.h>

namespace ProyecThor::UI {

namespace {

std::string ToLower(const std::string& str) {
    std::string out = str;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

}

PanelPickerFullscreen::PanelPickerFullscreen(LibraryPanel* library, UIManager* uiManager)
    : m_Library(library), m_UIManager(uiManager)
{
    InitItems();
}

void PanelPickerFullscreen::InitItems()
{
    m_Items.clear();

    m_Items.push_back({
        "multimedia",
        "Multimedia & Fondos",
        "Biblioteca de videos en loop, animaciones, imágenes y fondos",
        "CONTENIDO",
        PickerCategory::Content,
        IM_COL32(16, 185, 129, 255),
        Library::DrawIcon_Multimedia,
        [this]() {
            if (m_Library) m_Library->SelectCategory(LibraryCategory::Multimedia);
        },
        "multimedia videos imagenes fotos fondos loops backgrounds clips"
    });

    m_Items.push_back({
        "songs",
        "Canciones & Letras",
        "Gestor y proyector de letras, versos, estrofas y listas de canciones",
        "CONTENIDO",
        PickerCategory::Content,
        IM_COL32(34, 197, 94, 255),
        Library::DrawIcon_Music,
        [this]() {
            if (m_Library) m_Library->SelectCategory(LibraryCategory::Songs);
        },
        "canciones letras musica worship alabanza versos estrofas acordes playlists"
    });

    m_Items.push_back({
        "bibles",
        "Biblia Sagrada 2.0",
        "Búsqueda ultrarrápida de libros, capítulos y versículos bíblicos",
        "CONTENIDO",
        PickerCategory::Content,
        IM_COL32(20, 184, 166, 255),
        Library::DrawIcon_Cross,
        [this]() {
            if (m_Library) m_Library->SelectCategory(LibraryCategory::Bibles);
        },
        "biblia santas escrituras versiculos capitulos reina valera nvi palabra"
    });

    m_Items.push_back({
        "documents",
        "Documentos & Presentaciones",
        "Visor y renderizador de presentaciones PDF, PPT y diapositivas",
        "CONTENIDO",
        PickerCategory::Content,
        IM_COL32(6, 182, 212, 255),
        Library::DrawIcon_Document,
        [this]() {
            if (m_Library) m_Library->SelectCategory(LibraryCategory::Documents);
        },
        "documentos pdf diapositivas presentaciones ppt powerpoint slides"
    });

    m_Items.push_back({
        "render",
        "Conversor de Medios (Render)",
        "Conversión y compresión acelerada por hardware con FFmpeg",
        "HERRAMIENTAS",
        PickerCategory::Tools,
        IM_COL32(236, 72, 153, 255),
        AppIcons::DrawIcon_Swap,
        [this]() {
            if (m_Library) m_Library->SelectSideMode(LibrarySideMode::Render);
        },
        "render conversor video audio codec ffmpeg compresor exportar formato"
    });

    m_Items.push_back({
        "overlay",
        "Overlays Gráficos",
        "Diseñador de capas, tercios inferiores, zócalos y marquesinas",
        "HERRAMIENTAS",
        PickerCategory::Tools,
        IM_COL32(59, 130, 246, 255),
        AppIcons::DrawIcon_Overlay,
        [this]() {
            if (m_Library) m_Library->SelectSideMode(LibrarySideMode::Overlay);
        },
        "overlay capas zocalos lower thirds banners marquesina diseno graficos"
    });

    m_Items.push_back({
        "web",
        "Navegador Web & HTML",
        "Navegador en vivo y montaje de proyectos HTML locales sin servidor",
        "HERRAMIENTAS",
        PickerCategory::Tools,
        IM_COL32(99, 102, 241, 255),
        AppIcons::DrawIcon_Globe,
        [this]() {
            if (m_Library) m_Library->SelectSideMode(LibrarySideMode::Web);
        },
        "web html navegador url chrome browser offline local internet paginas"
    });

    m_Items.push_back({
        "model3d",
        "Modelos & Recursos 3D",
        "Visor 3D interactivo y catálogo de mallas OBJ, STL, PLY, GLTF y GLB",
        "HERRAMIENTAS",
        PickerCategory::Tools,
        IM_COL32(139, 92, 246, 255),
        AppIcons::DrawIcon_Cube3D,
        [this]() {
            if (m_Library) m_Library->SelectSideMode(LibrarySideMode::Model3D);
        },
        "3d modelos mallas obj stl gltf glb wireframe render tridimensional"
    });

    m_Items.push_back({
        "lab",
        "Laboratorio Matemático",
        "Graficador de funciones matemáticas, fórmulas y curvas en vivo",
        "HERRAMIENTAS",
        PickerCategory::Tools,
        IM_COL32(168, 85, 247, 255),
        AppIcons::DrawIcon_Formula,
        [this]() {
            if (m_Library) m_Library->SelectSideMode(LibrarySideMode::Lab);
        },
        "lab matematicas formulas funciones graficas calculo fx curvas"
    });
}

void PanelPickerFullscreen::Open()
{
    m_IsOpen = true;
    m_JustOpened = true;
    m_SearchBuffer[0] = '\0';
    m_SelectedCategory = PickerCategory::All;
}

void PanelPickerFullscreen::Close()
{
    m_IsOpen = false;
    if (m_UIManager) {
        m_UIManager->ExitFullscreenEditor();
    }
}

bool PanelPickerFullscreen::MatchesSearch(const PickerItem& item, const std::string& query) const
{
    if (query.empty()) return true;
    std::string q = ToLower(query);
    if (ToLower(item.title).find(q) != std::string::npos) return true;
    if (ToLower(item.subtitle).find(q) != std::string::npos) return true;
    if (ToLower(item.categoryLabel).find(q) != std::string::npos) return true;
    if (ToLower(item.keywords).find(q) != std::string::npos) return true;
    return false;
}

void PanelPickerFullscreen::Render()
{
    if (!m_IsOpen) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        Close();
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(11, 13, 19, 252));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (!ImGui::Begin("##PanelPickerFullscreen", nullptr, kFlags)) {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    dl->AddRectFilledMultiColor(
        winPos,
        ImVec2(winPos.x + winSize.x, winPos.y + 4.0f),
        IM_COL32(0, 180, 255, 255), IM_COL32(160, 50, 255, 255),
        IM_COL32(160, 50, 255, 120), IM_COL32(0, 180, 255, 120)
    );

    float topBarH = 74.0f;
    float padX = 28.0f;

    ImVec2 titlePos = ImVec2(winPos.x + padX, winPos.y + 16.0f);
    AppIcons::DrawIcon_Grid(dl, titlePos, 22.0f, IM_COL32(0, 200, 255, 255));
    dl->AddText(ImVec2(titlePos.x + 30.0f, titlePos.y + 2.0f), IM_COL32(240, 245, 255, 255), "PANELES & HERRAMIENTAS");
    dl->AddText(ImVec2(titlePos.x + 30.0f, titlePos.y + 22.0f), IM_COL32(130, 140, 160, 255), "F8 · Barra lateral izquierda");

    float searchW = std::clamp(winSize.x * 0.34f, 260.0f, 440.0f);
    ImVec2 searchPos = ImVec2(winPos.x + (winSize.x - searchW) * 0.5f, winPos.y + 18.0f);
    ImGui::SetCursorScreenPos(searchPos);
    ImGui::PushItemWidth(searchW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(22, 26, 38, 240));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(30, 36, 52, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(36, 44, 64, 255));

    if (m_JustOpened) {
        ImGui::SetKeyboardFocusHere();
        m_JustOpened = false;
    }

    ImGui::InputTextWithHint("##pickerSearch", "Buscar en la barra lateral...", m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopItemWidth();

    ImVec2 closeBtnPos = ImVec2(winPos.x + winSize.x - padX - 86.0f, winPos.y + 18.0f);
    ImGui::SetCursorScreenPos(closeBtnPos);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 32, 46, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(220, 50, 70, 240));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 40, 60, 255));

    if (ImGui::Button("✕ Cerrar", ImVec2(86.0f, 34.0f))) {
        Close();
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    dl->AddLine(
        ImVec2(winPos.x + padX, winPos.y + topBarH),
        ImVec2(winPos.x + winSize.x - padX, winPos.y + topBarH),
        IM_COL32(36, 42, 60, 160), 1.0f
    );

    float filterBarY = winPos.y + topBarH + 12.0f;
    ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, filterBarY));

    struct CatFilter {
        PickerCategory cat;
        const char* label;
    };
    static const CatFilter kFilters[] = {
        { PickerCategory::All,       "Todos" },
        { PickerCategory::Content,   "Contenido" },
        { PickerCategory::Tools,     "Herramientas" },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));

    for (int i = 0; i < 3; ++i) {
        bool selected = (m_SelectedCategory == kFilters[i].cat);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 168, 255, 230));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 180, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 150, 230, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(20, 24, 36, 180));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(32, 38, 56, 220));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 48, 70, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 172, 196, 240));
        }

        std::string btnId = std::string(kFilters[i].label) + "##catFilter";
        if (ImGui::Button(btnId.c_str())) {
            m_SelectedCategory = kFilters[i].cat;
        }

        ImGui::PopStyleColor(4);
        ImGui::SameLine();
    }
    ImGui::NewLine();
    ImGui::PopStyleVar(3);

    float cardsStartY = (filterBarY - winPos.y) + 44.0f;
    float contentW = winSize.x - padX * 2.0f;
    float contentH = winSize.y - cardsStartY - 16.0f;

    ImGui::SetCursorPos(ImVec2(padX, cardsStartY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##pickerCardsRegion", ImVec2(contentW, contentH), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PopStyleVar();

    std::vector<const PickerItem*> visibleItems;
    for (const auto& item : m_Items) {
        if (m_SelectedCategory != PickerCategory::All && item.category != m_SelectedCategory) {
            continue;
        }
        if (!MatchesSearch(item, m_SearchBuffer)) {
            continue;
        }
        visibleItems.push_back(&item);
    }

    if (visibleItems.empty()) {
        ImGui::Dummy(ImVec2(contentW, 60.0f));
        float textW = ImGui::CalcTextSize("No se encontraron herramientas coincidentes").x;
        ImGui::SetCursorPosX((contentW - textW) * 0.5f);
        ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "No se encontraron herramientas coincidentes");

        if (m_SearchBuffer[0] != '\0') {
            ImGui::Dummy(ImVec2(contentW, 12.0f));
            float btnW = 140.0f;
            ImGui::SetCursorPosX((contentW - btnW) * 0.5f);
            if (ImGui::Button("Limpiar búsqueda", ImVec2(btnW, 32.0f))) {
                m_SearchBuffer[0] = '\0';
            }
        }
    } else {
        const float kCardSpacingX = 16.0f;
        const float kCardSpacingY = 16.0f;
        float regionW = ImGui::GetContentRegionAvail().x;
        int cols = std::max(2, static_cast<int>((regionW + kCardSpacingX) / (320.0f + kCardSpacingX)));
        float cardW = std::floor((regionW - (cols - 1) * kCardSpacingX) / cols);
        float cardH = 138.0f;

        const PickerItem* clickedItem = nullptr;

        for (size_t i = 0; i < visibleItems.size(); ++i) {
            const auto* it = visibleItems[i];
            int colIdx = static_cast<int>(i % cols);

            std::string btnId = "##cardBtn_" + it->id;
            bool clicked = ImGui::InvisibleButton(btnId.c_str(), ImVec2(cardW, cardH));
            if (clicked) {
                clickedItem = it;
            }

            ImVec2 cardMin = ImGui::GetItemRectMin();
            ImVec2 cardMax = ImGui::GetItemRectMax();
            bool hovered = ImGui::IsItemHovered();

            ImU32 bgCol = hovered ? IM_COL32(28, 34, 50, 245) : IM_COL32(18, 22, 32, 220);
            ImU32 borderCol = hovered ? it->badgeColor : IM_COL32(40, 46, 66, 170);
            float borderThick = hovered ? 1.8f : 1.0f;

            dl->AddRectFilled(cardMin, cardMax, bgCol, 10.0f);
            dl->AddRect(cardMin, cardMax, borderCol, 10.0f, ImDrawFlags_RoundCornersAll, borderThick);

            ImVec2 badgeTextSz = ImGui::CalcTextSize(it->categoryLabel.c_str());
            float badgeW = badgeTextSz.x + 14.0f;
            float badgeH = 20.0f;
            ImVec2 badgeMin = ImVec2(cardMax.x - badgeW - 12.0f, cardMin.y + 12.0f);
            ImVec2 badgeMax = ImVec2(badgeMin.x + badgeW, badgeMin.y + badgeH);
            ImU32 badgeBg = (it->badgeColor & 0x00FFFFFF) | 0x33000000;

            dl->AddRectFilled(badgeMin, badgeMax, badgeBg, 4.0f);
            dl->AddText(ImVec2(badgeMin.x + 7.0f, badgeMin.y + 3.0f), it->badgeColor, it->categoryLabel.c_str());

            ImVec2 iconPos = ImVec2(cardMin.x + 14.0f, cardMin.y + 14.0f);
            float iconSz = 30.0f;
            it->drawIcon(dl, iconPos, iconSz, hovered ? it->badgeColor : IM_COL32(200, 215, 240, 240));

            ImVec2 titlePosCard = ImVec2(cardMin.x + 52.0f, cardMin.y + 18.0f);
            dl->AddText(titlePosCard, IM_COL32(250, 252, 255, 255), it->title.c_str());

            ImVec2 descPos = ImVec2(cardMin.x + 14.0f, cardMin.y + 54.0f);
            float wrapW = cardW - 28.0f;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), descPos, IM_COL32(145, 155, 175, 240), it->subtitle.c_str(), nullptr, wrapW);

            if (hovered) {
                const char* actionHint = "Abrir panel ➔";
                ImVec2 actSz = ImGui::CalcTextSize(actionHint);
                ImVec2 actPos = ImVec2(cardMax.x - actSz.x - 14.0f, cardMax.y - actSz.y - 10.0f);
                dl->AddText(actPos, it->badgeColor, actionHint);
            }

            if (colIdx < cols - 1 && i + 1 < visibleItems.size()) {
                ImGui::SameLine(0.0f, kCardSpacingX);
            } else if (i + 1 < visibleItems.size()) {
                ImGui::Dummy(ImVec2(0.0f, kCardSpacingY));
            }
        }

        if (clickedItem) {
            auto action = clickedItem->action;
            Close();
            if (action) {
                action();
            }
        }
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

}
