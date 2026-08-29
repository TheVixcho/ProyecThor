#pragma once
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <imgui.h>

#include "NetworkStreamServer.h"
#include "ChatMessageStore.h"
#include "frontend/windowing/SecondaryOutputWindow.h"
#include "frontend/panels/overlay/OverlayTypes.h"

struct GLFWwindow;

namespace ProyecThor::UI {
    class AudioPanel;
    class Announcements;
    class OClock;
    class CapturePanel;
}

namespace ProyecThor::Core {

    class VLCBasePlayer;

    enum class ItemType { None = -1, Video = 0, Image = 1, Song = 2, Bible = 3, Documents = 4, Audio = 5 };

    // Contenido que manda la salida Inalambrica (LAN, ver NetworkStreamServer/
    // WireNetworkServerProviders) -- a diferencia de Publico/Stage, que hoy
    // siempre reflejan lo mismo que esta en vivo, LAN puede quedar "clavada"
    // en otra cosa (ej. solo el reloj) mientras Publico/Stage siguen
    // mostrando lo que este en vivo normalmente. Ver ViewPanel::RenderContent
    // (toolbar "vaPreviewSource") para el selector.
    enum class OutputContentMode { Live = 0, ClockOnly = 1, Blank = 2 };

    struct LibrarySelection {
        std::string title;
        ItemType type = ItemType::None;
        std::vector<std::string> contentData;
    };

    // Efectos visuales sobre el texto proyectado (Layer2) -- dibujados a
    // mano en capas con ImDrawList (sin FBO/shader, ver TextEffectsRenderer.h):
    // fondo detras del bloque, borde = copias offset en anillo antes del
    // texto, sombra = una copia offset, aberracion cromatica = copias R/G/B
    // desfasadas, glow/neon = varias copias a radios crecientes y alpha
    // decreciente (mismo truco que el halo de los pads MIDI de ViewPanel),
    // subrayado = una linea bajo el bloque de texto. Cada uno con un solo
    // slider de intensidad, mismo criterio "un control" que Grain/Vignette.
    struct TextEffectsData {
        bool  bgEnabled = false;
        float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.55f };

        bool  borderEnabled = false;
        float borderColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float borderWidth = 0.4f; // 0..1

