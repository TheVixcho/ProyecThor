#include "SongView.h"
#include "backend/core/PresentationCore.h"
#include "UIStrings.h"
#include "LibrarySongs.h"
#include "LibrarySongMeta.h"
#include "SongBackgroundPicker.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include "frontend/ui/bin/StyleGeneralApp.h"
#include "frontend/ui/SongPlayStats.h"
#include "frontend/ui/DesignSystem.h"

namespace ProyecThor::UI {

SongView::SongView()
    : m_CurrentSongTitle("")
    , m_ActiveStanzaIndex(-1)
    , m_HasRecordedCurrentSongProjection(false)
{
}

void SongView::ReloadTempoMeta(const std::string& songFilename)
{
    ProyecThor::Library::SongMeta meta = ProyecThor::Library::GetSongMeta(songFilename);
    m_TempoBpm = meta.tempoBpm;
    m_VerseDurationOverrideMs = meta.verseDurationOverrideMs;
    m_AutoAdvancePlaying = false;
}

float SongView::ComputeVerseDurationSeconds(const std::string& stanzaText, int stanzaIndex) const
{
    if (stanzaIndex >= 0 && stanzaIndex < (int)m_VerseDurationOverrideMs.size() &&
        m_VerseDurationOverrideMs[stanzaIndex] > 0)
        return std::max(0.3f, m_VerseDurationOverrideMs[stanzaIndex] / 1000.0f);

    int ms = ProyecThor::Library::CalcVerseDurationMs(stanzaText, m_TempoBpm);
    return std::max(0.3f, ms / 1000.0f);
}

void SongView::RenderSettingsCard(const std::string& songFilename, ImVec2 p_min, ImVec2 p_max, bool isHovered)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cardSize = { p_max.x - p_min.x, p_max.y - p_min.y };
    float barH = std::clamp(cardSize.y * 0.20f, 16.0f, 26.0f);

    dl->AddRectFilled(p_min, p_max, IM_COL32(54, 48, 30, 255), 10.0f);
    if (isHovered)
        dl->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 14), 10.0f);
    dl->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 24), 10.0f, 0, 1.0f);

    std::string style = ProyecThor::Library::GetSongStyle(songFilename);
    ProyecThor::Library::SongBackground bg = ProyecThor::Library::GetSongBackground(songFilename);

    ImVec2 contentMax = { p_max.x, p_max.y - barH };
    dl->PushClipRect(p_min, contentMax, true);

    const char* title = "Ajustes";
    ImVec2 titleSz = ImGui::CalcTextSize(title);
    float  midY    = p_min.y + (cardSize.y - barH) * 0.5f;

    std::string styleLine = style.empty() ? "Estilo: (ninguno)" : ("Estilo: " + style);
    std::string bgLine    = bg.path.empty() ? "Fondo: (ninguno)" : ("Fondo: " + std::filesystem::path(bg.path).filename().string());
    ImVec2 s1 = ImGui::CalcTextSize(styleLine.c_str());
    ImVec2 s2 = ImGui::CalcTextSize(bgLine.c_str());

    float blockH = titleSz.y + 6.0f + s1.y + 2.0f + s2.y;
    float y0 = midY - blockH * 0.5f;

    dl->AddText({ p_min.x + (cardSize.x - titleSz.x) * 0.5f, y0 }, IM_COL32(232, 226, 198, 255), title);
    dl->AddText({ p_min.x + (cardSize.x - s1.x) * 0.5f, y0 + titleSz.y + 6.0f }, IM_COL32(200, 195, 170, 190), styleLine.c_str());
    dl->AddText({ p_min.x + (cardSize.x - s2.x) * 0.5f, y0 + titleSz.y + 6.0f + s1.y + 2.0f }, IM_COL32(200, 195, 170, 190), bgLine.c_str());

    dl->PopClipRect();

    ImVec2 barMin = { p_min.x, p_max.y - barH };
    dl->AddRectFilled(barMin, p_max, IM_COL32(168, 148, 44, 255), 10.0f, ImDrawFlags_RoundCornersBottom);
    const char* barLabel = "Inicio";
    ImVec2 barLabelSz = ImGui::CalcTextSize(barLabel);
    dl->AddText({ p_min.x + 8.0f, barMin.y + (barH - barLabelSz.y) * 0.5f }, IM_COL32(32, 27, 10, 255), barLabel);
}

