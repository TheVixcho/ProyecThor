#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace ProyecThor::UI {

    enum class DocumentType {
        Unknown,
        PDF,
        PPTX
    };

    struct ConversionResult {
        bool                     success = false;
        std::string              errorMessage;
        std::vector<std::string> pagePaths;
    };

    using ProgressCallback = std::function<void(int, int)>;

    class DocumentConverter {
    public:
        DocumentConverter() = default;
        ~DocumentConverter() = default;

        ConversionResult Convert(
            const std::string&   filePath,
            const std::string&   cacheDir,
            int                  dpi            = 150,
            ProgressCallback     onProgress     = nullptr
        );

        void ClearCache(const std::string& filePath, const std::string& cacheDir);

        static DocumentType DetectType(const std::string& filePath);

    private:
        ConversionResult ConvertPDF(
            const std::string& filePath,
            const std::string& outputDir,
            int                dpi,
            ProgressCallback   onProgress
        );

        ConversionResult ConvertPPTX(
            const std::string& filePath,
            const std::string& outputDir,
            int                dpi,
            ProgressCallback   onProgress
        );

        std::string BuildCacheKey(const std::string& filePath);

        std::string FindLibreOfficeBinary();
    };

}

