#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ProyecThor::UI {

struct BgEntry {
    std::string fullPath;   // ruta absoluta en disco
    std::string name;       // nombre de archivo sin extensión
    std::string ext;        // extensión en minúsculas (.mp4, .png, etc.)
    std::string folder;     // nombre de la subcarpeta relativa (vacío = raíz)
    bool        isImage    = false;
    bool        isExternal = false; // true = vinculado desde carpeta externa (ej. Descargas) sin duplicar
};

class LayersBgTab; // Tu clase original que renderiza los fondos

// Ya no es un IPanel independiente: vive como seccion del sidebar del hub de
// Diseño (ver StylesHubPanel.h/.cpp).
class BackgroundsPanel {
public:
    BackgroundsPanel();
    ~BackgroundsPanel();

    void RenderContent();
    void PreloadThumbnails(const std::function<void(int done, int total, const std::string& name)>& onProgress = nullptr);
    std::string GetName() const { return "Fondos"; }

private:
    std::unique_ptr<LayersBgTab> m_BgTab;
};

} // namespace ProyecThor::UI