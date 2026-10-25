#pragma once

#include "ui/framework/GlassRenderer.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

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

    void LoadPreviewTexture(const std::string& path);
    void FreePreviewTexture();
};

} // namespace ProyecThor::UI
