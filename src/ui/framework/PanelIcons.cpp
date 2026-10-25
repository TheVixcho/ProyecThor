#include "PanelIcons.h"
#include "stb_image.h" // la implementacion (STB_IMAGE_IMPLEMENTATION)
                                        // ya vive en StreamingPanel.cpp, aca solo
                                        // se necesita la declaracion.
#include <iostream>
#include <cstring>

namespace ProyecThor::UI {

void PanelIcons::RegisterIcon(const std::string& id, const std::string& pngPath)
{
    if (m_Icons.count(id)) return;

    int w = 0, h = 0, ch = 0;
    if (!stbi_info(pngPath.c_str(), &w, &h, &ch)) {
        std::cerr << "[PanelIcons] No se pudo leer '" << pngPath << "'\n";
        return;
    }

    ImGuiIO& io   = ImGui::GetIO();
    ImFont*  font = io.FontDefault ? io.FontDefault
                                   : (io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0]);

    ImWchar codepoint = m_NextCodepoint++;

    // Alto del glifo = alto de la fuente base, ancho proporcional al PNG
    // original, para que se vea prolijo al lado de texto normal si hace falta.
    // En 1.92+ ya no existe font->FontSize (fuentes de tamano dinamico);
    // usamos LegacySize, que es el tamano con el que se cargo la fuente.
    float targetH  = font ? font->LegacySize : 16.0f;
    float scale    = targetH / static_cast<float>(h);
    float advanceX = static_cast<float>(w) * scale + 4.0f; // pequeno respiro

    int rectIndex = io.Fonts->AddCustomRectFontGlyph(
        font, codepoint, w, h, advanceX, ImVec2(0.0f, targetH - h * scale));

    PendingIcon pending;
    pending.path      = pngPath;
    pending.rectIndex = rectIndex;
    pending.codepoint = codepoint;
    m_Icons[id] = pending;
}

void PanelIcons::UploadPixelsAfterBuild()
{
    if (m_Uploaded) return;
    m_Uploaded = true;

    ImGuiIO& io = ImGui::GetIO();
    unsigned char* atlasPixels = nullptr;
    int atlasW = 0, atlasH = 0;
    io.Fonts->GetTexDataAsRGBA32(&atlasPixels, &atlasW, &atlasH);

    for (auto& [id, pending] : m_Icons)
    {
        if (pending.rectIndex < 0) continue;

        ImFontAtlasRect rect;
        if (!io.Fonts->GetCustomRect(pending.rectIndex, &rect)) continue;

        int w = 0, h = 0, ch = 0;
        unsigned char* pixels = stbi_load(pending.path.c_str(), &w, &h, &ch, 4);
        if (!pixels) {
            std::cerr << "[PanelIcons] No se pudo cargar '" << pending.path << "'\n";
            continue;
        }

        for (int y = 0; y < h; ++y) {
            unsigned char* dst = atlasPixels + ((rect.y + y) * atlasW + rect.x) * 4;
            unsigned char* src = pixels + y * w * 4;
            std::memcpy(dst, src, static_cast<size_t>(w) * 4);
        }
        stbi_image_free(pixels);
    }
}
std::string PanelIcons::GetGlyph(const std::string& id, const std::string& fallback) const
{
    auto it = m_Icons.find(id);
    if (it == m_Icons.end() || it->second.codepoint == 0) return fallback;

    // Codificamos el codepoint (rango 0xE000+, cabe en 3 bytes UTF-8) a
    // mano para no depender de <codecvt>.
    ImWchar cp = it->second.codepoint;
    std::string out;
    out += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
    out += static_cast<char>(0x80 | ((cp >> 6)  & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
    return out;
}

} // namespace ProyecThor::UI