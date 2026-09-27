#include "StyleGeneralApp.h"
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "stb_image.h" 

#include <filesystem>
#include <vector>

void* LoadTextureFromFile(const char* filename, int* width, int* height) {
    if (!filename || filename[0] == '\0') return nullptr;

    std::string baseFile = std::filesystem::path(filename).filename().string();
    std::vector<std::string> candidatePaths = {
        filename,
        std::string("assets/") + filename,
        std::string("bin/") + filename,
        std::string("assets/bin/assets/icons/ui/") + baseFile,
        std::string("bin/assets/icons/ui/") + baseFile,
        std::string("build-win/bin/assets/icons/ui/") + baseFile,
        std::string("build-linux/bin/assets/icons/ui/") + baseFile
    };

    int channels = 0;
    unsigned char* data = nullptr;
    std::string loadedFrom;

    for (const auto& candidate : candidatePaths) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            data = stbi_load(candidate.c_str(), width, height, &channels, 4);
            if (data != nullptr) {
                loadedFrom = candidate;
                break;
            }
        }
    }

    if (data == nullptr) {
        // Ultimo intento directo
        data = stbi_load(filename, width, height, &channels, 4);
        if (data != nullptr) loadedFrom = filename;
    }

    if (data == nullptr) {
        std::cerr << "[ERROR] stb_image no pudo cargar: " << filename 
                  << " | Motivo: " << stbi_failure_reason() << std::endl;
        return nullptr;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // IMPORTANTE: Evita errores con imágenes que no son potencia de 2 (NPOT)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Filtrado (Linear para suavizado)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Subir a la VRAM
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *width, *height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);

    std::cout << "[INFO] Textura cargada con éxito: " << loadedFrom << " (ID: " << texture << ")" << std::endl;

    return (void*)(intptr_t)texture;
}

void StyleGeneralApp::LoadAppIcon(const std::string& name, const std::string& path) {

    int width = 0, height = 0;
    
    // Verificamos si hay un contexto OpenGL activo antes de intentar generar la textura
    if (glfwGetCurrentContext() == nullptr) {
        std::cerr << "[FATAL ERROR] Intentando cargar icono '" << name 
                  << "' pero NO hay contexto OpenGL activo." << std::endl;
        return;
    }

    void* texId = LoadTextureFromFile(path.c_str(), &width, &height);
    if (texId) {
        Icons[name] = { texId, width, height };
    } else {
        std::cerr << "[ERROR] Fallo al registrar el icono: " << name << std::endl;
    }
}