#pragma once
#include "IPanel.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

    // Definición de categorías para la biblioteca
    enum class LibraryCategory { Songs = 1, Videos = 2, Images = 3, Bibles = 4 };

    class LibraryPanel : public IPanel {
    public:
        LibraryPanel();
        ~LibraryPanel() override = default;

        void Render() override;
        std::string GetName() const override { return "Biblioteca"; }

    private:
        // --- Métodos de Renderizado Interno ---
        void RenderCategoryButtons();
        void RenderSideList();
        void RenderSongEditor();
        void RefreshList();

        // --- Lógica de Archivos y Datos ---
        std::vector<std::string> LoadSongVerses(const std::string& title);
        void SaveSong(const std::string& title, const std::string& content);
        void CreateNewSong();
        void ImportFile();

        // --- Estado del Panel ---
        LibraryCategory m_CurrentCategory = LibraryCategory::Songs;
        std::vector<std::string> m_Items;
        int m_SelectedIndex = -1;
        char m_SearchBuffer[128] = "";

        // --- Estado del Editor (Modales) ---
        bool m_ShowSongEditor = false;
        char m_EditTitle[128] = "";
        char m_EditContent[4096] = ""; // Buffer para la letra de la canción
    };

} // namespace ProyecThor::UI