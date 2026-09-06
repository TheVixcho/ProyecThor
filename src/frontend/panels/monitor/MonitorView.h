#pragma once
#include <GL/glew.h>
#include <string>
#include <vector>
#include <cstdint>
#include <imgui.h>
#include "frontend/views/ImageView.h"
#include "frontend/ui/SpinningDisc.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include "MonitorQueueEngine.h"

namespace ProyecThor::UI {

class UIManager;

class MonitorView {
public:
    MonitorView()  = default;
    ~MonitorView() = default;

    void Render(Core::VLCBasePlayer* player);

    // Necesario para el boton de pantalla completa del Preview (ver
    // RequestPreviewFullscreen -> UIManager::EnterFullscreenEditor). Mismo
    // patron que LibraryPanel::SetUIManager -- se llama una vez al armar
    // los paneles (ver HomePanel::Render()).
    void SetUIManager(UIManager* mgr) { m_UIManagerRef = mgr; }

    // Avanza la cola (detecta fin de clip real via VLC y pasa al siguiente
    // item) sin importar si este panel esta visible. IMPORTANTE: debe
    // llamarse UNA VEZ POR FRAME de forma incondicional (ver HomePanel::
    // Render(), junto a OClock::Update()) — antes esto solo corria dentro
    // de RenderQueue(), que solo se ejecuta con "Home" activo Y un video
    // seleccionado; en cuanto el operador miraba otra pestaña o
    // seleccionaba una cancion/pasaje mientras la cola reproducia, dejaba
    // de detectar el fin del clip y se quedaba pegada en el mismo video
    // para siempre.
    void Update();

    void AddToQueue(const std::string& fullPath);
    void AddURLToQueue(const std::string& url);

    void LoadPlayQueue();
    void SavePlayQueue();

private:
    void RenderPreviewMonitor(Core::VLCBasePlayer* player, float w, float h);
    void RenderCenterColumn(float w, float h, Core::VLCBasePlayer* previewPlayer);
    void RenderPreviewControls(Core::VLCBasePlayer* player, float w);
    void RenderQueue(float totalW);

    // Timeline + botones de transporte (skip/replay/play-pause/forward/stop)
    // -- compartido entre RenderPreviewControls (barra acoplada normal) y
    // RenderFullscreenToolbar (barra flotante auto-oculta), antes duplicado
    // en los dos lugares.
    void RenderTransportRow(Core::VLCBasePlayer* player, float innerW);

    // Boton mute/unmute del audio de Preview -- ver m_PreviewAudioEnabled.
    void RenderPreviewAudioToggle(Core::VLCBasePlayer* player, float btnSize);

    // ── Pantalla completa del Preview ────────────────────────────────────
    void RequestPreviewFullscreen(Core::VLCBasePlayer* player);
    void RenderPreviewFullscreenContent(Core::VLCBasePlayer* player);
    void RenderFullscreenToolbar(Core::VLCBasePlayer* player, ImVec2 avail);
    // Unico punto de salida (boton "X" y Esc lo llaman a este) -- ademas de
    // ExitFullscreenEditor(), restaura la ventana del sistema operativo si
    // fue ESTE flujo el que la puso en fullscreen (ver m_EnteredOSFullscreen).
    void ExitPreviewFullscreen();

    bool DrawIconButton(const char* iconName, float size,
                        ImVec4 bgCol, ImVec4 hov, ImVec4 act,
                        ImVec2 btnSize, bool isActiveState = false);

    // Punto unico de entrada para reproducir un indice de la cola desde la
    // UI. Mantiene m_LivePlaying sincronizado para el resto de paneles
    // (RenderCenterColumn, RenderPreviewControls) que aun lo consultan.
    void PlayQueueItem(int index);

