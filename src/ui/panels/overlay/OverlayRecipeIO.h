#pragma once
#include "OverlayTypes.h"
#include <filesystem>
#include <string>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayRecipeIO — lectura/escritura de la receta editable de un overlay
//  (archivo "<name>.overlay", texto plano clave=valor, ver comentarios en el
//  .cpp para el formato exacto). Extraido de OverlayLibraryTab para que
//  SyncServer.cpp (control remoto desde el celular) pueda leer/escribir
//  exactamente el mismo formato sin duplicar el parser -- "dir" es la
//  carpeta de overlays (antes resuelta por separado en cada lado, ver
//  ProyecThor::OverlaysPath() en AppPaths.h).
// ─────────────────────────────────────────────────────────────────────────────
bool SaveOverlayRecipe(const std::filesystem::path& dir, const std::string& name, const OverlayDoc& doc);
bool LoadOverlayRecipe(const std::filesystem::path& dir, const std::string& name, OverlayDoc& out);

} // namespace ProyecThor::UI
