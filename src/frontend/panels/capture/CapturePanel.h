#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ProyecThor::Settings { struct CaptureSceneSettings; }

// ─────────────────────────────────────────────────────────────────────────────
//  CaptureSource — abstracción de cualquier fuente de captura
// ─────────────────────────────────────────────────────────────────────────────
namespace ProyecThor::UI {

#ifdef PT_HAVE_WAYLAND_CAPTURE
class WaylandScreenCapture;
#endif

enum class CaptureSourceType {
    Camera,         // Cámara física (webcam, capturadora HDMI, etc.)
    Window,         // Ventana específica del sistema operativo (X11 puro)
    Monitor,        // Monitor / pantalla completa (X11 puro)
    Screen,         // Pantalla o ventana mediada por el portal de escritorio
                     // (Wayland) — ver WaylandScreenCapture. Window/Monitor
                     // no sirven bajo Wayland: el compositor no vuelca
                     // contenido real a la ventana raiz X11 heredada.
    Unknown         // Antes "None" — renombrado para no chocar con la
                     // macro None de X11 (Xlib.h la define como 0L)
};

struct CaptureSource {
    CaptureSourceType type   = CaptureSourceType::Unknown;
    std::string       name;
    int               index  = -1;
    std::string       handle;
};

// ─────────────────────────────────────────────────────────────────────────────
//  CapturePanel
// ─────────────────────────────────────────────────────────────────────────────
// Ya no es un IPanel independiente: vive como la sección "Captura" del
// sidebar de HomePanel. Ver HomePanel.cpp.
class CapturePanel {
public:
    CapturePanel();
    ~CapturePanel();

    // Dibuja los controles (solo cuando la sección "Captura" esta activa).
    // Ya no abre su propia ventana — HomePanel es dueño de esa.
    void        RenderContent();
    std::string GetName()  const { return "Captura"; }

    // ── Control público ──────────────────────────────────────────────────────

    /// Envía el frame actual de la captura activa al proyector.
    /// Se llama desde UIManager cuando isProjecting == true.
    void        RenderOnProjector(ImDrawList* dl,
                                  float px, float py,
                                  float pw, float ph);

    bool        IsLive()   const { return m_IsCapturing; }

    // Para BroadcastPanel (Streaming > Capture -> "Mostrar en Layer"): la
    // misma textura/tamaño que ya usa el preview interno de este panel,
    // expuestos tal cual (sin disparar un grab nuevo -- GetCurrentTexture
    // ya cachea por frame de ImGui, ver su comentario).
    void* GetPreviewTexture() { return GetCurrentTexture(); }
    int   GetFrameWidth()  const { return m_FrameW; }
    int   GetFrameHeight() const { return m_FrameH; }

    // Para paneles externos (ej. ViewPanel > "Limpiar captura"/"Borrar
    // Todo") que necesitan cortarla sin pasar por los controles internos.
    void        Stop() { StopCapture(); }

    // Versión parametrizada de la lógica de "Escenas rápidas" (ver
    // SaveCurrentAsScene/RecallScene mas abajo), para que paneles externos
    // (ViewPanel > Pads) puedan guardar/aplicar una disposicion de
    // captura sin pasar por los 8 slots fijos de este panel.
    // Calificado con "::" porque, en headers incluidos junto con
    // SettingsPanel.h (backend/settings), "Settings::" sin calificar dentro
    // de namespace ProyecThor::UI resuelve al OTRO namespace anidado
    // ProyecThor::UI::Settings (panel de Ajustes), no a ProyecThor::Settings
    // (datos, ver SettingsManager.h).
    bool        SnapshotCurrentCapture(::ProyecThor::Settings::CaptureSceneSettings& out) const;
    void        ApplyCaptureScene(const ::ProyecThor::Settings::CaptureSceneSettings& scene);

