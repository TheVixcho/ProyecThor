#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace ProyecThor::UI {

class GlassRenderer; // fwd decl (ver GlassRenderer.h)

class Announcements {
public:
    Announcements();

    void Render(GlassRenderer& glass);

    void RenderOnProjector(void* drawList,
                           float screenX, float screenY,
                           float screenW, float screenH,
                           float deltaTime);

    bool IsLive() const { return m_IsLive && !m_Messages.empty(); }

    // Para paneles externos (ej. ViewPanel > "Limpiar anuncios" o QuickNotes) que
    // necesitan interactuar o agregar mensajes al banner.
    void SetLive(bool live) { m_IsLive = live; }
    void AddMessage(const std::string& text, const std::string& tag = "Anuncios", bool enabled = true);

private:
    void               TickScroll(float deltaTime, float contentWidth, float screenW);
    const std::string& GetCurrentMessage() const;
    void               RenderNotesImportModal();

    // Sincroniza la lista de fuentes desde disco via PresentationCore
    void SyncFontList();

    struct Message {
        char text[512] = {};
        char tag[64]   = "Anuncios"; // "Anuncios", "Avisos", "Urgente", "Culto", "General"
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

    // Modo 0: elegir estilo guardado existente
    // Modo 1: editar fuente, tamaño y color inline
    int  m_StyleMode              = 0;

    // Modo 0: nombre del estilo guardado seleccionado
    char m_AssignedStyleName[128] = {};

    // Modo 1: propiedades editables inline
    float m_FontSize          = 48.0f;
    float m_TextColor[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
    int   m_SelectedFontIndex = 0;

    // Lista de fuentes disponibles (sincronizada desde disco)
    std::vector<std::string> m_FontList;

    // Popup de guardado de estilo inline
    char m_SaveStyleName[128] = {};

    bool m_IsLive = false;

    std::chrono::steady_clock::time_point m_LastFrameTime;
    bool                                  m_FirstFrame = true;
};

} // namespace ProyecThor::UI