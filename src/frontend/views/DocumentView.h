#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "ImageView.h"
#include "documents/DocumentConverter.h"

namespace ProyecThor::UI {

    enum class DocumentLoadState {
        Idle,
        Converting,
        Ready,
        Error
    };

    class DocumentView {
    public:
        DocumentView();
        ~DocumentView();

        void LoadDocument(const std::string& filePath, const std::string& cacheDir);

        void Render(const std::string& docTitle, const std::vector<std::string>& pages);

        void Render();

        DocumentLoadState GetLoadState() const { return m_LoadState.load(); }
        const std::string& GetLastError() const { return m_LastError; }

    private:
        void GoToPage(int pageIndex);
        void StartConversion(const std::string& filePath, const std::string& cacheDir);

        std::atomic<DocumentLoadState> m_LoadState{ DocumentLoadState::Idle };
        std::thread                    m_ConversionThread;
        std::mutex                     m_PagesMutex;

        std::atomic<int>               m_ConversionProgress{ 0 };
        std::atomic<int>               m_ConversionTotal{ 0 };

        std::string              m_FilePath;
        std::string              m_DocTitle;
        std::vector<std::string> m_Pages;
        std::string              m_LastError;

        std::string m_LastDocument = "";
        int         m_CurrentPage  = 0;

        ImageView          m_PageViewer;
        DocumentConverter  m_Converter;
    };

}
