#pragma once

#include "ui/framework/GlassRenderer.h"
#include <string>
#include <vector>
#include <filesystem>

namespace ProyecThor::UI {

struct CreatedImageItem {
    std::string filename;
    std::string fullPath;
    std::string timeStr;
    uint64_t fileSize = 0;
    unsigned int thumbTexId = 0;
    int thumbW = 0;
    int thumbH = 0;
    bool isImported = false;
};

class ProImageGeneratorPanel {
public:
    ProImageGeneratorPanel() = default;
    ~ProImageGeneratorPanel();

    void Render(bool* pOpen, GlassRenderer& glass);

private:
    char m_PromptBuf[512] = "";
    std::string m_LastGeneratedPath;
    unsigned int m_LastTextureId = 0;
    int m_TexW = 0;
    int m_TexH = 0;

    std::string m_StatusFeedback;
    bool m_StatusIsSuccess = true;

    // Imagen de referencia adjunta
    std::string m_AttachedImagePath;
    std::string m_AttachedImageName;
    bool m_ShowLocalBgModal = false;

    // Galería de imágenes creadas (cuadro lateral derecho)
    std::vector<CreatedImageItem> m_CreatedImages;
    bool m_NeedScanImages = true;

    void ScanCreatedImages();
    void FreeThumbnails();
    void EnsureThumbnailLoaded(CreatedImageItem& item);
    bool ImportToMultimedia(const std::string& srcPath, std::string& outMsg);
    void SetAsLiveBackground(const std::string& fullPath);

    void LoadPreviewTexture(const std::string& path);
    void FreePreviewTexture();
};

} // namespace ProyecThor::UI
