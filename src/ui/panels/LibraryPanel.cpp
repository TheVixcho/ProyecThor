#include "LibraryPanel.h"
#include <windows.h>
#include "../../PresentationCore.h"
#include <imgui.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator> 

namespace fs = std::filesystem;

// Espacio de nombres anónimo para variables internas de este archivo.
// Esto permite forzar la actualización de la lista sin tener que editar tu archivo .h
namespace {
    bool g_ForceListUpdate = true;
}

namespace ProyecThor::UI {

    LibraryPanel::LibraryPanel() {
        try {
            fs::create_directories("assets/songs");
            fs::create_directories("assets/videos");
            fs::create_directories("assets/images");
            fs::create_directories("assets/bibles");
        } catch (const std::exception& e) {
            std::cerr << "Advertencia IO: " << e.what() << '\n';
        }
        RefreshList();
    }

    void LibraryPanel::RefreshList() {
        m_Items.clear();
        std::string path = "";

        switch (m_CurrentCategory) {
            case LibraryCategory::Songs:  path = "assets/songs"; break;
            case LibraryCategory::Videos: path = "assets/videos"; break;
            case LibraryCategory::Images: path = "assets/images"; break;
            case LibraryCategory::Bibles: path = "assets/bibles"; break;
        }

        try {
            if (fs::exists(path)) {
                for (const auto& entry : fs::directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        m_Items.push_back(entry.path().filename().string()); 
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error IO: " << e.what() << std::endl;
        }

        if (m_Items.empty() && m_CurrentCategory == LibraryCategory::Songs) {
            m_Items = { "Cuan_Grande_es_El.txt", "Gracia_Sublime.txt" }; 
        }

        // ¡CLAVE DE LA SOLUCIÓN!
        // Al escanear el disco duro, le avisamos a la interfaz que regenere su caché visual.
        g_ForceListUpdate = true;
    }

    void LibraryPanel::Render() {
        // NOMBRE EXACTO PARA EL DOCKING (No cambiar)
        ImGui::Begin("Biblioteca"); 

        RenderCategoryButtons();

        ImGui::Separator();
        ImGui::Spacing();

        RenderSideList(); 
        RenderSongEditor();

        ImGui::End();
    }

    void LibraryPanel::RenderCategoryButtons() {
        auto ButtonStyle = [this](LibraryCategory cat, const char* label) {
            bool isCurrent = (m_CurrentCategory == cat);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
            }
            
            if (ImGui::Button(label, ImVec2(ImGui::GetContentRegionAvail().x / 4.1f, 30))) {
                m_CurrentCategory = cat;
                m_SelectedIndex = -1;
                // Al llamar a RefreshList, internamente se cambia g_ForceListUpdate a true
                RefreshList();
            }

            if (isCurrent) ImGui::PopStyleColor(2);
            ImGui::SameLine();
        };

        ButtonStyle(LibraryCategory::Songs, "Canciones");
        ButtonStyle(LibraryCategory::Videos, "Videos");
        ButtonStyle(LibraryCategory::Images, "Imagenes");
        ButtonStyle(LibraryCategory::Bibles, "Biblia");
        ImGui::NewLine();
    }

    void LibraryPanel::RenderSideList() {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##search", "Buscar por nombre o letra...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));
        ImGui::Spacing();

        // Sistema de filtrado en caché para no matar el rendimiento leyendo archivos cada frame
        static std::vector<std::string> filteredItems;
        static std::string lastSearch = "";
        std::string currentSearch = m_SearchBuffer;
        
        // Convertir búsqueda a minúsculas
        std::transform(currentSearch.begin(), currentSearch.end(), currentSearch.begin(), ::tolower);

        // Si la búsqueda cambia O si la lista base cambió (pestaña nueva, importación, actualización)
        if (currentSearch != lastSearch || g_ForceListUpdate) {
            filteredItems.clear();
            for (const auto& item : m_Items) {
                std::string itemLower = item;
                std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), ::tolower);
                
                bool match = false;
                if (currentSearch.empty() || itemLower.find(currentSearch) != std::string::npos) {
                    match = true;
                } else if (m_CurrentCategory == LibraryCategory::Songs) {
                    // Buscar dentro del contenido de la letra
                    auto verses = LoadSongVerses(item);
                    for (const auto& v : verses) {
                        std::string vLower = v;
                        std::transform(vLower.begin(), vLower.end(), vLower.begin(), ::tolower);
                        if (vLower.find(currentSearch) != std::string::npos) {
                            match = true; 
                            break;
                        }
                    }
                }
                
                if (match) filteredItems.push_back(item);
            }
            lastSearch = currentSearch;
            g_ForceListUpdate = false; // La caché ya está al día, apagamos la bandera
        }

