#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace ProyecThor::UI {

class GlassRenderer;

class Announcements {
public:
    Announcements();

    void Render(GlassRenderer& glass);

    void RenderOnProjector(void* drawList,
                           float screenX, float screenY,
                           float screenW, float screenH,
                           float deltaTime);

    bool IsLive() const { return m_IsLive && !m_Messages.empty(); }

    void SetLive(bool live) { m_IsLive = live; }
    void AddMessage(const std::string& text, const std::string& tag = "Anuncios", bool enabled = true);

private:
    void               TickScroll(float deltaTime, float contentWidth, float screenW);
    const std::string& GetCurrentMessage() const;
    void               RenderNotesImportModal();

    void SyncFontList();

    struct Message {
        char text[512] = {};
        char tag[64]   = "Anuncios";
        bool enabled   = true;
    };

    std::vector<Message> m_Messages;
    std::string          m_CategoryFilter = "Todos";
    int                  m_ActiveIndex = 0;
    bool                 m_ShowNotesImportModal = false;
    char                 m_ImportSearchFilter[128] = {};

    enum class Direction { RightToLeft = 0, LeftToRight = 1 };

    Direction m_Direction     = Direction::RightToLeft;
    float     m_SpeedPxPerSec = 120.0f;
    float     m_ScrollOffset  = 0.0f;
    bool      m_Paused        = false;
    float     m_GapWidth      = 200.0f;

    int   m_VPosition       = 2;
    float m_BannerHeightPct = 0.07f;
    float m_BgR = 0.04f, m_BgG = 0.04f, m_BgB = 0.08f, m_BgA = 0.82f;
    bool  m_ShowBg          = true;

    int  m_StyleMode              = 0;

    char m_AssignedStyleName[128] = {};

    float m_FontSize          = 48.0f;
    float m_TextColor[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
    int   m_SelectedFontIndex = 0;

    std::vector<std::string> m_FontList;

    char m_SaveStyleName[128] = {};

    bool m_IsLive = false;

    std::chrono::steady_clock::time_point m_LastFrameTime;
    bool                                  m_FirstFrame = true;
};

}
