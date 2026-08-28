#include "OClock.h"
#include "GlassRenderer.h"
#include "DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include "frontend/ui/UIStrings.h"
#include "frontend/ui/WikiHelp.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace ProyecThor::UI {

static ImU32 ColU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}
static ImU32 ColA(ImU32 col, int a) {
    return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
}
// DS:: expone colores como ImU32; ImGui::TextColored/PushStyleColor piden ImVec4.
static ImVec4 ToVec4(ImU32 col) {
    return ImGui::ColorConvertU32ToFloat4(col);
}

// Sombra suave reutilizando el mismo patrón visual que el resto de paneles.
static void DrawSoftShadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding) {
    for (float i = 1.0f; i <= 5.0f; i += 1.0f) {
        int alpha = static_cast<int>(34.0f - (i * 5.0f));
        dl->AddRectFilled(
            ImVec2(p0.x - i, p0.y - i + 3.0f),
            ImVec2(p1.x + i, p1.y + i + 3.0f),
            IM_COL32(0, 0, 0, std::max(0, alpha)), rounding + i);
    }
}

static constexpr int kPresetMinutes[] = { 5, 10, 15, 20, 30, 45 };

// Constantes de espaciado, centralizadas para que todo el panel respete la
// misma grilla en vez de numeros sueltos repartidos por cada funcion (eso
// era buena parte de por que la UI se sentia "en el aire": cada sección
// usaba su propio gap arbitrario, sin relacion con las demas).
namespace {
    constexpr float kGapTight  = 5.0f;   // separacion entre elementos muy relacionados (ej. checkboxes)
    constexpr float kGapNormal = 7.0f;   // separacion estandar entre campos de un mismo grupo
    constexpr float kGapWide   = 12.0f;  // separacion entre grupos distintos dentro de la misma seccion
    constexpr float kCardH     = 48.0f;  // alto estandar de las tarjetas seleccionables (modo/direccion)
}

// ── Ciclo de vida / lógica de tiempo ────────────────────────────────────────

OClock::OClock() : m_ElapsedTime(std::chrono::seconds(0)) {}

void OClock::Start(int minutes, int seconds) {
    if (m_PausedElapsed.count() <= 0.0) {
        m_TargetTime = std::chrono::minutes(minutes) + std::chrono::seconds(seconds);
    }
    auto now    = std::chrono::steady_clock::now();
    m_StartTime = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_PausedElapsed);
    m_IsRunning = true;
}

void OClock::Stop() {
    m_PausedElapsed = m_ElapsedTime;
    m_IsRunning      = false;
}

void OClock::Reset() {
    m_IsRunning     = false;
    m_IsOvertime    = false;
    m_ElapsedTime   = std::chrono::seconds(0);
    m_PausedElapsed = std::chrono::seconds(0);
}

void OClock::ApplyPreset(int minutes) {
    m_InputMin = minutes;
    m_InputSec = 0;
}

std::string OClock::GetFormattedTime() const {
    // ── Modo reloj de pared: hora actual del dispositivo ────────────────
    if (m_Mode == OClockMode::WallClock) {
        auto        now = std::chrono::system_clock::now();
        std::time_t tt   = std::chrono::system_clock::to_time_t(now);
        std::tm     localTm{};
#ifdef _WIN32
        localtime_s(&localTm, &tt);
#else
        localtime_r(&tt, &localTm);
#endif
        int hour = localTm.tm_hour;
        int mins = localTm.tm_min;
        int secs = localTm.tm_sec;

        std::string suffix;
        if (!m_WallClock24h) {
            suffix = (hour >= 12) ? " PM" : " AM";
            hour   = hour % 12;
            if (hour == 0) hour = 12;
        }

        char buffer[24];
        if (m_WallClockShowSeconds)
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, mins, secs);
        else
            snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, mins);

        return std::string(buffer) + suffix;
    }

    // ── Modo cronometro / cuenta regresiva (comportamiento original) ────
    int targetSecs  = static_cast<int>(m_TargetTime.count());
    int elapsedSecs = std::max<int>(
        0, (int)std::chrono::duration_cast<std::chrono::seconds>(m_ElapsedTime).count());

    // El "cruce a final" (m_IsOvertime) es siempre elapsed >= target, sin
    // importar el sentido. Lo que cambia es que numero se muestra:
    //  - CountUp:   se muestra el elapsed tal cual (sigue subiendo en overtime).
    //  - CountDown: se muestra target-elapsed mientras sea >= 0; una vez
    //               cruzado el 0, se muestra el excedente con signo "-".
    int displaySecs;
    if (m_Direction == OClockDirection::CountDown) {
        int remaining = targetSecs - elapsedSecs;
        displaySecs = (remaining >= 0) ? remaining : -remaining;
    } else {
        displaySecs = elapsedSecs;
    }

    int mins = displaySecs / 60;
    int secs = displaySecs % 60;
    char buffer[20];

    if (m_ShowSignPrefix && m_IsOvertime) {
        char sign = (m_Direction == OClockDirection::CountDown) ? '+' : '+';
        snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign, mins, secs);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    }
    return std::string(buffer);
}

