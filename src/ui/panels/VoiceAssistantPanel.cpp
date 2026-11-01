#include "VoiceAssistantPanel.h"
#include "core/VoiceAssistant.h"
#include "core/PresentationCore.h"
#include "ui/framework/DesignSystem.h"
#include "ui/panels/home/HomeIcons.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <shellapi.h>
#endif

namespace ProyecThor::UI {

namespace {

// ── Constantes ──────────────────────────────────────────────────────────────
constexpr double kMicAutoRefreshSeconds = 10.0;  // solo cuando NO se esta escuchando
constexpr double kStartTimeoutSeconds   = 4.0;   // tiempo maximo esperando que arranque
constexpr double kRestartDelaySeconds   = 0.45;  // pausa entre Stop() y Start() para liberar el dispositivo
constexpr double kSilenceHintSeconds    = 20.0;  // aviso si no llega voz en este tiempo
constexpr double kActionDebounceSeconds = 0.60;  // evita doble clic en Iniciar
constexpr float  kCardHeight            = 58.0f;
constexpr float  kLiveHeight            = 64.0f;

const ImU32 kWarnColor = IM_COL32(245, 175, 65, 255);

#if defined(_WIN32)
constexpr bool kHasSystemSettings = true;
#else
constexpr bool kHasSystemSettings = false;
#endif

// ── Utilidades ──────────────────────────────────────────────────────────────
inline ImVec4 Col(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

void OpenSystemSettings(const char* uri)
{
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", uri, nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)uri;
#endif
}

// Recorta el final con "..." para que el texto quepa en maxWidth (respeta UTF-8).
std::string TruncateHead(const std::string& text, float maxWidth)
{
    if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

    std::string result = "...";
    size_t end = text.size();
    while (end > 0) {
        --end;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
        result = text.substr(0, end) + "...";
        if (ImGui::CalcTextSize(result.c_str()).x <= maxWidth) break;
    }
    return result;
}

// Conserva el final del texto (lo ultimo que se escucho) y recorta el inicio.
std::string TruncateTail(const std::string& text, float maxWidth)
{
    if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

    size_t start = 0;
    if (text.size() > 240) {
        start = text.size() - 240;
        while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) ++start;
    }

    std::string result = "...";
    while (start < text.size()) {
        ++start;
        while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) ++start;
        result = "..." + text.substr(start);
        if (ImGui::CalcTextSize(result.c_str()).x <= maxWidth) break;
    }
    return result;
}

// ── Tabla de errores conocidos del motor ────────────────────────────────────
struct ErrorInfo {
    const char* code;
    const char* message;
    const char* detail;        // puede ser nullptr
    const char* buttonLabel;   // boton de configuracion del sistema
    const char* settingsUri;
    bool        isWarning;
};

const ErrorInfo kKnownErrors[] = {
    { "PRIVACY_POLICY_REQUIRED",
      "Windows requiere activar la opción 'Reconocimiento de voz en línea' para permitir escuchar el micrófono en esta PC.",
      nullptr,
      "Abrir configuración de privacidad de voz",
      "ms-settings:privacy-speech", false },

    { "MIC_ACCESS_DENIED",
      "El acceso al micrófono está bloqueado en la configuración de privacidad de esta PC.",
      "Activa 'Permitir que las aplicaciones accedan al micrófono' en Windows.",
      "Abrir privacidad del micrófono en Windows",
      "ms-settings:privacy-microphone", false },

    { "NO_MICROPHONE_FOUND",
      "No se detectó ningún micrófono conectado o activo en esta PC.",
      "Conecta un micrófono o revisa los dispositivos de entrada en Windows.",
      "Abrir configuración de sonido de Windows",
      "ms-settings:sound", false },

    { "NO_SPANISH_SPEECH_PACK",
      "Esta PC no tiene instalado el paquete de reconocimiento de voz en Español de Windows.",
      "Para reconocer versículos y canciones habladas, agrega el idioma de voz Español en Windows.",
      "Descargar voz en Español (configuración de Windows)",
      "ms-settings:speech", true },
};

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
//  Control del motor
// ═════════════════════════════════════════════════════════════════════════════

void VoiceAssistantPanel::BeginStart(Core::VoiceAssistant& va, double now)
{
    m_UserStopped         = false;
    m_UnexpectedStop      = false;
    m_StartFailedSilently = false;
    m_StartRequestedTime  = now;
    va.Start();
}

void VoiceAssistantPanel::StartListening(Core::VoiceAssistant& va, double now)
{
    if ((now - m_LastActionTime) < kActionDebounceSeconds) return;
    m_LastActionTime = now;

    m_PendingRestartAt = -1.0;
    BeginStart(va, now);
}

void VoiceAssistantPanel::StopListening(Core::VoiceAssistant& va)
{
    m_UserStopped         = true;
    m_StartRequestedTime  = -1.0;
    m_PendingRestartAt    = -1.0;
    m_UnexpectedStop      = false;
    m_StartFailedSilently = false;
    va.Stop();
}

void VoiceAssistantPanel::RestartListening(Core::VoiceAssistant& va, double now)
{
    // Detiene el motor de inmediato y agenda un Start diferido tras una pausa breve
    // (kRestartDelaySeconds) para dar tiempo a Windows de cerrar el handle del dispositivo.
    m_UserStopped        = false;
    m_StartRequestedTime = -1.0;
    va.Stop();
    m_PendingRestartAt   = now + kRestartDelaySeconds;
}

void VoiceAssistantPanel::ProcessDeferredRestart(Core::VoiceAssistant& va, double now)
{
    if (m_PendingRestartAt >= 0.0 && now >= m_PendingRestartAt) {
        m_PendingRestartAt = -1.0;
        BeginStart(va, now);
    }
}

void VoiceAssistantPanel::UpdateState(bool running, bool hearing, bool hasError, double now)
{
    // 1) Deteccion de arranque fallido en silencio (se pidio Start pero pasaron >4s
    //    sin que el motor reporte IsRunning() ni deje un GetLastError()).
    if (m_StartRequestedTime >= 0.0) {
        if (running) {
            m_StartRequestedTime = -1.0; // arranco con exito
        } else if (hasError) {
            m_StartRequestedTime = -1.0; // fallo pero dejo error explicito
        } else if ((now - m_StartRequestedTime) > kStartTimeoutSeconds) {
            m_StartRequestedTime  = -1.0;
            m_StartFailedSilently = true;
        }
    }

    // 2) Deteccion de caida inesperada del motor (estaba corriendo y se apago solo).
    if (m_WasRunning && !running && !m_UserStopped && !hasError && m_PendingRestartAt < 0.0) {
        m_UnexpectedStop = true;
    }
    m_WasRunning = running;

    // 3) Registro de actividad de audio para el aviso de silencio.
    if (hearing) {
        m_LastAudioTime = now;
    } else if (!running) {
        m_LastAudioTime = now;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Microfonos
// ═════════════════════════════════════════════════════════════════════════════

void VoiceAssistantPanel::RefreshMicrophones(Core::VoiceAssistant& va, double now)
{
    m_CachedMics          = va.GetAvailableMicrophones();
    m_LastMicsRefreshTime = now;
    m_MicsInitialized     = true;
}

void VoiceAssistantPanel::RenderMicSelector(Core::VoiceAssistant& va, bool running, double now)
{
    ImGui::TextColored(Col(DS::TextSecondary), "Dispositivo de entrada:");

    // Enumerar dispositivos mientras el motor captura puede provocar cortes de audio.
    // Solo se refresca de forma automatica cuando NO se esta escuchando.
    if (!m_MicsInitialized) {
        RefreshMicrophones(va, now);
    } else if (!running && (now - m_LastMicsRefreshTime) > kMicAutoRefreshSeconds) {
        RefreshMicrophones(va, now);
    }

    const std::string selectedMicId = va.GetSelectedMicrophone();

    // Nombre mostrado y comprobacion de que el dispositivo elegido sigue existiendo.
    std::string currentMicName = m_CachedMics.empty() ? "Ningún micrófono detectado"
                                                      : "Predeterminado del sistema";
    bool selectedFound = selectedMicId.empty();
    for (const auto& m : m_CachedMics) {
        if ((selectedMicId.empty() && m.isDefault) || (m.id == selectedMicId)) {
            currentMicName = m.name;
            selectedFound  = true;
            break;
        }
    }

    const float spacing   = ImGui::GetStyle().ItemSpacing.x;
    const float refreshW  = 84.0f;
    const float soundW    = kHasSystemSettings ? 72.0f : 0.0f;
    const float extraW    = refreshW + spacing + (kHasSystemSettings ? (soundW + spacing) : 0.0f);
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - extraW));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    const bool comboOpen = ImGui::BeginCombo("##micSelect", currentMicName.c_str());
    if (comboOpen) {
        if (!m_ComboWasOpen) RefreshMicrophones(va, now);

        if (m_CachedMics.empty()) {
            ImGui::Selectable("(Sin dispositivos conectados)", false, ImGuiSelectableFlags_Disabled);
        } else {
            for (const auto& m : m_CachedMics) {
                const bool isSel = (m.id == selectedMicId) || (selectedMicId.empty() && m.isDefault);
                const std::string label = m.name + (m.isDefault ? " (Predeterminado)" : "") + "##mic_" + m.id;
                if (ImGui::Selectable(label.c_str(), isSel)) {
                    if (m.id != selectedMicId) {
                        va.SetSelectedMicrophone(m.id);
                        if (running) RestartListening(va, now);
                    }
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    m_ComboWasOpen = comboOpen;

    ImGui::SameLine();
    if (ImGui::Button("Actualizar##refreshMics", ImVec2(refreshW, 0.0f))) {
        RefreshMicrophones(va, now);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Actualizar lista de micrófonos conectados");
    }

    if (kHasSystemSettings) {
        ImGui::SameLine();
        if (ImGui::Button("Sonido##soundSettings", ImVec2(soundW, 0.0f))) {
            OpenSystemSettings("ms-settings:sound");
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Abrir configuración de sonido de Windows");
        }
    }

    ImGui::PopStyleVar(2);

    // ── Captura de Audio del Escritorio / Salida / Auriculares ──
    const bool hasDesktop = va.HasDesktopAudioDevice();
    const bool isDesktopSel = va.IsDesktopAudioSelected();

    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    if (isDesktopSel) {
        ImGui::PushStyleColor(ImGuiCol_Text, Col(DS::SuccessColor));
        ImGui::TextUnformatted("● Capturando Audio del Escritorio (PC)");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        if (ImGui::SmallButton("Volver a Micrófono")) {
            for (const auto& m : m_CachedMics) {
                if (!m.isDesktopAudio && !m.isRenderDevice) {
                    va.SetSelectedMicrophone(m.id);
                    if (running) RestartListening(va, now);
                    break;
                }
            }
        }
        ImGui::PopStyleVar();

        if (!hasDesktop) {
            ImGui::TextColored(Col(kWarnColor), "Nota: Para capturar audio de PC, activa 'Mezcla estéreo' en Windows.");
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            if (ImGui::SmallButton("Abrir Grabación")) {
                ShellExecuteA(NULL, "open", "control.exe", "mmsys.cpl,,recording", NULL, SW_SHOWNORMAL);
            }
            ImGui::PopStyleVar();
        }
    } else if (hasDesktop) {
        if (DS::GlassButton("Usar Audio del Escritorio (PC / Auriculares)", ImVec2(0.0f, 26.0f), DS::AccentColor)) {
            va.SelectDesktopAudioDevice();
            if (running) RestartListening(va, now);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Captura el sonido reproducido en la PC (videos, Zoom, música)\npara reconocer versículos y canciones directamente.");
        }
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        if (ImGui::Button("¿Cómo capturar audio de auriculares / PC?##setupDesktopAudio")) {
            m_ShowDesktopAudioHelp = !m_ShowDesktopAudioHelp;
        }
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Guía para escuchar sonido de audífonos o que sale de la PC.");
        }

        if (m_ShowDesktopAudioHelp) {
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 25, 29, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusSmall);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::BeginChild("##desktopAudioHelpBox", ImVec2(0.0f, 130.0f), true);

            ImGui::TextColored(Col(DS::AccentColor), "Capturar Audio del Escritorio en Windows:");
            ImGui::TextWrapped("Para escuchar videos o audio reproducido directamente en esta PC:");

            ImGui::BulletText("Opción 1: Activar 'Mezcla estéreo' (Stereo Mix).");
            ImGui::SameLine();
            if (ImGui::SmallButton("Abrir Grabación")) {
                ShellExecuteA(NULL, "open", "control.exe", "mmsys.cpl,,recording", NULL, SW_SHOWNORMAL);
            }
            ImGui::TextColored(Col(DS::TextHint), "   (Clic derecho en el espacio vacío -> 'Mostrar deshabilitados' -> Activar)");

            ImGui::BulletText("Opción 2: Instalar cable virtual gratuito VB-CABLE.");
            ImGui::SameLine();
            if (ImGui::SmallButton("Descargar")) {
                ShellExecuteA(NULL, "open", "https://vb-audio.com/Cable/", NULL, NULL, SW_SHOWNORMAL);
            }

            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
    }

    // El micrófono guardado ya no esta disponible (desconectado o cambiado de puerto).
    if (!selectedFound) {
        ImGui::PushStyleColor(ImGuiCol_Text, Col(kWarnColor));
        ImGui::TextWrapped("El micrófono seleccionado ya no está disponible.");
        ImGui::PopStyleColor();
        if (DS::GlassButton("Usar micrófono predeterminado", ImVec2(0.0f, 26.0f), DS::AccentColor)) {
            va.SetSelectedMicrophone(std::string());
            if (running) RestartListening(va, now);
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Secciones de UI
// ═════════════════════════════════════════════════════════════════════════════

void VoiceAssistantPanel::RenderStatusCard(Core::VoiceAssistant& va, bool running, bool starting, double now)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 cardMin = ImGui::GetCursorScreenPos();
    const float  availW  = ImGui::GetContentRegionAvail().x;
    const ImVec2 cardMax = { cardMin.x + availW, cardMin.y + kCardHeight };

    ImGui::Dummy(ImVec2(availW, kCardHeight));

    ImU32 bg, border, iconColor, titleColor, subColor;
    const char* title;
    const char* subtitle;

    if (running) {
        bg         = IM_COL32(20, 36, 26, 255);
        border     = DS::SuccessColor;
        iconColor  = DS::SuccessColor;
        titleColor = DS::SuccessColor;
        subColor   = DS::TextSecondary;
        title      = "Escuchando voz en vivo";
        subtitle   = "Reconocimiento continuo activo";
    } else if (starting) {
        bg         = IM_COL32(36, 32, 20, 255);
        border     = kWarnColor;
        iconColor  = kWarnColor;
        titleColor = kWarnColor;
        subColor   = DS::TextSecondary;
        title      = "Iniciando micrófono...";
        subtitle   = "Conectando dispositivo de audio";
    } else {
        bg         = IM_COL32(24, 25, 29, 255);
        border     = DS::GlassBorder;
        iconColor  = DS::TextSecondary;
        titleColor = DS::TextPrimary;
        subColor   = DS::TextHint;
        title      = "Micrófono detenido";
        subtitle   = "Haz clic en Iniciar para comenzar a capturar";
    }

    dl->AddRectFilled(cardMin, cardMax, bg, DS::RadiusMedium);
    dl->AddRect(cardMin, cardMax, border, DS::RadiusMedium, 0, 1.0f);

    // Icono de micrófono a la izquierda
    HomeIcons::DrawIcon_Microphone(dl, { cardMin.x + 14.0f, cardMin.y + 13.0f }, 32.0f, iconColor);

    const float btnW     = 130.0f;
    const float btnH     = 28.0f;
    const float textX    = cardMin.x + 56.0f;
    const float textMaxW = std::max(40.0f, availW - 56.0f - btnW - 16.0f);

    ImGui::SetCursorScreenPos({ textX, cardMin.y + 11.0f });
    ImGui::TextColored(Col(titleColor), "%s", TruncateHead(title, textMaxW).c_str());
    ImGui::SetCursorScreenPos({ textX, cardMin.y + 31.0f });
    ImGui::TextColored(Col(subColor), "%s", TruncateHead(subtitle, textMaxW).c_str());

    // Botón de acción a la derecha
    ImGui::SetCursorScreenPos({ cardMax.x - btnW - 12.0f, cardMin.y + (kCardHeight - btnH) * 0.5f });
    if (running) {
        if (DS::GlassButton("Detener", ImVec2(btnW, btnH), DS::DangerColor)) {
            StopListening(va);
        }
    } else if (starting) {
        ImGui::BeginDisabled();
        DS::GlassButton("Iniciando...", ImVec2(btnW, btnH), kWarnColor);
        ImGui::EndDisabled();
    } else {
        if (DS::GlassButton("Iniciar Escucha", ImVec2(btnW, btnH), DS::SuccessColor)) {
            StartListening(va, now);
        }
    }

    ImGui::SetCursorScreenPos({ cardMin.x, cardMax.y });
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

void VoiceAssistantPanel::RenderErrorBlock(Core::VoiceAssistant& va, const std::string& err, double now)
{
    const ErrorInfo* info = nullptr;
    for (const auto& e : kKnownErrors) {
        if (err == e.code) { info = &e; break; }
    }
    const ImU32 color = (info && info->isWarning) ? kWarnColor : DS::DangerColor;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(36, 20, 22, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusSmall);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("##voiceErrorCard", ImVec2(availW, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    ImGui::PushStyleColor(ImGuiCol_Text, Col(color));
    if (info) ImGui::TextWrapped("%s", info->message);
    else      ImGui::TextWrapped("Error del asistente de voz: %s", err.c_str());
    ImGui::PopStyleColor();

    if (info && info->detail) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, Col(DS::TextSecondary));
        ImGui::TextWrapped("%s", info->detail);
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if (kHasSystemSettings && info && info->settingsUri) {
        if (DS::GlassButton(info->buttonLabel, ImVec2(0.0f, 26.0f), DS::AccentColor)) {
            OpenSystemSettings(info->settingsUri);
        }
        ImGui::SameLine(0.0f, 8.0f);
    }
    if (DS::GlassButton("Reintentar", ImVec2(100.0f, 26.0f), DS::SuccessColor)) {
        StartListening(va, now);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

void VoiceAssistantPanel::RenderLiveBox(Core::VoiceAssistant& va, bool running, bool hearing)
{
    const std::string liveHypo  = va.GetLiveHypothesis();
    const std::string lastHeard = va.GetLastHeardText();
    const double now = ImGui::GetTime();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 liveMin = ImGui::GetCursorScreenPos();
    const float  liveW   = ImGui::GetContentRegionAvail().x;
    const ImVec2 liveMax = { liveMin.x + liveW, liveMin.y + kLiveHeight };

    ImGui::Dummy(ImVec2(liveW, kLiveHeight));

    // Fondo oscuro moderno y cuadrado
    const ImU32 liveBg     = hearing ? IM_COL32(18, 30, 24, 255) : IM_COL32(22, 23, 26, 255);
    const ImU32 liveBorder = hearing ? DS::SuccessColor : DS::GlassBorder;
    dl->AddRectFilled(liveMin, liveMax, liveBg, DS::RadiusMedium);
    dl->AddRect(liveMin, liveMax, liveBorder, DS::RadiusMedium, 0, 1.0f);

    // ── Barra superior interna del cuadro: Tag + Visualizador de audio ──
    const float tagY = liveMin.y + 7.0f;
    dl->AddText({ liveMin.x + 12.0f, tagY }, DS::TextHint, "TRANSCRIPCIÓN EN TIEMPO REAL");

    // Medidor de audio animado de 5 barras
    const float barW = 3.5f;
    const float barGap = 2.5f;
    const float totalBarsW = 5 * barW + 4 * barGap;
    const float barsRight = liveMax.x - 14.0f;
    const float barsCenterY = tagY + 6.0f;

    for (int i = 0; i < 5; ++i) {
        float barX = barsRight - totalBarsW + i * (barW + barGap);
        float h = 3.0f;
        ImU32 bCol = DS::TextHint;

        if (hearing) {
            float phase = (float)now * 14.0f + i * 1.1f;
            h = 4.0f + std::abs(std::sin(phase)) * 10.0f;
            bCol = DS::SuccessColor;
        } else if (running) {
            h = 3.0f;
            bCol = DS::TextSecondary;
        }

        dl->AddRectFilled({ barX, barsCenterY - h * 0.5f }, { barX + barW, barsCenterY + h * 0.5f }, bCol, 1.0f);
    }

    // ── Texto de transcripción ──
    const float textMaxW = std::max(40.0f, liveW - 24.0f);
    const float textY    = liveMin.y + 28.0f;
    ImGui::SetCursorScreenPos({ liveMin.x + 12.0f, textY });

    if (!liveHypo.empty()) {
        std::string display = "\"" + liveHypo + "...\"";
        ImGui::TextColored(Col(DS::SuccessColor), "%s", TruncateTail(display, textMaxW).c_str());
    } else if (!lastHeard.empty()) {
        std::string display = "\"" + lastHeard + "\"";
        ImGui::TextColored(Col(DS::TextPrimary), "%s", TruncateTail(display, textMaxW).c_str());
    } else {
        const char* hint = !running ? "Micrófono detenido. Haz clic en Iniciar para escuchar."
                         : hearing  ? "Captando sonido..."
                                    : "Habla o canta hacia el micrófono (ej. 'Génesis 1:1')...";
        ImGui::TextColored(Col(DS::TextHint), "%s", hint);
    }

    ImGui::SetCursorScreenPos({ liveMin.x, liveMax.y });
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Panel principal
// ═════════════════════════════════════════════════════════════════════════════

void VoiceAssistantPanel::Render(bool* pOpen, GlassRenderer& glass)
{
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize({ 500.0f, 560.0f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({ 380.0f, 380.0f }, { 1200.0f, 1600.0f });

    const bool open = DS::BeginGlassPanel("Asistente de Voz", glass, pOpen,
        ImGuiWindowFlags_NoCollapse,
        ImVec2(14.0f, 12.0f));

    if (open) {
        auto& va = Core::VoiceAssistant::Get();
        const double now = ImGui::GetTime();

        // 1) Reinicio diferido pendiente (cambio de microfono o "Reiniciar escucha").
        ProcessDeferredRestart(va, now);

        // 2) Una sola lectura del estado por frame
        const bool        running  = va.IsRunning();
        const bool        hearing  = running && va.IsReceivingAudio();
        const std::string lastErr  = va.GetLastError();
        const bool        hasError = !lastErr.empty();

        UpdateState(running, hearing, hasError, now);

        const bool restarting = (m_PendingRestartAt >= 0.0);
        const bool starting   = !running && (restarting || (m_StartRequestedTime >= 0.0 && !hasError));

        // ── Tarjeta de Estado y Botón Principal ─────────────────────────────
        RenderStatusCard(va, running, starting, now);

        // ── Errores y avisos ────────────────────────────────────────────────
        if (!running && !starting) {
            if (hasError) {
                RenderErrorBlock(va, lastErr, now);
            } else if (m_StartFailedSilently) {
                ImGui::PushStyleColor(ImGuiCol_Text, Col(DS::DangerColor));
                ImGui::TextWrapped("El motor de voz no arrancó. Revisa que el micrófono no esté en uso por otra app.");
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if (DS::GlassButton("Reintentar arranque", ImVec2(0.0f, 26.0f), DS::SuccessColor)) {
                    StartListening(va, now);
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            } else if (m_UnexpectedStop) {
                ImGui::PushStyleColor(ImGuiCol_Text, Col(kWarnColor));
                ImGui::TextWrapped("La escucha se detuvo. El micrófono pudo desconectarse.");
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if (DS::GlassButton("Reanudar escucha", ImVec2(0.0f, 26.0f), DS::SuccessColor)) {
                    StartListening(va, now);
                }
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            }
        }

        // ── Selector de micrófono ───────────────────────────────────────────
        RenderMicSelector(va, running, now);

        // ── Visualización de captura en vivo ────────────────────────────────
        RenderLiveBox(va, running, hearing);

        // Aviso de silencio prolongado con motor activo
        if (running && !hearing && (now - m_LastAudioTime) > kSilenceHintSeconds) {
            ImGui::PushStyleColor(ImGuiCol_Text, Col(kWarnColor));
            ImGui::TextWrapped("Sin voz detectada desde hace %d s. Verifica que el micrófono no esté silenciado.",
                               static_cast<int>(now - m_LastAudioTime));
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            if (DS::GlassButton("Reiniciar escucha", ImVec2(140.0f, 26.0f), DS::AccentColor)) {
                RestartListening(va, now);
            }
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
        }

        // ── Coincidencia detectada ──────────────────────────────────────────
        if (va.HasPendingMatch()) {
            const auto match = va.GetPendingMatch();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 mMin = ImGui::GetCursorScreenPos();
            float mW = ImGui::GetContentRegionAvail().x;
            ImVec2 mMax = { mMin.x + mW, mMin.y + 44.0f };

            ImGui::Dummy(ImVec2(mW, 44.0f));

            dl->AddRectFilled(mMin, mMax, IM_COL32(20, 38, 28, 255), DS::RadiusSmall);
            dl->AddRect(mMin, mMax, DS::SuccessColor, DS::RadiusSmall, 0, 1.0f);

            ImGui::SetCursorScreenPos({ mMin.x + 10.0f, mMin.y + 6.0f });
            ImGui::TextColored(Col(DS::SuccessColor), "● Coincidencia detectada:");
            ImGui::SetCursorScreenPos({ mMin.x + 10.0f, mMin.y + 22.0f });
            ImGui::TextColored(Col(DS::TextPrimary), "%s", match.title.c_str());

            if (!va.GetAutoProject()) {
                ImGui::SetCursorScreenPos({ mMax.x - 180.0f, mMin.y + 8.0f });
                if (DS::GlassButton("Proyectar [Enter]", ImVec2(110.0f, 26.0f), DS::SuccessColor)) {
                    va.ConfirmPendingMatch();
                }
                ImGui::SameLine(0.0f, 6.0f);
                if (DS::GlassButton("X", ImVec2(26.0f, 26.0f), DS::DangerColor)) {
                    va.DismissPendingMatch();
                }
            }

            ImGui::SetCursorScreenPos({ mMin.x, mMax.y });
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // ── Modo de búsqueda por voz ────────────────────────────────────────
        ImGui::TextColored(Col(DS::AccentColor), "MODO DE RECONOCIMIENTO");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        int currentScope = static_cast<int>(va.GetMatchScope());
        const char* scopes[] = { "Todo el repertorio", "Solo Biblia", "Canción activa" };
        const char* scopeDescs[] = {
            "Busca en toda la biblioteca (citas bíblicas y letras de canciones).",
            "Solo citas y versículos bíblicos (recomendado durante la prédica).",
            "Sigue automáticamente las estrofas de la canción activa en pantalla."
        };

        const float totalW = ImGui::GetContentRegionAvail().x;
        const float tabW   = std::floor((totalW - 2 * 6.0f) / 3.0f);

        for (int s = 0; s < 3; ++s) {
            if (s > 0) ImGui::SameLine(0.0f, 6.0f);
            const bool isSel = (currentScope == s);

            ImVec4 btnBg = isSel ? ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim) : ImVec4(1.0f, 1.0f, 1.0f, 0.04f);
            ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
            ImGui::PushStyleColor(ImGuiCol_Border, isSel ? ImGui::ColorConvertU32ToFloat4(DS::AccentColor) : ImGui::ColorConvertU32ToFloat4(DS::GlassBorder));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            std::string btnId = std::string(scopes[s]) + "##scope_" + std::to_string(s);
            if (ImGui::Button(btnId.c_str(), ImVec2(tabW, 30.0f))) {
                va.SetMatchScope(static_cast<Core::VoiceMatchScope>(s));
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextColored(Col(DS::TextHint), "%s", scopeDescs[std::clamp(currentScope, 0, 2)]);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Si está en modo 'Solo canción activa', mostrar estado de la canción
        if (currentScope == 2) {
            auto sel = Core::PresentationCore::Get().PeekSelection();
            if (sel.type == Core::ItemType::Song && !sel.contentData.empty()) {
                ImGui::TextColored(Col(DS::SuccessColor), "● Canción activa: %s (%d estrofas)",
                                   sel.title.c_str(), static_cast<int>(sel.contentData.size()));
            } else {
                ImGui::TextColored(Col(kWarnColor), "⚠ Ninguna canción seleccionada en pantalla.");
                ImGui::TextColored(Col(DS::TextHint), "Selecciona una canción en el panel de canciones para iniciar el seguimiento.");
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        // Opciones de comportamiento
        bool autoProj = va.GetAutoProject();
        if (ImGui::Checkbox("Proyectar automáticamente al detectar", &autoProj)) {
            va.SetAutoProject(autoProj);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Si está activo, cambia la pantalla automáticamente.\nSi está apagado, muestra una sugerencia para confirmar con Enter.");
        }

        if (currentScope == 0 || currentScope == 1) {
            bool audioFix = va.GetAudioFixEnabled();
            if (ImGui::Checkbox("Interpretar números hablados como citas (ej. 'Génesis 3 4' -> Génesis 3:4)", &audioFix)) {
                va.SetAudioFixEnabled(audioFix);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Detecta números consecutivos dichos por voz como Capítulo:Versículo.");
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        float conf = va.GetConfidenceThreshold();
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::SliderFloat("Sensibilidad", &conf, 0.30f, 0.90f, "%.2f")) {
            va.SetConfidenceThreshold(conf);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0.50 es el valor recomendado.\nValores más bajos aceptan coincidencias con menos palabras similares.");
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // ── Simulación / Prueba rápida ──────────────────────────────────────
        ImGui::TextColored(Col(DS::AccentColor), "SIMULACIÓN DE VOZ (PRUEBA RÁPIDA)");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 88.0f);
        const bool enterPressed = ImGui::InputTextWithHint("##testInput",
            "Escribe una frase (ej. Juan 3 16 o letra)...",
            m_TestInputBuf, sizeof(m_TestInputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0.0f, 6.0f);
        const bool probarPressed = ImGui::Button("Probar", ImVec2(80.0f, 0.0f));
        if ((probarPressed || enterPressed) && std::strlen(m_TestInputBuf) > 0) {
            va.ProcessSpokenText(m_TestInputBuf);
        }

        ImGui::PopStyleVar(2);

        // Botones de prueba rápida (chips)
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        const char* kQuickChips[] = { "Genesis 1:1", "Salmos 23", "Juan 3:16", "Romanos 8:28" };
        for (int i = 0; i < 4; ++i) {
            if (i > 0) ImGui::SameLine(0.0f, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusSmall);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
            if (ImGui::SmallButton(kQuickChips[i])) {
                std::strncpy(m_TestInputBuf, kQuickChips[i], sizeof(m_TestInputBuf) - 1);
                m_TestInputBuf[sizeof(m_TestInputBuf) - 1] = '\0';
                va.ProcessSpokenText(kQuickChips[i]);
            }
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(Col(DS::TextHint), "Reconocimiento de voz local y seguro, sin enviar audio a servidores externos.");
    }

    DS::EndGlassPanel();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Banner de sugerencia (modo asistido)
// ═════════════════════════════════════════════════════════════════════════════

void VoiceAssistantPanel::RenderAssistedPromptBanner()
{
    auto& va = Core::VoiceAssistant::Get();
    if (!va.HasPendingMatch() || va.GetAutoProject()) return;

    const auto match = va.GetPendingMatch();

    // Atajos de teclado
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            va.ConfirmPendingMatch();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            va.DismissPendingMatch();
            return;
        }
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const float bannerW = 560.0f;
    const float bannerH = 46.0f;
    const ImVec2 bannerPos = { vp->WorkPos.x + (vp->WorkSize.x - bannerW) * 0.5f,
                               vp->WorkPos.y + vp->WorkSize.y - 66.0f };

    ImGui::SetNextWindowPos(bannerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(bannerW, bannerH), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(vp->ID);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 22, 26, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, DS::SuccessColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 9.0f));

    if (ImGui::Begin("##voicePromptBanner", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cur = ImGui::GetCursorScreenPos();
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        const float projW    = 120.0f;
        const float dismissW = 75.0f;
        const float btnH     = 26.0f;
        const float buttonsW = projW + spacing + dismissW;
        const float contentW = ImGui::GetContentRegionAvail().x;

        HomeIcons::DrawIcon_Microphone(dl, { cur.x + 2.0f, cur.y + 2.0f }, 22.0f, DS::SuccessColor);

        // Título de la coincidencia
        const float titleMaxW = std::max(40.0f, contentW - buttonsW - 32.0f - 10.0f);
        const float lineH     = ImGui::GetTextLineHeight();
        ImGui::SetCursorScreenPos({ cur.x + 32.0f, cur.y + (btnH - lineH) * 0.5f });
        ImGui::TextColored(Col(DS::SuccessColor), "%s",
                           TruncateHead("Detectado: " + match.title, titleMaxW).c_str());

        // Botones de acción alineados a la derecha
        ImGui::SetCursorScreenPos({ cur.x + contentW - buttonsW, cur.y });
        if (DS::GlassButton("Proyectar [Enter]", ImVec2(projW, btnH), DS::SuccessColor)) {
            va.ConfirmPendingMatch();
        }
        ImGui::SameLine(0.0f, spacing);
        if (DS::GlassButton("Cerrar [Esc]", ImVec2(dismissW, btnH), DS::DangerColor)) {
            va.DismissPendingMatch();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI
