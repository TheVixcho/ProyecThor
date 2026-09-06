#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "backend/core/PresentationCore.h"

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI {

class TabTypography;
class TabEffects;

struct CanvaPalette {
    static inline ImVec4 Accent       = ImVec4(0.39f, 0.44f, 0.97f, 1.0f);
    static inline ImVec4 AccentHov    = ImVec4(0.49f, 0.54f, 1.00f, 1.0f);
    static inline ImVec4 AccentActive = ImVec4(0.30f, 0.35f, 0.90f, 1.0f);
    static inline ImVec4 Green        = ImVec4(0.10f, 0.79f, 0.55f, 1.0f);
    static inline ImVec4 Red          = ImVec4(0.93f, 0.26f, 0.36f, 1.0f);
    static inline ImVec4 Surface0     = ImVec4(0.09f, 0.09f, 0.11f, 1.0f);
    static inline ImVec4 Surface1     = ImVec4(0.12f, 0.13f, 0.16f, 1.0f);
    static inline ImVec4 Surface2     = ImVec4(0.16f, 0.17f, 0.22f, 1.0f);
    static inline ImVec4 Border       = ImVec4(0.22f, 0.23f, 0.30f, 1.0f);
    static inline ImVec4 Text         = ImVec4(0.92f, 0.92f, 0.94f, 1.0f);
    static inline ImVec4 TextMuted    = ImVec4(0.50f, 0.52f, 0.60f, 1.0f);
    static inline ImVec4 Gold         = ImVec4(0.95f, 0.72f, 0.20f, 1.0f);
    static inline ImVec4 Pink         = ImVec4(0.93f, 0.40f, 0.70f, 1.0f);
    static ImU32 ToU32(const ImVec4& c);

    static void Sync(const ProyecThor::Settings::ThemeSettings& theme);
};

// StyleData — un estilo guardable. "lyrics" es la caja de Letras: dibuja
// TODO el contenido de texto (canciones y tambien el CUERPO de un
// versiculo biblico, ambos con el mismo diseno). "index" es una caja
// OPCIONAL e independiente que dibuja SOLO la referencia biblica (ej.
// "Genesis 1:1"), nunca el cuerpo -- ver indexEnabled. Cada caja tiene su
// propia posicion, tamano, fuente, color, alineacion y efectos (ver
// ProyecThor::Core::TextBoxStyle). Alineacion dentro de cada caja:
// 0 = izquierda/arriba, 1 = centro, 2 = derecha/abajo.
struct StyleData {
    ProyecThor::Core::TextBoxStyle lyrics;
    ProyecThor::Core::TextBoxStyle index;
    bool                           indexEnabled = false;
};

class CanvaStyleEditor {
public:
    using OnSaveCallback         = std::function<void(const std::string&, const StyleData&)>;
    using OnFontImportedCallback = std::function<void(const std::string&)>;

    explicit CanvaStyleEditor(std::vector<std::string>* fontList,
                               OnFontImportedCallback    onFontImported = nullptr);
    ~CanvaStyleEditor();

    void OpenNew(const StyleData& defaults = {});
    void OpenEdit(const std::string& existingName, const StyleData& existingData);

    // Devuelve true el frame en que el usuario presiona Guardar. Editor a
    // pantalla completa (arma su propia ventana, ver Render()) -- pensado
    // para llamarse dentro de UIManager::EnterFullscreenEditor, igual que
    // OverlayCanvasEditor.
    bool Render(OnSaveCallback onSave);

    bool IsOpen() const { return m_IsOpen; }

    static bool PrimaryButton(const char* label, ImVec2 size = {});
    static bool GhostButton  (const char* label, ImVec2 size = {});
    static void Badge        (const char* label, ImVec4 color);
    static void SectionLabel (const char* label);
    static void SegmentedButtons(const char* prefix,
                              const char** labels, int count, int* current,
                              float totalWidth, float height,
                              const ImVec4& activeColor);

private:
    // flagIdx: 0 = Letras, 1 = Indice.
    ProyecThor::Core::TextBoxStyle& SelectedBox();

    void RenderHeader (ImVec2 winPos, ImVec2 winSize, OnSaveCallback& onSave, bool& savedThisFrame);
    void RenderRibbon (float width);
    void RenderRibbonFontAlign(float width);
    void RenderRibbonEffects  (float width);
    void RenderRibbonBackground(float width);
    void RenderCanvas  (float availW, float availH);
    void RenderFlag    (int flagIdx, ImVec2 p0, ImVec2 canvasSize, ImDrawList* dl, ImDrawList* fgDl);
    void RenderResizeHandle(int flagIdx, ImVec2 handlePos, int corner, ImDrawList* fgDl);

    std::unique_ptr<TabTypography> m_TabTypography;
    std::unique_ptr<TabEffects>    m_TabEffects;

    std::vector<std::string>* m_FontList = nullptr;

    StyleData   m_Data;
    char        m_Name[128]         = {};
    bool        m_IsOpen            = false;
    bool        m_IsEditingExisting = false;

    int m_SelectedFlag = 0; // 0 = Letras, 1 = Indice -- ver SelectedBox()
    int m_RibbonTab    = 0; // 0 = Fuente/Alinear, 1 = Efectos, 2 = Fondo de pantalla

    // Arrastre/redimension de flags sobre el canvas -- mismo patron que
    // OverlayCanvasEditor (delta de mouse normalizado por tamano de canvas,
    // esquina opuesta fija al redimensionar). Sin rotacion (no aplica a
    // cajas de texto).
    ImVec2 m_CanvasScreenPos  = { 0.0f, 0.0f };
    ImVec2 m_CanvasScreenSize = { 0.0f, 0.0f };

    int    m_DraggingFlag  = -1;
    ImVec2 m_DragStartMouse;
    float  m_DragStartPosX = 0.0f, m_DragStartPosY = 0.0f;

    int    m_ResizingFlag  = -1;
    int    m_ResizeCorner  = 0; // 0=TL 1=TR 2=BL 3=BR
    ImVec2 m_ResizeStartMouse;
    float  m_ResizeStartPosX = 0.0f, m_ResizeStartPosY = 0.0f;
    float  m_ResizeStartSizeW = 0.0f, m_ResizeStartSizeH = 0.0f;
};

} // namespace ProyecThor::UI