#pragma once
#include "LibraryContext.h"

namespace ProyecThor::Library {

// Renderiza el sidebar izquierdo con los botones de categoria.
// Muta ctx.currentCategoryInt, ctx.selectedIndex y llama ctx.refreshList().
void RenderCategoryButtons(LibraryContext& ctx);

} // namespace ProyecThor::Library