float OClock::GetProgressRatio() const {
    if (m_Mode != OClockMode::Timer) return 0.0f;

    double targetSecs = static_cast<double>(m_TargetTime.count());
    if (targetSecs <= 0.0) return 0.0f;
    double elapsedSecs = m_ElapsedTime.count();
    return static_cast<float>(std::clamp(elapsedSecs / targetSecs, 0.0, 1.0));
}

// ── Título / mensaje ─────────────────────────────────────────────────────

std::string OClock::GetCurrentTitle() const {
    if (m_TitleIndex < 0 || m_TitleIndex >= (int)m_Titles.size()) return "";
    return m_Titles[m_TitleIndex];
}

void OClock::AdvanceTitle() {
    if (m_Titles.empty()) return;
    m_TitleIndex = (m_TitleIndex + 1) % (int)m_Titles.size();
}

void OClock::SyncTransmission(const std::string& timeStr) {
    auto& core = Core::PresentationCore::Get();

    bool wasLAN = (m_PrevTransmitMode == OClockTransmitMode::LAN);
    bool isLAN  = (m_TransmitMode     == OClockTransmitMode::LAN);

    // Estilo (solo LAN): si el usuario definio un "estilo final" explicito, se
    // usa al llegar al final del conteo (overtime). Si no definio uno, se usa el
    // estilo normal configurado para LAN + color de peligro forzado via colorOverride.
    // El estilo de la pantalla publica NUNCA se modifica desde aca (para el
    // publico se usa la capa Clock del editor de overlays).
    bool usingFinalStyle = m_IsOvertime && !m_FinalStyleName.empty();
    const std::string& styleToApply = usingFinalStyle ? m_FinalStyleName : m_StyleName;

    ImVec4 dangerV4 = ImGui::ColorConvertU32ToFloat4(DS::DangerColor);
    float  dangerRGBA[4] = { dangerV4.x, dangerV4.y, dangerV4.z, dangerV4.w };
    const float* colorOverride = (m_IsOvertime && !usingFinalStyle) ? dangerRGBA : nullptr;

    // Título activo + tiempo, combinados en un solo bloque de texto.
    std::string title    = GetCurrentTitle();
    std::string fullText = title.empty() ? timeStr : (title + "\n" + timeStr);

    if (isLAN) {
        core.SetLiveQuickNoteLAN(fullText, colorOverride, styleToApply);
    } else if (wasLAN) {
        core.ClearQuickNoteLAN();
    }

    // Reloj en overlay: se publica SIEMPRE (no depende de m_TransmitMode) --
    // la pantalla principal ya no tiene un modo on/off propio, la visibilidad
    // la decide exclusivamente si el overlay activo tiene o no un cuadro de
    // reloj (ver PresentationCore::HasOverlayClockLayer, consumido en
    // LiveContentRenderer.cpp/UIManager.cpp). Publicar sin esa capa es
    // inofensivo: simplemente no se dibuja en ningun lado.
    core.SetLiveOverlayClockText(fullText, colorOverride);

    m_PrevTransmitMode = m_TransmitMode;
}

// ── Update: logica pura, sin ImGui, corre todos los frames ─────────────────

