#include "BibleView.h"
#include "../../../PresentationCore.h" // Ajustar ruta según tu proyecto
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    void BibleView::LoadXMLBible(const std::string& path) {
        m_CurrentBible = BibleData();
        m_BibleLoaded = false;
        m_LoadedBiblePath = path;

        const char* bookNames[] = {
            "Génesis", "Éxodo", "Levítico", "Números", "Deuteronomio",
            "Josué", "Jueces", "Rut", "1 Samuel", "2 Samuel", "1 Reyes", "2 Reyes",
            "1 Crónicas", "2 Crónicas", "Esdras", "Nehemías", "Ester", "Job", "Salmos",
            "Proverbios", "Eclesiastés", "Cantares", "Isaías", "Jeremías", "Lamentaciones",
            "Ezequiel", "Daniel", "Oseas", "Joel", "Amós", "Abdías", "Jonás", "Miqueas",
            "Nahúm", "Habacuc", "Sofonías", "Hageo", "Zacarías", "Malaquías",
            "Mateo", "Marcos", "Lucas", "Juan", "Hechos", "Romanos", "1 Corintios",
            "2 Corintios", "Gálatas", "Efesios", "Filipenses", "Colosenses", "1 Tesalonicenses",
            "2 Tesalonicenses", "1 Timoteo", "2 Timoteo", "Tito", "Filemón", "Hebreos",
            "Santiago", "1 Pedro", "2 Pedro", "1 Juan", "2 Juan", "3 Juan", "Judas", "Apocalipsis"
        };

        std::ifstream file(path);
        if (!file.is_open()) return;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string xml = buffer.str();

        size_t bookPos = 0;
        while ((bookPos = xml.find("<book", bookPos)) != std::string::npos) {
            BookData book;
            int bookNum = 0;
            
            size_t numStart = xml.find("number=\"", bookPos);
            if (numStart != std::string::npos) {
                numStart += 8;
                size_t numEnd = xml.find("\"", numStart);
                try {
                    bookNum = std::stoi(xml.substr(numStart, numEnd - numStart));
                } catch(...) {}
            }

            if (bookNum >= 1 && bookNum <= 66) {
                book.name = bookNames[bookNum - 1];
            } else { 
                book.name = "Libro " + std::to_string(bookNum); 
            }

            size_t nextBookPos = xml.find("<book", bookPos + 5);
            if (nextBookPos == std::string::npos) nextBookPos = xml.length();

            size_t chapPos = bookPos;
            while ((chapPos = xml.find("<chapter", chapPos)) != std::string::npos && chapPos < nextBookPos) {
                ChapterData chapter;
                size_t cnumStart = xml.find("number=\"", chapPos);
                if (cnumStart != std::string::npos && cnumStart < nextBookPos) {
                    cnumStart += 8;
                    size_t cnumEnd = xml.find("\"", cnumStart);
                    try {
                        chapter.number = std::stoi(xml.substr(cnumStart, cnumEnd - cnumStart));
                    } catch(...) {}
                }

                size_t nextChapPos = xml.find("<chapter", chapPos + 8);
                if (nextChapPos == std::string::npos) nextChapPos = nextBookPos;

                size_t versPos = chapPos;
                while ((versPos = xml.find("<verse", versPos)) != std::string::npos && versPos < nextChapPos) {
                    VerseData verse;
                    size_t vnumStart = xml.find("number=\"", versPos);
                    if (vnumStart != std::string::npos && vnumStart < nextChapPos) {
                        vnumStart += 8;
                        size_t vnumEnd = xml.find("\"", vnumStart);
                        try {
                            verse.number = std::stoi(xml.substr(vnumStart, vnumEnd - vnumStart));
                        } catch(...) {}
                    }
                    
                    size_t textStart = xml.find(">", versPos) + 1;
                    size_t textEnd = xml.find("</verse>", textStart);
                    if (textStart != std::string::npos && textEnd != std::string::npos && textStart < textEnd) {
                        verse.text = xml.substr(textStart, textEnd - textStart);
                    }

                    chapter.verses.push_back(verse);
                    versPos = textEnd;
                }
                
                if (!chapter.verses.empty()) {
                    book.chapters.push_back(chapter);
                }
                chapPos = nextChapPos;
            }
            
            if (!book.chapters.empty()) {
                m_CurrentBible.books.push_back(book);
            }
            bookPos = nextBookPos;
        }
        
        m_CurrentBible.name = fs::path(path).stem().string();
        m_BibleLoaded = !m_CurrentBible.books.empty();
    }

    void BibleView::Render() {
        auto selection = Core::PresentationCore::Get().GetSelection();

        // Control de cambio de archivo de Biblia
        if (selection.title != m_LastSelectedFile) {
            m_LastSelectedFile = selection.title;
            LoadXMLBible("assets/bibles/" + selection.title);
            m_SelectedBook = 0;
            m_SelectedChapter = 0;
        }

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "📖 MODO BIBLIA ACTIVO: %s", m_CurrentBible.name.c_str());
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputTextWithHint("##quickBibleSearch", "🔍 Buscar (Ej: Juan 3 16)...", m_QuickSearch, sizeof(m_QuickSearch), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string q(m_QuickSearch);
            std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c){ return std::tolower(c); });
            
            size_t firstSpace = q.find(' ');
            size_t secondSpace = q.find(' ', firstSpace + 1);
            
            if (firstSpace != std::string::npos) {
                std::string bookQuery = q.substr(0, firstSpace);
                int targetChapter = -1;
                
                try {
                    if (secondSpace != std::string::npos) {
                        targetChapter = std::stoi(q.substr(firstSpace + 1, secondSpace - firstSpace - 1));
                    } else {
                        targetChapter = std::stoi(q.substr(firstSpace + 1));
                    }
                } catch (...) {}

                for (size_t i = 0; i < m_CurrentBible.books.size(); i++) {
                    std::string bName = m_CurrentBible.books[i].name;
                    std::transform(bName.begin(), bName.end(), bName.begin(), [](unsigned char c){ return std::tolower(c); });
                    
                    if (bName.find(bookQuery) != std::string::npos) {
                        m_SelectedBook = static_cast<int>(i);
                        if (targetChapter > 0 && targetChapter <= m_CurrentBible.books[i].chapters.size()) {
                            m_SelectedChapter = targetChapter - 1;
                        } else {
                            m_SelectedChapter = 0;
                        }
                        break;
                    }
                }
            }
            m_QuickSearch[0] = '\0';
        }

        ImGui::Spacing();

        if (!m_BibleLoaded) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: No se pudo cargar el archivo XML de la Biblia.");
            ImGui::TextWrapped("Asegúrate de que el archivo XML esté correctamente estructurado.");
        } else {
            
            if (ImGui::BeginTable("BibleTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn("Historial", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Navegación", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn("Versículos", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                
                // COLUMNA 1: HISTORIAL
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
                ImGui::BeginChild("HistoryChild", ImVec2(0, 0), true);
                if (ImGui::Button("Limpiar Historial", ImVec2(-1, 25))) m_VerseHistory.clear();
                ImGui::Separator();
                
                for (int i = (int)m_VerseHistory.size() - 1; i >= 0; i--) { 
                    ImGui::PushID(i);
                    std::string ref = m_VerseHistory[i].substr(0, m_VerseHistory[i].find('\n'));
                    if (ImGui::Button(ref.c_str(), ImVec2(-1, 30))) {
                        Core::PresentationCore::Get().SetLayer2_Text(m_VerseHistory[i]);
                        Core::PresentationCore::Get().SetProjecting(true);
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                // COLUMNA 2: LIBROS Y CAPÍTULOS
                ImGui::TableSetColumnIndex(1);
                ImGui::BeginChild("NavChild", ImVec2(0, 0), false);
                
                ImGui::Text("Libros:");
                if (ImGui::BeginListBox("##Books", ImVec2(-1, ImGui::GetContentRegionAvail().y * 0.6f))) {
                    for (size_t i = 0; i < m_CurrentBible.books.size(); i++) {
                        const bool is_selected = (m_SelectedBook == (int)i);
                        if (ImGui::Selectable(m_CurrentBible.books[i].name.c_str(), is_selected)) {
                            m_SelectedBook = (int)i;
                            m_SelectedChapter = 0; 
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndListBox();
                }
                
                ImGui::Text("Capítulos:");
                if (m_SelectedBook >= 0 && m_SelectedBook < (int)m_CurrentBible.books.size()) {
                    if (ImGui::BeginListBox("##Chapters", ImVec2(-1, -1))) {
                        auto& book = m_CurrentBible.books[m_SelectedBook];
                        for (size_t i = 0; i < book.chapters.size(); i++) {
                            const bool is_selected = (m_SelectedChapter == (int)i);
                            std::string capLabel = "Capítulo " + std::to_string(book.chapters[i].number);
                            if (ImGui::Selectable(capLabel.c_str(), is_selected)) {
                                m_SelectedChapter = (int)i;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndListBox();
                    }
                }
                ImGui::EndChild();

                // COLUMNA 3: VERSÍCULOS
                ImGui::TableSetColumnIndex(2);
                ImGui::BeginChild("VersesChild", ImVec2(0, 0), false);
                
                if (m_SelectedBook >= 0 && m_SelectedBook < (int)m_CurrentBible.books.size() && 
                    m_SelectedChapter >= 0 && m_SelectedChapter < (int)m_CurrentBible.books[m_SelectedBook].chapters.size()) {
                    
                    auto& book = m_CurrentBible.books[m_SelectedBook];
                    auto& chap = book.chapters[m_SelectedChapter];
                    
                    for (auto& verse : chap.verses) {
                        ImGui::PushID(verse.number);
                        
                        std::string buttonLabel = std::to_string(verse.number) + ". " + verse.text;
                        std::string projectionText = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number) + "\n" + verse.text;

                        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 10);
                        
                        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0))) {
                            if (m_VerseHistory.empty() || m_VerseHistory.back() != projectionText) {
                                m_VerseHistory.push_back(projectionText);
                            }
                            
                            Core::PresentationCore::Get().SetLayer2_Text(projectionText);
                            Core::PresentationCore::Get().SetProjecting(true);
                        }
                        
                        ImGui::PopTextWrapPos();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
                
                ImGui::EndTable();
            }
        }
    }

} // namespace ProyecThor::UI