#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <optional>
#include <string>
#include <GLFW/glfw3.h>
#include "IPanel.h"
#include "../toolbar/ConfigPanel.h"
#include "settings/SettingsPanel.h"
#include "panels/capture/CapturePanel.h"
#include "panels/TransitionPanel.h"
#include "Hub.h"
#include "GlassRenderer.h"
#include "panels/PerformancePanel.h"
#include "panels/StreamingPanel.h"
#include "panels/TeamChatPanel.h"
#include "panels/BroadcastPanel.h"
#include "panels/SyncPanel.h"
#include "panels/OSCPanel.h"
#include "panels/AIAssistantPanel.h"
#include "frontend/views/QuickNotes.h"
#include "backend/core/SubtitleImporter.h"
#include "backend/core/PresentationCore.h"

namespace ProyecThor::UI {

class LibraryPanel;

enum class ActiveLeftPanel {
    Library,
    Canva,
    None
};

// ── Modo de workspace ────────────────────────────────────────────────────────
// La toolbar de segundo nivel (ver RenderModeToolbar) reemplaza TODO el
// contenido de abajo segun el modo activo -- no son paneles dockeados mas,
// son secciones completas de la app:
//  - Hub: pantalla de inicio/novedades (Hub.cpp), tal cual ya existia.
//  - Projector: el workspace de siempre (Biblioteca/Home/Vista en Vivo/
//    Herramientas/Diseño dockeados), antes controlado por el bool m_HubMode.
// Yggdrasil (OSC/Red/Chat/Streaming) y Biblia (BibleView a pantalla completa)
// se retiraron del todo: OSC/Red/Streaming ahora son subcategorias de
// Ajustes > Proyeccion (ver CategoryProjection.cpp), Capture/Layer/Iniciar
// tambien viven en Vista en Vivo (ver ViewPanel::RenderStreamingPopup),
// Red/Chat ya estaban duplicados en Library/Vista en Vivo, y Biblia ya se
// puede buscar desde Home -- ninguno necesitaba su propio modo de workspace.
enum class WorkspaceMode {
    Hub,
    Projector,
};

class UIManager {
public:
    UIManager();
    ~UIManager();
// textTransitionTrigger visto en el ultimo frame (ver RenderAll()) —
// solo texto: el fondo/video tiene su propio crossfade independiente.
uint64_t m_LastTransitionTrigger = 0;
    bool Initialize(GLFWwindow* window);
    std::shared_ptr<TransitionPanel> GetTransitionPanelOwned() const { return m_TransitionPanelOwned; }
    void AddPanel(std::shared_ptr<ProyecThor::UI::IPanel> panel);
    void RenderAll();
    void Shutdown();
    void RequestSettings();

    GlassRenderer& GetGlassRenderer() { return m_GlassRenderer; }

    // true si ESE panel (por su GetName(): "Library"/"Diseño"/"Vista en
    // Vivo"/"Home") esta lo bastante colapsado (Alt Gr + 1..4) como para
    // que el panel mismo deba omitir dibujar su ventana/toolbar este frame.
    // Cada panel la consulta desde su propio Render() -- DESPUES de correr
    // cualquier "pump incondicional" propio (ver LibraryPanel::m_OClock.
    // Update() / HomePanel::m_MonitorView.Update()), nunca antes: esos
    // pumps deben seguir corriendo aunque el panel este oculto.
    bool IsPanelCollapsedForRender(const std::string& name) const;

    // Antes enfocaba "Control" (eliminado) al resetear el layout; ahora
    // enfoca "Vista en Vivo", que es el panel principal de ese dock.
    bool m_FocusViewNextFrame = false;

    ActiveLeftPanel GetActiveLeftPanel() const { return m_ActiveLeftPanel; }
    void SetActiveLeftPanel(ActiveLeftPanel p) { m_ActiveLeftPanel = p; }

    void OpenHub();

    // "Biblioteca" (Settings::WorkspaceLayoutPreset::Library) es un preset
    // MAS de Entorno de trabajo (ver BuildWorkspaceLayoutLibrary), igual que
    // Clasico/Simple/Transmision -- se elige desde Ajustes > Apariencia o el
    // menu Espacio de trabajo y se GUARDA (persiste entre sesiones, como
    // los demas). Debe llamarse SetLibraryPanelRef() una vez al armar los
    // paneles (ver main.cpp, mismo momento que SetAudioPanelRef) para que
    // RenderAll() pueda avisarle a Biblioteca que se restrinja a Medios
    // mientras ese preset este activo.
    void SetLibraryPanelRef(LibraryPanel* p) { m_LibraryPanelRef = p; }
    LibraryPanel* GetLibraryPanelRef() const { return m_LibraryPanelRef; }