void OClock::Update() {
    // Mensajes pedidos desde el celular (ver PresentationCore::
    // PushRemoteClockTitle / SyncServer POST /remote/clock-message) -- se
    // agregan a la lista igual que "Agregar" a mano, pero se activan de
    // inmediato (a diferencia del boton de escritorio, que no cambia la
    // selección activa): el sentido de "enviar" desde el celular es verlo
    // en el momento. Si llegara mas de uno en el mismo frame, gana el
    // ultimo (queda como m_TitleIndex final).
    for (auto& text : Core::PresentationCore::Get().DrainRemoteClockTitles()) {
        if (text.empty()) continue;
        m_Titles.push_back(text);
        m_TitleIndex = (int)m_Titles.size() - 1;
    }

    if (m_Mode == OClockMode::Timer) {
        // m_ElapsedTime/m_IsOvertime son independientes del sentido de
        // visualizacion: siempre representan "cuanto paso desde Start()" y
        // "si ya cruzamos el objetivo". GetFormattedTime() decide como
        // mostrarlo.
        if (m_IsRunning) {
            auto now      = std::chrono::steady_clock::now();
            m_ElapsedTime = now - m_StartTime;
            m_IsOvertime  = m_ElapsedTime >= m_TargetTime;
        }
    } else {
        // En modo reloj de pared no existe concepto de "objetivo excedido".
        m_IsOvertime = false;
    }

    // Se sincroniza la transmision (proyector/LAN) en cada llamada a
    // Update(), sin importar si el panel esta visible o no. Antes esta
    // linea vivia unicamente dentro de Render(), y como Render() solo se
    // ejecuta cuando la pestaña de OClock esta activa, al cambiar de
    // pestaña el texto transmitido quedaba congelado en el ultimo valor
    // dibujado, aunque el tiempo interno siguiera corriendo correctamente
    // por detras (se recalcula desde steady_clock/system_clock, nunca se
    // "pausa" solo por no dibujarse). Quien integra este widget debe
    // llamar OClock::Update() una vez por frame de forma incondicional,
    // junto al resto de las actualizaciones de fondo de la aplicacion.
    SyncTransmission(GetFormattedTime());

    // Cue de cambio de estilo pendiente: "consumir una vez", así no pisa un
    // cambio manual del operador en RenderStyleSelector salvo que realmente
    // haya una cue pendiente que lo pida.
    std::string clockCue = Core::PresentationCore::Get().ConsumeClockStyleCue();
    if (!clockCue.empty())
        m_StyleName = clockCue;
}