    // Dibuja la grilla de "Escenas rápidas" (los mismos 8 slots de
    // Settings::CaptureSettings::scenes que usa este panel). Publico para
    // que ViewPanel > Pads pueda mostrarla tal cual debajo de sus
    // propios pads "General" -- son las mismas escenas, sincronizadas
    // (un solo dato de fondo), no una copia aparte.
    void        RenderSceneButtons();

private:
    // ── Enumeración de fuentes ───────────────────────────────────────────────
    void EnumerateCameras();
    void EnumerateWindows();
    void EnumerateMonitors();
    void RefreshSources();

    // ── Captura ──────────────────────────────────────────────────────────────
    bool StartCapture(const CaptureSource& src);
    void StopCapture();

    /// Obtiene el último frame como textura OpenGL.
    /// Devuelve (void*)texture_id o nullptr si no hay frame.
    void* GetCurrentTexture();

    // ── UI helpers ───────────────────────────────────────────────────────────
    void RenderSourceSelector();
    void RenderPreview();
    void RenderControls();
    void RenderProjectButton();

    // ── Estado ───────────────────────────────────────────────────────────────
    std::vector<CaptureSource> m_Sources;
    int                        m_SelectedIdx   = -1;
    CaptureSource              m_ActiveSource;

    bool  m_IsCapturing     = false;
    bool  m_ProjectOnScreen = false;   // Si true, también envía al proyector
    float m_Opacity         = 1.0f;
    bool  m_StretchToFill   = true;    // Solo aplica en PlacementMode::Fullscreen

    // ── Ubicación en el proyector ─────────────────────────────────────────────
    // Fullscreen: cubre todo el proyector (comportamiento de siempre, ver
    // m_StretchToFill). Custom: un recuadro con bordes redondeados que el
    // usuario mueve/redimensiona libremente, como una capa de canvas — ver
    // RenderPlacementEditor().
    enum class PlacementMode { Fullscreen, Custom };
    PlacementMode m_PlacementMode = PlacementMode::Fullscreen;

    // Recuadro en modo Custom, normalizado 0..1 respecto al proyector
    // (x0,y0)=esquina superior izquierda, (x1,y1)=esquina inferior derecha.
    float m_CustomX0 = 0.25f, m_CustomY0 = 0.25f;
    float m_CustomX1 = 0.75f, m_CustomY1 = 0.75f;

    void RenderPlacementEditor();

    // ── Escenas rápidas (posiciones libres guardadas) ────────────────────
    // 8 botones de color: click corto aplica la escena guardada en ese
    // slot (fuente + recuadro + opacidad, ver SettingsManager::
    // CaptureSceneSettings), click derecho abre un menú para guardar la
    // posición libre actual ahí o borrarla. Persisten en Settings, no en
    // memoria de sesión. (RenderSceneButtons es publico, ver mas arriba.)
    void SaveCurrentAsScene(int slot);
    void RecallScene(int slot);
    void ClearScene(int slot);

    // ── Textura de preview ───────────────────────────────────────────────────
    unsigned int m_PreviewTexID = 0;   // GLuint como uint para evitar include de GL aquí
    int          m_FrameW       = 0;
    int          m_FrameH       = 0;

    // Evita re-capturar/re-subir la textura mas de una vez por frame real de
    // ImGui: preview (RenderContent) y proyector (RenderOnProjector) piden
    // ambos GetCurrentTexture() en el mismo frame, y sin este cache cada uno
    // dispara su propio grab (para camara, un cap.read() real cada vez).
    int  m_LastGrabFrameCount = -1;
    void* m_LastGrabResult    = nullptr;

    // ── Backend de captura (opaco — implementado en .cpp) ────────────────────
    struct CaptureBackend;
    std::unique_ptr<CaptureBackend> m_Backend;

#ifdef PT_HAVE_WAYLAND_CAPTURE
    // Captura de pantalla/ventana para sesiones Wayland (fuente Screen), via
    // portal de escritorio + PipeWire. Ver WaylandScreenCapture.h.
    std::unique_ptr<WaylandScreenCapture> m_WaylandCapture;
#endif

    // ── Búsqueda / filtro ────────────────────────────────────────────────────
    char m_SearchBuf[128] = {};
};

} // namespace ProyecThor::UI
