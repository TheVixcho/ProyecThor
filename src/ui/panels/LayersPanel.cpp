#include "LayersPanel.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <windows.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    LayersPanel::LayersPanel() {
        std::error_code ec;
        fs::create_directories("assets/themes", ec);
        fs::create_directories("assets/backgrounds", ec); // Aseguramos que la carpeta exista
        
        m_VAlignment = 1; 
        m_TempVAlignment = 1;
        
        LoadThemeList();
        LoadBackgroundsList();
    }

    void LayersPanel::LoadThemeList() {
        m_AvailableThemes.clear();
        try {
            if (fs::exists("assets/themes")) {
                for (const auto& entry : fs::directory_iterator("assets/themes")) {
                    if (entry.path().extension() == ".theme") {
                        m_AvailableThemes.push_back(entry.path().stem().string());
                    }
                }
            }
        } catch (...) {}
    }

    void LayersPanel::LoadBackgroundsList() {
        m_AvailableBackgrounds.clear();
        try {
            if (fs::exists("assets/backgrounds")) {
                for (const auto& entry : fs::directory_iterator("assets/backgrounds")) {
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    // Filtramos solo videos (y fotos si lo deseas)
                    if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".jpg" || ext == ".png") {
                        m_AvailableBackgrounds.push_back(entry.path().string());
                    }
                }
            }
        } catch (...) {}
    }

    void LayersPanel::SaveTheme(const std::string& name, bool isFromEditor) {
        if (name.empty()) return;
        
        std::error_code ec;
        fs::create_directories("assets/themes", ec); 

        std::ofstream file("assets/themes/" + name + ".theme");
        if (file.is_open()) {
            if (isFromEditor) {
                file << "textColor=" << m_TempTextColor[0] << "," << m_TempTextColor[1] << "," << m_TempTextColor[2] << "," << m_TempTextColor[3] << "\n";
                file << "textSize=" << m_TempTextSize << "\n";
                file << "textAlign=" << m_TempTextAlignment << "\n";
                file << "vAlign=" << m_TempVAlignment << "\n"; 
                file << "margins=" << m_TempMargins[0] << "," << m_TempMargins[1] << "," << m_TempMargins[2] << "," << m_TempMargins[3] << "\n";
                file << "autoScale=" << (m_TempAutoScale ? 1 : 0) << "\n";
            } else {
                file << "textColor=" << m_TextColor[0] << "," << m_TextColor[1] << "," << m_TextColor[2] << "," << m_TextColor[3] << "\n";
                file << "textSize=" << m_TextSize << "\n";
                file << "textAlign=" << m_TextAlignment << "\n";
                file << "vAlign=" << m_VAlignment << "\n"; 
                file << "margins=" << m_Margins[0] << "," << m_Margins[1] << "," << m_Margins[2] << "," << m_Margins[3] << "\n";
                file << "autoScale=" << (m_AutoScale ? 1 : 0) << "\n";
            }
            file.close();
        }
        LoadThemeList();
    }

    void LayersPanel::ApplyTheme(const std::string& name) {
        std::ifstream file("assets/themes/" + name + ".theme");
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream is_line(line);
            std::string key;
            if (std::getline(is_line, key, '=')) {
                std::string value;
                if (std::getline(is_line, value)) {
                    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
                    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());

                    if (key == "textSize") m_TextSize = std::stof(value);
                    else if (key == "textAlign") m_TextAlignment = std::stoi(value);
                    else if (key == "vAlign") m_VAlignment = std::stoi(value);
                    else if (key == "autoScale") m_AutoScale = std::stoi(value) != 0;
                    else if (key == "textColor") sscanf(value.c_str(), "%f,%f,%f,%f", &m_TextColor[0], &m_TextColor[1], &m_TextColor[2], &m_TextColor[3]);
                    else if (key == "margins") sscanf(value.c_str(), "%f,%f,%f,%f", &m_Margins[0], &m_Margins[1], &m_Margins[2], &m_Margins[3]);
                }
            }
        }

        m_SelectedTheme = name;
        Core::PresentationCore::Get().UpdateTextStyle(m_TextSize, m_TextColor, m_TextAlignment, m_Margins, m_AutoScale);
    }

    void LayersPanel::DeleteTheme(const std::string& name) {
        fs::remove("assets/themes/" + name + ".theme");
        LoadThemeList();
        if (m_SelectedTheme == name) m_SelectedTheme = "";
    }

    void LayersPanel::LoadThemeToEditor(const std::string& name) {
        std::ifstream file("assets/themes/" + name + ".theme");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream is_line(line);
                std::string key, value;
                if (std::getline(is_line, key, '=') && std::getline(is_line, value)) {
                    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
                    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());

                    if (key == "textSize") m_TempTextSize = std::stof(value);
                    else if (key == "textAlign") m_TempTextAlignment = std::stoi(value);
                    else if (key == "vAlign") m_TempVAlignment = std::stoi(value);
                    else if (key == "autoScale") m_TempAutoScale = std::stoi(value) != 0;
                    else if (key == "textColor") sscanf(value.c_str(), "%f,%f,%f,%f", &m_TempTextColor[0], &m_TempTextColor[1], &m_TempTextColor[2], &m_TempTextColor[3]);
                    else if (key == "margins") sscanf(value.c_str(), "%f,%f,%f,%f", &m_TempMargins[0], &m_TempMargins[1], &m_TempMargins[2], &m_TempMargins[3]);
                }
            }
        }
    }

    void LayersPanel::RenderBackgroundsGrid() {
        ImGui::Spacing();
        if (ImGui::Button("Recargar Carpeta de Fondos", ImVec2(-1, 35))) {
            LoadBackgroundsList();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_AvailableBackgrounds.empty()) {
            ImGui::TextDisabled("No hay videos en la carpeta 'assets/backgrounds'.");
            return;
        }

        float thumbnailWidth = 160.0f;
        float thumbnailHeight = 90.0f;
        float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        ImGuiStyle& style = ImGui::GetStyle();

        for (size_t i = 0; i < m_AvailableBackgrounds.size(); i++) {
            ImGui::PushID((int)i);
            
            std::string fullPath = m_AvailableBackgrounds[i];
            std::string fileName = fs::path(fullPath).filename().string();

            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            
            // Botón invisible que actúa como thumbnail
            if (ImGui::Button("##bgBtn", ImVec2(thumbnailWidth, thumbnailHeight))) {
                Core::PresentationCore::Get().SetBackgroundMedia(fullPath, true);
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Dibujar fondo de thumbnail (simulando miniatura)
            drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + thumbnailWidth, cursorPos.y + thumbnailHeight), IM_COL32(40, 40, 45, 255), 4.0f);
            
            // Ícono de película en el centro
            drawList->AddText(ImGui::GetFont(), 30.0f, ImVec2(cursorPos.x + (thumbnailWidth - 30)/2, cursorPos.y + 15), IM_COL32(255, 255, 255, 100), "🎬");
            
            // Fondo oscuro para el texto
            drawList->AddRectFilled(
                ImVec2(cursorPos.x, cursorPos.y + thumbnailHeight - 25), 
                ImVec2(cursorPos.x + thumbnailWidth, cursorPos.y + thumbnailHeight), 
                IM_COL32(0, 0, 0, 200), 4.0f, ImDrawFlags_RoundCornersBottom);
            
            // Nombre del archivo truncado si es muy largo
            std::string displayTxt = fileName;
            if (displayTxt.length() > 20) displayTxt = displayTxt.substr(0, 17) + "...";
            
            ImVec2 labelSize = ImGui::CalcTextSize(displayTxt.c_str());
            drawList->AddText(
                ImVec2(cursorPos.x + (thumbnailWidth - labelSize.x)*0.5f, cursorPos.y + thumbnailHeight - 20), 
                IM_COL32(255, 255, 255, 255), displayTxt.c_str());

            float lastButtonX = ImGui::GetItemRectMax().x;
            float nextButtonX = lastButtonX + style.ItemSpacing.x + thumbnailWidth;
            if (i + 1 < m_AvailableBackgrounds.size() && nextButtonX < windowVisibleX) ImGui::SameLine();

            ImGui::PopID();
        }
    }

    void LayersPanel::RenderThemeGrid() {
        float thumbnailWidth = 140.0f;
        float thumbnailHeight = 80.0f;
        float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        ImGuiStyle& style = ImGui::GetStyle();
        
        ImGui::Spacing();
        if (ImGui::Button("Crear Nuevo Estilo de Letra", ImVec2(-1, 35))) {
            m_IsEditingExisting = false;
            memset(m_EditThemeName, 0, sizeof(m_EditThemeName));
            
            memcpy(m_TempTextColor, m_TextColor, sizeof(m_TextColor));
            memcpy(m_TempMargins, m_Margins, sizeof(m_Margins));
            m_TempTextSize = m_TextSize;
            m_TempTextAlignment = m_TextAlignment;
            m_TempVAlignment = m_VAlignment; 
            m_TempAutoScale = m_AutoScale;
            
            m_ShowThemeEditor = true;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (size_t i = 0; i < m_AvailableThemes.size(); i++) {
            ImGui::PushID((int)i);
            
            const std::string& themeName = m_AvailableThemes[i];
            bool isSelected = (m_SelectedTheme == themeName);

            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            
            if (isSelected) {
                ImGui::GetWindowDrawList()->AddRect(
                    ImVec2(cursorPos.x - 2, cursorPos.y - 2), 
                    ImVec2(cursorPos.x + thumbnailWidth + 2, cursorPos.y + thumbnailHeight + 2), 
                    IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
            }

            if (ImGui::Button("##themeBtn", ImVec2(thumbnailWidth, thumbnailHeight))) {
                ApplyTheme(themeName);
            }

            if (ImGui::BeginPopupContextItem("ThemeContextMenu")) {
                ImGui::TextDisabled("Estilo: %s", themeName.c_str());
                ImGui::Separator();
                if (ImGui::Selectable("✏️ Editar")) {
                    m_IsEditingExisting = true;
                    strcpy_s(m_EditThemeName, themeName.c_str());
                    LoadThemeToEditor(themeName);
                    m_ShowThemeEditor = true;
                }
                if (ImGui::Selectable("🗑️ Eliminar")) {
                    DeleteTheme(themeName);
                }
                ImGui::EndPopup();
            }

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 textPos = ImVec2(cursorPos.x + 10, cursorPos.y + 10);
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "Aa Bb Cc");
            drawList->AddText(ImVec2(textPos.x, textPos.y + 15), IM_COL32(200, 200, 200, 255), "1 2 3 4 5");
            
            ImVec2 labelSize = ImGui::CalcTextSize(themeName.c_str());
            drawList->AddRectFilled(
                ImVec2(cursorPos.x, cursorPos.y + thumbnailHeight - 20), 
                ImVec2(cursorPos.x + thumbnailWidth, cursorPos.y + thumbnailHeight), 
                IM_COL32(0, 0, 0, 200));
            drawList->AddText(
                ImVec2(cursorPos.x + (thumbnailWidth - labelSize.x)*0.5f, cursorPos.y + thumbnailHeight - 18), 
                IM_COL32(255, 255, 255, 255), themeName.c_str());

            float lastButtonX = ImGui::GetItemRectMax().x;
            float nextButtonX = lastButtonX + style.ItemSpacing.x + thumbnailWidth;
            if (i + 1 < m_AvailableThemes.size() && nextButtonX < windowVisibleX) ImGui::SameLine();

            ImGui::PopID();
        }
    }

    void LayersPanel::RenderThemeEditorModal() {
        if (m_ShowThemeEditor) ImGui::OpenPopup("Editor de Estilo de Texto");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(1000, 750)); 

        if (ImGui::BeginPopupModal("Editor de Estilo de Texto", &m_ShowThemeEditor, ImGuiWindowFlags_NoSavedSettings)) {
            
            ImGui::Spacing();
            ImGui::Columns(2, "FormatCols", false);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Tipografía");
            ImGui::ColorEdit4("Color de Letra", m_TempTextColor);
            ImGui::SliderFloat("Tamaño Inicial", &m_TempTextSize, 20.0f, 300.0f, "%.0f px");
            ImGui::Checkbox("Auto-Reducir tamaño si el texto es largo", &m_TempAutoScale);
            
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Alineación Horizontal");
            ImGui::RadioButton("Izquierda", &m_TempTextAlignment, 0); ImGui::SameLine();
            ImGui::RadioButton("Centro", &m_TempTextAlignment, 1); ImGui::SameLine();
            ImGui::RadioButton("Derecha", &m_TempTextAlignment, 2);

            ImGui::Spacing();
            
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Alineación Vertical");
            ImGui::RadioButton("Arriba", &m_TempVAlignment, 0); ImGui::SameLine();
            ImGui::RadioButton("Medio", &m_TempVAlignment, 1); ImGui::SameLine();
            ImGui::RadioButton("Abajo", &m_TempVAlignment, 2);
            
            ImGui::NextColumn();
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Zona Segura (Márgenes)");
            ImGui::TextWrapped("Define el cuadro donde el texto está permitido (Referencia 1920x1080).");
            ImGui::DragFloat4("Izq/Arr/Der/Aba", m_TempMargins, 2.0f, 0.0f, 900.0f, "%.0f px");
            
            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::Spacing();

            // Preview
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            float availX = ImGui::GetContentRegionAvail().x;
            
            if (availX > 50.0f) { 
                ImVec2 canvasSize = ImVec2(availX, availX * 0.5625f); 
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                
                // Fondo negro falso para el preview
                drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 30, 30, 255));

                float screenScale = canvasSize.x / 1920.0f;
                float marginL = m_TempMargins[0] * screenScale;
                float marginT = m_TempMargins[1] * screenScale;
                float marginR = m_TempMargins[2] * screenScale;
                float marginB = m_TempMargins[3] * screenScale;

                float boxX = canvasPos.x + marginL;
                float boxY = canvasPos.y + marginT;
                float boxW = std::max(10.0f, canvasSize.x - marginL - marginR); 
                float boxH = std::max(10.0f, canvasSize.y - marginT - marginB);

                drawList->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), IM_COL32(255, 255, 0, 150), 0.0f, 0, 2.0f);

                const char* testText = "Esto es un texto de prueba";
                float targetFontSize = m_TempTextSize * screenScale;
                
                if (m_TempAutoScale) {
                    while (targetFontSize > 10.0f) {
                        ImVec2 tSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, testText);
                        if (tSize.y <= boxH) break; 
                        targetFontSize -= 1.0f; 
                    }
                }
                
                ImVec2 finalBlockSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, testText);

                float textX = boxX;
                if (m_TempTextAlignment == 1) textX += (boxW - finalBlockSize.x) * 0.5f; 
                else if (m_TempTextAlignment == 2) textX += (boxW - finalBlockSize.x); 

                float textY = boxY;
                if (m_TempVAlignment == 1) textY += (boxH - finalBlockSize.y) * 0.5f; 
                else if (m_TempVAlignment == 2) textY += (boxH - finalBlockSize.y); 

                ImVec2 textPos = ImVec2(textX, textY);
                ImU32 textColor = IM_COL32(m_TempTextColor[0]*255, m_TempTextColor[1]*255, m_TempTextColor[2]*255, m_TempTextColor[3]*255);
                ImU32 shadowColor = IM_COL32(0, 0, 0, 220);

                drawList->AddText(ImGui::GetFont(), targetFontSize, ImVec2(textPos.x + 2, textPos.y + 2), shadowColor, testText, NULL, boxW);
                drawList->AddText(ImGui::GetFont(), targetFontSize, textPos, textColor, testText, NULL, boxW);

                ImGui::Dummy(canvasSize); 
            }

            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 45); 
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Nombre del Estilo:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(250);
            ImGui::InputText("##themeNameInput", m_EditThemeName, IM_ARRAYSIZE(m_EditThemeName));
            
            ImGui::SameLine(ImGui::GetWindowWidth() - 220);
            if (ImGui::Button("Cancelar", ImVec2(100, 30))) {
                m_ShowThemeEditor = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Guardar", ImVec2(100, 30))) {
                if (strlen(m_EditThemeName) > 0) {
                    SaveTheme(m_EditThemeName, true); 
                    ApplyTheme(m_EditThemeName); 
                    m_ShowThemeEditor = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopStyleColor();

            ImGui::EndPopup();
        }
    }

    void LayersPanel::Render() {
        ImGui::Begin("Inspector de Capas y Fondos");

        if (ImGui::BeginTabBar("InspectorTabs")) {
            
            if (ImGui::BeginTabItem("Fondos Animados")) {
                RenderBackgroundsGrid();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Estilos de Letra")) {
                RenderThemeGrid();
                
                ImGui::Spacing();
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Ajustes Manuales (Sin Guardar)")) {
                    bool textChanged = false;
                    textChanged |= ImGui::ColorEdit4("Color de Letra", m_TextColor);
                    textChanged |= ImGui::SliderFloat("Tamaño", &m_TextSize, 20.0f, 300.0f, "%.0f px");

                    ImGui::Columns(2, "AlignCols", false);
                    ImGui::Text("Horizontal:");
                    textChanged |= ImGui::RadioButton("Izquierda", &m_TextAlignment, 0); ImGui::SameLine();
                    textChanged |= ImGui::RadioButton("Centro", &m_TextAlignment, 1); ImGui::SameLine();
                    textChanged |= ImGui::RadioButton("Der", &m_TextAlignment, 2);
                    
                    ImGui::NextColumn();
                    ImGui::Text("Vertical:");
                    textChanged |= ImGui::RadioButton("Arriba", &m_VAlignment, 0); ImGui::SameLine();
                    textChanged |= ImGui::RadioButton("Medio", &m_VAlignment, 1); ImGui::SameLine();
                    textChanged |= ImGui::RadioButton("Abajo", &m_VAlignment, 2);
                    ImGui::Columns(1);

                    if (textChanged) {
                        Core::PresentationCore::Get().UpdateTextStyle(m_TextSize, m_TextColor, m_TextAlignment, m_Margins, m_AutoScale);
                    }
                } 
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }

        RenderThemeEditorModal();
        ImGui::End();
    }

} // namespace ProyecThor::UI