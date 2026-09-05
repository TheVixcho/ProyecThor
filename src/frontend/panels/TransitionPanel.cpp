#include "TransitionPanel.h"
#include "backend/core/PresentationCore.h"
#include "DesignSystem.h"
#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <vector>
#include "UIStrings.h"

namespace ProyecThor::UI {

// =============================================================================
//  Nombre <-> tipo — ver TransitionPanel.h. Nombres estables en ingles (no
//  las etiquetas traducidas de RenderContent), usados para persistir la
//  eleccion de transicion de un MacroCue.
// =============================================================================
const char* TransitionTypeToName(TransitionType t)
{
    switch (t) {
        case TransitionType::None:         return "None";
        case TransitionType::Fade:         return "Fade";
        case TransitionType::ZoomIn:       return "ZoomIn";
        case TransitionType::ZoomOut:      return "ZoomOut";
        case TransitionType::SlideLeft:    return "SlideLeft";
        case TransitionType::SlideRight:   return "SlideRight";
        case TransitionType::SlideUp:      return "SlideUp";
        case TransitionType::SlideDown:    return "SlideDown";
        case TransitionType::CoverLeft:    return "CoverLeft";
        case TransitionType::CoverRight:   return "CoverRight";
        case TransitionType::CoverUp:      return "CoverUp";
        case TransitionType::CoverDown:    return "CoverDown";
        case TransitionType::UncoverLeft:  return "UncoverLeft";
        case TransitionType::UncoverRight: return "UncoverRight";
        case TransitionType::UncoverUp:    return "UncoverUp";
        case TransitionType::UncoverDown:  return "UncoverDown";
        case TransitionType::Iris:         return "Iris";
    }
    return "None";
}

TransitionType TransitionTypeFromName(const std::string& name)
{
    if (name == "Fade")         return TransitionType::Fade;
    if (name == "ZoomIn")       return TransitionType::ZoomIn;
    if (name == "ZoomOut")      return TransitionType::ZoomOut;
    if (name == "SlideLeft")    return TransitionType::SlideLeft;
    if (name == "SlideRight")   return TransitionType::SlideRight;
    if (name == "SlideUp")      return TransitionType::SlideUp;
    if (name == "SlideDown")    return TransitionType::SlideDown;
    if (name == "CoverLeft")    return TransitionType::CoverLeft;
    if (name == "CoverRight")   return TransitionType::CoverRight;
    if (name == "CoverUp")      return TransitionType::CoverUp;
    if (name == "CoverDown")    return TransitionType::CoverDown;
    if (name == "UncoverLeft")  return TransitionType::UncoverLeft;
    if (name == "UncoverRight") return TransitionType::UncoverRight;
    if (name == "UncoverUp")    return TransitionType::UncoverUp;
    if (name == "UncoverDown")  return TransitionType::UncoverDown;
    if (name == "Iris")         return TransitionType::Iris;
    return TransitionType::None;
}

// =============================================================================
//  EaseInOut — curva suave para la transicion
// =============================================================================
float TransitionPanel::EaseInOut(float t)
{
    // Smoothstep: 3t^2 - 2t^3
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// =============================================================================
//  Trigger / Update
// =============================================================================
void TransitionPanel::RestoreAfterOverrideIfNeeded()
{
    if (!m_HasSavedForOverride) return;
    m_SelectedType        = m_SavedType;
    m_Duration            = m_SavedDuration;
    m_HasSavedForOverride = false;
}

void TransitionPanel::Trigger()
{
    // Override puntual desde un MacroCue: pisa el tipo/duracion SOLO para
    // esta transicion (ver RestoreAfterOverrideIfNeeded, llamado cuando
    // termina). No toca la eleccion persistente del operador en este panel.
    std::string ovName;
    float       ovDuration = -1.0f;
    if (Core::PresentationCore::Get().ConsumePendingTransitionOverride(ovName, ovDuration))
    {
        if (!m_HasSavedForOverride) {
            m_SavedType           = m_SelectedType;
            m_SavedDuration       = m_Duration;
            m_HasSavedForOverride = true;
        }
        m_SelectedType = TransitionTypeFromName(ovName);
        if (ovDuration > 0.0f) m_Duration = ovDuration;
    }

    if (m_SelectedType == TransitionType::None) {
        RestoreAfterOverrideIfNeeded();
        return;
    }
    m_Elapsed  = 0.0f;
    m_Progress = 0.0f;
    // FIX: antes esto esperaba a que PresentationCore::IsBackgroundSwapPending()
    // fuera false antes de arrancar el cronometro (pensado para cuando este
    // Trigger() podia venir de un cambio de fondo/video). Ahora que
    // Trigger() SOLO se llama por cambios de texto (ver UIManager::
    // RenderAll(), que dispara esto desde textTransitionTrigger, no desde
    // el trigger de fondo/video), no hay ningun swap de video del que
    // depender — el texto siempre esta listo de inmediato. Esperar a un
    // swap de fondo ajeno hacia que, si habia uno en curso, el texto NUEVO
    // ya se mostrara instantaneo (correcto) pero luego, tarde, la animacion
    // arrancara igual trayendo de vuelta el texto VIEJO como "saliente" —
    // se veia como si la letra "se repitiera" o apareciera una letra sin
    // relacion por un instante.
    m_Active = true;
}

void TransitionPanel::Update(float dt)
{
    if (!m_Active) return;

    m_Elapsed += dt;
    float raw  = (m_Duration > 0.0f) ? (m_Elapsed / m_Duration) : 1.0f;
    m_Progress = EaseInOut(std::clamp(raw, 0.0f, 1.0f));

    if (m_Elapsed >= m_Duration) {
        m_Elapsed  = 0.0f;
        m_Progress = 1.0f;
        m_Active   = false;
        RestoreAfterOverrideIfNeeded();
    }
}

// =============================================================================
//  Offsets (Posición X / Y) — movimiento continuo, usan m_Progress completo
// =============================================================================
float TransitionPanel::GetOutgoingOffsetX() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideLeft:
        case TransitionType::UncoverLeft:
            return -m_Progress;
        case TransitionType::SlideRight:
        case TransitionType::UncoverRight:
            return m_Progress;
        default:
            return 0.0f;
    }
}