void SongView::RenderSongSettingsPopup(const std::string& songFilename)
{
    if (m_OpenSongSettingsRequest) {
        ImGui::OpenPopup("songSettingsPopup");
        m_OpenSongSettingsRequest = false;
    }

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.07f, 0.07f, 0.08f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DS::RadiusLarge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

    if (ImGui::BeginPopup("songSettingsPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Preset de esta canción");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        std::string currentStyle = ProyecThor::Library::GetSongStyle(songFilename);
        std::vector<std::string> names = Core::PresentationCore::Get().GetSavedStyleNames();

        ImGui::TextUnformatted("Estilo:");
        ImGui::SetNextItemWidth(240.0f);
        const char* preview = currentStyle.empty() ? "(ninguno)" : currentStyle.c_str();
        if (ImGui::BeginCombo("##songStyleCombo", preview))
        {
            if (ImGui::Selectable("(ninguno)", currentStyle.empty()))
                ProyecThor::Library::SetSongStyle(songFilename, "");
            for (const auto& name : names)
            {
                bool sel = (name == currentStyle);
                if (ImGui::Selectable(name.c_str(), sel))
                    ProyecThor::Library::SetSongStyle(songFilename, name);
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ProyecThor::Library::SongBackground bg = ProyecThor::Library::GetSongBackground(songFilename);
        std::vector<SongBgEntry> bgEntries = ListSongBackgrounds();

        ImGui::TextUnformatted("Fondo:");
        ImGui::SetNextItemWidth(240.0f);
        std::string bgPreview = bg.path.empty() ? "(ninguno)" : std::filesystem::path(bg.path).filename().string();
        if (ImGui::BeginCombo("##songBgCombo", bgPreview.c_str()))
        {
            if (ImGui::Selectable("(ninguno)", bg.path.empty()))
                ProyecThor::Library::ClearSongBackground(songFilename);
            for (const auto& entry : bgEntries)
            {
                bool sel = (entry.fullPath == bg.path);
                if (ImGui::Selectable(entry.label.c_str(), sel))
                    ProyecThor::Library::SetSongBackground(songFilename, entry.fullPath, !entry.isImage);
            }
            if (bgEntries.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
                ImGui::TextWrapped("Sin fondos en la biblioteca (agregalos desde la pestaña Fondos).");
                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void SongView::RenderStanzaColorBar(const std::string& songFilename, int stanzaIndex, ImVec2 p_min, ImVec2 p_max, float barH, bool matchesSearch)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 barMin = { p_min.x, p_max.y - barH };

    unsigned int colU32 = ProyecThor::Library::GetStanzaColor(songFilename, stanzaIndex);
    ImU32 barCol = colU32 != 0u ? (ImU32)colU32 : (matchesSearch ? IM_COL32(180, 130, 30, 255) : IM_COL32(58, 60, 66, 255));

    dl->AddRectFilled(barMin, p_max, barCol, 10.0f, ImDrawFlags_RoundCornersBottom);

    char numBuf[32];
    if (matchesSearch)
        snprintf(numBuf, sizeof(numBuf), "%d  🔍 Coincidencia", stanzaIndex + 1);
    else
        snprintf(numBuf, sizeof(numBuf), "%d", stanzaIndex + 1);

    ImVec2 numSz = ImGui::CalcTextSize(numBuf);
    ImU32  numCol = colU32 != 0u ? IM_COL32(20, 20, 22, 235) : (matchesSearch ? IM_COL32(255, 235, 160, 255) : IM_COL32(200, 200, 205, 220));
    dl->AddText({ p_min.x + 8.0f, barMin.y + (barH - numSz.y) * 0.5f }, numCol, numBuf);

    float swatchSize = std::max(10.0f, barH * 0.55f);
    ImVec2 swMin = { p_max.x - swatchSize - 6.0f, barMin.y + (barH - swatchSize) * 0.5f };
    ImVec2 swMax = { swMin.x + swatchSize, swMin.y + swatchSize };

    ImGui::SetCursorScreenPos(swMin);
    ImGui::PushID(stanzaIndex);
    bool swClicked = ImGui::InvisibleButton("##colorSwatch", { swatchSize, swatchSize });
    ImGui::PopID();

    dl->AddRectFilled(swMin, swMax, colU32 != 0u ? barCol : IM_COL32(255, 255, 255, 55), 3.0f);
    dl->AddRect(swMin, swMax, IM_COL32(0, 0, 0, 130), 3.0f, 0, 1.0f);

    if (swClicked) {
        m_ColorPickerForStanza   = stanzaIndex;
        m_OpenColorPickerRequest = true;
    }
}

void SongView::Render()
{
    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.PeekSelection();

    if (selection.title.empty() || selection.type != Core::ItemType::Song)
        return;

    if (m_CurrentSongTitle != selection.title)
    {
        m_CurrentSongTitle  = selection.title;
        m_ActiveStanzaIndex = -1;
        m_HasRecordedCurrentSongProjection = false;
        ReloadTempoMeta(selection.title);
    }

    {
        std::string pendingOpenFile;
        if (core.ConsumeSongEditorOpenRequest(pendingOpenFile) && pendingOpenFile == selection.title)
        {
            m_EditView.Open(pendingOpenFile);
            m_ShowEditor = true;
        }
    }

    if (m_ShowEditor)
    {
        bool stillEditing = m_EditView.Render();

        const std::string& currentFilename = m_EditView.GetFilename();
        if (currentFilename != selection.title)
        {
            selection.title    = currentFilename;
            m_CurrentSongTitle  = currentFilename;
            core.SetSelection(selection);
        }

        if (!stillEditing)
        {
            m_ShowEditor = false;
            ReloadTempoMeta(selection.title);
        }
        return;
    }

    RenderBrowseGrid();
}

void SongView::RenderBrowseGrid()
{
    auto& core      = Core::PresentationCore::Get();
    auto  selection = core.PeekSelection();

    ImFont* previewFont = ImGui::GetFont();
    const ImU32 textColor = IM_COL32(235, 235, 238, 255);

    auto TryRecordProjection = [&](bool userInitiated) {
        if (!userInitiated) return;
        if (selection.title.empty() || selection.contentData.size() < 2) return;
        if (m_ActiveStanzaIndex < 0) return;
        if (m_HasRecordedCurrentSongProjection) return;
        ProyecThor::UI::RecordSongProjection(selection.title, (int)selection.contentData.size());
        m_HasRecordedCurrentSongProjection = true;
    };

    auto PushNextStanzaText = [&](int idx) {
        int nextIdx = idx + 1;
        core.SetNextText(nextIdx < (int)selection.contentData.size()
                          ? selection.contentData[nextIdx] : "");
    };

    auto GoToStanza = [&](int idx) {
        m_ActiveStanzaIndex = idx;
        core.SetLayer2_Text(selection.contentData[idx]);
        PushNextStanzaText(idx);
        if (core.IsProjecting())
            core.SetProjecting(true);
        TryRecordProjection(true);
        if (m_AutoAdvancePlaying)
            m_AutoAdvanceDeadline = ImGui::GetTime() + ComputeVerseDurationSeconds(selection.contentData[idx], idx);
    };

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !selection.contentData.empty())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_ActiveStanzaIndex < (int)selection.contentData.size() - 1)
            GoToStanza(m_ActiveStanzaIndex + 1);
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_ActiveStanzaIndex > 0)
            GoToStanza(m_ActiveStanzaIndex - 1);
    }

    if (m_AutoAdvancePlaying && !selection.contentData.empty())
    {
        if (ImGui::GetTime() >= m_AutoAdvanceDeadline)
        {
            if (m_ActiveStanzaIndex < (int)selection.contentData.size() - 1)
                GoToStanza(m_ActiveStanzaIndex + 1);
            else
                m_AutoAdvancePlaying = false;
        }
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tamano");
    ImGui::SameLine();
    DS::ModernSlider("##stanzaZoom", &m_StanzaCardZoom, 0.55f, 1.8f, 140.0f);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Tamano de las tarjetas");

    ImGui::SameLine();
    if (DS::GlassButton("Editar", { 90.f, DS::ButtonHeight }, DS::TextSecondary))
    {
        m_AutoAdvancePlaying = false;
        m_EditView.Open(selection.title);
        m_ShowEditor = true;
    }

    ImGui::SameLine(0.0f, 16.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tempo");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(56.0f);
    if (ImGui::InputInt("##tempoBpm", &m_TempoBpm, 0, 0))
    {
        m_TempoBpm = std::clamp(m_TempoBpm, 0, 400);
        ProyecThor::Library::SongMeta meta = ProyecThor::Library::GetSongMeta(selection.title);
        meta.tempoBpm = m_TempoBpm;
        ProyecThor::Library::SetSongMeta(selection.title, meta);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Tempo (BPM) de la canción. 0 = auto-avance desactivado.");

    ImGui::SameLine();
    bool canAutoAdvance = m_TempoBpm > 0 && !selection.contentData.empty();
    ImGui::BeginDisabled(!canAutoAdvance);
    const char* playLabel = m_AutoAdvancePlaying ? "Pausar" : "Reproducir";
    if (DS::GlassButton(playLabel, { 100.f, DS::ButtonHeight },
                        m_AutoAdvancePlaying ? DS::DangerColor : DS::SuccessColor))
    {
        if (m_AutoAdvancePlaying)
        {
            m_AutoAdvancePlaying = false;
        }
        else
        {
            if (m_ActiveStanzaIndex < 0)
                GoToStanza(0);
            m_AutoAdvancePlaying = true;
            m_AutoAdvanceDeadline = ImGui::GetTime() +
                ComputeVerseDurationSeconds(selection.contentData[m_ActiveStanzaIndex], m_ActiveStanzaIndex);
        }
    }
    if (!canAutoAdvance && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
        ImGui::SetTooltip("Configura un tempo (BPM) para poder reproducir automáticamente.");
    ImGui::EndDisabled();

    ImGui::SameLine();
    float titleMaxW = std::max(20.0f, ImGui::GetContentRegionAvail().x - 8.0f);
    std::string titleTrunc = selection.title;
    if (ImGui::CalcTextSize(titleTrunc.c_str()).x > titleMaxW) {
        while (!titleTrunc.empty() && ImGui::CalcTextSize((titleTrunc + "...").c_str()).x > titleMaxW)
            titleTrunc.pop_back();
        titleTrunc += "...";
    }
    float rightX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(titleTrunc.c_str()).x;
    if (rightX > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(rightX);
    ImGui::TextDisabled("%s", titleTrunc.c_str());

    ImGui::Spacing();

    float availWidth = ImGui::GetContentRegionAvail().x;
    float colWidth   = 250.0f * m_StanzaCardZoom;
    float cardHeight = 110.0f * m_StanzaCardZoom;
    int   columns    = std::max(1, static_cast<int>(availWidth / colWidth));

    auto splitLines = [](const std::string& stanza) {
        std::vector<std::string> lines;
        size_t sp = 0, ep = stanza.find('\n');
        while (true) {
            std::string ln = stanza.substr(sp, ep - sp);
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            lines.push_back(ln);
            if (ep == std::string::npos) break;
            sp = ep + 1;
            ep = stanza.find('\n', sp);
        }
        return lines;
    };

    auto measureLines = [&](const std::vector<std::string>& lines, float size, float& outW, float& outH) {
        outW = 0.0f;
        for (const auto& ln : lines) {
            if (ln.empty()) continue;
            ImVec2 sz = previewFont->CalcTextSizeA(size, FLT_MAX, 0.0f, ln.c_str());
            outW = std::max(outW, sz.x);
        }
        outH = lines.size() * size;
    };

    float barH = std::clamp(cardHeight * 0.20f, 16.0f, 26.0f);

    float uniformSize = 96.0f;
    {
        float cardW_est  = std::max(10.0f, availWidth / (float)columns - 4.0f);
        float pad_est    = std::clamp(std::min(cardW_est, cardHeight) * 0.10f, 6.0f, 18.0f);
        float safeW_est  = std::max(10.0f, cardW_est  - pad_est * 2.0f);
        float safeH_est  = std::max(10.0f, cardHeight - barH - pad_est * 2.0f);

        for (const auto& stanza : selection.contentData) {
            std::vector<std::string> lines = splitLines(stanza);
            float size = 10.0f;
            while (size < 96.0f) {
                float next = size + 1.0f, w, h;
                measureLines(lines, next, w, h);
                if (w > safeW_est || h > safeH_est) break;
                size = next;
            }
            uniformSize = std::min(uniformSize, size);
        }
    }

    if (ImGui::BeginTable("StanzasGrid", columns, ImGuiTableFlags_SizingStretchSame))
    {
        {
            ImGui::TableNextColumn();
            ImGui::PushID("settingsCard");

            ImVec2 p_min    = ImGui::GetCursorScreenPos();
            ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, cardHeight);
            ImVec2 p_max    = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

            if (ImGui::InvisibleButton("##settings_btn", cardSize))
                m_OpenSongSettingsRequest = true;
            bool settingsHovered = ImGui::IsItemHovered();

            RenderSettingsCard(selection.title, p_min, p_max, settingsHovered);

            ImGui::PopID();
        }

        std::string searchQuery = core.GetSongSearchQuery();
        std::string searchLo    = searchQuery;
        std::transform(searchLo.begin(), searchLo.end(), searchLo.begin(), [](unsigned char c){ return (char)::tolower(c); });

        for (size_t i = 0; i < selection.contentData.size(); ++i)
        {
            ImGui::TableNextColumn();

            const std::string& stanza     = selection.contentData[i];
            bool               isSelected = (m_ActiveStanzaIndex == static_cast<int>(i));

            bool stanzaMatchesSearch = false;
            if (!searchLo.empty()) {
                std::string stanzaLo = stanza;
                std::transform(stanzaLo.begin(), stanzaLo.end(), stanzaLo.begin(), [](unsigned char c){ return (char)::tolower(c); });
                stanzaMatchesSearch = (stanzaLo.find(searchLo) != std::string::npos);
            }

            ImGui::PushID((int)i);

            ImVec2 p_min    = ImGui::GetCursorScreenPos();
            ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, cardHeight);
            ImVec2 p_max    = ImVec2(p_min.x + cardSize.x, p_min.y + cardSize.y);

            if (ImGui::InvisibleButton("##select_btn", cardSize))
                GoToStanza((int)i);

            bool isHovered = ImGui::IsItemHovered();

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            {
                const float cell = 10.0f;
                const ImU32 cDark  = IM_COL32(20, 21, 25, 255);
                const ImU32 cLight = IM_COL32(36, 38, 44, 255);

                drawList->PushClipRect(p_min, p_max, true);
                drawList->AddRectFilled(p_min, p_max, cDark, 10.0f);

                int cols = (int)std::ceil(cardSize.x / cell);
                int rows = (int)std::ceil(cardSize.y / cell);
                for (int ry = 0; ry < rows; ry++) {
                    for (int rx = 0; rx < cols; rx++) {
                        if ((rx + ry) % 2 == 0) {
                            ImVec2 ca = { p_min.x + rx * cell, p_min.y + ry * cell };
                            ImVec2 cb = { std::min(p_max.x, ca.x + cell), std::min(p_max.y, ca.y + cell) };
                            drawList->AddRectFilled(ca, cb, cLight);
                        }
                    }
                }
                drawList->PopClipRect();
            }

            auto bgIt = StyleGeneralApp::Icons.find("song_card_bg");
            if (bgIt != StyleGeneralApp::Icons.end() && bgIt->second.textureID != nullptr)
            {
                ImU32 tint = isSelected ? IM_COL32(255,255,255,220) : IM_COL32(205,205,205,180);
                drawList->AddImageRounded(bgIt->second.textureID, p_min, p_max,
                                          ImVec2(0,0), ImVec2(1,1), tint, 10.0f);
            }

            if (isSelected)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(110, 130, 255, 60), 10.0f);
            else if (stanzaMatchesSearch)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(245, 180, 40, 35), 10.0f);
            else if (isHovered)
                drawList->AddRectFilled(p_min, p_max, IM_COL32(255, 255, 255, 16), 10.0f);

            ImU32 borderColor = isSelected
                ? IM_COL32(180, 182, 190, 200)
                : (stanzaMatchesSearch ? IM_COL32(245, 180, 50, 220) : IM_COL32(255, 255, 255, 22));
            float borderSize = isSelected ? 1.5f : (stanzaMatchesSearch ? 1.5f : 1.0f);
            drawList->AddRect(p_min, p_max, borderColor, 10.0f, 0, borderSize);

            ImVec2 textAreaMax = { p_max.x, p_max.y - barH };
            drawList->PushClipRect(p_min, textAreaMax, true);

float pad  = std::clamp(std::min(cardSize.x, cardSize.y) * 0.10f, 6.0f, 18.0f);
float padL = pad, padT = pad, padR = pad, padB = pad;
float textAreaH = cardSize.y - barH;

std::vector<std::string> lines = splitLines(stanza);
float displaySize = uniformSize;

float blockW, blockH;
measureLines(lines, displaySize, blockW, blockH);

float startY = std::max(padT, (textAreaH - blockH) * 0.5f);

float currentY = startY;
for (const auto& line : lines)
{
    if (!line.empty())
    {
        ImVec2 lineSz = previewFont->CalcTextSizeA(displaySize, FLT_MAX, 0.0f, line.c_str());
        float localX = std::max(padL, (cardSize.x - lineSz.x) * 0.5f);

        bool lineMatches = false;
        if (!searchLo.empty()) {
            std::string lineLo = line;
            std::transform(lineLo.begin(), lineLo.end(), lineLo.begin(), [](unsigned char c){ return (char)::tolower(c); });
            lineMatches = (lineLo.find(searchLo) != std::string::npos);
        }

        if (lineMatches) {
            ImVec2 hlMin(p_min.x + localX - 4.0f, p_min.y + currentY - 1.0f);
            ImVec2 hlMax(p_min.x + localX + lineSz.x + 4.0f, p_min.y + currentY + displaySize + 1.0f);
            drawList->AddRectFilled(hlMin, hlMax, IM_COL32(235, 175, 40, 55), 3.0f);
        }

        ImU32 lineCol = lineMatches ? IM_COL32(255, 235, 130, 255) : textColor;
        drawList->AddText(previewFont, displaySize,
                          ImVec2(p_min.x + localX, p_min.y + currentY),
                          lineCol, line.c_str());
    }
    currentY += displaySize;
}

drawList->PopClipRect();

RenderStanzaColorBar(selection.title, (int)i, p_min, p_max, barH, stanzaMatchesSearch);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    RenderSongSettingsPopup(selection.title);

    if (m_OpenColorPickerRequest) {
        ImGui::OpenPopup("stanzaColorPopup");
        m_OpenColorPickerRequest = false;
    }
    if (ImGui::BeginPopup("stanzaColorPopup"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::TextUnformatted("Color de la tarjeta");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        static const ImU32 kPalette[] = {
            IM_COL32(214, 84, 84, 255),
            IM_COL32(214, 140, 64, 255),
            IM_COL32(214, 190, 64, 255),
            IM_COL32(96, 190, 110, 255),
            IM_COL32(74, 160, 214, 255),
            IM_COL32(120, 110, 214, 255),
            IM_COL32(214, 90, 160, 255),
            IM_COL32(150, 150, 158, 255),
        };

        for (int p = 0; p < (int)(sizeof(kPalette) / sizeof(kPalette[0])); ++p)
        {
            if (p % 4 != 0) ImGui::SameLine();
            ImGui::PushID(p);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::ColorConvertU32ToFloat4(kPalette[p]));
            if (ImGui::Button("##swatch", { 28.f, 28.f }))
            {
                ProyecThor::Library::SetStanzaColor(selection.title, m_ColorPickerForStanza, kPalette[p]);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (DS::GlassButton("Quitar color", { 130.f, DS::ButtonHeight }, DS::TextSecondary))
        {
            ProyecThor::Library::SetStanzaColor(selection.title, m_ColorPickerForStanza, 0u);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}