void OClock::RenderStyleSelector() {
    auto& core = Core::PresentationCore::Get();
    std::vector<std::string> styleNames = core.GetSavedStyleNames();

    // Un solo combo reutilizable para "estilo normal" y "estilo final".
    auto renderCombo = [&](const char* label, const char* comboId,
                           std::string& target, const char* emptyHint) {
        ImGui::Spacing();
        ImGui::TextColored(ToVec4(DS::TextHint), "%s", label);

        std::string preview = target.empty() ? "Usar estilo actual" : target;

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::SetNextItemWidth(-1.0f);

        if (ImGui::BeginCombo(comboId, preview.c_str())) {
            bool noneSelected = target.empty();
            if (ImGui::Selectable("Usar estilo actual", noneSelected))
                target.clear();
            if (noneSelected) ImGui::SetItemDefaultFocus();

            for (const auto& name : styleNames) {
                bool sel = (target == name);
                if (ImGui::Selectable(name.c_str(), sel))
                    target = name;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        if (target.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
            ImGui::TextWrapped("%s", emptyHint);
            ImGui::PopStyleColor();
        }
    };

    renderCombo("Estilo (LAN)", "##oclockStyle", m_StyleName,
        "Hereda el último estilo activo.");

    renderCombo("Estilo al finalizar (LAN)", "##oclockFinalStyle", m_FinalStyleName,
        "Usa el estilo normal + color de peligro.");
}

// ── Selector de modo: Cronómetro vs Hora actual ─────────────────────────

void OClock::RenderModeSelector() {
    ImGui::TextColored(ToVec4(DS::TextHint), "Modo");
    ImGui::Spacing();

    float w    = ImGui::GetContentRegionAvail().x;
    float gap  = kGapNormal;
    float half = (w - gap) * 0.5f;

    struct ModeOpt { const char* label; const char* sub; OClockMode mode; };
    ModeOpt opts[2] = {
        { "Cronómetro",  "Cuenta con objetivo (arriba o abajo)", OClockMode::Timer     },
        { "Hora actual", "Muestra la hora del dispositivo",      OClockMode::WallClock },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 2; ++i) {
        auto& opt   = opts[i];
        bool active = (m_Mode == opt.mode);

        ImVec2 p0 = ImVec2(rowStart.x + i * (half + gap), rowStart.y);
        ImVec2 p1 = ImVec2(p0.x + half, p0.y + kCardH);

        ImU32 bg  = active ? ColA(DS::AccentColor, 45) : ImU32(IM_COL32(255, 255, 255, 10));
        ImU32 bdr = active ? ColA(DS::AccentColor, 200) : ColA(DS::TextHint, 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, bdr, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##oclockMode", ImVec2(half, kCardH));
        ImGui::PopID();

        if (clicked && m_Mode != opt.mode) {
            m_Mode = opt.mode;
            // Cambiar a "Hora actual" corta cualquier cronómetro en curso:
            // evita que quede corriendo (y consumiendo overtime) de forma
            // invisible mientras se muestra la hora del dispositivo.
            if (opt.mode == OClockMode::WallClock)
                Stop();
        }

        ImU32 labelCol = active ? DS::AccentLight : DS::TextSecondary;
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f), labelCol, opt.label);

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.80f,
                    ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                    ColA(DS::TextHint, 210), opt.sub, nullptr, half - 20.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + kCardH));
    ImGui::Dummy(ImVec2(w, kCardH));
}

void OClock::RenderDirectionSelector() {
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "Sentido del conteo");
    ImGui::Spacing();

    float w    = ImGui::GetContentRegionAvail().x;
    float gap  = kGapNormal;
    float half = (w - gap) * 0.5f;

    struct DirOpt { const char* label; const char* sub; OClockDirection dir; };
    DirOpt opts[2] = {
        { "Ascendente",  "Cuenta desde 0 hacia el objetivo",   OClockDirection::CountUp   },
        { "Descendente", "Cuenta regresiva desde el objetivo", OClockDirection::CountDown },
    };

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 2; ++i) {
        auto& opt   = opts[i];
        bool active = (m_Direction == opt.dir);

        ImVec2 p0 = ImVec2(rowStart.x + i * (half + gap), rowStart.y);
        ImVec2 p1 = ImVec2(p0.x + half, p0.y + kCardH);

        ImU32 bg  = active ? ColA(DS::AccentColor, 45) : ImU32(IM_COL32(255, 255, 255, 10));
        ImU32 bdr = active ? ColA(DS::AccentColor, 200) : ColA(DS::TextHint, 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, bdr, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        bool clicked = ImGui::InvisibleButton("##dir", ImVec2(half, kCardH));
        ImGui::PopID();
        if (clicked) m_Direction = opt.dir;

        ImU32 labelCol = active ? DS::AccentLight : DS::TextSecondary;
        dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 8.0f), labelCol, opt.label);

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.80f,
                    ImVec2(p0.x + 10.0f, p0.y + 28.0f),
                    ColA(DS::TextHint, 210), opt.sub, nullptr, half - 20.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + kCardH));
    ImGui::Dummy(ImVec2(w, kCardH));
}

// ── Opciones de formato para el modo reloj de pared ─────────────────────

void OClock::RenderWallClockOptions() {
    ImGui::Spacing();
    ImGui::TextColored(ToVec4(DS::TextHint), "Formato");
    ImGui::Spacing();

    ImGui::Checkbox("Formato 24 horas", &m_WallClock24h);
    ImGui::SameLine(0, kGapWide);
    ImGui::Checkbox("Mostrar segundos", &m_WallClockShowSeconds);
}

