#include "SettingsPanel.h"
#include "SettingsManager.h"
#include <imgui.h>

namespace ProyecThor::UI::Settings {

    struct UIStrings {
        const char* appTitle;
        const char* library;
        const char* preview;
        const char* inspector;
        const char* control;
        const char* search;
        const char* clearHistory;
        const char* bibleLabel;
        const char* books;
        const char* chapters;
        const char* preferences;
        const char* save;
        const char* reset;
    };

    static const UIStrings k_Strings[] = {
        // Spanish
        {
            "ProyecThor",
            "Biblioteca", "Previsualización", "Inspector de Capas y Fondos", "Control",
            "Buscar (Libro Abreviado + 1:1)", "Limpiar Historial",
            "Biblia:", "Libros", "Capítulos",
            "Preferencias", "Guardar", "Restablecer"
        },
        // English
        {
            "ProyecThor",
            "Library", "Preview", "Layers & Backgrounds Inspector", "Control",
            "Search (Abbrev. Book + 1:1)", "Clear History",
            "Bible:", "Books", "Chapters",
            "Preferences", "Save", "Reset"
        },
        // Portuguese
        {
            "ProyecThor",
            "Biblioteca", "Pré-visualização", "Inspetor de Camadas e Fundos", "Controlo",
            "Pesquisar (Livro Abrev. + 1:1)", "Limpar Histórico",
            "Bíblia:", "Livros", "Capítulos",
            "Preferências", "Guardar", "Repor"
        },
    };

    // Definida DENTRO del namespace — sin calificadores externos
    static const UIStrings& GetUIStrings() {
        int idx = static_cast<int>(
            ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language
        );
        if (idx < 0 || idx >= 3) idx = 0;
        return k_Strings[idx];
    }

    void SettingsPanel::RenderCategoryLanguage() {
        auto& g = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;

        ImGui::TextDisabled("Selecciona el idioma de la interfaz de usuario.");
        ImGui::Spacing();

        SectionTitle("Idioma de la Interfaz");

        int langIdx   = static_cast<int>(g.language);
        int langCount = static_cast<int>(ProyecThor::Settings::Language::COUNT);

        for (int i = 0; i < langCount; i++) {
            bool selected = (langIdx == i);
            auto lang = static_cast<ProyecThor::Settings::Language>(i);

            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.886f, 0.753f, 0.408f, 1.0f));

            if (ImGui::RadioButton(ProyecThor::Settings::LanguageName(lang), selected))
                g.language = lang;

            if (selected)
                ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.53f, 1.0f));
        ImGui::TextWrapped(
            "El cambio de idioma se aplica al guardar y reiniciar la aplicación.\n"
            "Algunas cadenas de texto pueden requerir reinicio completo."
        );
        ImGui::PopStyleColor();

        ImGui::Spacing();
        SectionTitle("Vista Previa de Cadenas");

        const auto& str = GetUIStrings();
        ImGui::Columns(2, "langpreview", false);
        ImGui::SetColumnWidth(0, 180);

        auto Row = [](const char* key, const char* val) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.48f, 1.0f));
            ImGui::TextUnformatted(key);
            ImGui::PopStyleColor();
            ImGui::NextColumn();
            ImGui::TextUnformatted(val);
            ImGui::NextColumn();
        };

        Row("Biblioteca:",  str.library);
        Row("Buscar:",      str.search);
        Row("Libros:",      str.books);
        Row("Capítulos:",   str.chapters);
        Row("Guardar:",     str.save);
        Row("Restablecer:", str.reset);

        ImGui::Columns(1);
    }

} // namespace ProyecThor::UI::Settings
