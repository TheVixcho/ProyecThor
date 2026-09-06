#pragma once
#include "LibraryContext.h"

namespace ProyecThor::Library {

// Abre el preview a pantalla completa (dentro de la propia ventana de
// ProyecThor, no una ventana de SO aparte en otro monitor -- "asi siempre
// funciona", sin depender de detectar en que pantalla esta proyectando)
// para ctx.items[index]. Siguiente/Anterior dentro del preview navegan
// sobre TODO ctx.items (no aplican el filtro de busqueda que este activo
// en ese momento en la lista).
void OpenVideoPreview(LibraryContext& ctx, int index);

// Dibuja el overlay a pantalla completa si esta abierto (no hace nada si
// no). Llamar siempre desde RenderVideoSection, sin importar la pestaña
// activa (Archivos/Stream) -- tiene que poder seguir cerrandose aunque el
// operador haya cambiado de pestaña mientras estaba abierto.
void RenderVideoPreviewOverlay(LibraryContext& ctx);

} // namespace ProyecThor::Library
