#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

// Elemento de filtro para selectores de archivos
struct FileFilterItem {
    std::string name;                   // Nombre visible, ej. "Videos" o "Imágenes"
    std::vector<std::string> patterns;  // Patrones glob, ej. {"*.mp4", "*.mkv"} o {"*.jpg", "*.png"}
};

// -----------------------------------------------------------------------------
// Selectores Genéricos Nativos / Portal XDG Desktop (Flatpak & Linux / Windows)
// -----------------------------------------------------------------------------

// Abre un selector para UN archivo existente.
// En Linux: usa el Portal XDG FileChooser (máxima compatibilidad con sandbox Flatpak/Flathub),
// con fallback a zenity/kdialog si el portal no está disponible.
// En Windows: usa IFileOpenDialog.
std::string PickFile(const std::string& title,
                     const std::vector<FileFilterItem>& filters);

// Abre un selector para MÚLTIPLES archivos existentes.
std::vector<std::string> PickMultipleFiles(const std::string& title,
                                           const std::vector<FileFilterItem>& filters);

// Selector de guardado ("Guardar archivo como")
std::string PickSaveFile(const std::string& title,
                         const std::string& defaultPath,
                         const std::vector<FileFilterItem>& filters);

// Selector de carpetas (directorio)
std::string PickFolder(const std::string& title = "Elegir carpeta");

// -----------------------------------------------------------------------------
// Funciones de conveniencia (mantienen 100% retrocompatibilidad con la app)
// -----------------------------------------------------------------------------
std::string PickImageOrVideoFile();
std::string PickImageFile();
std::string PickHtmlFile();
std::string PickAudioFile();
std::string PickFontFile();
std::string PickSvgFile();

std::string PickSaveVideoPath(const std::string& defaultPath);
std::string PickSaveTextPath(const std::string& defaultPath);

// Extension-sniffing simple para decidir si un path va por el pipeline de video o imagen
bool LooksLikeVideoPath(const std::string& path);

} // namespace ProyecThor::UI
