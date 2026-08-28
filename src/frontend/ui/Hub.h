#pragma once

#include <chrono>
#include <array>
#include <thread>
#include <mutex>
#include <optional>
#include <string>
#include <imgui.h>
#include "backend/core/SubtitleImporter.h"

struct GLFWmonitor;

namespace ProyecThor::UI {

class Hub {
public:
    Hub();
    ~Hub(); // une m_DownloadSubsThread si sigue viva -- ver definicion en Hub.cpp

    bool Render();
    void ForceOpen();

    bool IsOpen()               const { return m_Open; }
    bool SettingsRequested()    const { return m_OpenSettingsRequested; }
    void ClearSettingsRequest()       { m_OpenSettingsRequested = false; }
    int  GetActiveTab()         const { return m_ActiveTab; }

private:
    // Layout de un solo flujo central de paneles, con las acciones principales
    // apiladas y las utilidades compactas debajo.
    void RenderContent(float w, float h);
    void RenderNovedadesPanel();
    void RenderUpdateDetailModal();
    void RenderDownloadSubtitlesPanel();

    void UpdateAnimations(float dt);

    bool  m_Open                  = true;
    bool  m_Appearing             = true;
    float m_AppearProgress        = 0.0f;
    float m_Time                  = 0.0f;

    bool  m_LaunchRequested       = false;
    bool  m_OpenSettingsRequested = false;

    int   m_ActiveTab             = 0;
    int   m_SelectedMonitor       = -1;

    // --- Panel "Novedades" (parche destacado + historial de versiones),
    // abierto a demanda con la tecla N o la tarjeta del mismo nombre ---
    bool  m_NovedadesOpen         = false;
    float m_NovedadesAnim         = 0.0f;

    // --- Modal universal de detalle de actualizacion -- compartido entre
    // el hero de Novedades ("Ver todo el detalle") y su lista de historial ---
    bool  m_IsUpdateModalOpen     = false;
    int   m_SelectedUpdateVer     = 15; // id de kUpdateRegistry; arranca en la mas reciente
    float m_UpdateModalAnim       = 0.0f;

    // --- "Descargar subtitulos" -- utilidad independiente de la Biblioteca:
    // baja los subtitulos de una URL (mismo fetch que "Importar desde URL"
    // del menu Archivo, ver SubtitleImporter.h) y los guarda como .txt
    // suelto, sin crear una cancion. Corre en un hilo de fondo por la
    // misma razon que UIManager::RenderUrlImportModal (depende de la red).
    bool        m_DownloadSubsOpen           = false;
    bool        m_DownloadSubsRunning        = false;
    char        m_DownloadSubsUrlBuf[512]    = {};
    bool        m_DownloadSubsAskEachTime    = true;
    std::string m_DownloadSubsPresetFolder;
    std::string m_DownloadSubsLastError;
    std::string m_DownloadSubsSavedPath; // no vacio tras un exito -- se muestra "Guardado en: ..."
    std::thread m_DownloadSubsThread;
    std::mutex  m_DownloadSubsMutex;
    std::optional<ProyecThor::Core::SubtitleFetchResult> m_DownloadSubsResult;

    // --- Canvas de particulas (fondo animado) ---
    struct BgParticle {
        float x, y;
        float vx, vy;
        float r;
        float phase;
        bool  isCyan;
    };

    static constexpr int   BG_PARTICLE_COUNT = 70;
    static constexpr float BG_CONNECT_DIST   = 130.0f;
    static constexpr float BG_GRID_SIZE      = 80.0f;

    std::array<BgParticle, BG_PARTICLE_COUNT> m_BgParticles;
    bool m_BgParticlesInit = false;

    void InitBgParticles(float w, float h);
    void UpdateBgParticles(float dt, float w, float h);
    void RenderBgCanvas(ImDrawList* dl, ImVec2 origin, float w, float h);

    // --- Nebulosas de fondo (solo tema "Galaxia") -----------------------------
    // Manchas grandes y suaves (varios circulos concentricos con alpha
    // decreciente, sin textura) que derivan lento por el fondo del Hub,
    // tenidas con el acento del tema -- para el preset Galaxia, que ya es un
    // violeta profundo, esto le da sensacion de nebulosa/espacio en vez de
    // fondo plano. Solo se inicializan/dibujan si el preset activo es Galaxy.
    struct NebulaBlob { float x, y, r, vx, vy; };
    static constexpr int NEBULA_COUNT = 4;
    std::array<NebulaBlob, NEBULA_COUNT> m_Nebulas;
    bool m_NebulasInit = false;
    void InitNebulas(float w, float h);
    void UpdateNebulas(float dt, float w, float h);

    std::chrono::steady_clock::time_point m_LastFrameTime;
};

} // namespace ProyecThor::UI