float TransitionPanel::GetOutgoingOffsetY() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideUp:
        case TransitionType::UncoverUp:
            return -m_Progress;
        case TransitionType::SlideDown:
        case TransitionType::UncoverDown:
            return m_Progress;
        default:
            return 0.0f;
    }
}

float TransitionPanel::GetIncomingOffsetX() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideLeft:
        case TransitionType::CoverLeft:
            return 1.0f - m_Progress;
        case TransitionType::SlideRight:
        case TransitionType::CoverRight:
            return -(1.0f - m_Progress);
        default:
            return 0.0f;
    }
}

float TransitionPanel::GetIncomingOffsetY() const
{
    switch (m_SelectedType) {
        case TransitionType::SlideUp:
        case TransitionType::CoverUp:
            return 1.0f - m_Progress;
        case TransitionType::SlideDown:
        case TransitionType::CoverDown:
            return -(1.0f - m_Progress);
        default:
            return 0.0f;
    }
}

// =============================================================================
//  Progreso local por mitad — solo para tipos secuenciales (Fade/Zoom)
// =============================================================================
float TransitionPanel::GetOutgoingLocalT() const
{
    // Mitad de SALIDA: [0, kSequentialSplit] remapeado a 0..1 local.
    return std::clamp(m_Progress / kSequentialSplit, 0.0f, 1.0f);
}

float TransitionPanel::GetIncomingLocalT() const
{
    // Mitad de ENTRADA: [kSequentialSplit, 1] remapeado a 0..1 local.
    return std::clamp((m_Progress - kSequentialSplit) / (1.0f - kSequentialSplit), 0.0f, 1.0f);
}

// =============================================================================
//  Alpha (Opacidad) — Fade/Zoom: sale por completo, luego entra por completo.
//  No es un crossfade simultaneo: evita que dos capas identicas dibujadas en
//  el mismo lugar generen un "dip" de opacidad a mitad de camino.
// =============================================================================
float TransitionPanel::GetOutgoingAlpha() const
{
    switch (m_SelectedType) {
        case TransitionType::Fade:
        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
        case TransitionType::Iris:
            return 1.0f - GetOutgoingLocalT();
        default:
            return 1.0f;
    }
}

float TransitionPanel::GetIncomingAlpha() const
{
    switch (m_SelectedType) {
        case TransitionType::Fade:
        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
        case TransitionType::Iris:
            return GetIncomingLocalT();
        default:
            return 1.0f;
    }
}

