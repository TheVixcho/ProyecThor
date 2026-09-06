#pragma once
#include "backend/core/AudioRecorder.h"
#include "backend/core/AudioMixdown.h"
#include "backend/media/VLCBasePlayer.h"
#include <imgui.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  AudioDawPanel — mini-DAW funcional embebido en la pestaña "Audio" de
//  VideoEditorPanel (ver Settings::WorkspaceLayoutPreset::Video). Pedido
//  explicito: grabar audio real, poder cortar/mover clips en la linea de
//  tiempo, y renderizar (exportar) la mezcla con distintos codecs/formatos,
//  ademas de un panel de Media para agregar canciones/audio ya importado.
//
//  Alcance deliberado (para que sea real en vez de aspiracional): el modelo
//  de edicion es NO DESTRUCTIVO -- cada clip solo guarda que parte de su
//  archivo fuente usa (offset/duracion) y donde lo puso en la linea de
//  tiempo, nunca se reescribe audio a mano.
//
//  "Reproducir todo" (ver StartMixPlayback/UpdateMixPlayback) reproduce
//  TODAS las pistas a la vez usando un pool de reproductores persistentes
//  (uno por pista, reusados -- nunca uno nuevo por clip, ver comentario de
//  AudioDawPanel.cpp) con un cabezal de reproduccion avanzado por software
//  (ImGui::GetTime()). Es una sincronizacion "razonable para revisar", no
//  grado broadcast/sample-accurate -- cada pista arranca su clip con un
//  SetPosition() normalizado (VLCBasePlayer no tiene Seek en ms), asi que
//  puede haber una deriva chica entre pistas. Para escuchar el resultado
//  FINAL de verdad, Renderizar (ver AudioMixdown) mezcla todo via ffmpeg,
//  sample-accurate, sin depender de este cabezal por software.
// ─────────────────────────────────────────────────────────────────────────────
class AudioDawPanel {
public:
    AudioDawPanel();
    ~AudioDawPanel();

    // Llamar una vez por frame mientras el tab "Audio" este vivo (pump de
    // grabacion/exportacion en curso, sin importar si el usuario esta
    // mirando otra pestaña de VideoEditorPanel en este instante).
    void Update();

    // Dibuja el contenido -- asume que ya hay una ventana/child abierta.
    void Render();

private:
    struct DawClip {
        std::string sourcePath;
        std::string displayName;
        int64_t sourceOffsetMs      = 0;
        int64_t durationMs          = 1000;
        int64_t timelinePosMs       = 0;
        int64_t sourceTotalMs       = 0; // duracion real del archivo fuente, para no recortar mas alla
    };
    struct DawTrack {
        std::string          name;
        bool                  muted = false;
        std::vector<DawClip>  clips;
    };

    void RenderTransport();
    void RenderMediaPanel(float w, float h);
    void RenderTracks();
    void RenderExportBar();

    void RefreshMediaList();
    void RefreshInputDevices();
    void AddClipToTrack(int trackIdx, const std::string& path, const std::string& displayName);
    void PreviewClip(const DawClip& clip);
    void StopPreview();

    // ── "Reproducir todo" (mezcla en vivo aproximada, ver comentario de
    // arriba) ────────────────────────────────────────────────────────────
    void StartMixPlayback();
    void StopMixPlayback();
    void UpdateMixPlayback();
    int  FindClipAt(int trackIdx, int64_t posMs) const; // -1 si ninguno

    static int64_t ProbeDurationMs(const std::string& path);

    std::vector<DawTrack> m_Tracks;

    std::vector<std::pair<std::string, std::string>> m_MediaList; // <fullPath, displayName>
    bool m_MediaListLoaded = false;

    int m_SelectedTrack    = 0;
    int m_SelClipTrack     = -1;
    int m_SelClipIdx       = -1;

    // ── Previsualizacion de UN clip (doble click / boton "Reproducir clip") ─
    Core::VLCBasePlayer m_PreviewPlayer;
    bool                m_PreviewPlaying = false;

    // ── "Reproducir todo" (todas las pistas a la vez) ──────────────────────
    // Pool de reproductores PERSISTENTES, uno por pista -- se reusan llamando
    // Play() de nuevo cuando cambia el clip activo, nunca se crea/destruye
    // una instancia por clip (VLCBasePlayer es pesado de construir, ver
    // BackgroundLayer::NativePlayback/m_PlayerA-B, mismo criterio ya
    // establecido en el resto de la app).
    std::vector<std::unique_ptr<Core::VLCBasePlayer>> m_TrackPlayers;
    std::vector<int>    m_TrackActiveClip; // por pista: indice del clip sonando ahora, -1 = ninguno
    bool                 m_MixPlaying = false;
    double                m_MixPlayheadMs = 0.0;
    double                m_MixLastFrameTime = 0.0;

    // ── Grabacion ────────────────────────────────────────────────────────
    Core::AudioRecorder                    m_Recorder;
    std::vector<Core::AudioInputDevice>    m_InputDevices;
    bool                                    m_InputDevicesLoaded = false;
    int                                      m_SelectedInputDevice = 0;
    std::string                             m_RecordingPath;

    // ── Linea de tiempo ──────────────────────────────────────────────────
    float m_PixelsPerMs = 0.05f;

    enum class DragMode { None, Move, TrimLeft, TrimRight };
    DragMode m_DragMode = DragMode::None;
    ImVec2   m_DragStartMouse{};
    int64_t  m_DragStartPosMs = 0, m_DragStartOffsetMs = 0, m_DragStartDurMs = 0;

    // ── Exportar/Renderizar ──────────────────────────────────────────────
    Core::AudioMixdown m_Mixdown;
    int                 m_ExportFormatIdx = 0; // indice en Core::AudioExportFormat
    char                m_ExportPathBuf[512] = {};
    bool                m_ExportPathInit = false;
    std::string         m_StatusMessage;
    bool                m_StatusIsError = false;
};

} // namespace ProyecThor::UI
