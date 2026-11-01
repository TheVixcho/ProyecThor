#pragma once
#include "LibraryContext.h"
#include "core/PresentationCore.h"
#include <imgui.h>
#include <functional>
#include <string>
#include <vector>

namespace ProyecThor::Library {

struct MMItem {
    std::string    filename;
    Core::ItemType type;
    std::string    customFullPath = "";
};

const std::vector<MMItem>& GetVideoList();
const std::vector<MMItem>& GetImageList();
const std::vector<MMItem>& GetAudioList();

ImTextureID GetMMVideoThumbnail(const std::string& path);
ImTextureID GetMMImageThumbnail(const std::string& path);
ImTextureID GetMMAudioThumbnail(const std::string& path);
std::string GetMMItemFullPath(const MMItem& it);

// Filtro de tipo dentro de la seccion Multimedia (ver RenderMultimediaSection).
enum class MultimediaFilter { All = 0, Video, Audio, Image };

// Seccion unificada de Proyeccion: Video + Audio + Imagen en un solo lugar,
// separados en secciones con encabezado (o una sola si hay un filtro activo).
void RenderMultimediaSection(char* searchBuffer, int searchBufferSize, MultimediaFilter& filter, std::function<void()> importFile = nullptr);
inline void RenderMultimediaSection(LibraryContext& ctx, MultimediaFilter& filter) {
    RenderMultimediaSection(ctx.searchBuffer, ctx.searchBufferSize, filter, ctx.importFile);
}

// Vuelve a escanear las 3 carpetas (video/audio/imagen). Llamado al entrar a
// la categoria y despues de importar/renombrar/borrar un item.
void RefreshMultimediaLists();

// Precarga real de miniaturas de imágenes, portadas de audio y fotogramas de
// video en texturas OpenGL durante el SplashScreen, reportando progreso real.
using MultimediaPreloadProgressFn = std::function<void(int done, int total, const std::string& itemName)>;
void PreloadMultimediaThumbnails(const MultimediaPreloadProgressFn& onProgress = nullptr);

// Permite reutilizar miniaturas de video ya precargadas desde otras vistas (ej. LibraryVideos).
ImTextureID QuerySharedVideoThumbnail(const std::string& path);

// Diálogo de importación para archivos multimedia (videos, audios, imágenes).
void ImportMultimediaFile();

} // namespace ProyecThor::Library
