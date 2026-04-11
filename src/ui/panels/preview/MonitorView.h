#pragma once

#include <string>
#include <vector>
#include <imgui.h>
#include "../../PresentationCore.h"

namespace ProyecThor::UI {

struct QueueEntry {
    std::string title;
    std::string fullPath;
    std::string type;   // "video" | "image" | "song"
    bool        played = false;
};

class MonitorView {
public:
    MonitorView()  = default;
    ~MonitorView() = default;

    // Punto de entrada principal — llamar una vez por frame
    void Render(Core::VLCVideoLayer* player);

    // Acceso a la cola (usado externamente si se necesita)
    void EnqueueItem(const QueueEntry& entry);
    void ClearQueue();

private:
    // ── Estado de reproduccion ────────────────────────────────────────────────
    bool        m_IsPlaying      = false;
    bool        m_IsLooping      = false;
    bool        m_IsLoopingQueue = false;
    bool        m_AutoAdvance    = true;
    std::string m_CurrentTitle;
    std::string m_CurrentPath;
    std::string m_CurrentType;
    int         m_CurrentIndex   = -1;

    // ── Cola ─────────────────────────────────────────────────────────────────
    std::vector<QueueEntry> m_Queue;

    // ── Canciones (versos) ────────────────────────────────────────────────────
    int m_SelectedVerse = -1;

    // ── UI flags ─────────────────────────────────────────────────────────────
    bool  m_ShowQueue    = true;
    bool  m_ShowAudio    = true;
    float m_PreviewAspect = 16.f / 9.f;

    // ── Audio ─────────────────────────────────────────────────────────────────
    float m_Volume  = 0.85f;
    bool  m_IsMuted = false;

    // Dispositivos de audio
    // AudioDevice::id          → identificador interno para VLC
    // AudioDevice::description → nombre legible para la UI
    std::vector<Core::VLCVideoLayer::AudioDevice> m_AudioDevices;
    std::string m_MonitorDeviceId; // salida del operador (sin proyeccion)
    std::string m_PublicDeviceId;  // salida hacia el publico (LIVE)

    // ── VU-meter ─────────────────────────────────────────────────────────────
    float m_VULevel = 0.f;
    float m_VUPeak  = 0.f;
    float m_VUDecay = 0.f;

    // ── Operaciones de control ────────────────────────────────────────────────
    void DualPlay        (Core::VLCVideoLayer* player, const std::string& path);
    void DualPause       (Core::VLCVideoLayer* player, bool pause);
    void DualSeek        (Core::VLCVideoLayer* player, float normalizedPos);
    void DualStop        (Core::VLCVideoLayer* player);
    void DualSetVolume   (Core::VLCVideoLayer* player, int vol);
    void ApplyAudioDevice(Core::VLCVideoLayer* player);

    // ── Cola ─────────────────────────────────────────────────────────────────
    void PlayIndex(int index, Core::VLCVideoLayer* player);
    void PlayNext (Core::VLCVideoLayer* player);
    void PlayPrev (Core::VLCVideoLayer* player);
    bool IsVideoOrImage(const QueueEntry& e) const;

    // ── Render sub-secciones ─────────────────────────────────────────────────
    void RenderTopBar       ();
    void RenderVideoPreview (Core::VLCVideoLayer* player, float width);
    void RenderTransportBar (Core::VLCVideoLayer* player);
    void RenderAudioSection (Core::VLCVideoLayer* player);
    void RenderQueue        (Core::VLCVideoLayer* player);
    void RenderSongPanel    (Core::VLCVideoLayer* player);
    void RenderStatusBar    (Core::VLCVideoLayer* player);

    // ── Helpers ───────────────────────────────────────────────────────────────
    static std::string FormatTime    (int64_t ms);
    static std::string TruncateLabel (const std::string& s, int maxChars);

    void DrawVUMeter     (const char* id, float level, float peak,
                          float width, float height);
    bool TransportButton (const char* label, const char* tooltip,
                          float w, float h, bool highlighted = false,
                          ImVec4 hlColor = { 0.227f, 0.525f, 0.800f, 1.f });
};

} // namespace ProyecThor::UI
