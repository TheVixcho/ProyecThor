#pragma once
#include "LibraryContext.h"

namespace ProyecThor::Library {

// Contenedor con tabs Archivos / Stream
void RenderVideoSection(LibraryContext& ctx);

// Lista de videos locales con drag-drop, renombrar y eliminar
void RenderLocalVideoList(LibraryContext& ctx);

// Seccion de URLs de streaming con campo de entrada, lista y cola
void RenderStreamURLSection(LibraryContext& ctx);

} // namespace ProyecThor::Library