    // ── Pantalla completa del SISTEMA OPERATIVO (F11) ─────────────────────
    // Publicos (antes privados) para que el Preview a pantalla completa
    // (ver MonitorView::RequestPreviewFullscreen) pueda activarla el mismo,
    // como si el operador hubiera apretado F11 -- pedido explicito.
    void ToggleFullscreen();
    bool IsWindowFullscreen() const { return m_Window && glfwGetWindowMonitor(m_Window) != nullptr; }

    // Usado por "Abrir con ProyecThor" (ver main.cpp): activa el preset
    // "Biblioteca" SOLO en memoria para esta sesion (nunca llama Save()),
    // sin pisar el preset que el usuario tiene guardado de verdad -- la
    // proxima vez que abra la app normalmente, LoadSettings() vuelve a leer
    // su preferencia real del disco.
    void EnterLibraryWorkspaceMode();

    // ── Editor a pantalla completa (Overlay/Estilos/Preview) ──────────────
    // Permite a un panel (editor de Overlays, editor de Estilos, Preview de
    // Biblioteca) tomar TODA el area de "main" por un frame, ocultando
    // Biblioteca/Home/Diseño/etc. La toolbar superior (RenderModeToolbar)
    // sigue dibujandose siempre por default -- eso es la regla para
    // Overlay/Estilos, que la necesitan visible. hideToolbar=true (pedido
    // explicito para el Preview de Biblioteca) la oculta tambien, para un
    // "de verdad toda la pantalla" real. El llamador es dueño del ciclo de
    // vida: entra al abrir el editor, sale al Guardar/Cancelar/Cerrar.
    void EnterFullscreenEditor(std::function<void()> renderFn, bool hideToolbar = false) {
        m_FullscreenEditorActive       = true;
        m_FullscreenEditorRenderFn     = std::move(renderFn);
        m_FullscreenEditorHidesToolbar = hideToolbar;
    }
    void ExitFullscreenEditor() {
        m_FullscreenEditorActive       = false;
        m_FullscreenEditorRenderFn     = nullptr;
        m_FullscreenEditorHidesToolbar = false;
    }
    bool IsFullscreenEditorActive() const { return m_FullscreenEditorActive; }
    bool FullscreenEditorHidesToolbar() const { return m_FullscreenEditorHidesToolbar; }

    // Red (LAN)/Chat/Streaming viven aca (no en Yggdrasil ni en Biblioteca/
    // Herramientas) para que Update() corra SIEMPRE, sin importar el
    // WorkspaceMode activo -- una transmision o el chat no se pueden pausar
    // solo porque el operador esta mirando Proyector. Yggdrasil,
    // LibraryPanel (grupo "Red") y ViewPanel (popup "Chat", ver
    // RenderChatPopup) reciben un puntero a la MISMA instancia (ver
    // main.cpp), así que aparecen "en varias partes" pero comparten un
    // unico servidor de verdad.
    StreamingPanel& GetRedPanel()      { return m_Red; }
    TeamChatPanel&  GetChatPanel()     { return m_Chat; }
    BroadcastPanel& GetBroadcastPanel() { return m_Broadcast; }
    SyncPanel&      GetSyncPanel()      { return m_Sync; }
    OSCPanel&       GetOSCPanel()       { return m_OSC; }

    void ToggleNotesWindow();
    void ToggleAIAssistant() { m_ShowAIAssistant = !m_ShowAIAssistant; }
    void ToggleConnectionsWindow() { m_ShowConnectionsWindow = !m_ShowConnectionsWindow; }
    void ToggleStageQuick(bool active);

private:
    void BeginDockspace();
    void EndDockspace();

