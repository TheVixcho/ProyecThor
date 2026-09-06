#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "frontend/ui/UIStrings.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ProyecThor::UI::Settings {

// Progreso animado (0..1) de hover/seleccion por-item -- copia local del
// mismo patron usado en CategoryTheme.cpp (ImGuiStorage + lerp con
// DeltaTime); cada Category*.cpp es su propia unidad de traduccion, asi que
// no se puede compartir el `static` de otro archivo sin promoverlo a un
// header propio, y esto es chico como para justificarlo.
static float AnimT(ImGuiID baseId, ImU32 salt, bool target, float speed = 12.0f) {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float* t = storage->GetFloatRef(baseId ^ salt, target ? 1.0f : 0.0f);
    float dst = target ? 1.0f : 0.0f;
    *t += (dst - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// Tarjeta de idioma: insignia con el código (ES/EN/PT) + nombre completo
// debajo, mismo lenguaje visual que PresetSwatch en CategoryTheme.cpp
// (anillo animado + check al seleccionar) para que "Idioma" se sienta parte
// de la misma pasada de modernización que "Temas".
static bool LanguageCard(const char* code, const char* label, bool selected,
                          ImVec4 accent, ImVec4 base, ImVec2 origin, float cardW, float cardH) {
    ImGui::PushID(code);
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("##langcard", ImVec2(cardW, cardH));
    bool clicked = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();

    ImGuiID id      = ImGui::GetID("##langcard");
    float   hoverT  = AnimT(id, 0xD4u, hovered, 14.0f);
    float   selectT = AnimT(id, 0xE5u, selected, 9.0f);

    float  lift = hoverT * 2.0f;
    ImVec2 p0(origin.x, origin.y - lift);
    ImVec2 p1(p0.x + cardW, p0.y + cardH);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 3; i >= 1; --i) {
        float t   = (float)i / 3.0f;
        float off = (4.0f + hoverT * 4.0f) * t;
        int   a   = (int)((22.0f + hoverT * 10.0f) * (1.0f - t * 0.5f));
        dl->AddRectFilled(ImVec2(p0.x - off * 0.3f, p0.y + off * 0.45f),
                          ImVec2(p1.x + off * 0.3f, p1.y + off * 0.8f),
                          IM_COL32(0, 0, 0, a), 12.0f + off * 0.2f);
    }

    ImVec4 baseHover(std::min(1.0f, base.x + 0.04f * hoverT),
                      std::min(1.0f, base.y + 0.04f * hoverT),
                      std::min(1.0f, base.z + 0.04f * hoverT), 1.0f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(baseHover), 12.0f);

    // Insignia con el código de idioma.
    ImVec2 badgeC(p0.x + 28.0f, p0.y + 27.0f);
    dl->AddCircleFilled(badgeC, 16.0f,
        ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, selected ? 0.90f : 0.55f)));
    ImVec2 codeSz = ImGui::CalcTextSize(code);
    dl->AddText(ImVec2(badgeC.x - codeSz.x * 0.5f, badgeC.y - codeSz.y * 0.5f), IM_COL32(255, 255, 255, 255), code);

    // Nombre completo del idioma, abajo a la izquierda.
    dl->AddText(ImVec2(p0.x + 16.0f, p1.y - ImGui::GetTextLineHeight() - 14.0f),
        ImGui::ColorConvertFloat4ToU32(selected ? ImVec4(1, 1, 1, 1) : ImVec4(0.80f, 0.82f, 0.87f, 1.0f)), label);

    // Anillo: gris tenue en reposo, se funde al acento con un pulso suave
    // mientras esta seleccionado.
    float  pulse = selected ? (0.85f + 0.15f * std::sin((float)ImGui::GetTime() * 2.4f)) : 1.0f;
    ImVec4 idle(1.0f, 1.0f, 1.0f, 0.14f);
    ImVec4 ring(
        idle.x + (accent.x - idle.x) * selectT,
        idle.y + (accent.y - idle.y) * selectT,
        idle.z + (accent.z - idle.z) * selectT,
        (idle.w + (1.0f - idle.w) * selectT) * pulse);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(ring), 12.0f, 0, 1.0f + selectT * 1.6f);

    if (selectT > 0.02f) {
        ImVec2 c(p1.x - 16.0f, p0.y + 16.0f);
        ImU32  badgeCol = ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, selectT));
        ImU32  checkCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, selectT));
        dl->AddCircleFilled(c, 8.0f, badgeCol);
        dl->PathLineTo(ImVec2(c.x - 3.6f, c.y));
        dl->PathLineTo(ImVec2(c.x - 0.7f, c.y + 2.9f));
        dl->PathLineTo(ImVec2(c.x + 4.0f, c.y - 3.6f));
        dl->PathStroke(checkCol, false, 1.7f);
    }

    ImGui::PopID();
    return clicked;
}

