#pragma once
#include <imgui.h>
#include <string>

namespace ProyecThor::UI {

class HelpPanel {
public:
    HelpPanel();
    void Render(bool* isOpen);
    void SetCategory(int idx) { m_SelectedCategory = idx; }
    int  GetSelectedCategory() const { return m_SelectedCategory; }

private:
    int         m_SelectedCategory        = 0;
    bool        m_WasOpenLastFrame        = false;
    float       m_CopiedTimer             = 0.0f;
    std::string m_CopiedMessage           = "";
    int         m_ActiveQrCard            = -1; // -1 = ninguno, 0 = Discord, 1 = IG, 2 = WhatsApp, 3 = Ko-fi

    void RenderSidebar();
    void RenderContent();

    void RenderCategoryCommunity();
    void RenderCategoryCredits();
    void RenderCategoryPatchNotes();
    void RenderCategoryShortcuts();
    void RenderCategoryDonations();

    void RenderQrPreview(const char* url, const char* label);
    void CopyToClipboard(const char* text, const char* label);
};

} // namespace ProyecThor::UI
