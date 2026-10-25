#include "WikiHelp.h"
#include "DesignSystem.h"
#include "settings/SettingsManager.h"

#include <imgui.h>
#include <string>

namespace ProyecThor::UI::Wiki {

namespace {

struct Entry {
    const char* title;
    const char* body;
};

static ImVec4 ToVec4(ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); }

// [Topic][idioma: 0 ES, 1 EN, 2 PT] -- mismo índice que GetUIStrings().
const Entry kEntries[][3] = {
    // Topic::ShadersRender
    {
        { "Efectos de Render",
          "Estos filtros se aplican en tiempo real sobre la salida en vivo "
          "(Audiencia), no sobre el Preview ni sobre los archivos originales.\n\n"
          "FSR y NIS escalan y afilan video de baja resolución (son excluyentes "
          "entre sí: activar uno apaga el otro). El resto son efectos visuales "
          "(CRT, grano, saturación, viñetado, blur, nitidez, bloom, aberración "
          "cromática, VHS, cine, contraste, luminosidad, FXAA/TAA) que se pueden "
          "combinar libremente.\n\n"
          "Los cambios se ven al instante en la pantalla del público; nada de "
          "esto se guarda en el archivo del proyecto, son ajustes de la sesión "
          "actual." },
        { "Render Effects",
          "These filters apply in real time to the live output (Audience), not "
          "to the Preview or the original files.\n\n"
          "FSR and NIS upscale and sharpen low-resolution video (mutually "
          "exclusive: enabling one turns off the other). The rest are visual "
          "effects (CRT, grain, saturation, vignette, blur, sharpen, bloom, "
          "chromatic aberration, VHS, cine, contrast, luminosity, FXAA/TAA) that "
          "can be combined freely.\n\n"
          "Changes apply instantly on the audience screen; none of this is "
          "saved to the project file, these are current-session settings." },
        { "Efeitos de Render",
          "Estes filtros aplicam-se em tempo real sobre a saída ao vivo "
          "(Audiência), não sobre o Preview nem sobre os ficheiros originais.\n\n"
          "FSR e NIS aumentam a escala e nitidez de vídeo de baixa resolução "
          "(são exclusivos entre si: ativar um desliga o outro). Os restantes "
          "são efeitos visuais (CRT, grão, saturação, vinheta, blur, nitidez, "
          "bloom, aberração cromática, VHS, cine, contraste, luminosidade, "
          "FXAA/TAA) que podem ser combinados livremente.\n\n"
          "As alterações aplicam-se de imediato no ecrã do público; nada disto "
          "é guardado no ficheiro do projeto, são definições da sessão atual." },
    },
    // Topic::OClock
    {
        { "Contadores",
          "Cronómetro / cuenta regresiva que se puede mostrar al público "
          "agregando un cuadro de tipo Reloj en el overlay activo (Diseño > "
          "Overlays); este panel solo controla el número, no decide por sí "
          "mismo si se ve en pantalla.\n\n"
          "Modo Cronómetro: cuenta hacia un objetivo en minutos/segundos (o en "
          "sentido ascendente libre); al llegar a cero entra en \"overtime\" y "
          "sigue sumando con signo.\n"
          "Modo Reloj de pared: muestra la hora actual del equipo, en formato "
          "12/24h.\n\n"
          "\"Transmitir a Red (LAN)\" es un interruptor aparte: envía el número "
          "a los dispositivos (celulares/tablets) conectados, independiente de "
          "si se ve en el proyector." },
        { "Clock & Timers",
          "Timer / countdown that can be shown to the audience by adding a "
          "Clock box to the active overlay (Design > Overlays); this panel "
          "only controls the number, it doesn't decide on its own whether it's "
          "visible on screen.\n\n"
          "Timer mode: counts toward a target in minutes/seconds (or counts up "
          "freely); once it reaches zero it enters \"overtime\" and keeps "
          "adding with a sign.\n"
          "Wall clock mode: shows the device's current time, in 12/24h "
          "format.\n\n"
          "\"Broadcast to Network (LAN)\" is a separate switch: it sends the "
          "number to connected devices (phones/tablets), independent of "
          "whether it's visible on the projector." },
        { "Relógio e Contadores",
          "Cronómetro / contagem decrescente que pode ser mostrado ao público "
          "adicionando uma caixa de tipo Relógio ao overlay ativo (Design > "
          "Overlays); este painel apenas controla o número, não decide sozinho "
          "se aparece no ecrã.\n\n"
          "Modo Cronómetro: conta até um objetivo em minutos/segundos (ou em "
          "sentido ascendente livre); ao chegar a zero entra em \"overtime\" e "
          "continua a somar com sinal.\n"
          "Modo Relógio de parede: mostra a hora atual do equipamento, em "
          "formato 12/24h.\n\n"
          "\"Transmitir para Rede (LAN)\" é um interruptor à parte: envia o "
          "número para os dispositivos (telemóveis/tablets) ligados, "
          "independente de aparecer ou não no projetor." },
    },
};

int LangIndex() {
    int idx = static_cast<int>(
        ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language);
    return (idx < 0 || idx >= 3) ? 0 : idx;
}

} // namespace

void InfoButton(Topic topic) {
    const Entry& entry = kEntries[static_cast<int>(topic)][LangIndex()];
    std::string popupId = "##wikiHelp" + std::to_string(static_cast<int>(topic));

    ImGui::PushStyleColor(ImGuiCol_Text,          ToVec4(DS::TextHint));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 100.0f);
    bool clicked = ImGui::SmallButton("(i)");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", entry.title);

    if (clicked)
        ImGui::OpenPopup(popupId.c_str());

    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId.c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(DS::AccentLight));
        ImGui::TextUnformatted(entry.title);
        ImGui::PopStyleColor();
        DS::GlassSeparator();
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 340.0f);
        ImGui::TextUnformatted(entry.body);
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
}

} // namespace ProyecThor::UI::Wiki