void SettingsPanel::RenderCategoryLanguage() {
    auto&       g     = ProyecThor::Settings::SettingsManager::Get().GetSettings().general;
    const auto& theme = ProyecThor::Settings::SettingsManager::Get().GetSettings().theme;
    // Ya no hay un struct UIStrings duplicado solo para esta pagina --
    // pedido explicito de "renovar Lenguaje y UIStrings": esto reusa el
    // MISMO catalogo real (frontend/ui/UIStrings.h) que ya usan Hub,
    // LibraryPanel, etc., en vez de un mini catalogo de 13 cadenas que
    // vivia clonado y desactualizado adentro de este archivo.
    const auto& str = ProyecThor::UI::GetUIStrings();

    ImVec4 accent(theme.accent[0], theme.accent[1], theme.accent[2], 1.0f);
    ImVec4 base  (theme.surface2[0], theme.surface2[1], theme.surface2[2], 1.0f);
    ImVec4 textDim(theme.textDim[0], theme.textDim[1], theme.textDim[2], theme.textDim[3]);

    // Sin SectionTitle acá: esta página es una sola cosa (elegir idioma +
    // ver la vista previa), un subtítulo "Idioma de la interfaz" repitiendo
    // el nombre de la categoría ("Idioma") de arriba era pura redundancia.
    // Tampoco hay más aviso de "reiniciar para aplicar" -- GetUIStrings()
    // relee el idioma activo en cada llamada, así que cualquier texto que sí
    // esté enchufado al catálogo (como esta misma página) cambia al toque,
    // sin reinicio.
    static const struct { const char* code; ProyecThor::Settings::Language lang; } kLangs[] = {
        { "ES", ProyecThor::Settings::Language::Spanish },
        { "EN", ProyecThor::Settings::Language::English },
        { "PT", ProyecThor::Settings::Language::Portuguese },
    };
    const int   langCount = (int)(sizeof(kLangs) / sizeof(kLangs[0]));
    const float cardW = 152.0f, cardH = 78.0f, gap = 14.0f;
    const ImVec2 langOrigin = ImGui::GetCursorScreenPos();

    for (int i = 0; i < langCount; i++) {
        bool   selected = (g.language == kLangs[i].lang);
        ImVec2 cardOrigin(langOrigin.x + i * (cardW + gap), langOrigin.y);
        if (LanguageCard(kLangs[i].code, ProyecThor::Settings::LanguageName(kLangs[i].lang),
                         selected, accent, base, cardOrigin, cardW, cardH))
            g.language = kLangs[i].lang;
    }
    ImGui::SetCursorScreenPos(ImVec2(langOrigin.x, langOrigin.y + cardH + gap + 22.0f));

    // Vista previa de cadenas: prueba en vivo de que el catálogo real (no
    // una copia) está detrás de esto -- cambiar de tarjeta arriba actualiza
    // esta lista en el mismo frame. Caption chico en vez de otro
    // SectionTitle completo, mismo criterio que ColorChipGroup en
    // CategoryTheme.cpp.
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.55f, 0.62f, 1.0f));
        ImGui::SetWindowFontScale(0.86f);
        ImGui::TextUnformatted("VISTA PREVIA EN VIVO");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }

    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      p0 = ImGui::GetCursorScreenPos();
        float       w  = ImGui::GetContentRegionAvail().x;

        struct Row { const char* key; const char* val; };
        const Row rows[] = {
            { "library",   str.library },
            { "search",    str.search },
            { "books",     str.bibleBooks },
            { "chapters",  str.bibleChapters },
            { "save",      str.save },
            { "reset",     str.reset },
            { "start",     str.hubStartProjecting },
            { "quickNotes", str.quickNotesTitle },
        };
        const int   rowCount = (int)(sizeof(rows) / sizeof(rows[0]));
        const float rowH  = 30.0f;
        const float boxH  = rowCount * rowH + 16.0f;
        const float keyW  = 130.0f;

        ImVec4 surf1(theme.surface1[0], theme.surface1[1], theme.surface1[2], theme.surface1[3]);
        dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + boxH), ImGui::ColorConvertFloat4ToU32(surf1), 10.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + 8.0f));
        for (int i = 0; i < rowCount; i++) {
            ImGui::SetCursorScreenPos(ImVec2(p0.x + 16.0f, p0.y + 8.0f + i * rowH));
            ImGui::PushStyleColor(ImGuiCol_Text, textDim);
            ImGui::TextUnformatted(rows[i].key);
            ImGui::PopStyleColor();

            ImGui::SetCursorScreenPos(ImVec2(p0.x + keyW, p0.y + 8.0f + i * rowH));
            ImGui::PushTextWrapPos(p0.x + w - 16.0f);
            ImGui::TextUnformatted(rows[i].val);
            ImGui::PopTextWrapPos();

            if (i + 1 < rowCount) {
                float lineY = p0.y + 8.0f + (i + 1) * rowH - 3.0f;
                dl->AddLine(ImVec2(p0.x + 16.0f, lineY), ImVec2(p0.x + w - 16.0f, lineY),
                    IM_COL32(255, 255, 255, 12), 1.0f);
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + boxH));
        ImGui::Dummy(ImVec2(w, 4.0f));
    }
}

} // namespace ProyecThor::UI::Settings
