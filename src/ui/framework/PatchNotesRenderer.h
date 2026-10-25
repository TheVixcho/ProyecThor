#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include "stb_image.h"
#include "PatchNotesData.h"
#include "Version.h"

namespace ProyecThor::UI {

struct PatchNotesStyleConfig {
    ImVec4 windowBg;
    ImVec4 border;
    ImVec4 accent;
    ImVec4 accentLight;
    ImVec4 textPrimary;
    ImVec4 textSecondary;
    ImVec4 cardBg;
    ImVec4 cardBorder;
    float  scale = 1.0f;
};

struct PatchNotesViewState {
    int  selectedVersionId = -2; // -2: auto-default al mas reciente, -1: todo el historial
    char searchFilter[64] = "";
};

struct PatchNotesCoverTexEntry {
    GLuint tex       = 0;
    int    width     = 0;
    int    height    = 0;
    bool   attempted = false;
};

inline GLuint GetPatchNotesCoverTexture(const char* coverFile, int& outW, int& outH)
{
    outW = 0;
    outH = 0;
    if (!coverFile || !coverFile[0]) return 0;

    GLFWwindow* ctx = glfwGetCurrentContext();
    if (!ctx) return 0;

    static std::unordered_map<std::string, PatchNotesCoverTexEntry> s_CoverCache;
    const std::string key = std::to_string(reinterpret_cast<uintptr_t>(ctx)) + "|" + coverFile;

    auto it = s_CoverCache.find(key);
    if (it != s_CoverCache.end()) {
        outW = it->second.width;
        outH = it->second.height;
        return it->second.tex;
    }

    PatchNotesCoverTexEntry entry;
    entry.attempted = true;

    const std::string raw(coverFile);
    const std::string candidates[4] = {
        raw,
        "assets/" + raw,
        "../assets/" + raw,
        "bin/" + raw
    };

    unsigned char* pixels = nullptr;
    int w = 0, h = 0, ch = 0;
    for (const auto& cand : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(cand, ec)) {
            pixels = stbi_load(cand.c_str(), &w, &h, &ch, 4);
            if (pixels) break;
        }
    }

    if (pixels && w > 0 && h > 0) {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);

        entry.tex    = tex;
        entry.width  = w;
        entry.height = h;
    } else if (pixels) {
        stbi_image_free(pixels);
    }

    s_CoverCache[key] = entry;
    outW = entry.width;
    outH = entry.height;
    return entry.tex;
}

