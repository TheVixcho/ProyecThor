#include "PreviewPanel.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype> // Necesario para std::tolower

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    PreviewPanel::PreviewPanel() {
        m_PreviewPlayer = std::make_unique<Core::VLCVideoLayer>(1280, 720);
    }

    PreviewPanel::~PreviewPanel() {
        if (m_PreviewPlayer) {
            m_PreviewPlayer->Stop();
        }
    }

    std::string PreviewPanel::FormatTime(int64_t ms) {
        if (ms < 0) ms = 0;
        int totalSeconds = ms / 1000;
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << minutes << ":" 
            << std::setfill('0') << std::setw(2) << seconds;
        return oss.str();
    }

    // Lector Adaptado para el formato XML proporcionado
    void PreviewPanel::LoadXMLBible(const std::string& path) {
        m_CurrentBible = BibleData();
        m_BibleLoaded = false;
        m_LoadedBiblePath = path;

        // Diccionario para traducir <book number="1"> a "Génesis", etc.
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
            
            // Extraer el número del libro
            size_t numStart = xml.find("number=\"", bookPos);
            if (numStart != std::string::npos) {
                numStart += 8;
                size_t numEnd = xml.find("\"", numStart);
                try {
                    bookNum = std::stoi(xml.substr(numStart, numEnd - numStart));
                } catch(...) {}
            }

            // Asignar nombre real
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

    void PreviewPanel::Render() {
        if (m_PreviewPlayer) m_PreviewPlayer->UpdateTexture();

        ImGui::Begin("Preview"); 

        auto state = Core::PresentationCore::Get().GetState();
        auto selection = Core::PresentationCore::Get().GetSelection();
        bool isBibleMode = (selection.type == Core::ItemType::Bible);

        // --- MANEJO DE SELECCIÓN Y AUTOPLAY ---
        if (selection.title != m_LastSelectedFile) {
            m_LastSelectedFile = selection.title;
            if (selection.type == Core::ItemType::Video || selection.type == Core::ItemType::Image) {
                std::string folder = (selection.type == Core::ItemType::Video) ? "assets/videos/" : "assets/images/";
                m_PreviewPlayer->PlayVideo(folder + selection.title);
                m_IsPlayingPreview = true;
            } else if (isBibleMode) {
                m_PreviewPlayer->Stop();
                LoadXMLBible("assets/bibles/" + selection.title);
                m_SelectedBook = 0;
                m_SelectedChapter = 0;
            } else {
                m_PreviewPlayer->Stop();
            }
        }

        // =====================================================================
        // MODO NORMAL (Canciones, Videos, Imágenes) -> Mostrar Monitores Duales
        // =====================================================================
        if (!isBibleMode) {
            ImGui::Columns(2, "DualMonitors", false);
            
            // 1. PREVIEW (IZQUIERDA)
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "PREVIEW (VISUALIZACIÓN PRIVADA)");
            ImVec2 previewCanvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x - 10, (ImGui::GetContentRegionAvail().x - 10) * 0.5625f);
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 prev_max = ImVec2(previewCanvasPos.x + canvasSize.x, previewCanvasPos.y + canvasSize.y);
            
            drawList->AddRectFilled(previewCanvasPos, prev_max, IM_COL32(15, 15, 15, 255));
            
            if (selection.type == Core::ItemType::Video || selection.type == Core::ItemType::Image) {
                void* texID = m_PreviewPlayer->GetTextureID();
                if (texID) drawList->AddImage(texID, previewCanvasPos, prev_max);
            } else if (selection.type != Core::ItemType::None) {
                ImVec2 textPos = ImVec2(previewCanvasPos.x + 15, previewCanvasPos.y + canvasSize.y / 2);
                drawList->AddText(textPos, IM_COL32(100, 100, 100, 255), ("Seleccionado:\n" + selection.title).c_str());
            }
            
            ImGui::Dummy(canvasSize);
            ImGui::NextColumn();

            // 2. LIVE PROGRAM (DERECHA)
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "PROGRAM (PANTALLA EN VIVO)");
            ImVec2 liveCanvasPos = ImGui::GetCursorScreenPos();
            ImVec2 p_max = ImVec2(liveCanvasPos.x + canvasSize.x, liveCanvasPos.y + canvasSize.y);
            
            if (state.bgType == Core::PresentationState::BackgroundType::Video) {
                void* texID = Core::PresentationCore::Get().GetBackgroundTexture();
                if (texID) drawList->AddImage(texID, liveCanvasPos, p_max);
                else drawList->AddRectFilled(liveCanvasPos, p_max, IM_COL32(0,0,0,255));
            } else {
                drawList->AddRectFilled(liveCanvasPos, p_max, IM_COL32(state.bgColor[0]*255, state.bgColor[1]*255, state.bgColor[2]*255, 255));
            }

            if (state.showText && !state.currentText.empty()) {
                float screenScale = canvasSize.x / 1920.0f; 
                float marginL = state.margins[0] * screenScale;
                float marginT = state.margins[1] * screenScale;
                float marginR = state.margins[2] * screenScale;
                float marginB = state.margins[3] * screenScale;

                float boxX = liveCanvasPos.x + marginL;
                float boxY = liveCanvasPos.y + marginT;
                float boxW = canvasSize.x - marginL - marginR;
                float boxH = canvasSize.y - marginT - marginB;

                float targetFontSize = state.textSize * screenScale;
                
                if (state.autoScale) {
                    while (targetFontSize > 5.0f) {
                        ImVec2 tSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, state.currentText.c_str());
                        if (tSize.y <= boxH) break; 
                        targetFontSize -= 1.0f; 
                    }
                }

                ImVec2 finalBlockSize = ImGui::GetFont()->CalcTextSizeA(targetFontSize, FLT_MAX, boxW, state.currentText.c_str());

                float textX = boxX;
                if (state.textAlignment == 1) textX += (boxW - finalBlockSize.x) * 0.5f; 
                else if (state.textAlignment == 2) textX += (boxW - finalBlockSize.x); 

                float textY = boxY + (boxH - finalBlockSize.y) * 0.5f; 
                ImVec2 textPos = ImVec2(textX, textY);
                
                ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(state.textColor[0], state.textColor[1], state.textColor[2], state.textColor[3]));
                
                drawList->AddText(ImGui::GetFont(), targetFontSize, ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), state.currentText.c_str(), NULL, boxW); 
                drawList->AddText(ImGui::GetFont(), targetFontSize, textPos, col, state.currentText.c_str(), NULL, boxW);
            }

            ImGui::Dummy(canvasSize);
            ImGui::Columns(1); 
            
            ImGui::Separator();
            ImGui::Spacing();

            // --- CONTROLES INFERIORES NORMALES ---
            if (selection.type == Core::ItemType::None) {
                ImGui::TextDisabled("Seleccione un elemento de la biblioteca.");
            } 
            else if (selection.type == Core::ItemType::Song) {
                ImGui::TextDisabled("DOCUMENTO: %s", selection.title.c_str());
                ImGui::BeginChild("VerseList", ImVec2(0, 0), true);
                for (size_t i = 0; i < selection.contentData.size(); i++) {
                    ImGui::PushID((int)i);
                    if (ImGui::Button(selection.contentData[i].c_str(), ImVec2(-1, 60))) {
                        Core::PresentationCore::Get().SetLayer2_Text(selection.contentData[i]);
                        Core::PresentationCore::Get().SetProjecting(true); // Auto-proyectar
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            } 
            else if (selection.type == Core::ItemType::Video || selection.type == Core::ItemType::Image) {
                ImGui::TextDisabled("ARCHIVO MEDIA: %s", selection.title.c_str());
                ImGui::BeginChild("MediaControls", ImVec2(0, 0), true);
                
                if (selection.type == Core::ItemType::Video && m_PreviewPlayer) {
                    float currentPos = m_PreviewPlayer->GetPosition();
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::SliderFloat("##timeline", &currentPos, 0.0f, 1.0f, "")) m_PreviewPlayer->SetPosition(currentPos);
                    int64_t currentTimeMs = m_PreviewPlayer->GetTime();
                    int64_t totalTimeMs = m_PreviewPlayer->GetLength();
                    ImGui::TextDisabled("%s", (FormatTime(currentTimeMs) + " / " + FormatTime(totalTimeMs)).c_str());
                    
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x / 2 - 40);
                    if (m_IsPlayingPreview) {
                        if (ImGui::Button("Pausa", ImVec2(80, 30))) { m_PreviewPlayer->SetPause(true); m_IsPlayingPreview = false; }
                    } else {
                        if (ImGui::Button("Play", ImVec2(80, 30))) { m_PreviewPlayer->SetPause(false); m_IsPlayingPreview = true; }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Detener", ImVec2(80, 30))) { m_PreviewPlayer->SetPosition(0.0f); m_PreviewPlayer->SetPause(true); m_IsPlayingPreview = false; }
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                }

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                std::string folder = (selection.type == Core::ItemType::Video) ? "assets/videos/" : "assets/images/";
                if (ImGui::Button("PROYECTAR AL PUBLICO (LIVE)", ImVec2(-1, 50))) {
                    Core::PresentationCore::Get().SetBackgroundMedia(folder + selection.title, true);
                    Core::PresentationCore::Get().SetProjecting(true); 
                }
                ImGui::PopStyleColor(2);
                ImGui::EndChild();
            }
        } 
        // =====================================================================
        // MODO BIBLIA -> Ocultar Monitores y Mostrar UI Dedicada de 3 Columnas
        // =====================================================================
        else {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "📖 MODO BIBLIA ACTIVO: %s", m_CurrentBible.name.c_str());
            ImGui::SameLine();
            
            // BÚSQUEDA RÁPIDA
            static char quickSearch[64] = "";
            ImGui::SetNextItemWidth(300);
            if (ImGui::InputTextWithHint("##quickBibleSearch", "🔍 Buscar (Ej: Juan 3 16)...", quickSearch, sizeof(quickSearch), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string q(quickSearch);
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

                    // Buscar el libro
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
                quickSearch[0] = '\0'; // Limpiar tras buscar (más seguro que memset)
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
                            
                            // Texto que se enviará al PresentationCore (Referencia + Texto)
                            std::string projectionText = book.name + " " + std::to_string(chap.number) + ":" + std::to_string(verse.number) + "\n" + verse.text;

                            // Formateo para que el texto haga wrap automático en la UI
                            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 10);
                            
                            // Botón dinámico que ajusta su altura según el contenido
                            if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0))) {
                                
                                // Añadir al historial (evitar duplicados inmediatos)
                                if (m_VerseHistory.empty() || m_VerseHistory.back() != projectionText) {
                                    m_VerseHistory.push_back(projectionText);
                                }
                                
                                // Mandar a proyectar
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
        
        ImGui::End();
    } // Fin de PreviewPanel::Render

} // namespace ProyecThor::UI