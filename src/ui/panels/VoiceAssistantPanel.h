#pragma once

#include "ui/framework/GlassRenderer.h"
#include "core/VoiceAssistant.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

class VoiceAssistantPanel {
public:
    VoiceAssistantPanel() = default;
    ~VoiceAssistantPanel() = default;

    void Render(bool* pOpen, GlassRenderer& glass);

    // Dibuja el banner flotante de sugerencia en pantalla si hay una coincidencia
    // pendiente en modo asistido. Llamar al final del frame para que quede por encima.
    void RenderAssistedPromptBanner();

private:
    // --- Control del motor (todas pasan por aqui para evitar rebotes y carreras) ---
    void BeginStart(Core::VoiceAssistant& va, double now);
    void StartListening(Core::VoiceAssistant& va, double now);
    void StopListening(Core::VoiceAssistant& va);
    void RestartListening(Core::VoiceAssistant& va, double now);
    void ProcessDeferredRestart(Core::VoiceAssistant& va, double now);
    void UpdateState(bool running, bool hearing, bool hasError, double now);

    // --- Microfonos ---
    void RefreshMicrophones(Core::VoiceAssistant& va, double now);

    // --- Secciones de UI ---
    void RenderStatusCard(Core::VoiceAssistant& va, bool running, bool starting, double now);
    void RenderErrorBlock(Core::VoiceAssistant& va, const std::string& err, double now);
    void RenderMicSelector(Core::VoiceAssistant& va, bool running, double now);
    void RenderLiveBox(Core::VoiceAssistant& va, bool running, bool hearing);

    // --- Estado de prueba ---
    char m_TestInputBuf[256] = {};

    // --- Cache de microfonos ---
    std::vector<Core::AudioInputDeviceInfo> m_CachedMics;
    double m_LastMicsRefreshTime = 0.0;
    bool   m_MicsInitialized     = false;
    bool   m_ComboWasOpen        = false;

    // --- Estado del motor observado desde la UI ---
    bool   m_ShowDesktopAudioHelp = false;
    bool   m_WasRunning          = false;
    bool   m_UserStopped         = false;   // el usuario pidio detener (no es un corte inesperado)
    bool   m_UnexpectedStop      = false;   // se detuvo solo, sin error reportado
    bool   m_StartFailedSilently = false;   // Start() no arranco y no hubo error
    double m_StartRequestedTime  = -1.0;    // <0 = no hay arranque en curso
    double m_PendingRestartAt    = -1.0;    // <0 = no hay reinicio diferido
    double m_LastActionTime      = -10.0;   // antirrebote de Iniciar
    double m_LastAudioTime       = 0.0;     // ultima vez que se recibio audio
};

} // namespace ProyecThor::UI