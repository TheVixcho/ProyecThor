#pragma once
#include "IPanel.h"
#include "../../PresentationCore.h"
#include <memory>
#include <string>
#include <vector>

namespace ProyecThor::UI {

    // --- ESTRUCTURAS DE DATOS PARA LA BIBLIA (XML) ---
    struct VerseData { 
        int number; 
        std::string text; 
    };
    struct ChapterData { 
        int number; 
        std::vector<VerseData> verses; 
    };
    struct BookData { 
        std::string name; 
        std::vector<ChapterData> chapters; 
    };
    struct BibleData { 
        std::string name; 
        std::vector<BookData> books; 
    };

    class PreviewPanel : public IPanel {
    public:
        PreviewPanel();
        ~PreviewPanel() override;

        void Render() override;
        std::string GetName() const override { return "Preview"; }

    private:
        // Motor de previsualización de media
        std::unique_ptr<Core::VLCVideoLayer> m_PreviewPlayer;
        std::string m_LastSelectedFile = "";
        bool m_IsPlayingPreview = true;
        std::string FormatTime(int64_t ms);

        // --- SISTEMA DE BIBLIAS ---
        BibleData m_CurrentBible;
        bool m_BibleLoaded = false;
        std::string m_LoadedBiblePath = "";
        int m_SelectedBook = 0;
        int m_SelectedChapter = 0;
        
        // Historial de proyección
        std::vector<std::string> m_VerseHistory;

        // Lector interno de XML (Estándar Zefania XML)
        void LoadXMLBible(const std::string& path);
    };

} // namespace ProyecThor::UI