        ImVec2 listSize = ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.5f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f)); 
        
        if (ImGui::BeginChild("ListChild", listSize, true)) {
            for (int n = 0; n < (int)filteredItems.size(); n++) {
                // Encontrar el índice original para mantener la selección coherente
                auto it = std::find(m_Items.begin(), m_Items.end(), filteredItems[n]);
                int originalIndex = (it != m_Items.end()) ? std::distance(m_Items.begin(), it) : -1;

                const bool is_selected = (m_SelectedIndex == originalIndex);
                fs::path p(filteredItems[n]);
                std::string displayName = p.stem().string();

                if (n % 2 == 0) {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        ImGui::GetCursorScreenPos(),
                        ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x, ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeightWithSpacing()),
                        IM_COL32(255, 255, 255, 7)
                    );
                }

                if (ImGui::Selectable(displayName.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_SelectedIndex = originalIndex;
                    Core::LibrarySelection sel;
                    sel.title = filteredItems[n]; 
                    sel.type = (Core::ItemType)m_CurrentCategory;

                    if (m_CurrentCategory == LibraryCategory::Songs) {
                        sel.contentData = LoadSongVerses(filteredItems[n]);
                    }

                    Core::PresentationCore::Get().SetSelection(sel);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Nuevo", ImVec2(80, 30))) { CreateNewSong(); }
        ImGui::SameLine();
        
        // El forzado de caché ya no es necesario aquí, RefreshList() se encarga de todo.
        if (ImGui::Button("Importar", ImVec2(80, 30))) { ImportFile(); } 
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Eliminar", ImVec2(80, 30))) { 
            if (m_SelectedIndex >= 0 && m_SelectedIndex < m_Items.size()) {
                std::string folder = (m_CurrentCategory == LibraryCategory::Songs) ? "assets/songs/" :
                                     (m_CurrentCategory == LibraryCategory::Videos) ? "assets/videos/" :
                                     (m_CurrentCategory == LibraryCategory::Images) ? "assets/images/" : "assets/bibles/";
                try {
                    fs::remove(folder + m_Items[m_SelectedIndex]);
                } catch (const std::exception& e) {
                    std::cerr << "Error al eliminar el archivo (posiblemente esté en uso): " << e.what() << "\n";
                }
                RefreshList();
                m_SelectedIndex = -1;
            }
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("Actualizar", ImVec2(-1, 30))) { 
            RefreshList(); 
        }
    }

    std::vector<std::string> LibraryPanel::LoadSongVerses(const std::string& filename) {
        std::vector<std::string> verses;
        std::ifstream file("assets/songs/" + filename); 
        
        if (file.is_open()) {
            std::string line, currentVerse;
            while (std::getline(file, line)) {
                if (line.empty()) {
                    if (!currentVerse.empty()) {
                        verses.push_back(currentVerse);
                        currentVerse.clear();
                    }
                } else {
                    currentVerse += line + "\n";
                }
            }
            if (!currentVerse.empty()) verses.push_back(currentVerse);
        } else {
            verses = { "Error: No se pudo leer el archivo." };
        }
        return verses;
    }

    void LibraryPanel::RenderSongEditor() {
        if (m_ShowSongEditor) ImGui::OpenPopup("Editor de Letras");

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(700, 550));

        if (ImGui::BeginPopupModal("Editor de Letras", &m_ShowSongEditor, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("Nombre del Archivo (Sin extensión):");
            ImGui::InputText("##editTitle", m_EditTitle, IM_ARRAYSIZE(m_EditTitle));
            ImGui::Spacing();
            ImGui::Text("Letra (Doble salto de línea separa estrofas):");
            ImGui::InputTextMultiline("##editContent", m_EditContent, IM_ARRAYSIZE(m_EditContent), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 22));

            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Guardar", ImVec2(120, 35))) {
                SaveSong(m_EditTitle, m_EditContent);
                m_ShowSongEditor = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(120, 35))) {
                m_ShowSongEditor = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void LibraryPanel::CreateNewSong() {
        memset(m_EditTitle, 0, sizeof(m_EditTitle));
        memset(m_EditContent, 0, sizeof(m_EditContent));
        m_ShowSongEditor = true;
    }
    
    void LibraryPanel::SaveSong(const std::string& title, const std::string& content) {
        if (title.empty()) return;
        std::string filename = title;
        if (filename.find(".txt") == std::string::npos) filename += ".txt";

        std::ofstream file("assets/songs/" + filename);
        if (file.is_open()) {
            file << content;
            file.close();
            RefreshList();
        }
    }

    void LibraryPanel::ImportFile() {
        char filename[MAX_PATH] = "";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        
        if (m_CurrentCategory == LibraryCategory::Videos) ofn.lpstrFilter = "Videos\0*.mp4;*.mkv;*.avi;*.mov\0Todos\0*.*\0";
        else if (m_CurrentCategory == LibraryCategory::Images) ofn.lpstrFilter = "Imagenes\0*.jpg;*.png;*.jpeg\0Todos\0*.*\0";
        else if (m_CurrentCategory == LibraryCategory::Songs) ofn.lpstrFilter = "Textos\0*.txt\0Todos\0*.*\0";
        else ofn.lpstrFilter = "Todos los Archivos\0*.*\0";
        
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

        if (GetOpenFileNameA(&ofn)) {
            try {
                fs::path src(filename);
                std::string destFolder = "";
                switch (m_CurrentCategory) {
                    case LibraryCategory::Songs:  destFolder = "assets/songs"; break;
                    case LibraryCategory::Videos: destFolder = "assets/videos"; break;
                    case LibraryCategory::Images: destFolder = "assets/images"; break;
                    case LibraryCategory::Bibles: destFolder = "assets/bibles"; break;
                }
                fs::copy(src, destFolder + "/" + src.filename().string(), fs::copy_options::overwrite_existing);
                RefreshList();
            } catch (const std::exception& e) {
                std::cerr << "Error al importar: " << e.what() << "\n";
            }
        }
    }

} // namespace ProyecThor::UI