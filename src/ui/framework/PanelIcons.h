#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace ProyecThor::UI {

// Permite usar un PNG como si fuera un "caracter" mas dentro de cualquier
// texto de ImGui (incluido el titulo de una ventana / pestana de dock).
// ImGui nunca dibuja imagenes dentro de un titulo -- solo texto -- asi que
// la unica forma real de lograrlo es meter el PNG como glifo del atlas de
// fuentes (tecnica soportada oficialmente via AddCustomRectFontGlyph).
class PanelIcons {
public:
    static PanelIcons& Get() {
        static PanelIcons instance;
        return instance;
    }

    // Paso 1 — llamar para CADA icono, ANTES de io.Fonts->Build().
    // Solo reserva el espacio del glifo (lee ancho/alto del PNG), todavia
    // no copia pixeles reales.
    void RegisterIcon(const std::string& id, const std::string& pngPath);

    // Paso 2 — llamar UNA sola vez, justo DESPUES de io.Fonts->Build()
    // y ANTES de que el backend suba la textura a la GPU
    // (ImGui_ImplOpenGL3_Init crea esa textura la primera vez que la pide,
    // asi que hay que hacer esto antes de esa llamada).
    void UploadPixelsAfterBuild();

    // Devuelve el string UTF-8 de 1 "caracter" que representa el icono.
    // Se concatena en cualquier texto/titulo para que se vea el icono ahi.
    // Si el id no fue registrado, devuelve 'fallback' (por defecto vacio).
    std::string GetGlyph(const std::string& id, const std::string& fallback = "") const;

private:
    struct PendingIcon {
        std::string path;
        int         rectIndex = -1;
        ImWchar     codepoint = 0;
    };

    ImWchar m_NextCodepoint = 0xE000; // Zona de Uso Privado de Unicode
    std::unordered_map<std::string, PendingIcon> m_Icons;
    bool m_Uploaded = false;
};

} // namespace ProyecThor::UI