inline void RenderFormattedPatchNotesSummary(const char* summary, int versionId, const PatchNotesStyleConfig& cfg)
{
    if (!summary || !summary[0]) return;

    std::string text(summary);
    size_t start = 0;
    int itemIdx = 0;

    while (start < text.size()) {
        size_t end = text.find('\n', start);
        std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
        start = (end == std::string::npos) ? text.size() : end + 1;

        // Trim inicial
        size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            continue;
        }
        std::string trimmed = line.substr(first);
        // Trim final
        size_t last = trimmed.find_last_not_of(" \t\r");
        if (last != std::string::npos) {
            trimmed = trimmed.substr(0, last + 1);
        }

        // 1. Encabezado de seccion: "### " o "## "
        if (trimmed.rfind("### ", 0) == 0 || trimmed.rfind("## ", 0) == 0) {
            size_t prefixLen = (trimmed[2] == '#') ? 4 : 3;
            std::string heading = trimmed.substr(prefixLen);

            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextColored(cfg.accentLight, "%s", heading.c_str());
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            continue;
        }

        // 2. Viñeta con etiqueta entre corchetes: e.g. "• [NUEVO] ...", "* [MEJORA] ..."
        std::string tag = "";
        std::string remainder = "";

        size_t tagOpen = std::string::npos;
        if ((trimmed.rfind("• [", 0) == 0) || (trimmed.rfind("* [", 0) == 0) || (trimmed.rfind("- [", 0) == 0)) {
            tagOpen = trimmed.find('[');
        } else if (trimmed.rfind("[", 0) == 0) {
            tagOpen = 0;
        }

        if (tagOpen != std::string::npos) {
            size_t tagClose = trimmed.find(']', tagOpen);
            if (tagClose != std::string::npos && tagClose > tagOpen + 1) {
                tag = trimmed.substr(tagOpen + 1, tagClose - tagOpen - 1);
                remainder = trimmed.substr(tagClose + 1);
                size_t remFirst = remainder.find_first_not_of(" :-\t");
                if (remFirst != std::string::npos) {
                    remainder = remainder.substr(remFirst);
                }
            }
        }

        if (!tag.empty()) {
            std::string itemTitle;
            std::string itemDesc;
            size_t colonPos = remainder.find(':');
            if (colonPos != std::string::npos && colonPos < 80) {
                itemTitle = remainder.substr(0, colonPos);
                size_t descFirst = remainder.find_first_not_of(" \t", colonPos + 1);
                if (descFirst != std::string::npos) {
                    itemDesc = remainder.substr(descFirst);
                }
            } else {
                itemTitle = remainder;
            }

            ImVec4 badgeCol = cfg.accentLight;
            std::string tagUpper = tag;
            for (char& c : tagUpper) c = (char)::toupper((unsigned char)c);

            if (tagUpper.find("NUEV") != std::string::npos) {
                badgeCol = ImVec4(0.25f, 0.90f, 0.45f, 1.0f);
            } else if (tagUpper.find("MEJOR") != std::string::npos || tagUpper.find("INTERF") != std::string::npos) {
                badgeCol = ImVec4(0.35f, 0.75f, 1.00f, 1.0f);
            } else if (tagUpper.find("CORREC") != std::string::npos || tagUpper.find("FIX") != std::string::npos || tagUpper.find("ARREGL") != std::string::npos) {
                badgeCol = ImVec4(1.00f, 0.70f, 0.25f, 1.0f);
            } else if (tagUpper.find("REND") != std::string::npos || tagUpper.find("OPT") != std::string::npos) {
                badgeCol = ImVec4(0.85f, 0.55f, 1.00f, 1.0f);
            }

            // Renderizar tarjeta del cambio ocupando todo el ancho
            ImGui::PushStyleColor(ImGuiCol_ChildBg, cfg.cardBg);
            ImGui::PushStyleColor(ImGuiCol_Border,  cfg.cardBorder);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));

            char cardId[48];
            snprintf(cardId, sizeof(cardId), "##c_%d_%d", versionId, itemIdx++);
            if (ImGui::BeginChild(cardId, ImVec2(-1.0f, 0.0f),
                    ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                // Etiqueta limpia en corchetes [TAG]
                ImGui::TextColored(badgeCol, "[%s]", tag.c_str());
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextColored(cfg.textPrimary, "%s", itemTitle.c_str());

                if (!itemDesc.empty()) {
                    ImGui::Dummy(ImVec2(0.0f, 2.0f));
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextColored(cfg.textSecondary, "%s", itemDesc.c_str());
                    ImGui::PopTextWrapPos();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            continue;
        }

        // 3. Viñeta regular
        if (trimmed.rfind("• ", 0) == 0 || trimmed.rfind("* ", 0) == 0 || trimmed.rfind("- ", 0) == 0) {
            std::string content = trimmed.substr(2);
            ImGui::TextColored(cfg.accent, "*");
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(cfg.textPrimary, "%s", content.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            continue;
        }

        // 4. Parrafo de texto normal
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(cfg.textSecondary, "%s", trimmed.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }
}

inline void RenderPatchNotesViewContent(PatchNotesViewState& state, const PatchNotesStyleConfig& cfg, float footerHeight)
{
    const auto& patchNotes = GetPatchNotesRegistry();
    if (patchNotes.empty()) return;

    if (state.selectedVersionId == -2) {
        state.selectedVersionId = patchNotes[0].id;
    }

    const float totalAvailW = ImGui::GetContentRegionAvail().x;
    const bool isCompact = (totalAvailW < 600.0f);

    auto RenderVersionHeroAndContent = [&](const UpdateVersionInfo& info) {
        ImGui::PushID(info.id);

        const char* badge = (info.modalBadge && info.modalBadge[0])
            ? info.modalBadge
            : (info.isBeta ? "ETAPA BETA" : "ESTABLE");
        const bool isCurrentVer = (std::string(info.version) == PROYECTHOR_VERSION_STRING);

        int coverW = 0, coverH = 0;
        GLuint coverTex = GetPatchNotesCoverTexture(info.coverFile, coverW, coverH);

        if (coverTex != 0 && coverW > 0 && coverH > 0) {
            const float bannerW = std::max(120.0f, ImGui::GetContentRegionAvail().x);
            const float bannerH = isCompact ? std::max(105.0f, 118.0f * cfg.scale) : 184.0f;
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 p1 = ImVec2(p0.x + bannerW, p0.y + bannerH);
            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Recorte centrado proporcional (object-fit: cover) para no deformar la foto de portada
            ImVec2 uv0(0.0f, 0.0f), uv1(1.0f, 1.0f);
            const float imgAspect = (float)coverW / (float)coverH;
            const float boxAspect = bannerW / bannerH;
            if (imgAspect > boxAspect) {
                float visW = boxAspect / imgAspect;
                uv0.x = (1.0f - visW) * 0.5f;
                uv1.x = uv0.x + visW;
            } else if (boxAspect > imgAspect) {
                float visH = imgAspect / boxAspect;
                // Leve sesgo superior-central (0.42f) para lucir fuegos artificiales / arte principal
                uv0.y = (1.0f - visH) * 0.42f;
                uv1.y = uv0.y + visH;
            }

            constexpr float kRounding = 8.0f;
            dl->AddImageRounded((ImTextureID)(intptr_t)coverTex, p0, p1, uv0, uv1, IM_COL32_WHITE, kRounding);

            // Viñeta degradada inferior para máxima legibilidad del título sobre la portada
            dl->PushClipRect(p0, p1, true);
            const float gradTopY = p0.y + bannerH * 0.28f;
            dl->AddRectFilledMultiColor(
                ImVec2(p0.x, gradTopY), p1,
                IM_COL32(8, 10, 16, 0),   IM_COL32(8, 10, 16, 0),
                IM_COL32(8, 10, 16, 232), IM_COL32(8, 10, 16, 232));

            // Píldora superior de categoría (ej. GRAN ACTUALIZACIÓN v1.1.0)
            const float padX = isCompact ? 12.0f : 16.0f;
            const float padY = isCompact ? 10.0f : 14.0f;

            ImVec2 badgeTxtSz = ImGui::CalcTextSize(badge);
            ImVec2 pillMin(p0.x + padX, p1.y - padY - ImGui::GetTextLineHeight() - badgeTxtSz.y - 14.0f);
            ImVec2 pillMax(pillMin.x + badgeTxtSz.x + 16.0f, pillMin.y + badgeTxtSz.y + 6.0f);
            ImU32 pillBgCol = info.isBeta
                ? IM_COL32(55, 60, 75, 215)
                : ImGui::ColorConvertFloat4ToU32(ImVec4(cfg.accent.x, cfg.accent.y, cfg.accent.z, 0.88f));
            dl->AddRectFilled(pillMin, pillMax, pillBgCol, (pillMax.y - pillMin.y) * 0.5f);
            dl->AddText(ImVec2(pillMin.x + 8.0f, pillMin.y + 3.0f), IM_COL32(255, 255, 255, 255), badge);

            // Título principal de versión abajo a la izquierda sobre la portada
            std::string verTitle = std::string("ProyecThor v") + info.version;
            if (std::string(info.version) == "1.1.0") {
                verTitle = "Nueva Gran Actualización — ProyecThor v1.1 (1.1.0)";
            }
            const float titleY = p1.y - padY - ImGui::GetTextLineHeight();
            dl->AddText(ImVec2(p0.x + padX + 1.0f, titleY + 1.0f), IM_COL32(0, 0, 0, 190), verTitle.c_str());
            dl->AddText(ImVec2(p0.x + padX, titleY), IM_COL32(255, 255, 255, 255), verTitle.c_str());

            if (isCurrentVer) {
                ImVec2 verSz = ImGui::CalcTextSize(verTitle.c_str());
                const char* curTag = "ACTUAL";
                ImVec2 curSz = ImGui::CalcTextSize(curTag);
                ImVec2 curMin(p0.x + padX + verSz.x + 10.0f, titleY - 2.0f);
                ImVec2 curMax(curMin.x + curSz.x + 12.0f, curMin.y + curSz.y + 4.0f);
                if (curMax.x < p1.x - 8.0f) {
                    dl->AddRectFilled(curMin, curMax, IM_COL32(16, 185, 129, 210), 4.0f);
                    dl->AddText(ImVec2(curMin.x + 6.0f, curMin.y + 2.0f), IM_COL32(255, 255, 255, 255), curTag);
                }
            }
            dl->PopClipRect();

            ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(
                info.isBeta ? ImVec4(0.35f, 0.35f, 0.40f, 0.55f) : cfg.accentLight);
            dl->AddRect(p0, p1, borderCol, kRounding, 0, 1.3f);

            ImGui::Dummy(ImVec2(bannerW, bannerH));
        } else {
            // Cabecera de respaldo si la versión histórica no tiene imagen de portada en disco
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(cfg.accent.x, cfg.accent.y, cfg.accent.z, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_Border,  info.isBeta ? ImVec4(0.35f, 0.35f, 0.40f, 0.50f) : cfg.accentLight);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));

            char heroId[48];
            snprintf(heroId, sizeof(heroId), "##hero_%d", info.id);
            if (ImGui::BeginChild(heroId, ImVec2(-1.0f, 0.0f),
                    ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                ImGui::TextColored(cfg.accentLight, "ProyecThor v%s", info.version);
                ImGui::SameLine(0.0f, 8.0f);
                ImGui::TextColored(info.isBeta ? ImVec4(0.65f, 0.65f, 0.70f, 1.0f) : cfg.accent, "[ %s ]", badge);

                if (isCurrentVer) {
                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "(Actual)");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // Cuerpo con formato
        RenderFormattedPatchNotesSummary(info.summary, info.id, cfg);

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::PopID();
    };

    if (isCompact) {
        // ==========================================
        // MODO COMPACTO: Pantallas pequeñas / Splash
        // ==========================================
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float comboW = (totalAvailW - gap) * 0.58f;
        const float searchW = totalAvailW - comboW - gap;

        // Selector desplegable de versiones
        const UpdateVersionInfo* currentSel = FindPatchNotesVersion(state.selectedVersionId);
        std::string currentLabel = (state.selectedVersionId == -1) ? "Todo el Historial" :
            (currentSel ? std::string("v") + currentSel->version + (currentSel->isBeta ? " (Beta)" : " (Estable)") : "Versión");

        ImGui::SetNextItemWidth(comboW);
        if (ImGui::BeginCombo("##VerComboCompact", currentLabel.c_str())) {
            if (ImGui::Selectable("Todo el Historial", state.selectedVersionId == -1)) {
                state.selectedVersionId = -1;
            }
            ImGui::Separator();
            for (const auto& info : patchNotes) {
                std::string itemLbl = std::string("v") + info.version + (info.isBeta ? " [Beta]" : " [Estable]");
                if (std::string(info.version) == PROYECTHOR_VERSION_STRING) {
                    itemLbl += " *";
                }
                if (ImGui::Selectable(itemLbl.c_str(), state.selectedVersionId == info.id)) {
                    state.selectedVersionId = info.id;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(searchW);
        ImGui::InputTextWithHint("##SearchCompact", "Buscar...", state.searchFilter, sizeof(state.searchFilter));

        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 3.0f));

        // Área de contenido 100% de ancho sin divisiones
        ImGui::BeginChild("##PatchNotesScrollCompact", ImVec2(-1.0f, -footerHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        std::string searchLower = state.searchFilter;
        for (char& c : searchLower) c = (char)::tolower((unsigned char)c);

        if (state.selectedVersionId == -1) {
            for (const auto& info : patchNotes) {
                if (!searchLower.empty()) {
                    std::string verLower = info.version;
                    for (char& c : verLower) c = (char)::tolower((unsigned char)c);
                    std::string sumLower = info.summary ? info.summary : "";
                    for (char& c : sumLower) c = (char)::tolower((unsigned char)c);
                    if (verLower.find(searchLower) == std::string::npos && sumLower.find(searchLower) == std::string::npos)
                        continue;
                }
                RenderVersionHeroAndContent(info);
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            }
        } else {
            const UpdateVersionInfo* sel = FindPatchNotesVersion(state.selectedVersionId);
            if (!sel) sel = &patchNotes[0];
            if (sel) {
                RenderVersionHeroAndContent(*sel);
            }
        }

        ImGui::EndChild();
    }
    else {
        // ==========================================
        // MODO AMPLIO: Pantallas de escritorio
        // ==========================================
        const float gap = 8.0f;
        const float sidebarW = std::clamp(totalAvailW * 0.28f, 170.0f, 230.0f);

        // Sidebar izquierda
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.14f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_Border,  cfg.cardBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));

        ImGui::BeginChild("##PatchNotesSidebarWide", ImVec2(sidebarW, -footerHeight), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##NotesSearchWide", "Buscar...", state.searchFilter, sizeof(state.searchFilter));

        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 3.0f));

        bool allSelected = (state.selectedVersionId == -1);
        if (allSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cfg.accent.x, cfg.accent.y, cfg.accent.z, 0.35f));
            ImGui::PushStyleColor(ImGuiCol_Border, cfg.accentLight);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.24f, 0.30f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.30f, 0.36f, 0.30f));
        }
        if (ImGui::Button("Todo el Historial", ImVec2(-1.0f, 24.0f))) {
            state.selectedVersionId = -1;
        }
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 3.0f));

        ImGui::BeginChild("##VersionListScrollWide", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        std::string searchLower = state.searchFilter;
        for (char& c : searchLower) c = (char)::tolower((unsigned char)c);

        for (size_t i = 0; i < patchNotes.size(); ++i) {
            const auto& info = patchNotes[i];

            if (!searchLower.empty()) {
                std::string verLower = info.version;
                for (char& c : verLower) c = (char)::tolower((unsigned char)c);
                std::string sumLower = info.summary ? info.summary : "";
                for (char& c : sumLower) c = (char)::tolower((unsigned char)c);
                if (verLower.find(searchLower) == std::string::npos && sumLower.find(searchLower) == std::string::npos)
                    continue;
            }

            bool isSelected = (state.selectedVersionId == info.id);
            ImGui::PushID(info.id);

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cfg.accent.x, cfg.accent.y, cfg.accent.z, 0.30f));
                ImGui::PushStyleColor(ImGuiCol_Border, cfg.accentLight);
                ImGui::PushStyleColor(ImGuiCol_Text,   cfg.textPrimary);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.24f, 0.25f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.30f, 0.36f, 0.30f));
                ImGui::PushStyleColor(ImGuiCol_Text,   cfg.textSecondary);
            }

            char vLabel[48];
            const char* badgeText = info.isBeta ? "Beta" : "Estable";
            snprintf(vLabel, sizeof(vLabel), "v%s [%s]##vbtn", info.version, badgeText);

            if (ImGui::Button(vLabel, ImVec2(-1.0f, 25.0f))) {
                state.selectedVersionId = info.id;
            }

            ImGui::PopStyleColor(3);
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        // Panel de contenido amplio
        ImGui::SameLine(0.0f, gap);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.17f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Border,  cfg.cardBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));

        ImGui::BeginChild("##PatchNotesContentWide", ImVec2(-1.0f, -footerHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (state.selectedVersionId == -1) {
            for (const auto& info : patchNotes) {
                RenderVersionHeroAndContent(info);
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.0f, 6.0f));
            }
        } else {
            const UpdateVersionInfo* sel = FindPatchNotesVersion(state.selectedVersionId);
            if (!sel) sel = &patchNotes[0];
            if (sel) {
                RenderVersionHeroAndContent(*sel);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
}

} // namespace ProyecThor::UI
