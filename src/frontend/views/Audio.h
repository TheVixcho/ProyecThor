#pragma once

#include "IPanel.h"
#include "audio/AudioAlbumArt.h"
#include "backend/media/VLCBasePlayer.h"
#include "backend/core/SubtitleImporter.h"
#include <string>
#include <vector>
#include <cstdint>
#include <thread>
#include <mutex>
#include <optional>

namespace ProyecThor::UI {

// ─── Datos de una pista ───────────────────────────────────────────────────────

struct AudioTrack {
    std::string filename;
    std::string displayName;
    std::string fullPath;

    // Color de acento procedural (hash del nombre, estable por pista)
    float accentH = 0.0f;

    // Portada embebida. Se extrae de forma lazy al reproducir la pista.
    // coverLoaded = false hasta que se intente la extraccion.
    ProyecThor::Audio::AlbumArt coverArt;
    bool coverLoaded = false;   // true = ya intentamos extraer (puede estar vacia)

    // ── Letra importada desde una URL (yt-dlp, ver SubtitleImporter) ────
    // Persistida en un sidecar "<fullPath>.lyrics.json" junto al archivo de
    // audio -- ver AudioPanel::LoadTrackLyricsSidecar/SaveTrackLyricsSidecar.
    // lyricsEnabled controla si se proyecta mientras esta pista esta en
    // vivo (ver AudioPanel::RefreshLiveLyrics), independiente de tenerla
    // guardada o no.
    std::string sourceUrl;
    std::string lyricsText;
    bool        lyricsEnabled = false;
};

enum class AudioRepeatMode { None, One, All };

// ─── Disco giratorio ──────────────────────────────────────────────────────────

struct SpinningDiscParams {
    float rotationAngle = 0.0f;
    float targetSpeed   = 0.0f;
    float currentSpeed  = 0.0f;
    float needleAngle   = -0.45f;
    bool  needleLifted  = true;
};

// ─── Estilo visual del "now playing" (disco/portada/ondas) ────────────────────
// Catalogo aparte del sistema de Estilos de texto (SavedStyle/TextBoxStyle,
// ver PresentationCore.h) -- ese es puramente tipografico y no tiene donde
// enchufar un tema visual de disco/ondas. Configurable desde el boton
// "Estilos" en la vista de reproductor (ver RenderPlayerView).
enum class AudioVisualStyle {
    Vinyl = 0,   // disco de vinilo giratorio + aguja (el de siempre)
    Minimal,     // portada cuadrada centrada, sin disco ni aguja
    Bars,        // solo ondas grandes centradas, sin disco ni portada
};

const char* AudioVisualStyleName(AudioVisualStyle style);

// ─── Panel de audio ───────────────────────────────────────────────────────────

class AudioPanel : public IPanel {
public:
    AudioPanel();
    ~AudioPanel() override;

    void Render() override;
    std::string GetName() const override { return "Audio"; }

    // Actualiza progreso, animaciones y waveform. Llamar cada frame.
    void Update();

    // Vista de biblioteca (lista de pistas + header)
    void RenderLibraryList();

    // Vista del reproductor (disco, controles, EQ) — se usa en HomePanel
    void RenderPlayerView();

    // ── "En vivo" en el proyector real ───────────────────────────────────
    // true mientras este panel es la fuente del fondo del proyector (ver
    // PresentationCore::SetBackgroundAudio/SetBgTypeLocked, que llama
    // SetLiveBackground(false) automaticamente si el operador manda otra
    // cosa en vivo desde otro lado — video, cancion, biblia).
    bool IsLiveBackground()      const { return m_IsLiveBackground; }
    // Ya no inline -- si pasa de true a false limpia la letra importada que
    // pudiera estar proyectandose (ver .cpp), sin importar si el que la
    // apaga es el propio boton "Enviar en vivo" o PresentationCore
    // (SetBgTypeLocked) porque el operador mando otra cosa en vivo.
    void SetLiveBackground(bool v);

    // Busca <filename> en la biblioteca de audio (releyendo la carpeta si
    // hace falta), lo reproduce y lo manda en vivo al proyector -- mismo
    // resultado que elegir la pista en RenderLibraryList y despues apretar
    // "En vivo", pero en un solo llamado. Usado por el control remoto del
    // celular (ver SyncServer.cpp POST /remote/multimedia/select). false si
    // el archivo no existe en la carpeta de audio.
    bool PlayFileLive(const std::string& filename);