    // Los 3 ordenamientos de Ajustes > Apariencia > Entorno de trabajo (ver
    // Settings::WorkspaceLayoutPreset) -- cada uno arma su propio arbol de
    // DockBuilder y puebla m_PanelCollapse[0..3] (Biblioteca/Diseño/Vista en
    // Vivo/Home, mismo orden que Alt Gr+1..4) con el nodo CONTENEDOR de cada
    // panel y el eje que le corresponde colapsar en ESE layout -- el mismo
    // panel puede colapsar por ancho en un preset y por alto en otro, segun
    // como quede orientado el split. Llamadas solo dentro del bloque de
    // reconstruccion de BeginDockspace (dockspace_id ya reseteado/limpio).
    void BuildWorkspaceLayoutClassic(ImGuiID dockspace_id);
    void BuildWorkspaceLayoutSimple(ImGuiID dockspace_id);
    void BuildWorkspaceLayoutBroadcast(ImGuiID dockspace_id);
    void BuildWorkspaceLayoutLibrary(ImGuiID dockspace_id);
    void BuildWorkspaceLayoutVideo(ImGuiID dockspace_id);

    // Ventanas nativas de salida real ("ProjectorLive"/"StageLive") -- se
    // llama SIEMPRE, una vez por frame, sin importar si el operador esta
    // viendo el Hub, el workspace normal, o un editor a pantalla completa
    // (Overlays/Estilos). Antes este render vivia inline dentro del bloque
    // exclusivo del modo Workspace::Projector, asi que dejaba de dibujarse
    // (y ImGui llegaba a destruir esas ventanas nativas por no volver a
    // someterlas) apenas se abria un editor a pantalla completa o se volvia
    // al Hub mientras se estaba proyectando/haciendo Stage — ver RenderAll().
    void RenderLiveOutputWindows();

    // Contenido de UNA salida de proyector (fondo/letras/overlay/reloj/
    // anuncios/captura), extraido de RenderLiveOutputWindows para poder
    // repetirlo en cada monitor extra elegido en Ajustes > Proyeccion
    // (ver PresentationState::extraTargetMonitors). isPrimary=true es
    // EXACTAMENTE el comportamiento de siempre (registra el viewport de
    // post-FX principal); isPrimary=false registra/usa una instancia de
    // post-FX propia para ese monitor (ver PresentationCore::
    // RegisterExtraProjectorViewport) y agrega su ImGuiID a
    // activeExtraViewportIds para que se pueda podar al final del frame.
    void RenderProjectorOutput(const char* windowName, int mx, int my,
                                const GLFWvidmode* mode,
                                const Core::PresentationState& state,
                                bool isPrimary,
                                std::vector<ImGuiID>* activeExtraViewportIds);

    // Idem para Stage -- mas simple, sin post-FX (Stage nunca lo tuvo).
    void RenderStageOutput(const char* windowName, int smx, int smy,
                            const GLFWvidmode* stageMode);

    void ApplyProfessionalTheme();
    void RenderMainMenuBar();
    void RenderModeToolbar();
    void RenderQuickSwitcher();

    // Puntos "Público"/"Stage" + "Borrar Todo" — antes vivian en ViewPanel
    // (arriba del video), pedido explicito de subirlos a la toolbar
    // superior (lado derecho) para liberarle mas espacio a "Vista en Vivo".
    void RenderModeToolbarStatusActions(float winW, float railH);
    void ToggleAudience(bool active);

    // Ventana flotante de Notas -- boton propio en RenderModeToolbar (junto
    // a los 5 modos) y atajo global Shift+Z, abre una ventana centrada tipo
    // "Preferencias" (ver Settings::SettingsPanel::Render) con QuickNotes
    // adentro, en vez de vivir dockeada en Home o en un panel propio. Se
    // somete desde el bloque "siempre" de RenderAll() (junto a
    // RenderUrlImportModal), asi queda disponible tanto en el Hub como en
    // el Proyector y nunca se interrumpe solo porque se esta proyectando.
    void         RenderNotesWindow();
    bool         m_ShowNotes = false;
    QuickNotes   m_NotesPanel;

    void            RenderAIAssistantWindow();
    bool            m_ShowAIAssistant = false;
    AIAssistantPanel m_AIAssistant;

    void        RenderUrlImportModal();
    bool        m_ShowUrlImport        = false;
    bool        m_UrlImportRunning     = false;
    char        m_UrlImportBuffer[512] = {};
    std::string m_UrlImportLastError;
    std::thread m_UrlImportThread;
    std::mutex  m_UrlImportMutex;
    std::optional<ProyecThor::Core::SubtitleFetchResult> m_UrlImportResult;

