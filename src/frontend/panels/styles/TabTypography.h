#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include "CanvaStyleEditor.h"

namespace ProyecThor::UI {

class TabTypography {
public:
    using OnFontImportedCallback = std::function<void(const std::string& fontPath)>;

    explicit TabTypography(std::vector<std::string>* fontList,
                           OnFontImportedCallback onFontImported = nullptr);

    void Render(ProyecThor::Core::TextBoxStyle& box, float colWidth);

private:
    void RenderFontSelector    (ProyecThor::Core::TextBoxStyle& box, float colWidth);
    void RenderColorPicker     (ProyecThor::Core::TextBoxStyle& box, float colWidth);
    void RenderSizeSlider      (ProyecThor::Core::TextBoxStyle& box, float colWidth);
    void RenderAutoScaleCheckbox(ProyecThor::Core::TextBoxStyle& box);
    void ImportFont();

    std::vector<std::string>* m_FontList       = nullptr;
    OnFontImportedCallback    m_OnFontImported;
    std::string               m_ImportStatus;
    float                     m_ImportMsgTimer  = 0.0f;
};

} // namespace ProyecThor::UI