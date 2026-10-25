#include "SettingsPanel.h"
#include "SettingsManager.h"
#include "biblio/LibraryTags.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace ProyecThor::UI::Settings {

// =============================================================================
//  RenderCategorySongs
//  Administracion de etiquetas de canciones: crear, renombrar, cambiar
//  color y eliminar. Cada cambio se persiste al instante en
//  song_tag_groups.ini (no depende del boton global "Guardar ajustes",
//  que es solo para AppSettings). Pensado como el primer bloque de una
//  categoria "Canciones" que a futuro puede sumar mas opciones.
// =============================================================================
void SettingsPanel::RenderCategorySongs() {
    ImGui::TextDisabled("Administra las etiquetas de canciones: se usan para "
                        "pintar el fondo de cada cancion en la Biblioteca.");
    ImGui::Spacing();

    if (!SectionTitle("Etiquetas")) return;

    static char        editNameBuffer[128] = {};
    static std::string editingId; // "" = ninguna fila en edicion inline

    auto groups = ProyecThor::Library::LoadSongTagGroups();

    if (groups.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.72f, 1.0f));
        ImGui::TextUnformatted("Todavia no hay etiquetas. Crea la primera abajo.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

    for (auto& g : groups) {
        ImGui::PushID(g.id.c_str());

        // Swatch de color — cambia y persiste al instante
        ImVec4 col = g.color;
        if (ImGui::ColorEdit4("##color", (float*)&col,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha)) {
            g.color = col;
            ProyecThor::Library::SaveSongTagGroup(g);
        }

        ImGui::SameLine();

        const bool isEditing = (editingId == g.id);
        if (isEditing) {
            ImGui::SetNextItemWidth(220.0f);
            bool commit = ImGui::InputText("##editname", editNameBuffer, sizeof(editNameBuffer),
                                           ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (commit || ImGui::SmallButton("Listo")) {
                g.name = editNameBuffer;
                if (g.name.empty()) g.name = g.id;
                ProyecThor::Library::SaveSongTagGroup(g);
                editingId.clear();
            }
        } else {
            ImGui::TextUnformatted(g.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Renombrar")) {
                editingId = g.id;
                memset(editNameBuffer, 0, sizeof(editNameBuffer));
                strncpy(editNameBuffer, g.name.c_str(), sizeof(editNameBuffer) - 1);
            }
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.40f, 1.0f));
        if (ImGui::SmallButton("Eliminar")) {
            ProyecThor::Library::DeleteSongTagGroup(g.id);
            if (editingId == g.id) editingId.clear();
        }
        ImGui::PopStyleColor();

        ImGui::PopID();
    }

    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::SeparatorText("Nueva etiqueta");

    static char   newNameBuffer[128] = {};
    static ImVec4 newColorBuffer     = ImVec4(0.35f, 0.55f, 0.95f, 1.0f);

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##newTagName", "Nombre de la etiqueta", newNameBuffer, sizeof(newNameBuffer));
    ImGui::SameLine();
    ImGui::ColorEdit4("##newTagColor", (float*)&newColorBuffer,
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
    ImGui::SameLine();

    const bool canCreate = newNameBuffer[0] != '\0';
    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("+ Crear")) {
        ProyecThor::Library::SongTagGroup g;
        g.name  = newNameBuffer;
        g.color = newColorBuffer;
        g.id    = ProyecThor::Library::NormalizeTagId(g.name);
        ProyecThor::Library::SaveSongTagGroup(g);

        memset(newNameBuffer, 0, sizeof(newNameBuffer));
        newColorBuffer = ImVec4(0.35f, 0.55f, 0.95f, 1.0f);
    }
    ImGui::EndDisabled();
}

} // namespace ProyecThor::UI::Settings