    bool  m_Initialized    = false;
    bool  m_PreviewPlaying = false;
    // El monitor "PGM"/Live y sus controles (transporte + VU meters) se
    // movieron a ViewPanel::RenderLiveTransport (pantallas chicas dejaban el
    // Monitor demasiado apretado). m_LivePlaying/m_LiveMuted/m_LiveVolume
    // siguen viviendo aca porque RenderCenterColumn (boton TRANSMITIR) y
    // RenderPreviewControls (deshabilitar scrubbing si comparte player con
    // el live) todavia los necesitan — se refrescan cada frame al principio
    // de Render() en vez de en la ahora-inexistente RenderLiveControls.
    bool  m_LivePlaying    = false;
    bool  m_LiveMuted      = false;
    float m_LiveVolume     = 0.8f;

    UIManager* m_UIManagerRef = nullptr;

    // Audio del Preview -- APAGADO por default: el player de Preview es
    // forceSilent=true de fabrica (ver VLCBasePlayer/PresentationCoreImpl),
    // pensado para nunca duplicar lo que ya suena en vivo. Este toggle es
    // la UNICA forma de que el operador lo escuche a proposito (ver
    // RenderPreviewAudioToggle) -- se apaga solo (m_PreviewAudioEnabled se
    // queda en su valor pero el player vuelve a forceSilent) cada vez que
    // se reconstruye este objeto (recarga de la app), nunca a mitad de
    // sesion sin que el operador lo pida.
    bool m_PreviewAudioEnabled = false;

    // Nivel de volumen del Preview (0.0-1.0) -- solo tiene efecto audible
    // mientras m_PreviewAudioEnabled esta activo (ver RenderPreviewAudioToggle/
    // RenderFullscreenToolbar). Slider propio pedido explicito para la
    // vista de pantalla completa.
    float m_PreviewVolume = 0.8f;

    // FSR (EASU+RCAS) para el Preview a pantalla completa -- APAGADO por
    // default (opt-in, ver "Opciones de reproduccion" en
    // RenderFullscreenToolbar): reescala el video fuente (normalmente mas
    // chico que la pantalla) al tamaño real de pantalla completa en vez de
    // que ImGui lo estire liso, mismo pipeline EASU+RCAS de AMD que ya usa
    // BackgroundLayer para el video en vivo (ver PostProcessorFSR.h).
    bool                        m_PreviewFSREnabled = false;
    Shaders::PostProcessorFSR   m_PreviewFSR;

    // true si RequestPreviewFullscreen fue quien puso la ventana del
    // sistema operativo en fullscreen (F11) -- si el operador YA estaba en
    // fullscreen de antes (lo puso el mismo con F11), ExitPreviewFullscreen
    // no la toca al salir, para no sacarlo de un estado que eligio aparte.
    bool m_EnteredOSFullscreen = false;

    // ── Cola (logica real en MonitorQueueEngine) ────────────────────────────
    MonitorQueueEngine m_QueueEngine;
    int m_DragSrcIndex = -1; // solo feedback visual mientras se arrastra

    // ── Preview de Imagen/Audio (Video usa el player VLC de siempre) ────────
    ImageView         m_ImageView;
    SpinningDiscState m_DiscState;
    ImTextureID       m_CurrentAudioArt = 0; // portada del audio en preview, 0 = sin portada

    // ── Ecualizador en vivo (ver PresentationCore::SetLiveEqualizer*) ───────
    // Estado "de verdad" para los sliders del popup -- se aplica al audio en
    // vivo (nunca al de Preview, que es mudo por diseño). Mismo criterio que
    // AudioPanel: la UI es la fuente de verdad, libVLC solo recibe valores.
    static constexpr int kEqBands = 10;
    bool  m_EqEnabled = false;
    float m_EqPreamp  = 0.0f;
    float m_EqBandAmps[kEqBands] = { 0.0f };
    bool  m_ShowEqPopup = false;
    void  RenderEqualizerPopup();

    // ── HUD flotante auto-oculto sobre Preview y Cola Plegable ─────────────
    float m_HudAlpha        = 1.0f;
    float m_HudIdleTimer    = 0.0f;
    bool  m_QueueCollapsed  = false;
    float m_QueueAnimW      = 0.0f;
};

} // namespace ProyecThor::UI