#pragma once
#include <string>
#include <map>

struct IconTexture {
    void* textureID = nullptr;
    int width = 0;
    int height = 0;
};

class StyleGeneralApp {
public:
    // Mapa para guardar muchos iconos y llamarlos por nombre
    static inline std::map<std::string, IconTexture> Icons;
    
    // Método para cargar y guardar el icono
   // Cambia LoadIcon por LoadAppIcon
static void LoadAppIcon(const std::string& name, const std::string& path);
};