    void RenderStylesPopup();

    void RenderConnectionsWindow();
    bool m_ShowConnectionsWindow = false;
    int  m_ConnectionsActiveTab   = 0;

    // Ver comentario de los getters (GetRedPanel/GetChatPanel/GetBroadcastPanel/GetOSCPanel).
    StreamingPanel m_Red;
    TeamChatPanel  m_Chat;
    BroadcastPanel m_Broadcast;
    SyncPanel      m_Sync;
    OSCPanel       m_OSC;
    GLFWwindow*                          m_Window               = nullptr;
    std::vector<std::shared_ptr<IPanel>> m_Panels;
    bool                                 m_ShowConfig           = false;
    Settings::SettingsPanel              m_SettingsPanel;
    PerformancePanel                     m_PerformancePanel;
    ActiveLeftPanel                      m_ActiveLeftPanel      = ActiveLeftPanel::Library;
    std::shared_ptr<TransitionPanel>     m_TransitionPanelOwned;
    TransitionPanel*                     m_TransitionPanel      = nullptr;
    std::string                          m_LastProjectedText;
    std::string                          m_OutgoingText;
    float                                m_TransitionLastTime   = 0.0f;
    bool                                 m_ResetLayout          = true;

    // Cache del ultimo Ajustes > Apariencia > Entorno de trabajo aplicado
    // (ver Settings::WorkspaceLayoutPreset) -- BeginDockspace() lo compara
    // contra el valor actual cada frame y dispara m_ResetLayout solo si
    // cambio, sin que la pagina de Ajustes necesite conocer a UIManager.
    // -1 = todavia no se aplico ninguno (fuerza el reset en el primer frame).
    int                                  m_LastWorkspacePreset  = -1;
    GlassRenderer                        m_GlassRenderer;

    // ── Pantalla completa (menu Ventana) ────────────────────────────────────
    // Geometria de la ventana ANTES de pasar a pantalla completa, para poder
    // restaurarla al salir (glfwSetWindowMonitor no la recuerda solo).
    int  m_WindowedX = 0, m_WindowedY = 0, m_WindowedW = 1280, m_WindowedH = 800;

    Hub           m_Hub;
    WorkspaceMode m_Mode = WorkspaceMode::Hub;

    // Ver LibraryPanel::SetMediaOnlyMode -- se sincroniza cada frame en
    // RenderAll() segun si el preset activo (Settings::WorkspaceSettings::
    // layoutPreset) es Library, no hace falta guardar estado propio aca.
    LibraryPanel* m_LibraryPanelRef = nullptr;

    // Selector rapido (Alt+Espacio) — ver RenderQuickSwitcher.
    bool m_QuickSwitchOpen  = false;
    int  m_QuickSwitchIndex = 0;

    // ── Colapso animado de paneles (Alt Gr + 1/2/3/4, reset con Alt Gr + 0) ──
    // Cada entrada colapsa/expande el NODO CONTENEDOR del split (dock_left/
    // dock_main_top/dock_right/dock_main_bottom, no la ventana individual)
    // para que el resto del layout recupere el espacio -- ver
    // UpdatePanelCollapseAnim() en UIManager.cpp. Orden fijo: 0=Biblioteca,
    // 1=Diseño, 2=Vista en Vivo, 3=Home (mismo orden que las teclas 1-4).
    struct PanelCollapseState {
        ImGuiID nodeId       = 0;
        ImVec2  expandedSize = ImVec2(0.0f, 0.0f); // capturado al (re)construir el layout
        bool    axisIsWidth  = true;  // true: colapsa ancho (split izq/der), false: alto (arriba/abajo)
        bool    collapsed    = false;
        float   animT        = 0.0f;  // 0 = expandido, 1 = colapsado
    };
    static constexpr int kCollapsiblePanelCount = 4;
    PanelCollapseState m_PanelCollapse[kCollapsiblePanelCount];
    void UpdatePanelCollapseAnim();
    void TogglePanelCollapse(int index);
    void ResetPanelCollapse();

    // Ver EnterFullscreenEditor/ExitFullscreenEditor.
    bool                   m_FullscreenEditorActive       = false;
    std::function<void()>  m_FullscreenEditorRenderFn;
    bool                   m_FullscreenEditorHidesToolbar = false;
};

} // namespace ProyecThor::UI