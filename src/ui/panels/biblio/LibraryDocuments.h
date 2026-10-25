#pragma once
#include "LibraryContext.h"
#include "ui/views/DocumentView.h"

namespace ProyecThor::Library {

// Lista de carpetas de documentos + visor embebido (DocumentView).
// documentView se pasa por referencia porque vive en LibraryPanel como miembro.
void RenderDocumentSection(LibraryContext& ctx, UI::DocumentView& documentView);

} // namespace ProyecThor::Library