    // Dibuja el fondo "now playing" (disco + caratula + ondas) en el
    // drawlist de la ventana ACTUAL — pensado para llamarse desde dentro
    // del Begin("ProjectorLive") de UIManager (ver ese archivo), así el
    // ImGui::GetWindowDrawList() que usa RenderSpinningDisc() cae en el
    // proyector real. (x,y,w,h) es el rectangulo completo del proyector.
    void RenderLiveBackground(float x, float y, float w, float h);

    // Acceso a los datos del waveform para que el proyector los dibuje
    const std::vector<float>& GetWaveBars()  const { return m_WaveVec; }
    float                     GetAccentHue() const;
    float                     GetTime()      const { return m_LastTime; }
    bool                      GetIsPlaying() const { return m_IsPlaying && !m_IsPaused; }
    void                      StopIfPathMatches(const std::string& path);

private:
    // ── Reproduccion ─────────────────────────────────────────────────────
    void Play(int trackIndex);
    void PlayCurrent();
    void Stop();
    void Pause();
    void TogglePlayPause();
    void Next();
    void Previous();
    void SeekTo(float normalizedPosition);
    void SetVolume(int volume);
    void ApplyGain(float gainDb);
    void ApplyEqualizerToPlayer();  // reaplica m_EqEnabled/m_EqBands/m_EqPreamp a m_VlcPlayer

    // Extrae y sube a GPU la portada de la pista actual (lazy, solo una vez)
    void EnsureCoverLoaded(int trackIndex);

    // ── Biblioteca ────────────────────────────────────────────────────────
    void RefreshLibrary();
    void ImportAudioFile();

    // ── Render por secciones ──────────────────────────────────────────────
    void RenderHeader();
    void RenderSpinningDisc(float cx, float cy, float radius);
    void RenderNowPlayingCard();
    void RenderProgressBar();
    void RenderTransportControls();
    void RenderVolumeRow();
    void RenderEqualizerButton();  // boton "EQ" -- mismo lugar/pinta que MonitorView, abre RenderEqualizerPopup
    void RenderEqualizerPopup();   // contenido del popup -- calcado de MonitorView::RenderEqualizerPopup
    void RenderPlaylist();
    void RenderStylePopup();   // catalogo de AudioVisualStyle + mostrar/ocultar ondas, ver boton "Estilos"

    // ── Letra importada desde URL (yt-dlp) ──────────────────────────────
    void RenderLyricsButton();  // boton "Letra"/"+ Letra", junto a Estilos/EQ
    void RenderLyricsPopup();
    void RequestLyricsImport(const std::string& url);      // dispara el fetch en un hilo de fondo
    void LoadTrackLyricsSidecar(AudioTrack& track) const;
    void SaveTrackLyricsSidecar(const AudioTrack& track) const;
    // Aplica/limpia SetLayer2_Text segun m_IsLiveBackground + la pista
    // actual -- se llama al ir/dejar de estar en vivo, al cambiar de pista
    // en vivo, y al tocar el toggle "Mostrar en vivo".
    void RefreshLiveLyrics();

    // ── Helpers ───────────────────────────────────────────────────────────
    std::string FormatTime(int64_t ms) const;
    static float DbToLinear(float dB);
    int  ComputeEffectiveVolume() const;
    static void ComputeTrackAccent(AudioTrack& track);

    // ── Reproductor ───────────────────────────────────────────────────────
    // VLCBasePlayer en vez de libVLC crudo: da EQ de 10 bandas, niveles de
    // audio REALES (GetAudioLevels, ver Update()) y seleccion de dispositivo
    // de salida ya probados y usados por Video/BackgroundLayer, en vez de
    // reimplementar esa interceptacion de samples aparte. forceSilent queda
    // en su default (false): a diferencia del player de Preview de Video
    // (que debe ser SIEMPRE mudo), aca el punto de "preview" de audio es
    // justamente poder escucharlo antes de mandarlo a escena.
    Core::VLCBasePlayer m_VlcPlayer;

    // ── Pistas ────────────────────────────────────────────────────────────
    std::vector<AudioTrack> m_Tracks;
    int  m_CurrentTrack = -1;
    bool m_IsPlaying    = false;
    bool m_IsPaused     = false;