        bool  shadowEnabled = true; // default: mismo comportamiento que antes
        float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.7f };
        float shadowIntensity = 0.4f; // 0..1

        bool  chromaticAberrationEnabled = false;
        float chromaticAberrationIntensity = 0.4f; // 0..1

        bool  glowEnabled = false; // "bloom"
        float glowColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float glowIntensity = 0.5f; // 0..1

        bool  neonEnabled = false;
        float neonColor[4] = { 0.15f, 0.9f, 1.0f, 1.0f };
        float neonIntensity = 0.6f; // 0..1

        bool  underlineEnabled = false;
        float underlineColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float underlineThickness = 0.3f; // 0..1

        // Texto 3D -- copias escalonadas en diagonal detras del texto
        // principal (extrusion "solida"), mismo truco que Sombra pero con
        // muchos pasos en vez de uno solo. Ver DrawStyledText.
        bool  text3dEnabled = false;
        float text3dColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float text3dDepth = 0.4f; // 0..1

        // Degradado de color -- interpola entre dos colores a lo largo del
        // angulo indicado (mismo esquema angulo->direccion->proyeccion que
        // ya usa la herramienta "Degradado" del editor de Overlays, ver
        // OverlayCanvasEditor::ApplyGradientPreview). Reemplaza el color
        // solido del texto cuando esta activo.
        bool  gradientEnabled = false;
        float gradientColorA[4] = { 1.0f, 0.35f, 0.35f, 1.0f };
        float gradientColorB[4] = { 0.35f, 0.45f, 1.0f, 1.0f };
        float gradientAngle = 0.0f; // grados, -180..180

        // Transparencia con angulo -- degradado de OPACIDAD (no de color)
        // a lo largo del angulo indicado, mismo esquema que el degradado de
        // color de arriba pero modulando solo el alpha.
        bool  opacityGradientEnabled = false;
        float opacityGradientAngle = 0.0f; // grados, -180..180
        float opacityGradientStrength = 0.5f; // 0..1
    };

    // TextBoxStyle -- recuadro de texto independiente (Letras o Versiculo),
    // editable a mano en el editor visual (ver CanvaStyleEditor). posX/posY
    // son el CENTRO del recuadro, normalizados 0..1 (misma convencion que
    // OverlayLayer::posX/posY); sizeW/sizeH tambien normalizados 0..1.
    // hAlign/vAlign alinean el texto DENTRO del recuadro (0 izq/arriba,
    // 1 centro, 2 der/abajo).
    struct TextBoxStyle {
        float       posX          = 0.5f;
        float       posY          = 0.5f;
        float       sizeW         = 0.92f;
        float       sizeH         = 0.89f;
        std::string fontName      = "Predeterminada";
        float       color[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        float       textSize      = 60.0f;
        int         hAlign        = 1;
        int         vAlign        = 1;
        bool        autoScale     = true;
        TextEffectsData effects;

        // Fondo de pantalla propio del recuadro -- OPCIONAL, imagen elegida
        // de la misma biblioteca de fondos que ya usa el resto de la app
        // (ver ListSongBackgrounds(), carpeta assets/backgrounds). Por
        // defecto deshabilitado (fondo transparente).
        bool        bgMediaEnabled = false;
        std::string bgMediaPath;
        float       bgMediaOpacity = 1.0f;
    };

    struct SavedStyle {
        std::string name;
        float       size          = 60.0f;
        float       color[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int         hAlign        = 1;
        int         vAlign        = 1;
        float       margins[4]    = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool        autoScale     = true;
        std::string fontName      = "Predeterminada";
        TextEffectsData effects;

        // Caja de Letras (ver TextBoxStyle) -- fuente real que consume
        // DrawTextBlock/DrawPublicContent para TODO el contenido proyectado
        // (canciones Y el cuerpo de un versiculo biblico: ambos son "texto"
        // con el mismo diseno). Los campos planos de arriba se conservan
        // solo como espejo de compatibilidad (StreamSnapshot/cliente
        // remoto, ver PresentationCore::UpdateLyricsBoxStyle).
        TextBoxStyle lyrics;

        // Caja del Indice -- OPCIONAL (ver indexEnabled), muestra solo la
        // referencia biblica (ej. "Genesis 1:1"), nunca el cuerpo del
        // versiculo. Independiente en posicion/tamano/estilo de la caja de
        // Letras -- el usuario la activa y la mueve a donde quiera.
        TextBoxStyle index;
        bool         indexEnabled = false;
    };

    // Empaqueta/desempaqueta TextEffectsData como una sola linea CSV para el
    // formato "key=value" de los archivos .theme -- usado tanto por
    // PresentationCore::SaveStyle/GetSavedStyle (el catalogo de estilos) como
    // por LayersStyleTab::SaveTheme/LoadThemeData (el editor real), que leen
    // y escriben los MISMOS archivos con parsers independientes. Ver
    // implementacion en PresentationCore.cpp.
    std::string PackTextEffects(const TextEffectsData& e);
    void        UnpackTextEffects(const std::string& v, TextEffectsData& e);

    struct PresentationState {
        // Audio: "now playing" (disco + caratula + ondas) — ver
        // PresentationCore::SetBackgroundAudio() y AudioPanel::RenderLiveBackground().
        enum class BackgroundType { SolidColor, Video, Audio };
        BackgroundType bgType = BackgroundType::SolidColor;
        std::string bgPath;
        float bgColor[3] = { 0.0f, 0.0f, 0.0f };

        bool isProjecting       = false;
        int  targetMonitorIndex = 0;

        // Monitores de salida publica ADICIONALES (opcional) -- espejo
        // runtime de Settings::ProjectionSettings::extraMonitors, poblado en
        // PresentationCore::SetTargetMonitor. Todos muestran exactamente lo
        // mismo que targetMonitorIndex -- ver UIManager::RenderProjectorOutput.
        std::vector<int> extraTargetMonitors;

        // Monitor de Control (Stage Display). Independiente de isProjecting:
        // el Stage puede estar activo con o sin proyeccion publica.
        bool isStaging          = false;
        int  stageMonitorIndex  = 0;

        // Idem extraTargetMonitors, para Stage -- poblado en SetStaging.
        std::vector<int> extraStageMonitors;

        std::string currentText;
        bool  showText          = false;

        // Referencia biblica del versiculo actual (ej. "Genesis 1:1"),
        // SEPARADA del cuerpo (currentText) -- ver PresentationCore::
        // SetCurrentRef. Solo se dibuja si indexEnabled es true (ver
        // indexBox mas abajo). Vacio para cualquier contenido que no sea
        // un versiculo biblico (SetLayer2_Text la limpia automaticamente).
        std::string currentRef;

        // Texto que vendra despues del actual (siguiente estrofa/versiculo),
        // solo para el Stage Display — nunca se muestra al publico.
        std::string nextText;

        // Fondo/video (Layer0) UNICAMENTE — el crossfade de BackgroundLayer
        // es propio y automatico (ver BackgroundLayer::Update/Render), no
        // depende de esto para nada visual. Se mantiene solo por si algo
        // externo (ej. LAN) necesita saber que el fondo cambio.
        uint64_t transitionTrigger  = 0;

        // Letras (Layer2) UNICAMENTE — es el unico disparador real de
        // TransitionPanel (ver UIManager::RenderAll). Separado de
        // transitionTrigger para que un cambio de fondo/video NUNCA anime
        // el texto, ni al reves (antes compartian un solo contador).
        uint64_t textTransitionTrigger = 0;

        int      transitionType     = 0;
        float    transitionDuration = 1.0f;

        float textSize          = 60.0f;
        float textColor[4]      = { 1.0f, 1.0f, 1.0f, 1.0f };
        int   textAlignment     = 1;
        int   vAlignment        = 1;
        float margins[4]        = { 50.0f, 50.0f, 50.0f, 50.0f };
        bool  autoScale         = true;
        std::string selectedFont = "Predeterminada";
        TextEffectsData effects;

        float refTextSize   = 28.0f;
        float verseTextSize = 60.0f;

        int songTextAlignment  = 1;
        int songVAlignment     = 1;
        int bibleTextAlignment = 1;
        int bibleVAlignment    = 1;

        // Caja de Letras -- fuente real que consumen DrawTextBlock
        // (UIManager.cpp) y DrawPublicContent (LiveContentRenderer.cpp) al
        // proyectar currentText (canciones Y el cuerpo de un versiculo
        // biblico, ambos con el mismo diseno). Los campos planos de arriba
        // (textSize/textColor/textAlignment/vAlignment/margins/autoScale/
        // selectedFont/effects) quedan como espejo de solo-lectura de
        // lyricsBox, mantenido por PresentationCore::UpdateLyricsBoxStyle,
        // para no romper a nada que ya los lea (Clock, QuickNotes,
        // ViewPanel::Pad, SyncServer, StreamSnapshot/cliente remoto).
        TextBoxStyle lyricsBox;

        // Caja del Indice -- OPCIONAL, dibuja SOLO currentRef (la
        // referencia biblica, ej. "Genesis 1:1"), nunca currentText. Se
        // dibuja unicamente si indexEnabled es true.
        TextBoxStyle indexBox;
        bool         indexEnabled = false;

        float livePosition      = 0.0f;
        int   liveVolume        = 100;
        bool  liveMuted         = false;
        bool  liveLoop          = false;

        std::string quickNoteText;
        bool showQuickNote = false;

        std::string lanQuickNoteText;
        bool        showLanQuickNote = false;

        bool        isStreamingNet = false;
        std::string networkURL;

        bool        isChatRunning = false;
        std::string chatURL;
    };

    class PresentationCoreImpl;

    class PresentationCore {
    public:
        static PresentationCore& Get() {
            static PresentationCore instance;
            return instance;
        }
void SetGlobalMute(bool mute);
    bool GetGlobalMute() const;
        PresentationCore();
        ~PresentationCore();

        PresentationCore(const PresentationCore&)            = delete;
        PresentationCore& operator=(const PresentationCore&) = delete;

        void Update();

        void RenderBackground(int outputW, int outputH);
        void RenderProjectorWindow(); // dibuja el contenido en vivo (background, no la ventana en si)
        PresentationState GetState();
        void ApplyStyleByName(const std::string& styleName);
        void ApplyStyleSnapshot(const SavedStyle& style); // aplica un snapshot directo (ver ViewPanel::ApplyPad), sin pasar por el catalogo de estilos guardados
        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;

        // Ver comentario de OutputContentMode arriba. Leido desde el hilo de
        // NetworkStreamServer (SnapshotProvider) Y desde RenderProjectorToFBO
        // (hilo de render), por eso atomic en vez de sumarlo a m_Mutex.
        void               SetLanContentMode(OutputContentMode mode) { m_LanContentMode = mode; }
        OutputContentMode  GetLanContentMode() const { return m_LanContentMode.load(); }

        // "Bucle falso" de Fondos (ver Ajustes > Proyeccion > Fondos y el
        // comentario largo en BackgroundLayer.h): reproduce hacia adelante
        // y despues "hacia atras" en vez de siempre cortar al mismo frame
        // 0, para disimular el salto de un loop real. Solo afecta a Fondos
        // (nunca a Videos/cola).
        void  SetBackgroundPingPongLoop(bool enabled);
        bool  GetBackgroundPingPongLoop() const;

        void ClearQuickNote();

        void SetTransitionConfig(int type, float durationSeconds);
        void SetBackgroundTransitionProgress(float progress);

        // Duracion del crossfade de fondo (BackgroundLayer::m_BlendSeconds)
        // -- sincronizada cada frame desde UIManager segun el preset de
        // transicion activo (ver TransitionPanel::AffectsBackground).
        void SetBackgroundBlendDuration(float seconds);

        void SetLiveQuickNote(const std::string& text, const float* colorOverride = nullptr);
        void SetLiveQuickNoteLAN(const std::string& text, const float* colorOverride = nullptr, const std::string& styleName = "");
        void ClearQuickNoteLAN();

        // ── Reloj y Contadores: títulos/mensajes pedidos desde el celular ──
        // Cola thread-safe (SyncServer corre en su propio hilo httplib, ver
        // POST /remote/clock-message) de textos para agregar a la lista de
        // títulos de OClock (m_Titles) -- OClock::Update() la drena una vez
        // por frame y los agrega tal cual si el operador hubiese apretado
        // "Agregar" a mano, activándolos de inmediato. Empty vector = nada
        // pendiente (caso normal).
        void PushRemoteClockTitle(const std::string& text);
        std::vector<std::string> DrainRemoteClockTitles();

        void*          GetBackgroundTexture();
        void*          GetProcessedBackgroundTexture(int targetW, int targetH);

        // Igual que GetProcessedBackgroundTexture, pero además corre los
        // mismos efectos de CompositePostChain (CRT/Grano/FXAA/Saturación/
        // Viñetado) a la resolución del preview -- para que el recuadro del
        // operador (ViewPanel/Monitor de Control) sea un reflejo fiel de lo
        // que ve el público, en vez de mostrar siempre el fondo sin
        // procesar (esos efectos antes SOLO corrian sobre la viewport real
        // "ProjectorLive"). Ver CompositePostChain::ProcessBackgroundForPreview.
        void*          GetPreviewBackgroundTexture(int targetW, int targetH);

        // "Rellenado" de letterbox/pillarbox: version muy desenfocada del
        // fondo, a pantalla completa, para dibujar DETRAS del contenido
        // nitido en vez de barras negras. nullptr si esta desactivado.
        void*          GetBackgroundFillTexture(int workW, int workH);
        void           SetFillBlurEnabled(bool enabled);
        bool           GetFillBlurEnabled() const;
        void           SetFillBlurBrightness(float v);
        float          GetFillBlurBrightness() const;

        VLCBasePlayer* GetBackgroundPlayer();
        void           GetBackgroundVideoSize(int& width, int& height);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        // Escalador alternativo exclusivo de NVIDIA (ver PostProcessorNIS.h
        // para la justificacion de por que es un fragment shader propio en
        // vez del compute shader original de NVIDIA). Mutuamente excluyente
        // con FSR (arriba): SetNISEnabled(true) apaga FSR y viceversa, no
        // tiene sentido correr los dos upscalers de la misma etapa a la vez.
        void  SetNISEnabled(bool enabled);
        bool  GetNISEnabled() const;
        void  SetNISSharpness(float sharpness);
        float GetNISSharpness() const;

        // Motor de renderizado del fondo de video (Ajustes > Proyeccion):
        // 0 = OpenGL compuesto (default, con overlays/texto encima), 1 =
        // VLC en ventana nativa (sin overlays/texto, ver BackgroundLayer::
        // SetUseNativeEngine para el detalle completo de las limitaciones
        // de este modo).
        void SetVideoRenderEngine(int engine);
        int  GetVideoRenderEngine() const;

        // ── Post-proceso del composite completo de "ProjectorLive" (fondo +
        //    overlays + texto + anuncios + captura) — ver CompositePostChain.h
        //    para la arquitectura. A diferencia de FSR (arriba), estos no
        //    hacen upscale: son filtros a resolucion de salida.
        void  SetCRTEnabled(bool enabled);
        bool  GetCRTEnabled() const;
        void  SetCRTScanlineIntensity(float intensity);
        float GetCRTScanlineIntensity() const;

        void  SetGrainEnabled(bool enabled);
        bool  GetGrainEnabled() const;
        void  SetGrainIntensity(float intensity);
        float GetGrainIntensity() const;

        void  SetFXAAEnabled(bool enabled);
        bool  GetFXAAEnabled() const;

        void  SetSaturationEnabled(bool enabled);
        bool  GetSaturationEnabled() const;
        void  SetSaturationAmount(float amount);
        float GetSaturationAmount() const;

        void  SetVignetteEnabled(bool enabled);
        bool  GetVignetteEnabled() const;
        void  SetVignetteIntensity(float intensity);
        float GetVignetteIntensity() const;

        void  SetBlurEnabled(bool enabled);
        bool  GetBlurEnabled() const;
        void  SetBlurIntensity(float intensity);
        float GetBlurIntensity() const;

        void  SetSharpenEnabled(bool enabled);
        bool  GetSharpenEnabled() const;
        void  SetSharpenIntensity(float intensity);
        float GetSharpenIntensity() const;

        void  SetBloomEnabled(bool enabled);
        bool  GetBloomEnabled() const;
        void  SetBloomIntensity(float intensity);
        float GetBloomIntensity() const;

        void  SetChromaticAberrationEnabled(bool enabled);
        bool  GetChromaticAberrationEnabled() const;
        void  SetChromaticAberrationIntensity(float intensity);
        float GetChromaticAberrationIntensity() const;

        void  SetVHSEnabled(bool enabled);
        bool  GetVHSEnabled() const;
        void  SetVHSIntensity(float intensity);
        float GetVHSIntensity() const;

        // tint: 0=rojo, 1=verde, 2=azul (ver PostProcessorCine::Tint).
        void  SetCineEnabled(bool enabled);
        bool  GetCineEnabled() const;
        void  SetCineIntensity(float intensity);
        float GetCineIntensity() const;
        void  SetCineTint(int tint);
        int   GetCineTint() const;

        void  SetContrastEnabled(bool enabled);
        bool  GetContrastEnabled() const;
        void  SetContrastAmount(float amount);
        float GetContrastAmount() const;

        void  SetLuminosityEnabled(bool enabled);
        bool  GetLuminosityEnabled() const;
        void  SetLuminosityAmount(float amount);
        float GetLuminosityAmount() const;

        // TAA (Temporal Anti-Aliasing simplificado, ver PostProcessorTAA.h):
        // mezcla el frame actual con el resultado del frame anterior.
        void  SetTAAEnabled(bool enabled);
        bool  GetTAAEnabled() const;
        void  SetTAAIntensity(float intensity);
        float GetTAAIntensity() const;

        // Usado por UIManager (justo tras ImGui::Begin("ProjectorLive",...))
        // para informar, cada frame, cual ImGuiID es esa viewport, y por el
        // override de Renderer_RenderWindow en main.cpp para preguntar si el
        // viewport que esta por dibujarse es esa (y no "StageLive" ni un
        // panel flotante cualquiera) antes de desviar su render hacia
        // RenderProjectorViewportPostFX.
        void   SetProjectorPostFXViewportID(ImGuiID id);
        bool   IsProjectorPostFXViewport(ImGuiID id) const;

        // HWND (como void*) de la ventana nativa real de "ProjectorLive" --
        // resuelto a partir de m_ProjectorPostFXViewportID (ver arriba).
        // Para consumidores que necesitan mostrar contenido nativo (ej. un
        // navegador embebido, ver WebBrowserPanel::SendToPublic) DIRECTO
        // sobre la salida real al publico, reparentando su propia ventana
        // nativa a esta. nullptr si no se esta proyectando o la ventana
        // todavia no existe este frame.
        void* GetProjectorNativeWindow() const;
        void   RenderProjectorViewportPostFX(ImGuiViewport* viewport,
                                              void (*defaultRenderFn)(ImGuiViewport*, void*));

        // ── Post-proceso para monitores de salida EXTRA (multi-monitor) ────
        // Misma cadena de efectos que el primario (ver los 14 setters de
        // arriba, que ahora tambien aplican a cada instancia de este mapa),
        // pero con su PROPIA instancia de CompositePostChain por viewport
        // extra -- reusar una sola instancia entre varios viewports en el
        // mismo frame thrashearia su FBO interno (ver CompositePostChain::
        // EnsureSized, detecta cambio de platformHandle y recrea el buffer).
        void RegisterExtraProjectorViewport(ImGuiID id);
        bool IsExtraProjectorViewport(ImGuiID id) const;
        void RenderExtraProjectorViewportPostFX(ImGuiID id, ImGuiViewport* viewport,
                                                 void (*defaultRenderFn)(ImGuiViewport*, void*));
        // Borra del mapa cualquier instancia cuyo id no este en la lista
        // (limpia memoria GPU cuando el usuario destildo un monitor extra o
        // dejo de proyectar) -- llamar una vez por frame tras dibujar todos
        // los extras del frame actual.
        void PruneExtraProjectorViewports(const std::vector<ImGuiID>& stillActiveThisFrame);

        // fromQueue=true: la seleccion viene de MonitorQueueEngine::PlayIndex
        // (solo para mostrar el titulo del item actual de la cola), NO de un
        // click manual del operador en la Biblioteca. MonitorView/MediaView
        // usan IsSelectionFromQueue() para NO disparar su propio "cargar en
        // preview" en ese caso — sin esto, cada avance de la cola hacia que
        // el reproductor de Preview intentara abrir el MISMO archivo que ya
        // esta en vivo (o precargandose en standby), dos instancias de VLC
        // abriendo el mismo archivo a la vez, lo que crasheaba en Windows.
        void             SetSelection(const LibrarySelection& selection, bool fromQueue = false);
        LibrarySelection GetSelection();
        LibrarySelection PeekSelection();
        bool             IsSelectionFromQueue() const { return m_SelectionFromQueue; }

        void StopBackgroundMedia();
        void BlockBackgroundPath(const std::string& path);
        void UnblockBackgroundPath();

        void SetLayer0_Color(float r, float g, float b);

        void SetLayer2_Text(const std::string& text);
        void ClearLayer2();
        void SetNextText(const std::string& text); // vista previa para el Stage Display, nunca al publico

        // Cue de cambio de estilo pendiente para OClock. ConsumeClockStyleCue()
        // devuelve "" si no hay ninguna pendiente. Patron "consumir una vez",
        // pensado originalmente para que un disparador externo (ej. un
        // secuenciador de cues) empuje un cambio de estilo sin acoplarse
        // directo a OClock.
        void        SetClockStyleCue(const std::string& styleName);
        std::string ConsumeClockStyleCue();

        // Transicion pendiente para la proxima vez que se dispare una (ver
        // TransitionPanel::Trigger(), que la consume). Se guarda como string,
        // no como UI::TransitionType, para que backend/core no dependa de
        // frontend/panels; el mapeo nombre<->enum vive en TransitionPanel.cpp.
        void SetPendingTransitionOverride(const std::string& name, float duration);
        bool ConsumePendingTransitionOverride(std::string& outName, float& outDuration);

        // Cue "consumir una vez" para que el rework del editor de canciones
        // pueda abrir el editor unificado directamente tras crear una
        // cancion nueva, sin popup modal — ver CreateNewSong (LibrarySongs.cpp)
        // y SongView::Render (que hace ConsumeSongEditorOpenRequest cada
        // frame y compara contra la seleccion actual). Mismo patron que
        // SetClockStyleCue/ConsumeClockStyleCue arriba.
        void        RequestSongEditorOpen(const std::string& filename);
        bool        ConsumeSongEditorOpenRequest(std::string& outFilename);

        void*          GetPreviewTexture();
        VLCBasePlayer* GetPreviewPlayer();
        void           SetPreviewMedia(const std::string& path);
        void           StopPreviewMedia();

        // Pide cargar/detener el reproductor de Preview en un hilo aparte
        // (ver PreviewLoadWorker) — a diferencia de llamar Play()/Stop()
        // directo sobre GetPreviewPlayer(), esto NUNCA bloquea el hilo
        // principal (el que actualiza/dibuja el video en vivo al publico).
        // Preferir esto sobre GetPreviewPlayer()->Play()/Stop() en
        // cualquier codigo de UI que reaccione a una seleccion cambiante.
        void RequestPreviewLoad(const std::string& path, bool loop, bool startMuted);
        void RequestPreviewStop();
        void StopPreviewSync();
        void ClearSelection();
        void ReleasePathUsages(const std::string& path);

        // Aplica la caja de Letras: guarda state.lyricsBox TAL CUAL y
        // ademas espeja sus campos hacia los campos planos legacy de
        // PresentationState (textSize/textColor/textAlignment/vAlignment/
        // margins/autoScale/selectedFont/effects) para que todo lo que ya
        // los lee (Clock, QuickNotes, ViewPanel::Pad, SyncServer,
        // StreamSnapshot/cliente remoto) siga funcionando sin cambios.
        void UpdateLyricsBoxStyle(const TextBoxStyle& box);

        // Aplica la caja del Indice y si esta habilitado o no. Sin espejo --
        // el indice nunca tuvo un camino de compatibilidad separado de Letras.
        void UpdateIndexBoxStyle(const TextBoxStyle& box, bool enabled);

        // Referencia biblica (ej. "Genesis 1:1"), SEPARADA del cuerpo del
        // texto (ver SetLayer2_Text, que limpia esto automaticamente para
        // cualquier contenido que no sea un versiculo). Solo BibleView y
        // SyncServer (proyeccion remota de versiculos) llaman esto.
        void SetCurrentRef(const std::string& ref);

        TextEffectsData GetTextEffects() const;

        void        SetProjecting(bool projecting);
        bool        IsProjecting() const;
        void        SetTargetMonitor(int index);
        void        SetProjectorSize(int w, int h);

        // Monitor de Control (Stage Display). El contenido real se dibuja en
        // el viewport ImGui "StageLive" de UIManager.cpp, leyendo estos campos.
        void        SetStaging(bool active, int monitorIndex = -1);
        bool        IsStaging() const;

        // ── Ventana principal ────────────────────────────────────────────
        // Necesaria para poder crear ventanas secundarias con contexto GL
        // compartido (texturas/shaders/buffers; VAO/FBO no se comparten,
        // ver BackgroundLayer.cpp). Se setea una vez desde main() apenas
        // se crea la ventana principal.
        void SetMainWindow(GLFWwindow* mainWindow) { m_MainWindow = mainWindow; }

        // ── Ventanas secundarias, API generica ──────────────────────────
        // Cualquier salida adicional (proyector, stage, un segundo stage
        // en otro monitor a futuro, etc.) se identifica por un id de
        // string unico. Agregar una N-esima ventana de salida en el
        // futuro (multi-monitor) es simplemente otro llamado a esto con
        // un id nuevo, no hay que tocar la clase.
        bool CreateSecondaryWindow(const std::string& id, int monitorIndex,
                                    const std::string& title,
                                    SecondaryOutputWindow::RenderFn renderFn);
        void DestroySecondaryWindow(const std::string& id);
        void DestroyAllSecondaryWindows();
        bool IsSecondaryWindowActive(const std::string& id) const;

        // Llamar UNA VEZ POR FRAME desde main(), DESPUES de core.Update(),
        // para refrescar todas las ventanas secundarias activas.
        void RenderAllSecondaryWindows();

        // ── Atajos con nombre fijo para los casos conocidos hoy ─────────
        bool        CreateProjectorWindow(int monitorIndex);
        void        DestroyProjectorWindow();
        bool        IsProjectorWindowActive() const;
        GLFWwindow* GetProjectorWindow() const;

        float GetLivePosition();
        void  SetLivePosition(float pos);
        int   GetLiveVolume();
        bool  GetLiveMute();
        void  SetLiveVolume(int volume);
        void  SetLiveMute(bool mute);

        // Ecualizador de 10 bandas sobre el audio en vivo (ver
        // BackgroundLayer::SetLiveEqualizer*/VLCBasePlayer::SetEqualizer*).
        // Sin getters: el estado "de verdad" (para dibujar los sliders) vive
        // en la UI que los llama (ver MonitorView), igual criterio que ya
        // usa AudioPanel con su propio ecualizador.
        void SetLiveEqualizerEnabled(bool enabled);
        void SetLiveEqualizerPreamp(float preampDb);
        void SetLiveEqualizerBand(int index, float ampDb);
        // Loop del player "general" (bg/PROGRAM). Antes era un bool local de
        // MonitorView; se subio al estado compartido porque el toggle (en
        // Monitor, ver MonitorCenterColumn) y el enforcement del auto-restart
        // al llegar al final (en ViewPanel, ver RenderLiveTransport) ahora
        // viven en dos clases distintas.
        bool  GetLiveLoop();
        void  SetLiveLoop(bool loop);

        void        LoadFontsIntoImGui();
        void        LoadSingleFontIntoImGui(const std::string& fontPath);
        void        SyncFontListFromDisk(std::vector<std::string>& outList);
        std::string GetActiveFontName() const;
        ImFont*     GetImGuiFont(const std::string& fontName, float size = 0.0f);
        std::string GetActiveFontFilePath() const;

        void                     SaveStyle(const SavedStyle& style);
        void                     DeleteStyle(const std::string& name);
        std::vector<std::string> GetSavedStyleNames() const;
        bool                     GetSavedStyle(const std::string& name, SavedStyle& outStyle) const;

        void        SetCategoryDefaultStyle(ItemType category, const std::string& styleName);
        std::string GetCategoryDefaultStyle(ItemType category) const;
        void        LoadCategoryStyles();
        void        SaveCategoryStyles() const;

        void ToggleNetworkStream(bool enable, int port = 8080);
        bool IsStreamingNet() const;
        bool RenderProjectorToFBO(int w, int h, std::vector<uint8_t>& outRGB);

        // Renderiza el FONDO actualmente en vivo (Publico -- video/imagen/
        // color, ver BackgroundLayer::Render) a una textura GL reusable
        // (mismo FBO que RenderProjectorToFBO, ver EnsureFBO) y devuelve su
        // ID directamente, sin el paso de lectura a CPU/PBO -- para
        // consumidores que solo necesitan mostrarlo como una textura mas
        // (ej. BroadcastPanel, capa "Vista en vivo"). Alcance igual que
        // RenderProjectorToFBO: solo el fondo, sin texto/overlay encima.
        // 0 si no hay contexto o el tamaño pedido es invalido.
        unsigned int RenderPublicCompositeToTexture(int w, int h);

        NetworkStreamServer* GetNetworkServer() { return m_NetworkServer.get(); }

        // Chat y Streaming comparten el MISMO NetworkStreamServer/puerto (ver
        // ChatMessageStore.h para el porque) — cualquiera de los dos puede
        // arrancarlo si todavia no esta corriendo; apagar uno no lo tira
        // abajo si el otro todavia lo esta usando.
        void ToggleChatServer(bool enable, int port = 8080);
        bool IsChatRunning() const;
        ChatMessageStore* GetChatMessageStore() { return &m_ChatMessageStore; }

       void PushFrame(std::vector<uint8_t> jpegData)
{
    // Un jpegData vacio significa que no hay frame real disponible
    // (fallo de captura/compresion). En ese caso hasFrame debe quedar
    // en false para que el cliente muestre el color solido de fondo
    // en vez de un JPEG corrupto. Si trae datos, hasFrame pasa a true.
    bool hasRealFrame = !jpegData.empty();

    {
        std::lock_guard<std::mutex> lk(m_FrameMutex);
        m_LatestFrame = std::move(jpegData);
    }
    m_FrameProviderActive.store(hasRealFrame);
    ++m_StreamVersion;
}

        // Default false: los fondos decorativos (Fondos/BackgroundsPanel) nunca
        // deben sonar. Solo los flujos de "enviar al monitor" pasan
        // allowAudio=true explicitamente (ver MonitorCenterColumn,
        // MonitorQueueEngine, LibraryVideos "Enviar al monitor").
        void SetBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);
        bool GetContentAllowsAudio() const;

        // ── Overlay (PNG transparente) ───────────────────────────────────────
        // Capa APARTE de Layer0 (fondo) y Layer2 (texto): se dibuja ENCIMA de
        // los dos, dejando ver lo que haya debajo gracias al canal alpha real
        // del PNG (a diferencia de SetBackgroundMedia, que REEMPLAZA el
        // fondo). Se compone tanto en la salida real ("ProjectorLive", ver
        // UIManager.cpp) como en el preview (LiveContentRenderer::
        // DrawPublicContent, usado por Vista en Vivo y el mirror de Stage).
        void        SetOverlayMedia(const std::string& pngPath);
        void        ClearOverlay();
        bool        HasOverlay() const;
        std::string GetOverlayPath() const;
        void*       GetOverlayTexture(); // GLuint cacheado, cargado on-demand desde el PNG

        // ── Cuadro de reloj del overlay activo ───────────────────────────────
        // Se fija una vez al activar un overlay (ver OverlayLibraryTab::
        // RenderCard/RenderRow), a partir de la capa Clock que tenga su
        // receta (.overlay) -- si no tiene ninguna, hasClock=false y no se
        // dibuja nada. El TEXTO en cambio se publica todos los frames desde
        // OClock::Update()/SyncTransmission(), independiente de si hay o no
        // overlay activo (publicar es inofensivo: el render solo lo usa si
        // HasOverlayClockLayer() es true). Ver LiveContentRenderer.cpp/
        // UIManager.cpp para donde se dibuja.
        void        SetOverlayClockLayer(bool hasClock, const ProyecThor::UI::OverlayLayer& layer,
                                          int canvasW, int canvasH);
        bool        HasOverlayClockLayer() const;
        ProyecThor::UI::OverlayLayer GetOverlayClockLayer() const;
        int         GetOverlayClockCanvasW() const;
        int         GetOverlayClockCanvasH() const;

        void         SetLiveOverlayClockText(const std::string& text, const float* colorOverride = nullptr);
        std::string  GetLiveOverlayClockText() const;
        bool         HasLiveOverlayClockColorOverride() const;
        void         GetLiveOverlayClockColorOverride(float outRGBA[4]) const;

        // ── Fondo "now playing" (disco + caratula + ondas) ──────────────────
        // Manda el bgType a Audio y para cualquier video/color previo (mismo
        // criterio que StopBackgroundMedia) — quien realmente dibuja el
        // visual es AudioPanel::RenderLiveBackground (ver GetAudioPanelRef),
        // esto solo prende el estado. Lo llama el boton "En vivo" del panel
        // de audio de la biblioteca.
        void SetBackgroundAudio();

        // Puntero no-propietario al panel de audio de la biblioteca (ver
        // LibraryPanel::GetAudioPanel), wireado una vez en main.cpp — mismo
        // patron que HomePanel::SetAudioPanel. Se usa para: (a) que
        // UIManager pueda pedirle que dibuje el fondo "now playing" en el
        // proyector real, y (b) que este mismo PresentationCore le avise si
        // hay que apagar su boton "En vivo" porque se mando otra cosa en
        // vivo desde otro lado.
        void              SetAudioPanelRef(ProyecThor::UI::AudioPanel* panel) { m_AudioPanelRef = panel; }
        ProyecThor::UI::AudioPanel* GetAudioPanelRef() const { return m_AudioPanelRef; }

        // Mismo patron que AudioPanelRef, pero estos tres se wirean solos
        // (cada panel dueño registra la direccion de su propio miembro en
        // su constructor — StylesHubPanel para Announcements/CapturePanel,
        // ViewToolsPanel para OClock — no hace falta tocar main.cpp). Se
        // usan desde ViewPanel para los botones "Limpiar X" especificos por
        // tipo de contenido, sin que ViewPanel necesite conocer StylesHubPanel
        // ni ViewToolsPanel directamente.
        void                        SetAnnouncementsRef(ProyecThor::UI::Announcements* a) { m_AnnouncementsRef = a; }
        ProyecThor::UI::Announcements* GetAnnouncementsRef() const { return m_AnnouncementsRef; }

        void                 SetOClockRef(ProyecThor::UI::OClock* c) { m_OClockRef = c; }
        ProyecThor::UI::OClock* GetOClockRef() const { return m_OClockRef; }

        void                       SetCapturePanelRef(ProyecThor::UI::CapturePanel* c) { m_CapturePanelRef = c; }
        ProyecThor::UI::CapturePanel* GetCapturePanelRef() const { return m_CapturePanelRef; }

        // ── Proyección de Modelos y Recursos 3D ─────────────────────────────
        void   SetLive3DModelActive(bool active) { m_Live3DModelActive = active; }
        bool   IsLive3DModelActive() const { return m_Live3DModelActive; }
        void   SetLive3DModelTexture(void* texID) { m_Live3DModelTexture = texID; }
        void*  GetLive3DModelTexture() const { return m_Live3DModelTexture; }

        // ── Preload adelantado (ver BackgroundLayer::Prefetch/CommitPrefetch) ──
        // Usado por la cola del Monitor para cargar el SIGUIENTE clip en
        // segundo plano mientras el actual sigue reproduciendose, sin
        // disparar la transicion visible (no toca transitionTrigger:
        // invisible para el operador hasta que se confirma con
        // CommitNextBackgroundMedia). El resultado es un corte instantaneo
        // en la transicion, en vez de recien abrir el archivo en ese momento.
        void PreloadNextBackgroundMedia(const std::string& path, bool allowAudio = false);
        void CommitNextBackgroundMedia(const std::string& path, bool isVideo, bool allowAudio = false);

        // Para el indicador de carga en el preview del operador (ver
        // ViewPanel) — nunca se muestra en la salida real al publico.
        bool  IsBackgroundSwapPending() const;
        float GetBackgroundSwapEta() const;

        // Crossfade del fondo (ver BackgroundLayer::Update/Render) expuesto
        // para que el rendering ImGui del proyector real ("ProjectorLive" en
        // UIManager.cpp) pueda blendear Active/Standby igual que ya hace
        // BackgroundLayer::Render() via BlitTexture — evita duplicar la
        // logica de CUANDO blendear, solo el COMO (ImGui::AddImage con tint
        // alpha en vez de BlitTexture con glBlendFunc).
        void*  GetStandbyBackgroundTexture();
        float  GetBackgroundBlendProgress() const;
        bool   IsBackgroundStandbyReady();

        // ── Logo / pantalla de carga PUBLICA (ver Ajustes > Proyeccion) ────
        // A diferencia de lo anterior, esto SI se muestra en la salida real
        // (ver BackgroundLayer::Render / RenderProjectorWindow / el bloque
        // "ProjectorLive" de UIManager.cpp) mientras haya algo cargando.
        void  SetLoadingLogoPath(const std::string& path);
        void* GetLoadingLogoTexture() const;
        int   GetLoadingLogoWidth()  const { return m_LoadingLogoW; }
        int   GetLoadingLogoHeight() const { return m_LoadingLogoH; }
        bool  ShouldShowLoadingScreen() const;

        // Textura GL del fondo de pantalla opcional de una caja (ver
        // TextBoxStyle::bgMediaEnabled/bgMediaPath). isLyrics distingue el
        // slot de cache a usar (Letras vs Indice). Devuelve 0 si path
        // esta vacio o no se pudo cargar. Recarga solo si el path cambio.
        unsigned int GetBoxBgTexture(bool isLyrics, const std::string& path);

    private:
        void RenderDefaultStyleCombo();
        void EnsureFBO(int w, int h);
        void DestroyFBO();

        std::string ResolveFontFilePath(const std::string& fontName) const;
bool m_GlobalMuted = false;
        unsigned int m_FBO          = 0;
        unsigned int m_FBOTex       = 0;
        unsigned int m_FBORenderBuf = 0;
        int          m_FBOWidth     = 0;
        int          m_FBOHeight    = 0;
        double       m_LastFBOCaptureTime = 0.0;

        unsigned int m_PBO[2] = { 0, 0 };
        int          m_PBOIndex = 0;

        bool m_stretchToFill = false;
        ImGuiID m_ProjectorPostFXViewportID = 0;
        std::unique_ptr<PresentationCoreImpl> m_Impl;

        // Ver SetOverlayMedia/ClearOverlay/HasOverlay/GetOverlayPath -- la
        // textura GL en si vive en PresentationCoreImpl (m_Impl), esto solo
        // guarda la ruta/estado bajo m_Mutex, igual que m_State.bgPath.
        std::string m_OverlayPath;
        bool        m_HasOverlay = false;

        // Ver SetOverlayClockLayer/SetLiveOverlayClockText -- posicion/estilo
        // se fija al activar un overlay, el texto se actualiza cada frame
        // desde OClock (ver comentario en el header publico de arriba).
        ProyecThor::UI::OverlayLayer m_OverlayClockLayer;
        bool        m_HasOverlayClockLayer  = false;
        int         m_OverlayClockCanvasW   = 1920;
        int         m_OverlayClockCanvasH   = 1080;
        std::string m_LiveOverlayClockText;
        bool        m_HasLiveOverlayClockColorOverride = false;
        float       m_LiveOverlayClockColorOverride[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        // Ver SetLiveQuickNoteLAN / ClearQuickNoteLAN -- estilo y color override
        // exclusivos para la salida LAN (reloj/contadores/notas LAN).
        std::string m_LiveQuickNoteLANStyleName;
        bool        m_HasLiveQuickNoteLANColorOverride = false;
        float       m_LiveQuickNoteLANColorOverride[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        mutable std::mutex m_Mutex;

        // Ver PushRemoteClockTitle/DrainRemoteClockTitles -- guardado bajo
        // el mismo m_Mutex de arriba, nada especial.
        std::vector<std::string> m_PendingClockTitles;

        PresentationState m_State;
        LibrarySelection  m_CurrentSelection;
        bool              m_SelectionFromQueue = false;

        // Cue de "cambiar estilo del reloj" pendiente. Patron "consumir una
        // vez", igual que ConsumeEndReached() en VLCBasePlayer:
        // OClock::Update() la lee y limpia cada frame, asi no compite con
        // que el operador cambie el estilo a mano desde el combo de OClock.
        std::string m_PendingClockStyleCue;
        bool        m_HasClockStyleCue = false;

        // Mismo patron "consumir una vez" que m_PendingClockStyleCue, para
        // una transicion pendiente (ver SetPendingTransitionOverride).
        std::string m_PendingTransitionName;
        float       m_PendingTransitionDuration = -1.0f;
        bool        m_HasTransitionOverride = false;

        // Ver RequestSongEditorOpen/ConsumeSongEditorOpenRequest arriba.
        std::string m_PendingSongEditorOpenFile;
        bool        m_HasSongEditorOpenRequest = false;

        // ── Logo (pantalla de carga, ver Ajustes > Proyeccion) ────────────
        // Textura GL cargada una sola vez (recargada si el path cambia),
        // mostrada a la salida real mientras ShouldShowLoadingScreen() es
        // true (ver SetLoadingLogoPath/GetLoadingLogoTexture).
        std::string  m_LoadingLogoPath;
        unsigned int m_LoadingLogoTex = 0;
        int          m_LoadingLogoW = 0;
        int          m_LoadingLogoH = 0;

        // ── Fondo de pantalla opcional por caja (Letras/Indice) ───────────
        // Mismo patron que el logo de arriba: una sola textura por caja,
        // recargada solo si el path cambia. Ver GetBoxBgTexture.
        std::string  m_LyricsBgTexPath;
        unsigned int m_LyricsBgTex = 0;
        std::string  m_IndexBgTexPath;
        unsigned int m_IndexBgTex = 0;

        int         m_ProjectorWidth  = 1920;
        int         m_ProjectorHeight = 1080;
        std::string m_ActiveFontName  = "Predeterminada";
        std::unordered_map<std::string, ImFont*>      m_ImGuiFonts;
        std::unordered_map<std::string, SavedStyle>   m_SavedStyles;
        std::unordered_map<int, std::string>          m_CategoryDefaultStyles;

        // ── Ventanas secundarias ─────────────────────────────────────────
        GLFWwindow* m_MainWindow = nullptr;

        struct SecondaryOutput {
            SecondaryOutputWindow           window;
            SecondaryOutputWindow::RenderFn renderFn;
        };
        std::unordered_map<std::string, SecondaryOutput> m_SecondaryWindows;
        mutable std::mutex m_SecondaryWindowsMutex;

        static constexpr const char* kProjectorId = "projector";

        // ── Streaming en red local ───────────────────────────────────────
        std::unique_ptr<NetworkStreamServer> m_NetworkServer;
        ChatMessageStore                     m_ChatMessageStore;

        // Arma los 3 providers (snapshot/frame/fuente) de un NetworkStreamServer
        // recien creado — lo llaman tanto ToggleNetworkStream como
        // ToggleChatServer cuando les toca crear el server compartido.
        void WireNetworkServerProviders(NetworkStreamServer& srv);

        // Ver SetAudioPanelRef/GetAudioPanelRef.
        ProyecThor::UI::AudioPanel*    m_AudioPanelRef    = nullptr;
        ProyecThor::UI::Announcements* m_AnnouncementsRef = nullptr;
        ProyecThor::UI::OClock*        m_OClockRef        = nullptr;
        ProyecThor::UI::CapturePanel*  m_CapturePanelRef  = nullptr;

        std::atomic<bool>              m_Live3DModelActive{ false };
        void*                          m_Live3DModelTexture = nullptr;

        // Unico lugar que escribe m_State.bgType: si se esta dejando Audio
        // por otra cosa, apaga el boton "En vivo" del panel de audio. Debe
        // llamarse con m_Mutex ya tomado por quien invoca.
        void SetBgTypeLocked(PresentationState::BackgroundType newType);
        std::atomic<uint64_t>                m_StreamVersion { 0 };

        mutable std::mutex    m_FrameMutex;
        std::vector<uint8_t>  m_LatestFrame;
        std::atomic<bool>     m_FrameProviderActive { false };

        std::atomic<OutputContentMode> m_LanContentMode { OutputContentMode::Live };
    };

} // namespace ProyecThor::Core