// =============================================================================
//  Scale (Escala para Zoom) — sincronizada con las mismas mitades que el alpha
// =============================================================================
float TransitionPanel::GetOutgoingScale() const
{
    switch (m_SelectedType) {
        case TransitionType::ZoomIn:
            return 1.0f + (GetOutgoingLocalT() * 0.5f); // Se agranda mientras desaparece
        case TransitionType::ZoomOut:
            return 1.0f - (GetOutgoingLocalT() * 0.5f); // Se achica mientras desaparece
        case TransitionType::Iris:
            return 1.0f;
        default:
            return 1.0f;
    }
}

float TransitionPanel::GetIncomingScale() const
{
    switch (m_SelectedType) {
        case TransitionType::ZoomIn:
            return 0.5f + (GetIncomingLocalT() * 0.5f); // Viene desde atrás (pequeño a normal)
        case TransitionType::ZoomOut:
            return 1.5f - (GetIncomingLocalT() * 0.5f); // Viene desde adelante (grande a normal)
        case TransitionType::Iris:
            return 0.2f + (GetIncomingLocalT() * 0.8f); // Revelado expansivo desde el centro
        default:
            return 1.0f;
    }
}

// =============================================================================
//  Render — panel de control de transiciones
//
//  Rediseñado para seguir el mismo lenguaje visual que el resto de la app
//  (OClock, playlists): tarjetas dibujadas a mano con InvisibleButton +
//  AddRectFilled/AddText, en vez de ImGui::BeginTable + CollapsingHeader.
//  Ademas de la consistencia visual, esto evita el bug que tenia el
//  acordeon (SetNextItemOpen(false, ...) cerraba la categoria activa en
//  vez de abrirla) simplemente porque ya no hay nada que abrir/cerrar: se
//  eligio una categoria por pestañas y sus tarjetas quedan siempre visibles.
// =============================================================================
namespace {
    constexpr float kGapTight  = 6.0f;
    constexpr float kGapNormal = 8.0f;
    constexpr float kCardH     = 56.0f;
    constexpr float kTabH      = 34.0f;

    const ImU32 kAccent      = IM_COL32(94, 107, 255, 255);   // 0.369,0.420,1.0
    const ImU32 kAccentLight = IM_COL32(133, 145, 255, 255);
    const ImU32 kTextDim     = IM_COL32(166, 173, 199, 255);
    const ImU32 kTextFaint   = IM_COL32(115, 122, 153, 255);

    ImU32 WithAlpha(ImU32 col, int a) {
        return (col & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(a, 0, 255)) << 24);
    }
}

struct TypeOption { TransitionType type; const char* label; const char* desc; };
struct TransitionCategory { const char* title; std::vector<TypeOption> options; };

