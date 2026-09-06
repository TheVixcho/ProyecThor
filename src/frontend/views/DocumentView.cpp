#include "DocumentView.h"
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

    DocumentView::DocumentView() = default;

    DocumentView::~DocumentView() {
        if (m_ConversionThread.joinable()) {
            m_ConversionThread.join();
        }
    }

    void DocumentView::LoadDocument(const std::string& filePath, const std::string& cacheDir) {
        if (m_LoadState.load() == DocumentLoadState::Converting) {
            return;
        }

        if (m_ConversionThread.joinable()) {
            m_ConversionThread.join();
        }

        m_FilePath  = filePath;
        m_DocTitle  = fs::path(filePath).stem().string();
        m_LastError = "";
        m_CurrentPage = 0;
        m_ConversionProgress.store(0);
        m_ConversionTotal.store(0);

        {
            std::lock_guard<std::mutex> lock(m_PagesMutex);
            m_Pages.clear();
        }

        m_LoadState.store(DocumentLoadState::Converting);
        StartConversion(filePath, cacheDir);
    }

    void DocumentView::StartConversion(const std::string& filePath, const std::string& cacheDir) {
        m_ConversionThread = std::thread([this, filePath, cacheDir]() {
            auto progressCb = [this](int current, int total) {
                m_ConversionProgress.store(current);
                m_ConversionTotal.store(total);
            };

            ConversionResult convResult = m_Converter.Convert(filePath, cacheDir, 150, progressCb);

            if (convResult.success) {
                std::lock_guard<std::mutex> lock(m_PagesMutex);
                m_Pages = convResult.pagePaths;
                m_LoadState.store(DocumentLoadState::Ready);
            } else {
                m_LastError = convResult.errorMessage;
                m_LoadState.store(DocumentLoadState::Error);
            }
        });
    }

    void DocumentView::GoToPage(int pageIndex) {
        std::lock_guard<std::mutex> lock(m_PagesMutex);
        if (pageIndex < 0 || pageIndex >= (int)m_Pages.size()) return;

        m_CurrentPage = pageIndex;
        m_PageViewer.LoadImageFromFile(m_Pages[m_CurrentPage]);
    }

    void DocumentView::Render() {
        std::vector<std::string> pagesCopy;
        {
            std::lock_guard<std::mutex> lock(m_PagesMutex);
            pagesCopy = m_Pages;
        }
        Render(m_DocTitle, pagesCopy);
    }

    void DocumentView::Render(const std::string& docTitle, const std::vector<std::string>& pages) {
        const auto& str = ProyecThor::UI::GetUIStrings();

        if (m_LoadState.load() == DocumentLoadState::Converting) {
            int progress = m_ConversionProgress.load();
            int total    = m_ConversionTotal.load();

            ImGui::TextDisabled("Convirtiendo documento...");

            if (total > 0) {
                float fraction = static_cast<float>(progress) / static_cast<float>(total);
                ImGui::ProgressBar(fraction, ImVec2(-1, 0));
                ImGui::TextDisabled("Página %d de %d", progress, total);
            } else {
                ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1, 0));
            }
            return;
        }

        if (m_LoadState.load() == DocumentLoadState::Error) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error al cargar el documento:");
            ImGui::TextWrapped("%s", m_LastError.c_str());
            return;
        }

        if (pages.empty()) {
            ImGui::TextDisabled("%s", str.docNoPages);
            return;
        }

        if (docTitle != m_LastDocument) {
            m_LastDocument = docTitle;
            m_CurrentPage  = 0;
            m_PageViewer.LoadImageFromFile(pages[m_CurrentPage]);
        }

        ImGui::Text(str.docTitle, docTitle.c_str());
        ImGui::TextDisabled(str.docPage, m_CurrentPage + 1, (int)pages.size());
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));

        if (ImGui::Button(str.docPrev, ImVec2(120, 30))) {
            if (m_CurrentPage > 0) {
                m_CurrentPage--;
                m_PageViewer.LoadImageFromFile(pages[m_CurrentPage]);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button(str.docNext, ImVec2(120, 30))) {
            if (m_CurrentPage < (int)pages.size() - 1) {
                m_CurrentPage++;
                m_PageViewer.LoadImageFromFile(pages[m_CurrentPage]);
            }
        }

        ImGui::PopStyleVar();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        if (ImGui::Button(str.docProject, ImVec2(-1, 40))) {
            Core::PresentationCore::Get().SetBackgroundMedia(pages[m_CurrentPage], false);
        }
        ImGui::PopStyleColor(2);

        ImGui::Separator();
        ImGui::Spacing();

        ImVec2 availSize = ImGui::GetContentRegionAvail();
        m_PageViewer.Render(availSize.x, availSize.y - 10.0f);
    }

}