void OClock::RenderTitleSection() {
    ImGui::Spacing();
    DS::GlassSectionHeader("TÍTULO / MENSAJE");
    ImGui::Spacing();

    float w       = ImGui::GetContentRegionAvail().x;
    float addBtnW = 90.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

    ImGui::SetNextItemWidth(w - addBtnW - kGapNormal);
    ImGui::InputTextWithHint("##oclock_title_input", "Nuevo mensaje...",
        m_TitleInputBuf, sizeof(m_TitleInputBuf));

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, kGapNormal);
    if (DS::GlassButton("Agregar", ImVec2(addBtnW, 0.0f), DS::AccentColorDim)) {
        std::string text(m_TitleInputBuf);
        if (!text.empty()) {
            m_Titles.push_back(text);
            if (m_TitleIndex < 0) m_TitleIndex = 0; // el primer mensaje se activa solo
            m_TitleInputBuf[0] = '\0';
        }
    }

    if (!m_Titles.empty()) {
        ImGui::Spacing();

        // Ancho del boton de borrar fijo, y el Selectable ocupa exactamente
        // el resto del ancho disponible. Antes el Selectable media
        // "w - 60" pero el botón se posicionaba a mano en "w - 50" con
        // ancho 40, dejando un hueco de 10px sin usar entre ambos y el
        // boton sin llegar al borde derecho real. Calculando todo a partir
        // del mismo "w" y encadenando con SameLine(0, gap) en vez de
        // coordenadas absolutas, ambos quedan perfectamente alineados y
        // ocupan el ancho completo sin importar el tamano de fuente.
        const float delBtnW = 40.0f;
        const float selW    = w - delBtnW - kGapNormal;

        for (int i = 0; i < (int)m_Titles.size(); ++i) {
            ImGui::PushID(i);
            bool isActive = (i == m_TitleIndex);

            ImGui::PushStyleColor(ImGuiCol_Text,
                isActive ? ToVec4(DS::AccentLight) : ToVec4(DS::TextSecondary));
            if (ImGui::Selectable(m_Titles[i].c_str(), isActive, 0, ImVec2(selW, 0.0f)))
                m_TitleIndex = i;
            ImGui::PopStyleColor();

            ImGui::SameLine(0, kGapNormal);
            if (DS::GlassButton("X", ImVec2(delBtnW, 0.0f), DS::DangerColor)) {
                m_Titles.erase(m_Titles.begin() + i);
                if (m_TitleIndex == i)
                    m_TitleIndex = m_Titles.empty() ? -1 : std::min(i, (int)m_Titles.size() - 1);
                else if (m_TitleIndex > i)
                    m_TitleIndex--;
                ImGui::PopID();
                break; // el vector cambio de tamano: cortamos el loop de este frame
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    float btnW = (w - kGapNormal) * 0.5f;

    ImGui::BeginDisabled(m_Titles.empty());
    if (DS::GlassButton("Avanzar >", ImVec2(btnW, 34.0f), DS::AccentColor))
        AdvanceTitle();
    ImGui::SameLine(0, kGapNormal);
    if (DS::GlassButton("Quitar título", ImVec2(btnW, 34.0f), DS::AccentColorDim))
        m_TitleIndex = -1;
    ImGui::EndDisabled();
}

// ── Render ───────────────────────────────────────────────────────────────

void OClock::Render(GlassRenderer& glass) {
    const auto& str  = ProyecThor::UI::GetUIStrings();
    auto&       core = Core::PresentationCore::Get();

    // Se llama tambien aca (ademas de la llamada global obligatoria desde
    // el tick de la aplicacion) para que, mientras el panel este visible,
    // el numero dibujado en este mismo frame sea el mas reciente posible.
    // Llamarlo dos veces en el mismo frame no tiene efectos secundarios
    // acumulativos: todo se recalcula desde cero a partir de los relojes
    // del sistema, nunca se incrementa nada.
    Update();

    (void)glass;
    std::string timeStr = GetFormattedTime();
    float       t       = static_cast<float>(ImGui::GetTime());

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.157f, 0.784f, 0.847f, 1.0f)); // acento cian
    ImGui::TextUnformatted(str.oclockTitle);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    Wiki::InfoButton(Wiki::Topic::OClock);
    DS::GlassSeparator();

    float w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Título / mensaje activo, arriba del display ─────────────────────
    std::string activeTitle = GetCurrentTitle();
    if (!activeTitle.empty()) {
        ImVec2 titleSz = ImGui::CalcTextSize(activeTitle.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - titleSz.x) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        ImGui::TextUnformatted(activeTitle.c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ── Display grande del tiempo ──────────────────────────────────────────
    {
        ImVec2 dispPos = ImGui::GetCursorScreenPos();
        float  dispH   = 92.0f;
        ImVec2 dispEnd = ImVec2(dispPos.x + w, dispPos.y + dispH);

        DrawSoftShadow(dl, dispPos, dispEnd, DS::RadiusLarge);

        ImU32 bgCol, borderCol, textCol;
        if (m_Mode == OClockMode::WallClock) {
            // El reloj de pared esta siempre "vivo": usamos el mismo
            // estilo que el cronometro corriendo, de forma permanente.
            bgCol     = ColU32(0.04f, 0.16f, 0.17f);
            borderCol = ColA(DS::AccentColor, 130);
            textCol   = DS::AccentLight;
        } else if (m_IsOvertime) {
            float pulse = 0.55f + 0.35f * std::sin(t * 3.0f);
            bgCol     = ColU32(0.22f, 0.05f, 0.06f);
            borderCol = ColA(DS::DangerColor, static_cast<int>(90 + 90 * pulse));
            textCol   = DS::DangerColor;
        } else if (m_IsRunning) {
            bgCol     = ColU32(0.04f, 0.16f, 0.17f);
            borderCol = ColA(DS::AccentColor, 130);
            textCol   = DS::AccentLight;
        } else {
            bgCol     = ColU32(0.08f, 0.09f, 0.12f);
            borderCol = ColA(DS::TextHint, 150);
            textCol   = DS::TextSecondary;
        }

        dl->AddRectFilled(dispPos, dispEnd, bgCol, DS::RadiusLarge);
        dl->AddRect(dispPos, dispEnd, borderCol, DS::RadiusLarge, 0, 1.5f);

        bool bigFont = (ImGui::GetIO().Fonts->Fonts.Size > 1);
        if (bigFont) ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

        bool showProgressBar = (m_Mode == OClockMode::Timer) && m_ShowProgressBar;

        ImVec2 textSz  = ImGui::CalcTextSize(timeStr.c_str());
        ImVec2 textPos = ImVec2(
            dispPos.x + (w - textSz.x) * 0.5f,
            dispPos.y + (dispH - textSz.y) * 0.5f - (showProgressBar ? 6.0f : 0.0f));

        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textCol, timeStr.c_str());

        if (bigFont) ImGui::PopFont();

        // ── Barra de progreso (objetivo) — solo tiene sentido en modo Timer ──
        if (showProgressBar) {
            float barH   = 6.0f;
            float barPad = 16.0f;
            ImVec2 bMin(dispPos.x + barPad, dispEnd.y - barH - 10.0f);
            ImVec2 bMax(dispEnd.x - barPad, bMin.y + barH);

            dl->AddRectFilled(bMin, bMax, ColA(DS::TextHint, 90), barH * 0.5f);

            if (m_IsOvertime) {
                dl->AddRectFilled(bMin, bMax, ColA(DS::DangerColor, 220), barH * 0.5f);
            } else {
                float ratio = GetProgressRatio();
                float fillX = bMin.x + (bMax.x - bMin.x) * ratio;
                if (fillX > bMin.x)
                    dl->AddRectFilled(bMin, ImVec2(fillX, bMax.y), ColA(DS::AccentColor, 230), barH * 0.5f);
            }
        }

        ImGui::Dummy(ImVec2(w, dispH));

        // Debajo del display: objetivo + estado (Timer) o etiqueta fija (WallClock)
        if (m_Mode == OClockMode::Timer) {
            char targetBuf[32];
            int  tgtSecs = static_cast<int>(m_TargetTime.count());
            snprintf(targetBuf, sizeof(targetBuf), "Objetivo: %02d:%02d", tgtSecs / 60, tgtSecs % 60);
            ImGui::TextColored(ToVec4(DS::TextSecondary), "%s", targetBuf);

            if (m_IsOvertime) {
                ImGui::SameLine();
                ImGui::TextColored(ToVec4(DS::DangerColor), "  •  Tiempo excedido");
            }
        } else {
            ImGui::TextColored(ToVec4(DS::TextSecondary), "Hora local del dispositivo");
        }
    }

    ImGui::Spacing();
    DS::GlassSeparator();

    // ── Configuración ────────────────────────────────────────────────────
    DS::GlassSectionHeader("CONFIGURACIÓN");
    ImGui::Spacing();

    RenderModeSelector();

    if (m_Mode == OClockMode::Timer) {
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.05f, 0.09f, 0.13f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.07f, 0.12f, 0.17f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        float halfW = (w - kGapNormal) * 0.5f;

        ImGui::BeginGroup();
        ImGui::TextColored(ToVec4(DS::TextHint), "%s", str.minutes);
        ImGui::SetNextItemWidth(halfW);
        ImGui::InputInt("##oclock_min", &m_InputMin, 0, 0);
        ImGui::EndGroup();

        ImGui::SameLine(0, kGapNormal);

        ImGui::BeginGroup();
        ImGui::TextColored(ToVec4(DS::TextHint), "%s", str.seconds);
        ImGui::SetNextItemWidth(halfW);
        ImGui::InputInt("##oclock_sec", &m_InputSec, 0, 0);
        ImGui::EndGroup();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        if (m_InputMin < 0)  m_InputMin = 0;
        if (m_InputSec < 0)  m_InputSec = 0;
        if (m_InputSec > 59) m_InputSec = 59;

        // ── Sentido del conteo ───────────────────────────────────────────
        RenderDirectionSelector();

        // ── Presets rápidos ─────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::TextColored(ToVec4(DS::TextHint), "Presets rápidos");
        ImGui::Spacing();

        int presetCount = static_cast<int>(sizeof(kPresetMinutes) / sizeof(kPresetMinutes[0]));
        float presetGap = kGapTight;
        float presetW   = (w - presetGap * (presetCount - 1)) / presetCount;

        for (int i = 0; i < presetCount; ++i) {
            bool active = (m_InputMin == kPresetMinutes[i] && m_InputSec == 0);
            char label[8];
            snprintf(label, sizeof(label), "%d'", kPresetMinutes[i]);
            if (DS::GlassButton(label, ImVec2(presetW, 30.0f), active ? DS::AccentColor : DS::AccentColorDim))
                ApplyPreset(kPresetMinutes[i]);
            if (i != presetCount - 1) ImGui::SameLine(0, presetGap);
        }

        ImGui::Spacing();

        // ── Opciones de visualización ─────────────────────────────────
        ImGui::Checkbox("Barra de progreso", &m_ShowProgressBar);
        ImGui::SameLine(0, kGapWide);
        ImGui::Checkbox("Prefijo signo en overtime", &m_ShowSignPrefix);
    } else {
        RenderWallClockOptions();
    }

    RenderStyleSelector();

    // ── Título / mensaje editable ────────────────────────────────────────
    RenderTitleSection();

    ImGui::Spacing();

    // ── Botones de control (solo modo Timer: en WallClock no hay nada que
    //    iniciar/pausar, el reloj del dispositivo corre siempre solo) ────
    if (m_Mode == OClockMode::Timer) {
        float btnW = (w - kGapNormal) * 0.5f;
        float btnH = 40.0f;

        if (!m_IsRunning) {
            const char* startLabel = (m_PausedElapsed.count() > 0.0) ? "Reanudar" : str.start;
            if (DS::GlassButton(startLabel, ImVec2(btnW, btnH), DS::SuccessColor))
                Start(m_InputMin, m_InputSec);
        } else {
            if (DS::GlassButton(str.pause, ImVec2(btnW, btnH), ColU32(0.85f, 0.6f, 0.1f)))
                Stop();
        }

        ImGui::SameLine(0, kGapNormal);
        if (DS::GlassButton(str.reset, ImVec2(btnW, btnH), DS::DangerColor))
            Reset();

        ImGui::Spacing();
    }

    DS::GlassSeparator();

    // ── En pantalla (overlay) ────────────────────────────────────────────
    // Ya no es un modo a elegir aca: aparece solo si el overlay activo
    // (Biblioteca > Overlays) tiene un cuadro de reloj configurado.
    {
        std::string overlayPath = core.GetOverlayPath();
        bool hasOverlay = !overlayPath.empty();
        bool hasClockBox = core.HasOverlayClockLayer();

        if (hasOverlay && hasClockBox) {
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::SuccessColor));
            ImGui::TextUnformatted("●");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 6);
            ImGui::TextColored(ToVec4(DS::AccentLight), "Mostrando en overlay activo");
        } else {
            const char* msg = !hasOverlay
                ? "● Sin overlay activo — activa uno con un cuadro de reloj para mostrarlo en pantalla."
                : "● El overlay activo no tiene un cuadro de reloj — agregalo desde el editor de Overlays.";
            ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
            ImGui::TextWrapped("%s", msg);
            ImGui::PopStyleColor();
        }
    }

    ImGui::Spacing();
    DS::GlassSeparator();

    // ── Transmitir a LAN ─────────────────────────────────────────────────
    DS::GlassSectionHeader("TRANSMITIR A RED (LAN)");

    bool netAvailable = core.IsStreamingNet();

    struct ModeOpt { const char* label; OClockTransmitMode mode; bool needsNet; };
    ModeOpt opts[2] = {
        { "Apagado", OClockTransmitMode::Off, false },
        { "Solo LAN", OClockTransmitMode::LAN, true  },
    };

    float cardGap = kGapNormal;
    float cardW   = (w - cardGap) * 0.5f;
    float cardH   = 40.0f;

    ImVec2 rowStart = ImGui::GetCursorScreenPos();

    for (int i = 0; i < 2; ++i) {
        auto& opt = opts[i];
        bool  disabled = opt.needsNet && !netAvailable;
        bool  active   = (m_TransmitMode == opt.mode) && !disabled;

        ImVec2 p0 = ImVec2(rowStart.x + i * (cardW + cardGap), rowStart.y);
        ImVec2 p1 = ImVec2(p0.x + cardW, p0.y + cardH);

        ImU32 bg  = active ? ColA(DS::AccentColor, 45) : ImU32(IM_COL32(255, 255, 255, 10));
        ImU32 bdr = active ? ColA(DS::AccentColor, 200) : ColA(DS::TextHint, disabled ? 60 : 120);

        dl->AddRectFilled(p0, p1, bg, DS::RadiusMedium);
        dl->AddRect(p0, p1, bdr, DS::RadiusMedium, 0, active ? 1.5f : 1.0f);

        ImGui::SetCursorScreenPos(p0);
        ImGui::PushID(i);
        ImGui::BeginDisabled(disabled);
        bool clicked = ImGui::InvisibleButton("##mode", ImVec2(cardW, cardH));
        ImGui::EndDisabled();
        ImGui::PopID();

        if (clicked && !disabled) m_TransmitMode = opt.mode;

        ImU32 labelCol = disabled ? ColA(DS::TextHint, 130) : (active ? DS::AccentLight : DS::TextSecondary);
        ImVec2 labelSz = ImGui::CalcTextSize(opt.label);
        dl->AddText(ImVec2(p0.x + (cardW - labelSz.x) * 0.5f, p0.y + (cardH - labelSz.y) * 0.5f), labelCol, opt.label);
    }

    ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + cardH));
    ImGui::Dummy(ImVec2(w, cardH));

    if (!netAvailable) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::TextHint));
        ImGui::TextWrapped("Inicia el servidor en Transmisión para habilitar \"Solo LAN\".");
        ImGui::PopStyleColor();
        if (m_TransmitMode == OClockTransmitMode::LAN) m_TransmitMode = OClockTransmitMode::Off;
    } else if (m_TransmitMode == OClockTransmitMode::LAN) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::DangerColor));
        ImGui::TextUnformatted("● Transmitiendo a la red local");
        ImGui::PopStyleColor();
    }
}

} // namespace ProyecThor::UI