void TransitionPanel::RenderContent()
{
    const auto& str = ProyecThor::UI::GetUIStrings();

    // ── Titulo ────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kAccentLight));
    ImGui::TextUnformatted(str.transTitle);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    static const TransitionCategory categories[] = {
        { "Básicas & Teatro", {
            { TransitionType::Iris,    "🎭 Teatro (Iris)", "Círculo expansivo desde el centro estilo teatro/cine." },
            { TransitionType::Fade,    "Disolver",        "Sale por completo, luego entra el nuevo." },
            { TransitionType::ZoomIn,  "Zoom In",         "El texto/fondo aparece desde el fondo." },
            { TransitionType::ZoomOut, "Zoom Out",        "El texto/fondo aparece desde el frente." },
            { TransitionType::None,    "Sin transición",  "El contenido cambia instantáneamente." }
        }},
        { "Barridos", {
            { TransitionType::SlideLeft,  "← Barrido Izq", "El nuevo empuja al anterior hacia la izq." },
            { TransitionType::SlideRight, "Barrido Der →", "El nuevo empuja al anterior hacia la der." },
            { TransitionType::SlideUp,    "↑ Barrido Arr", "El nuevo empuja al anterior hacia arriba." },
            { TransitionType::SlideDown,  "↓ Barrido Aba", "El nuevo empuja al anterior hacia abajo." }
        }},
        { "Cubrir", {
            { TransitionType::CoverLeft,  "← Cubrir Izq",  "El nuevo entra sobre el actual hacia la izq." },
            { TransitionType::CoverRight, "Cubrir Der →",  "El nuevo entra sobre el actual hacia la der." },
            { TransitionType::CoverUp,    "↑ Cubrir Arr",  "El nuevo entra desde abajo cubriendo." },
            { TransitionType::CoverDown,  "↓ Cubrir Aba",  "El nuevo entra desde arriba cubriendo." }
        }},
        { "Descubrir", {
            { TransitionType::UncoverLeft,  "← Revelar Izq", "El actual sale revelando el nuevo hacia la izq." },
            { TransitionType::UncoverRight, "Revelar Der →", "El actual sale revelando el nuevo hacia la der." },
            { TransitionType::UncoverUp,    "↑ Revelar Arr", "El actual sale revelando el nuevo hacia arriba." },
            { TransitionType::UncoverDown,  "↓ Revelar Aba", "El actual sale revelando el nuevo hacia abajo." }
        }}
    };
    constexpr int kCategoryCount = sizeof(categories) / sizeof(categories[0]);

    // Categoria activa: se recuerda entre frames y arranca posicionada en la
    // categoria a la que pertenece m_SelectedType, para que al abrir el
    // panel el usuario vea de entrada donde esta su transicion actual.
    static int s_ActiveCategory = -1;
    if (s_ActiveCategory < 0) {
        for (int i = 0; i < kCategoryCount; ++i)
            for (const auto& opt : categories[i].options)
                if (opt.type == m_SelectedType) { s_ActiveCategory = i; break; }
        if (s_ActiveCategory < 0) s_ActiveCategory = 0;
    }

    // ── Pestañas de categoria ────────────────────────────────────────────
    {
        float w    = ImGui::GetContentRegionAvail().x;
        float gap  = kGapTight;
        float tabW = (w - gap * (kCategoryCount - 1)) / kCategoryCount;

        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2 rowStart = ImGui::GetCursorScreenPos();

        for (int i = 0; i < kCategoryCount; ++i)
        {
            bool active = (s_ActiveCategory == i);

            ImVec2 p0(rowStart.x + i * (tabW + gap), rowStart.y);
            ImVec2 p1(p0.x + tabW, p0.y + kTabH);

            ImU32 bg  = active ? WithAlpha(kAccent, 45) : IM_COL32(255, 255, 255, 10);
            ImU32 bdr = active ? WithAlpha(kAccent, 200) : WithAlpha(kTextFaint, 120);

            dl->AddRectFilled(p0, p1, bg, 8.0f);
            dl->AddRect(p0, p1, bdr, 8.0f, 0, active ? 1.5f : 1.0f);

            ImGui::SetCursorScreenPos(p0);
            ImGui::PushID(i);
            bool clicked = ImGui::InvisibleButton("##transCat", ImVec2(tabW, kTabH));
            ImGui::PopID();
            if (clicked) s_ActiveCategory = i;

            ImVec2 labelSz  = ImGui::CalcTextSize(categories[i].title);
            ImU32  labelCol = active ? kAccentLight : kTextDim;
            dl->AddText(ImVec2(p0.x + (tabW - labelSz.x) * 0.5f, p0.y + (kTabH - labelSz.y) * 0.5f),
                        labelCol, categories[i].title);
        }

        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + kTabH));
        ImGui::Dummy(ImVec2(w, kTabH));
    }

    ImGui::Spacing();

    // ── Tarjetas de la categoria activa (grilla de 2 columnas) ───────────
    {
        const auto& options = categories[s_ActiveCategory].options;
        float w     = ImGui::GetContentRegionAvail().x;
        float gap   = kGapNormal;
        float cardW = (w - gap) * 0.5f;

        ImDrawList* dl    = ImGui::GetWindowDrawList();
        ImVec2 gridStart  = ImGui::GetCursorScreenPos();
        int    rows       = (static_cast<int>(options.size()) + 1) / 2;

        for (size_t i = 0; i < options.size(); ++i)
        {
            const auto& opt = options[i];
            bool selected   = (m_SelectedType == opt.type);

            int col = static_cast<int>(i) % 2;
            int row = static_cast<int>(i) / 2;

            ImVec2 p0(gridStart.x + col * (cardW + gap), gridStart.y + row * (kCardH + gap));
            ImVec2 p1(p0.x + cardW, p0.y + kCardH);

            ImU32 bg  = selected ? WithAlpha(kAccent, 55) : IM_COL32(255, 255, 255, 10);
            ImU32 bdr = selected ? WithAlpha(kAccent, 220) : WithAlpha(kTextFaint, 110);

            dl->AddRectFilled(p0, p1, bg, 9.0f);
            dl->AddRect(p0, p1, bdr, 9.0f, 0, selected ? 1.6f : 1.0f);

            ImGui::SetCursorScreenPos(p0);
            ImGui::PushID(static_cast<int>(i));
            bool clicked = ImGui::InvisibleButton("##transOpt", ImVec2(cardW, kCardH));
            ImGui::PopID();
            if (clicked) m_SelectedType = opt.type;

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kAccentLight));
                ImGui::TextUnformatted(opt.label);
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
                ImGui::TextUnformatted(opt.desc);
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }

            ImU32 labelCol = selected ? kAccentLight : ImGui::GetColorU32(ImGuiCol_Text);
            dl->AddText(ImVec2(p0.x + 12.0f, p0.y + 10.0f), labelCol, opt.label);

            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.82f,
                        ImVec2(p0.x + 12.0f, p0.y + 30.0f),
                        WithAlpha(kTextFaint, 235), opt.desc, nullptr, cardW - 22.0f);

            // Marca de seleccion (punto lleno) arriba a la derecha de la tarjeta.
            if (selected)
                dl->AddCircleFilled(ImVec2(p1.x - 14.0f, p0.y + 14.0f), 4.0f, kAccentLight);
        }

        // Sin +gap al final: el gap va SOLO entre filas, no despues de la
        // última. Antes "rows * (kCardH + gap)" reservaba un gap extra de
        // mas (8px) que quedaba como hueco muerto entre la grilla y la
        // seccion de Duracion, sin ningun elemento que lo llenara.
        float gridH = rows * kCardH + std::max(0, rows - 1) * gap;
        ImGui::SetCursorScreenPos(ImVec2(gridStart.x, gridStart.y + gridH));
        ImGui::Dummy(ImVec2(w, gridH));
    }

    // ── Duración + barra de progreso ─────────────────────────────────────
    // Espaciado apretado a proposito: separador -> label -> slider -> barra,
    // sin Spacing() de sobra entre medio. Antes cada paso sumaba su propio
    // Spacing(), y encadenados se sentian como un salto grande en vez de una
    // seccion compacta.
    if (m_SelectedType != TransitionType::None)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
        ImGui::Text("Duración de la transición   %.2f s", m_Duration);
        ImGui::PopStyleColor();

        DS::ModernSlider("##dur", &m_Duration, 0.1f, 3.0f, -1.0f, kAccent);

        // Barra de progreso dibujada a mano, pegada directo al slider (sin
        // Spacing() intermedio) para que se lea como una sola unidad
        // "duración + su barra", no como dos bloques separados.
        {
            float barH = 6.0f;
            float w    = ImGui::GetContentRegionAvail().x;
            ImVec2 p0  = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 4.0f);
            ImVec2 p1(p0.x + w, p0.y + barH);
            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddRectFilled(p0, p1, WithAlpha(kTextFaint, 60), barH * 0.5f);

            float fillX = p0.x + w * m_Progress;
            if (m_Active && fillX > p0.x)
                dl->AddRectFilled(p0, ImVec2(fillX, p1.y), WithAlpha(kAccent, 230), barH * 0.5f);

            ImGui::Dummy(ImVec2(w, barH + 4.0f));

            if (m_Active) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextFaint));
                ImGui::Text("Ejecutando... %d%%", static_cast<int>(m_Progress * 100));
                ImGui::PopStyleColor();
            }
        }
    }

    // ── Alcance: a que capa afecta esta transicion ───────────────────────
    // El fondo (imagen/video) tiene su propio crossfade de alpha automatico
    // (ver BackgroundLayer) -- si "Afecta a Fondos" esta activo, ese
    // crossfade pasa a usar la duracion de arriba en vez de su default fijo
    // (0.2s). "Afecta a Letras" controla si el texto anima con el tipo/
    // duracion de arriba o cambia al instante (igual que elegir "Sin
    // transición").
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextDim));
        ImGui::TextUnformatted("Afecta a");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImGui::ColorConvertU32ToFloat4(kAccent));
        ImGui::Checkbox("Fondos", &m_AffectsBackground);
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::Checkbox("Letras", &m_AffectsLyrics);
        ImGui::PopStyleColor();
    }

    // ── Pie de página / Ayuda ────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(kTextFaint));
    ImGui::TextWrapped("%s", str.transTip);
    ImGui::PopStyleColor();
}

} // namespace ProyecThor::UI