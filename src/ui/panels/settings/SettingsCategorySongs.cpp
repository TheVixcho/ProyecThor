#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "biblio/LibraryTags.h"
#include "biblio/LibrarySongs.h"
#include "biblio/LibraryHelpers.h"
#include "ui/framework/DesignSystem.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI::Settings {

// =============================================================================
//  RenderCategorySongs
//  Administracion de etiquetas de canciones: crear, renombrar, cambiar
//  color y eliminar. Cada cambio se persiste al instante en
//  song_tag_groups.ini y sincroniza de inmediato con la Biblioteca.
// =============================================================================
void SettingsPanel::RenderCategorySongs() {
    if (!SectionTitle("Etiquetas", "Canciones")) return;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextPrimary),
        "Etiquetas y Categorización de Canciones");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextSecondary),
        "Administra las etiquetas visuales para identificar y colorear tus canciones en la Biblioteca. "
        "Cada etiqueta se refleja al instante tanto en el buscador como en el fondo de la lista de canciones.");

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    static char        editNameBuffer[128] = {};
    static std::string editingId; // "" = ninguna fila en edicion inline
    static std::string deleteConfirmId;

    auto groups = ProyecThor::Library::LoadSongTagGroups();

    if (groups.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.15f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        if (ImGui::BeginChild("##noTagsBox", ImVec2(0.0f, 54.0f), true)) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::TextHint),
                "Aún no tienes etiquetas creadas. Usa el formulario inferior para crear tu primera etiqueta.");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::Dummy(ImVec2(0.0f, 12.0f));
    } else {
        for (auto& g : groups) {
            ImGui::PushID(g.id.c_str());

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
            ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(g.color.x, g.color.y, g.color.z, 0.25f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

            float cardH = 58.0f;
            if (ImGui::BeginChild("##tagCard", ImVec2(0.0f, cardH), true)) {
                // Swatch interactivo de color
                ImVec4 col = g.color;
                if (ImGui::ColorEdit4("##color", (float*)&col,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha)) {
                    g.color = col;
                    ProyecThor::Library::SaveSongTagGroup(g);
                    ProyecThor::Library::ClearSongIndexCache();
                    ProyecThor::Library::ForceListUpdate() = true;
                    ProyecThor::Library::ForceLibraryRescan() = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cambiar color de la etiqueta");

                ImGui::SameLine(0.0f, 12.0f);

                // Chip de previsualización visual idéntico al de Biblioteca
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 tagSz = ImGui::CalcTextSize(g.name.c_str());
                float pillW = tagSz.x + 22.0f;
                float pillH = 26.0f;
                ImVec2 p1(p0.x + pillW, p0.y + pillH);

                ImU32 pillBg   = ImGui::ColorConvertFloat4ToU32(ImVec4(g.color.x, g.color.y, g.color.z, 0.22f));
                ImU32 pillBrd  = ImGui::ColorConvertFloat4ToU32(ImVec4(g.color.x, g.color.y, g.color.z, 0.75f));
                ImU32 pillText = ImGui::ColorConvertFloat4ToU32(ImVec4(g.color.x * 0.4f + 0.6f, g.color.y * 0.4f + 0.6f, g.color.z * 0.4f + 0.6f, 1.0f));

                dl->AddRectFilled(p0, p1, pillBg, 5.0f);
                dl->AddRect(p0, p1, pillBrd, 5.0f, 0, 1.0f);
                dl->AddText(ImVec2(p0.x + 11.0f, p0.y + (pillH - tagSz.y) * 0.5f), pillText, g.name.c_str());

                ImGui::Dummy(ImVec2(pillW, pillH));

                ImGui::SameLine(0.0f, 14.0f);

                const bool isEditing = (editingId == g.id);
                if (isEditing) {
                    ImGui::SetNextItemWidth(240.0f);
                    bool commit = ImGui::InputText("##editname", editNameBuffer, sizeof(editNameBuffer),
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::SameLine(0.0f, 8.0f);
                    if (commit || DS::GlassButton("Guardar", ImVec2(80.0f, 26.0f), DS::SuccessColor)) {
                        std::string newName = editNameBuffer;
                        if (!newName.empty()) {
                            g.name = newName;
                            ProyecThor::Library::SaveSongTagGroup(g);
                            ProyecThor::Library::ClearSongIndexCache();
                            ProyecThor::Library::ForceListUpdate() = true;
                            ProyecThor::Library::ForceLibraryRescan() = true;
                        }
                        editingId.clear();
                    }
                    ImGui::SameLine(0.0f, 6.0f);
                    if (DS::GlassButton("Cancelar", ImVec2(80.0f, 26.0f))) {
                        editingId.clear();
                    }
                } else {
                    float availW = ImGui::GetContentRegionAvail().x;
                    ImGui::SameLine(availW - 190.0f);

                    if (DS::GlassButton("Renombrar", ImVec2(90.0f, 26.0f))) {
                        editingId = g.id;
                        memset(editNameBuffer, 0, sizeof(editNameBuffer));
                        strncpy_s(editNameBuffer, g.name.c_str(), sizeof(editNameBuffer) - 1);
                    }
                    ImGui::SameLine(0.0f, 8.0f);
                    if (DS::GlassButton("Eliminar", ImVec2(85.0f, 26.0f), DS::DangerColorDim)) {
                        deleteConfirmId = g.id;
                    }
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);

            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::PopID();
        }

        if (!deleteConfirmId.empty()) {
            ProyecThor::Library::DeleteSongTagGroup(deleteConfirmId);
            ProyecThor::Library::ClearSongIndexCache();
            ProyecThor::Library::ForceListUpdate() = true;
            ProyecThor::Library::ForceLibraryRescan() = true;
            deleteConfirmId.clear();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    // Formulario de creación de nueva etiqueta
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(DS::AccentColor), "+ Crear Nueva Etiqueta");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    static char   newNameBuffer[128] = {};
    static ImVec4 newColorBuffer     = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));

    if (ImGui::BeginChild("##newTagBox", ImVec2(0.0f, 76.0f), true)) {
        ImGui::SetNextItemWidth(260.0f);
        bool enterPressed = ImGui::InputTextWithHint("##newTagName", "Ej: Adoración, Alabanza...", newNameBuffer, sizeof(newNameBuffer),
                                                     ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine(0.0f, 12.0f);

        ImGui::ColorEdit4("##newTagColor", (float*)&newColorBuffer,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Elegir color para la nueva etiqueta");

        ImGui::SameLine(0.0f, 16.0f);

        const bool canCreate = newNameBuffer[0] != '\0';
        ImGui::BeginDisabled(!canCreate);
        if (DS::GlassButton("+ Crear Etiqueta", ImVec2(160.0f, 30.0f), DS::SuccessColor) || (enterPressed && canCreate)) {
            ProyecThor::Library::SongTagGroup g;
            g.name  = newNameBuffer;
            g.color = newColorBuffer;
            g.id    = ProyecThor::Library::NormalizeTagId(g.name);
            ProyecThor::Library::SaveSongTagGroup(g);
            ProyecThor::Library::ClearSongIndexCache();
            ProyecThor::Library::ForceListUpdate() = true;
            ProyecThor::Library::ForceLibraryRescan() = true;

            memset(newNameBuffer, 0, sizeof(newNameBuffer));
            newColorBuffer = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

} // namespace ProyecThor::UI::Settings