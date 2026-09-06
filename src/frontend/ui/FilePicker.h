#pragma once
#include <string>

namespace ProyecThor::UI {

// Selector nativo de archivo (imagen o video): IFileOpenDialog en Windows,
// zenity/kdialog en Linux (fallback en cadena, igual que Audio.cpp y
// LayersBgTab.cpp). Devuelve "" si el usuario cancela o no hay ninguna
// herramienta disponible.
std::string PickImageOrVideoFile();

// Mismo patron, pero solo imagenes (jpg/jpeg/png) — usado para el Logo de
// pantalla de carga (Ajustes > Proyeccion), donde un video no tendria
// sentido.
std::string PickImageFile();
std::string PickHtmlFile();

// Elegir una CARPETA (no un archivo) -- usado para fijar una carpeta de
// salida fija en el conversor de Render (ver LibraryPanel::
// RenderConverterSection). IFileOpenDialog + FOS_PICKFOLDERS en Windows,
// zenity --file-selection --directory / kdialog --getexistingdirectory en
// Linux -- funcionan igual bajo X11 o Wayland, son apps GTK/Qt propias que
// no dependen del compositor. Devuelve "" si el usuario cancela.
std::string PickFolder(const std::string& title = "Elegir carpeta");

// Elegir DONDE GUARDAR un archivo nuevo (a diferencia de los Pick* de
// arriba, que abren uno YA existente) -- usado por "Guardar como" del
// conversor de Render. IFileSaveDialog en Windows, zenity --file-selection
// --save / kdialog --getsavefilename en Linux (Wayland incluido, mismo
// motivo que PickFolder). `defaultPath` sugiere carpeta+nombre inicial
// (con extensión). Devuelve "" si cancela.
std::string PickSaveVideoPath(const std::string& defaultPath);

// Mismo patron que PickSaveVideoPath pero filtrado a texto plano (.txt) --
// usado por "Descargar subtitulos" del Hub (ver SubtitleImporter.h),
// donde el resultado es un .txt suelto y no una cancion de Biblioteca.
std::string PickSaveTextPath(const std::string& defaultPath);

// Extension-sniffing simple para decidir si un path va por el pipeline de
// video o de imagen (mismo criterio que BackgroundLayer).
bool LooksLikeVideoPath(const std::string& path);

} // namespace ProyecThor::UI
