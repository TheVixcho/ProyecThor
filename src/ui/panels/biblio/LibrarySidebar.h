#pragma once
#include "LibraryContext.h"

namespace ProyecThor::Library {

// Renderiza los botones de categoria (por defecto en toolbar horizontal inferior).
// Muta ctx.currentCategoryInt, ctx.selectedIndex y llama ctx.refreshList().
void RenderCategoryButtons(LibraryContext& ctx, bool horizontal = true);

} // namespace ProyecThor::Library