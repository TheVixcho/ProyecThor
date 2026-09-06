#include "LayersTransitionsTab.h"
#include "LayersTheme.h"
#include "../TransitionPanel.h"
#include "backend/settings/SettingsManager.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace ProyecThor::UI {

void LayersTransitionsTab::LoadPresetIntoLive(int idx) {
    if (!m_TransitionsRef) return;
    auto& presets = ProyecThor::Settings::SettingsManager::Get().GetSettings().transitions.presets;
    if (idx < 0 || idx >= (int)presets.size()) return;

    const auto& p = presets[idx];
    m_TransitionsRef->SetType(static_cast<TransitionType>(p.type));
    m_TransitionsRef->SetDuration(p.duration);
    m_TransitionsRef->SetAffectsBackground(p.affectsBackground);
    m_TransitionsRef->SetAffectsLyrics(p.affectsLyrics);
}

void LayersTransitionsTab::SaveLiveAsPreset(int idx) {
    if (!m_TransitionsRef) return;
    std::string name(m_EditNameBuf);
    if (name.empty()) return;

    ProyecThor::Settings::TransitionPresetSettings p;
    p.name              = name;
    p.type              = static_cast<int>(m_TransitionsRef->GetCurrentType());
    p.duration          = m_TransitionsRef->GetDuration();
    p.affectsBackground = m_TransitionsRef->AffectsBackground();
    p.affectsLyrics     = m_TransitionsRef->AffectsLyrics();

    auto& presets = ProyecThor::Settings::SettingsManager::Get().GetSettings().transitions.presets;
    if (idx >= 0 && idx < (int)presets.size())
        presets[idx] = p;
    else
        presets.push_back(p);

    ProyecThor::Settings::SettingsManager::Get().Save();
}

void LayersTransitionsTab::DeletePreset(int idx) {
    auto& presets = ProyecThor::Settings::SettingsManager::Get().GetSettings().transitions.presets;
    if (idx < 0 || idx >= (int)presets.size()) return;
    presets.erase(presets.begin() + idx);
    ProyecThor::Settings::SettingsManager::Get().Save();
}

void LayersTransitionsTab::RenderRow(int idx, float rowW) {
    auto& presets = ProyecThor::Settings::SettingsManager::Get().GetSettings().transitions.presets;
    if (idx < 0 || idx >= (int)presets.size()) return;
    const auto& p = presets[idx];

    ImGui::PushID(idx);
    const float rowH = 44.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, { pos.x + rowW, pos.y + rowH });
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = LPU32(ImVec4(
        LP::Surface1.x + (LP::Surface2.x - LP::Surface1.x) * t,
        LP::Surface1.y + (LP::Surface2.y - LP::Surface1.y) * t,
        LP::Surface1.z + (LP::Surface2.z - LP::Surface1.z) * t, 1.0f));
    dl->AddRectFilled(pos, { pos.x + rowW, pos.y + rowH }, bg, 6.0f);

    float px = pos.x + 14.0f;
    dl->AddText(ImGui::GetFont(), 15.0f, { px, pos.y + 6.0f }, LPU32(LP::Text), p.name.c_str());

    char sub[96];
    snprintf(sub, sizeof(sub), "%s - %.2fs",
             TransitionTypeToName(static_cast<TransitionType>(p.type)), p.duration);
    dl->AddText(ImGui::GetFont(), 11.0f, { px, pos.y + 24.0f }, LPU32(LP::TextMuted), sub);

    // Insignias de alcance, alineadas a la derecha.
    float badgeX = pos.x + rowW - 12.0f;
    float badgeY = pos.y + (rowH - 16.0f) * 0.5f;
    if (p.affectsLyrics) {
        ImVec2 sz = ImGui::CalcTextSize("Letras");
        badgeX -= (sz.x + 10.0f);
        LPBadge(dl, { badgeX, badgeY }, "Letras", LP::AccentDim, LP::Accent);
        badgeX -= 6.0f;
    }
    if (p.affectsBackground) {
        ImVec2 sz = ImGui::CalcTextSize("Fondos");
        badgeX -= (sz.x + 10.0f);
        LPBadge(dl, { badgeX, badgeY }, "Fondos", LP::GoldDim, LP::Gold);
    }

    ImGui::InvisibleButton("##trow", { rowW, rowH });
    if (ImGui::IsItemClicked())
        LoadPresetIntoLive(idx);

    if (ImGui::BeginPopupContextItem("TransCtx")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", p.name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            LoadPresetIntoLive(idx);
            std::strncpy(m_EditNameBuf, p.name.c_str(), sizeof(m_EditNameBuf) - 1);
            m_EditNameBuf[sizeof(m_EditNameBuf) - 1] = '\0';
            m_EditingIndex = idx;
            ImGui::OpenPopup("##transEditPopup");
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) DeletePreset(idx);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::Dummy({ 0.0f, 6.0f });
    ImGui::PopID();
}

void LayersTransitionsTab::RenderEditorPopup() {
    if (!m_TransitionsRef) return;

    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup("##transEditPopup")) return;

    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted(m_EditingIndex >= 0 ? "Editar transición" : "Nueva transición");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##transName", "Nombre de la transición", m_EditNameBuf, sizeof(m_EditNameBuf));
    ImGui::Spacing();

    m_TransitionsRef->RenderContent();

    ImGui::Spacing();
    bool nameEmpty = (m_EditNameBuf[0] == '\0');
    if (nameEmpty) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    if (ImGui::Button("Guardar", ImVec2(120.0f, 30.0f)) && !nameEmpty) {
        SaveLiveAsPreset(m_EditingIndex);
        ImGui::CloseCurrentPopup();
    }
    if (nameEmpty) ImGui::PopStyleVar();
    ImGui::SameLine();
    if (ImGui::Button("Cancelar", ImVec2(120.0f, 30.0f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void LayersTransitionsTab::RenderContent() {
    auto& presets = ProyecThor::Settings::SettingsManager::Get().GetSettings().transitions.presets;

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Transiciones");
    ImGui::PopStyleColor();

    const float btnSz = 26.0f;
    const float avail  = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - btnSz));
    if (LPCornerIconBtn("##newtrans", LPDrawPlus, "Nueva transición", { btnSz, btnSz }, true)) {
        m_EditingIndex = -1;
        std::memset(m_EditNameBuf, 0, sizeof(m_EditNameBuf));
        ImGui::OpenPopup("##transEditPopup");
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (presets.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::TextWrapped(
            "Crea tu primera transición con el botón + de arriba. Cada una define "
            "tipo, duración, y a qué afecta al proyectar (Fondos, Letras, o ambos).");
        ImGui::PopStyleColor();
    } else {
        float pW = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < (int)presets.size(); i++)
            RenderRow(i, pW);
    }

    RenderEditorPopup();
}

} // namespace ProyecThor::UI