    float   m_Progress      = 0.0f;
    int64_t m_CurrentTimeMs = 0;
    int64_t m_TotalTimeMs   = 0;
    bool    m_IsSeeking     = false;

    // ── Audio ─────────────────────────────────────────────────────────────
    int   m_Volume           = 80;
    float m_GainDb           = 0.0f;
    bool  m_Muted            = false;
    int   m_VolumeBeforeMute = 80;

    // ── Ecualizador ───────────────────────────────────────────────────────
    // Fuente de verdad para los sliders de la UI -- m_VlcPlayer solo recibe
    // los valores ya calculados (ver ApplyEqualizerToPlayer), mismo criterio
    // que MonitorView::m_EqEnabled/m_EqBandAmps.
    static constexpr int kEqBands = 10;
    float m_EqBands[kEqBands] = { 0.0f };
    float m_EqPreamp          = 0.0f;
    bool  m_EqEnabled         = false;

    static constexpr const char* kBandLabels[kEqBands] = {
        "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };

    // ── Modos ─────────────────────────────────────────────────────────────
    AudioRepeatMode m_RepeatMode = AudioRepeatMode::None;
    bool            m_Shuffle    = false;

    // ── Disco giratorio ───────────────────────────────────────────────────
    SpinningDiscParams m_Disc;

    // ── Estilo visual ─────────────────────────────────────────────────────
    AudioVisualStyle m_VisualStyle    = AudioVisualStyle::Vinyl;
    bool             m_ShowStylePopup = false;

    // Mostrar/ocultar las ondas en la salida real (ver RenderLiveBackground)
    // -- toggle propio en Estilos, pedido explicito, separado de
    // AudioVisualStyle (el estilo "Ondas" saca el disco pero deja las
    // ondas; esto las saca a ELLAS sin importar el estilo elegido).
    bool m_ShowWaveform = true;

    // ── Letra importada desde URL (yt-dlp) ──────────────────────────────
    // Mismo patron que UIManager::m_UrlImportThread/Result (Archivo >
    // Importar > Importar desde URL), pero self-contained aca: el fetch
    // guarda la letra en la PISTA actual en vez de crear una Cancion nueva.
    bool        m_ShowLyricsPopup     = false;
    bool        m_LyricsImportRunning = false;
    char        m_LyricsUrlBuffer[512] = {};
    std::string m_LyricsImportError;
    std::thread m_LyricsImportThread;
    std::mutex  m_LyricsImportMutex;
    std::optional<Core::SubtitleFetchResult> m_LyricsImportResult;

    // ── Waveform ──────────────────────────────────────────────────────────
    // Historial de picos de audio REALES (ver Update()/GetAudioLevels) --
    // cada 45ms se desplazan las barras una posicion y se empuja el pico mas
    // reciente, asi se ve como una forma de onda en el tiempo en vez de un
    // solo valor repetido. En Linux GetAudioLevels() siempre devuelve 0 (ver
    // VLCBasePlayer.h) -- las barras quedan planas ahi hasta que se agregue
    // interceptacion de samples nativa para esa plataforma.
    static constexpr int kWaveBars = 32;
    float m_WaveBars[kWaveBars]    = { 0.0f };
    float m_WaveTargets[kWaveBars] = { 0.0f };
    float m_WaveTimer              = 0.0f;

    // Vector para exponer el waveform al exterior (proyector)
    std::vector<float> m_WaveVec;

    // ── "En vivo" en el proyector real (ver IsLiveBackground/SetLiveBackground) ──
    bool m_IsLiveBackground = false;

    // Ultimo archivo de audio seleccionado desde AFUERA de este panel (ver
    // Update()) -- la grilla "Medios" de Biblioteca (LibraryMultimedia.cpp)
    // publica selecciones de audio reales por archivo via
    // PresentationCore::SetSelection, pero nadie las escuchaba para audio
    // (a diferencia de Video, que MonitorView si sigue) -- elegir un audio
    // distinto ahi no hacia nada, el que ya sonaba seguia sonando. Se
    // compara contra esto cada frame para reproducir solo cuando cambia de
    // verdad, no en cada frame.
    std::string m_LastExternalSelection;

    // Tiempo de la ultima animacion
    float m_LastTime = 0.0f;
};

} // namespace ProyecThor::UI
