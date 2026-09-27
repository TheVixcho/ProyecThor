#pragma once
#include <imgui.h>
#include <string>
#include <vector>

namespace ProyecThor::Library {

// =============================================================================
//  Etiquetas de canciones — grupos con nombre + color, usados para pintar
//  el FONDO de cada cancion en la lista y diferenciarlas visualmente, todas
//  juntas, en la misma lista (no una pestaña aparte).
//
//  Se ADMINISTRAN (crear / renombrar / cambiar color / eliminar) desde
//  Ajustes > Canciones. La Biblioteca solo las ASIGNA a cada cancion via
//  click derecho > Asignar etiqueta.
// =============================================================================

struct SongTagGroup {
    std::string id;
    std::string name;
    ImVec4 color = ImVec4(0.35f, 0.55f, 0.95f, 1.0f);
};

std::vector<SongTagGroup> LoadSongTagGroups();
void SaveSongTagGroups(const std::vector<SongTagGroup>& groups);

// Crea el grupo si "group.id" no existe, o lo actualiza si ya existe.
// Pensado para el panel de Ajustes, donde cada edicion (color, nombre) se
// persiste al instante sin pasar por el botón global "Guardar ajustes".
void SaveSongTagGroup(const SongTagGroup& group);

void DeleteSongTagGroup(const std::string& id);
SongTagGroup FindSongTagGroup(const std::string& id);

std::string NormalizeTagId(const std::string& raw);

// true si "filename" tiene asignada la etiqueta "tagId" (via songs_tags.ini)
bool SongHasTag(const std::string& filename, const std::string& tagId);

} // namespace ProyecThor::Library