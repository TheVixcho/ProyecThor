#pragma once
#include "LibraryContext.h"

namespace ProyecThor::Library {

// Filtro de tipo dentro de la seccion Multimedia (ver RenderMultimediaSection).
enum class MultimediaFilter { All = 0, Video, Audio, Image };

// Seccion unificada de Proyeccion: Video + Audio + Imagen en un solo lugar,
// separados en secciones con encabezado (o una sola si hay un filtro activo).
// Reemplaza el rol de RenderVideoSection (Video), la rama de imagenes de
// RenderSideList (LibrarySongs.cpp) y AudioPanel::RenderLibraryList() (Audio)
// como entry point del sidebar para estos 3 tipos.
void RenderMultimediaSection(LibraryContext& ctx, MultimediaFilter& filter);

// Vuelve a escanear las 3 carpetas (video/audio/imagen). Llamado al entrar a
// la categoria y despues de importar/renombrar/borrar un item.
void RefreshMultimediaLists();

} // namespace ProyecThor::Library
