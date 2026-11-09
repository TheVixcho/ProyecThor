#include "MediaImportModal.h"

#include "DesignSystem.h"
#include "core/MediaImportManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace ProyecThor::UI {

namespace {

std::string FormatBytesCompact(uint64_t bytes) {
    char buf[48];
    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.0f KB", (double)bytes / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    return buf;
}

std::string ToLowerStr(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

void DrawCheckMarkVec(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    ImVec2 pts[3] = {
        { c.x - s * 0.36f, c.y + s * 0.02f },
        { c.x - s * 0.08f, c.y + s * 0.30f },
        { c.x + s * 0.38f, c.y - s * 0.28f }
    };
    dl->AddPolyline(pts, 3, col, ImDrawFlags_None, 1.9f);
}

void DrawFolderLinkIcon(ImDrawList* dl, ImVec2 c, float s, ImU32 col) {
    float hw = s * 0.44f;
    float hh = s * 0.34f;
    dl->AddRect({ c.x - hw, c.y - hh * 0.55f }, { c.x + hw, c.y + hh }, col, 2.5f, 0, 1.5f);
    dl->AddLine({ c.x - hw, c.y - hh * 0.55f }, { c.x - hw * 0.2f, c.y - hh }, col, 1.5f);
    dl->AddLine({ c.x - hw * 0.2f, c.y - hh }, { c.x + hw * 0.2f, c.y - hh * 0.55f }, col, 1.5f);
}

} // namespace

void RenderMediaImportModal() {
    auto& mgr = Core::MediaImportManager::Get();
    if (!mgr.HasPendingPrompt())
        return;

    static bool s_WasOpenLastFrame = false;
    static bool s_CopyToAppFolder  = false;
    static char s_FilterBuf[128]   = {};

    if (!s_WasOpenLastFrame) {
        s_CopyToAppFolder = false;
        s_FilterBuf[0]    = '\0';
        s_WasOpenLastFrame = true;
    }

    ImGui::SetNextFrameWantCaptureKeyboard(true);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        mgr.DismissPendingImport();
        s_WasOpenLastFrame = false;
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();

    // Backdrop oscuro translúcido
    ImGui::SetNextWindowPos(vp->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(vp->Size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.62f));
    ImGui::Begin("##MediaImportBackdrop", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::End();
    ImGui::PopStyleColor();

    const float modalW = std::clamp(vp->WorkSize.x * 0.64f, 640.0f, 820.0f);
    const float modalH = std::clamp(vp->WorkSize.y * 0.72f, 440.0f, 620.0f);
    const ImVec2 center = vp->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.98f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(DS::GlassFillTop));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImGui::ColorConvertU32ToFloat4(DS::GlassBorder));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(20.0f, 18.0f));

    bool openFlag = true;
    constexpr ImGuiWindowFlags kWinFlags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##MediaImportSelectionModal", &openFlag, kWinFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        auto& candidates = mgr.GetCandidates();
        const bool isAuto = mgr.IsAutoDetectedPrompt();

        // ── 1. Cabecera ──────────────────────────────────────────────────────
        {
            ImVec2 iconCenter(ImGui::GetCursorScreenPos().x + 14.0f,
                              ImGui::GetCursorScreenPos().y + 15.0f);
            dl->AddCircleFilled(iconCenter, 14.0f, IM_COL32(56, 189, 248, 38), 20);
            DrawFolderLinkIcon(dl, iconCenter, 15.0f, IM_COL32(56, 189, 248, 255));

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 36.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextPrimary));
            ImGui::SetWindowFontScale(1.14f);
            ImGui::TextUnformatted(isAuto
                ? "¿Deseas agregar estos nuevos archivos a ProyecThor?"
                : "Seleccionar archivos y destino en ProyecThor");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 36.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
            if (isAuto) {
                ImGui::TextWrapped(
                    "Se detectaron %zu archivo%s nuevo%s. Elige cuáles deseas vincular y si van a Multimedia o "
                    "Background (Fondos). Quedarán en su carpeta original (ej. Descargas) sin duplicarse.",
                    candidates.size(),
                    candidates.size() == 1 ? "" : "s",
                    candidates.size() == 1 ? "" : "s");
            } else {
                ImGui::TextWrapped(
                    "Elige qué archivos vincular y si los quieres en Multimedia o en Background (Fondos). "
                    "Se abrirán directamente desde su carpeta original evitando archivos duplicados.");
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── 2. Barra de selección masiva y filtro ────────────────────────────
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));

            if (ImGui::Button("Marcar todos", ImVec2(102.0f, 27.0f))) {
                for (auto& c : candidates) {
                    const bool dup = (c.destination == Core::MediaImportDest::Multimedia)
                        ? c.alreadyInMultimedia
                        : c.alreadyInBackground;
                    if (!dup) c.selected = true;
                }
            }
            ImGui::SameLine(0.0f, 6.0f);
            if (ImGui::Button("Desmarcar", ImVec2(88.0f, 27.0f))) {
                for (auto& c : candidates) c.selected = false;
            }

            ImGui::SameLine(0.0f, 14.0f);

            // Botones masivos de destino
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.08f, 0.32f, 0.46f, 0.72f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.11f, 0.44f, 0.62f, 0.92f));
            if (ImGui::Button("Todo a Multimedia", ImVec2(134.0f, 27.0f))) {
                for (auto& c : candidates) {
                    c.destination = Core::MediaImportDest::Multimedia;
                    if (c.alreadyInMultimedia) c.selected = false;
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0.0f, 6.0f);

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.42f, 0.29f, 0.06f, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.39f, 0.08f, 0.95f));
            if (ImGui::Button("Todo a Background", ImVec2(138.0f, 27.0f))) {
                for (auto& c : candidates) {
                    if (c.kind == Core::CandidateMediaKind::Audio) continue;
                    c.destination = Core::MediaImportDest::Background;
                    if (c.alreadyInBackground) c.selected = false;
                }
            }
            ImGui::PopStyleColor(2);

            if (candidates.size() >= 4) {
                ImGui::SameLine(0.0f, 10.0f);
                ImGui::SetNextItemWidth(std::max(110.0f, ImGui::GetContentRegionAvail().x));
                ImGui::InputTextWithHint("##importFilter", "Filtrar nombre...", s_FilterBuf, sizeof(s_FilterBuf));
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        ImGui::Spacing();

        // ── 3. Lista desplazable de archivos candidatos ──────────────────────
        const float footerReserveH = 82.0f;
        const std::string filterLow = ToLowerStr(s_FilterBuf);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.045f, 0.06f, 0.55f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::BeginChild("##CandidatesScroll", ImVec2(0.0f, -footerReserveH), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImDrawList* listDl = ImGui::GetWindowDrawList();
        const float rowH   = 44.0f;

        for (size_t i = 0; i < candidates.size(); ++i) {
            auto& c = candidates[i];
            if (!filterLow.empty() && ToLowerStr(c.filename).find(filterLow) == std::string::npos)
                continue;

            const bool isAudio = (c.kind == Core::CandidateMediaKind::Audio);
            if (isAudio) c.destination = Core::MediaImportDest::Multimedia;

            const bool dupInTarget = (c.destination == Core::MediaImportDest::Multimedia)
                ? c.alreadyInMultimedia
                : c.alreadyInBackground;
            if (dupInTarget)
                c.selected = false;

            ImGui::PushID(static_cast<int>(i));

            const float rowW  = ImGui::GetContentRegionAvail().x;
            const ImVec2 rMin = ImGui::GetCursorScreenPos();
            const ImVec2 rMax(rMin.x + rowW, rMin.y + rowH);

            const float segGroupW = 204.0f;
            const float leftClickW = std::max(80.0f, rowW - segGroupW - 14.0f);

            const bool leftClicked = ImGui::InvisibleButton("##candLeftArea", ImVec2(leftClickW, rowH));
            const bool leftHovered = ImGui::IsItemHovered();
            if (leftClicked && !dupInTarget) {
                c.selected = !c.selected;
            }

            // Fondo de la fila
            ImU32 rowFill = c.selected
                ? IM_COL32(56, 189, 248, 22)
                : (leftHovered ? IM_COL32(255, 255, 255, 11) : IM_COL32(255, 255, 255, 5));
            ImU32 rowBrd = dupInTarget
                ? IM_COL32(245, 158, 11, 95)
                : (c.selected ? IM_COL32(56, 189, 248, 90) : IM_COL32(255, 255, 255, 16));
            listDl->AddRectFilled(rMin, rMax, rowFill, 7.0f);
            listDl->AddRect(rMin, rMax, rowBrd, 7.0f, 0, 1.0f);

            // Checkbox visual
            const float cbSz = 18.0f;
            const ImVec2 cbMin(rMin.x + 10.0f, rMin.y + (rowH - cbSz) * 0.5f);
            const ImVec2 cbMax(cbMin.x + cbSz, cbMin.y + cbSz);
            if (dupInTarget) {
                listDl->AddRectFilled(cbMin, cbMax, IM_COL32(80, 65, 30, 90), 4.5f);
                listDl->AddRect(cbMin, cbMax, IM_COL32(245, 158, 11, 130), 4.5f, 0, 1.0f);
            } else if (c.selected) {
                listDl->AddRectFilled(cbMin, cbMax, IM_COL32(38, 158, 235, 235), 4.5f);
                DrawCheckMarkVec(listDl, ImVec2((cbMin.x + cbMax.x) * 0.5f, (cbMin.y + cbMax.y) * 0.5f),
                                 12.0f, IM_COL32(255, 255, 255, 255));
            } else {
                listDl->AddRectFilled(cbMin, cbMax, IM_COL32(18, 20, 26, 200), 4.5f);
                listDl->AddRect(cbMin, cbMax, IM_COL32(255, 255, 255, 55), 4.5f, 0, 1.0f);
            }

            // Badge de tipo (VID / IMG / AUD)
            const char* typeTag = (c.kind == Core::CandidateMediaKind::Video) ? "VID"
                                : (c.kind == Core::CandidateMediaKind::Image) ? "IMG" : "AUD";
            ImU32 tagBg = (c.kind == Core::CandidateMediaKind::Video) ? IM_COL32(14, 165, 233, 48)
                        : (c.kind == Core::CandidateMediaKind::Image) ? IM_COL32(16, 185, 129, 48)
                                                                      : IM_COL32(168, 85, 247, 48);
            ImU32 tagFg = (c.kind == Core::CandidateMediaKind::Video) ? IM_COL32(125, 211, 252, 255)
                        : (c.kind == Core::CandidateMediaKind::Image) ? IM_COL32(110, 231, 183, 255)
                                                                      : IM_COL32(216, 180, 254, 255);
            const ImVec2 tagSz = ImGui::CalcTextSize(typeTag);
            const float tagW   = tagSz.x + 12.0f;
            const float tagH   = 20.0f;
            const ImVec2 tagMin(cbMax.x + 10.0f, rMin.y + (rowH - tagH) * 0.5f);
            listDl->AddRectFilled(tagMin, ImVec2(tagMin.x + tagW, tagMin.y + tagH), tagBg, 4.5f);
            listDl->AddText(ImVec2(tagMin.x + 6.0f, tagMin.y + (tagH - tagSz.y) * 0.5f), tagFg, typeTag);

            // Nombre de archivo + carpeta de origen + estado de duplicado
            const float textX = tagMin.x + tagW + 10.0f;
            const float textClipMaxX = rMax.x - segGroupW - 12.0f;

            listDl->PushClipRect(ImVec2(textX, rMin.y), ImVec2(textClipMaxX, rMax.y), true);
            listDl->AddText(ImVec2(textX, rMin.y + 5.0f),
                            dupInTarget ? DS::TextSecondary : DS::TextPrimary,
                            c.filename.c_str());

            std::string subLine = c.parentFolderLabel + " · " + FormatBytesCompact(c.fileSizeBytes);
            ImU32 subColor = DS::TextSecondary;
            if (dupInTarget) {
                subLine += (c.destination == Core::MediaImportDest::Multimedia)
                    ? "  •  Ya existe en Multimedia (duplicado evitado)"
                    : "  •  Ya existe en Background (duplicado evitado)";
                subColor = IM_COL32(251, 191, 36, 235);
            }
            listDl->AddText(ImVec2(textX, rMin.y + 23.0f), subColor, subLine.c_str());
            listDl->PopClipRect();

            if (leftHovered) {
                ImGui::SetTooltip("Ruta original:\n%s", c.fullPath.c_str());
            }

            // Selector segmentado por archivo: [ Multimedia ] [ Background ]
            const float pillH = 28.0f;
            const float pillW = (segGroupW - 6.0f) * 0.5f;
            const float pillY = rMin.y + (rowH - pillH) * 0.5f;
            const float pillX = rMax.x - segGroupW - 6.0f;

            ImGui::SetCursorScreenPos(ImVec2(pillX, pillY));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

            // Botón Multimedia
            {
                const bool activeMm = (c.destination == Core::MediaImportDest::Multimedia);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    activeMm ? ImVec4(0.08f, 0.42f, 0.62f, 0.92f) : ImVec4(0.10f, 0.11f, 0.14f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    activeMm ? ImVec4(0.10f, 0.50f, 0.74f, 1.0f)  : ImVec4(0.16f, 0.18f, 0.22f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Border,
                    activeMm ? ImVec4(0.32f, 0.78f, 0.98f, 0.85f) : ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    activeMm ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));

                if (ImGui::Button("Multimedia", ImVec2(pillW, pillH))) {
                    c.destination = Core::MediaImportDest::Multimedia;
                    c.selected    = !c.alreadyInMultimedia;
                }
                if (ImGui::IsItemHovered() && c.alreadyInMultimedia)
                    ImGui::SetTooltip("Ya existe un archivo con este nombre en Multimedia");
                ImGui::PopStyleColor(4);
            }

            ImGui::SameLine(0.0f, 6.0f);

            // Botón Background
            {
                const bool activeBg = (c.destination == Core::MediaImportDest::Background);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    isAudio  ? ImVec4(0.08f, 0.08f, 0.10f, 0.40f)
                  : activeBg ? ImVec4(0.55f, 0.37f, 0.06f, 0.92f) : ImVec4(0.10f, 0.11f, 0.14f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    isAudio  ? ImVec4(0.08f, 0.08f, 0.10f, 0.40f)
                  : activeBg ? ImVec4(0.66f, 0.45f, 0.08f, 1.0f)  : ImVec4(0.16f, 0.18f, 0.22f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_Border,
                    activeBg ? ImVec4(0.98f, 0.75f, 0.15f, 0.88f) : ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                    isAudio  ? ImGui::ColorConvertU32ToFloat4(DS::TextHint)
                  : activeBg ? ImVec4(1.0f, 0.97f, 0.88f, 1.0f) : ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));

                if (ImGui::Button("Background", ImVec2(pillW, pillH)) && !isAudio) {
                    c.destination = Core::MediaImportDest::Background;
                    c.selected    = !c.alreadyInBackground;
                }
                if (ImGui::IsItemHovered()) {
                    if (isAudio)
                        ImGui::SetTooltip("Los archivos de audio solo aplican a Multimedia");
                    else if (c.alreadyInBackground)
                        ImGui::SetTooltip("Ya existe un fondo con este nombre en Background");
                    else
                        ImGui::SetTooltip("Usar como fondo silencioso en bucle (Canciones / Biblia)");
                }
                ImGui::PopStyleColor(4);
            }

            ImGui::PopStyleVar(2);

            ImGui::SetCursorScreenPos(ImVec2(rMin.x, rMax.y + 5.0f));
            ImGui::Dummy(ImVec2(1.0f, 0.0f));
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        // ── 4. Pie del modal (modo sin copia + botones Sí / No) ──────────────
        int selTotal = 0, selMm = 0, selBg = 0;
        for (const auto& c : candidates) {
            if (!c.selected) continue;
            ++selTotal;
            if (c.destination == Core::MediaImportDest::Multimedia) ++selMm;
            else ++selBg;
        }

        ImGui::Spacing();
        ImGui::Checkbox("Copiar archivo dentro de la carpeta del programa",
                        &s_CopyToAppFolder);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(DS::TextSecondary));
        ImGui::Text("Seleccionados: %d  (%d Multimedia · %d Background)", selTotal, selMm, selBg);
        ImGui::PopStyleColor();

        const float noBtnW  = 155.0f;
        const float yesBtnW = 225.0f;
        const float btnH    = 34.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - 20.0f - (noBtnW + 10.0f + yesBtnW));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

        // Botón "No"
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.24f, 0.10f, 0.11f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.13f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.98f, 0.82f, 0.82f, 1.0f));
        if (ImGui::Button("No, no importar", ImVec2(noBtnW, btnH))) {
            mgr.DismissPendingImport();
            s_WasOpenLastFrame = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 10.0f);

        // Botón "Sí"
        const bool canConfirm = (selTotal > 0);
        ImGui::PushStyleColor(ImGuiCol_Button,
            canConfirm ? ImVec4(0.09f, 0.48f, 0.72f, 0.95f) : ImVec4(0.14f, 0.16f, 0.20f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            canConfirm ? ImVec4(0.12f, 0.58f, 0.86f, 1.0f)  : ImVec4(0.14f, 0.16f, 0.20f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            canConfirm ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)     : ImGui::ColorConvertU32ToFloat4(DS::TextHint));

        char yesLabel[64];
        std::snprintf(yesLabel, sizeof(yesLabel), "Sí, agregar seleccionados (%d)", selTotal);
        if (ImGui::Button(yesLabel, ImVec2(yesBtnW, btnH)) && canConfirm) {
            mgr.CommitPendingImport(s_CopyToAppFolder);
            s_WasOpenLastFrame = false;
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    if (!openFlag) {
        mgr.DismissPendingImport();
        s_WasOpenLastFrame = false;
    }
}

} // namespace ProyecThor::UI
