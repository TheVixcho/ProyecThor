#pragma once
#include "LibraryContext.h"

// Necesitamos ItemType para RenderDefaultStyleCombo
#include "backend/core/PresentationCore.h"

namespace ProyecThor::Library {

// Modal de renombrar archivo o URL.
// Muta ctx.showRenameModal, ctx.streamURLs, ctx.selectedURLIndex,
// ctx.selectedIndex, y llama ctx.refreshList() / ctx.saveStreamURLs().
void RenderRenameModal(LibraryContext& ctx);

// Combo de estilo por defecto para Songs y Bibles.
// Solo se renderiza si currentCategoryInt == Songs o Bibles.
// currentCategoryInt se interpreta como LibraryCategory.
//
// trailingReserve: pixeles a dejar libres en el extremo derecho del combo
// (por ejemplo, para ubicar el botón de "Actualizar" en la misma fila,
// en la esquina, sin que el combo se lo tape).
void RenderDefaultStyleCombo(LibraryContext& ctx, float trailingReserve = 0.0f);

} // namespace ProyecThor::Library