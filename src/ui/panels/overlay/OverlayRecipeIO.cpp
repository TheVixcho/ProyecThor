#include "OverlayRecipeIO.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>

namespace fs = std::filesystem;
namespace ProyecThor::UI {

static std::string EscapeNewlines(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\r') continue;
        else out += c;
    }
    return out;
}
static std::string UnescapeNewlines(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i+1] == 'n') { out += '\n'; i++; }
        else out += s[i];
    }
    return out;
}

bool SaveOverlayRecipe(const fs::path& dir, const std::string& name, const OverlayDoc& doc) {
    if (name.empty()) return false;
    std::ofstream f(dir / (name + ".overlay"));
    if (!f.is_open()) return false;

    f << "canvasW=" << doc.canvasW << "\n";
    f << "canvasH=" << doc.canvasH << "\n";
    f << "bgColor=" << doc.bgColor[0] << "," << doc.bgColor[1] << ","
                    << doc.bgColor[2] << "," << doc.bgColor[3] << "\n";
    f << "layerCount=" << doc.layers.size() << "\n";
    for (size_t i = 0; i < doc.layers.size(); i++) {
        const auto& l = doc.layers[i];
        const char* kindStr = l.kind == OverlayLayerKind::Image ? "image"
                             : l.kind == OverlayLayerKind::Shape ? "shape"
                             : l.kind == OverlayLayerKind::Clock ? "clock" : "text";
        f << "layer" << i << ".kind="  << kindStr << "\n";
        f << "layer" << i << ".text="  << EscapeNewlines(l.text) << "\n";
        f << "layer" << i << ".font="  << l.fontName << "\n";
        f << "layer" << i << ".size="  << l.fontSize << "\n";
        f << "layer" << i << ".color=" << l.color[0] << "," << l.color[1] << ","
                                        << l.color[2] << "," << l.color[3] << "\n";
        f << "layer" << i << ".image=" << l.imagePath << "\n";
        f << "layer" << i << ".shapeKind=" << (l.shapeKind == OverlayShapeKind::Ellipse ? "ellipse" : "rect") << "\n";
        f << "layer" << i << ".shapeFilled=" << (l.shapeFilled ? 1 : 0) << "\n";
        f << "layer" << i << ".shapeRounding=" << l.shapeRounding << "\n";
        f << "layer" << i << ".sizeW=" << l.sizeW << "\n";
        f << "layer" << i << ".sizeH=" << l.sizeH << "\n";
        f << "layer" << i << ".rotation=" << l.rotation << "\n";
        f << "layer" << i << ".posX="  << l.posX << "\n";
        f << "layer" << i << ".posY="  << l.posY << "\n";
        f << "layer" << i << ".opacity=" << l.opacity << "\n";

        f << "layer" << i << ".shadowOn="  << (l.shadowEnabled ? 1 : 0) << "\n";
        f << "layer" << i << ".shadowCol=" << l.shadowColor[0] << "," << l.shadowColor[1] << ","
                                            << l.shadowColor[2] << "," << l.shadowColor[3] << "\n";
        f << "layer" << i << ".shadowOffX=" << l.shadowOffsetX << "\n";
        f << "layer" << i << ".shadowOffY=" << l.shadowOffsetY << "\n";

        f << "layer" << i << ".outlineOn="  << (l.outlineEnabled ? 1 : 0) << "\n";
        f << "layer" << i << ".outlineCol=" << l.outlineColor[0] << "," << l.outlineColor[1] << ","
                                             << l.outlineColor[2] << "," << l.outlineColor[3] << "\n";
        f << "layer" << i << ".outlineW="   << l.outlineWidth << "\n";

        f << "layer" << i << ".bgOn="      << (l.bgEnabled ? 1 : 0) << "\n";
        f << "layer" << i << ".bgCol="     << l.bgColor[0] << "," << l.bgColor[1] << ","
                                            << l.bgColor[2] << "," << l.bgColor[3] << "\n";
        f << "layer" << i << ".bgPadX="    << l.bgPaddingX << "\n";
        f << "layer" << i << ".bgPadY="    << l.bgPaddingY << "\n";
        f << "layer" << i << ".bgRound="   << l.bgRounding << "\n";
    }
    return true;
}

bool LoadOverlayRecipe(const fs::path& dir, const std::string& name, OverlayDoc& out) {
    std::ifstream f(dir / (name + ".overlay"));
    if (!f.is_open()) return false;

    out = OverlayDoc{};
    out.layers.clear();

    std::string line;
    while (std::getline(f, line)) {
        std::string::size_type eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();

        if (k == "canvasW") out.canvasW = std::atoi(v.c_str());
        else if (k == "canvasH") out.canvasH = std::atoi(v.c_str());
        else if (k == "bgColor")
            sscanf(v.c_str(), "%f,%f,%f,%f", &out.bgColor[0], &out.bgColor[1], &out.bgColor[2], &out.bgColor[3]);
        else if (k == "layerCount") { /* solo informativo */ }
        else if (k.rfind("layer", 0) == 0) {
            std::string::size_type dot = k.find('.');
            if (dot == std::string::npos) continue;
            int idx = std::atoi(k.substr(5, dot - 5).c_str());
            std::string field = k.substr(dot + 1);
            if (idx < 0) continue;
            while ((int)out.layers.size() <= idx) out.layers.push_back(OverlayLayer{});

            auto& l = out.layers[idx];
            if      (field == "kind") {
                l.kind = (v == "image") ? OverlayLayerKind::Image
                       : (v == "shape") ? OverlayLayerKind::Shape
                       : (v == "clock") ? OverlayLayerKind::Clock
                                        : OverlayLayerKind::Text;
            }
            else if (field == "text")  l.text     = UnescapeNewlines(v);
            else if (field == "font")  l.fontName = v;
            else if (field == "size")  l.fontSize = (float)std::atof(v.c_str());
            else if (field == "image") l.imagePath = v;
            else if (field == "shapeKind") l.shapeKind = (v == "ellipse") ? OverlayShapeKind::Ellipse : OverlayShapeKind::Rectangle;
            else if (field == "shapeFilled") l.shapeFilled = (v != "0");
            else if (field == "shapeRounding") l.shapeRounding = (float)std::atof(v.c_str());
            else if (field == "sizeW") l.sizeW    = (float)std::atof(v.c_str());
            else if (field == "sizeH") l.sizeH    = (float)std::atof(v.c_str());
            else if (field == "rotation") l.rotation = (float)std::atof(v.c_str());
            else if (field == "posX")  l.posX     = (float)std::atof(v.c_str());
            else if (field == "posY")  l.posY     = (float)std::atof(v.c_str());
            else if (field == "opacity") l.opacity = (float)std::atof(v.c_str());
            else if (field == "color")
                sscanf(v.c_str(), "%f,%f,%f,%f", &l.color[0], &l.color[1], &l.color[2], &l.color[3]);
            else if (field == "shadowOn")   l.shadowEnabled = (v != "0");
            else if (field == "shadowCol")
                sscanf(v.c_str(), "%f,%f,%f,%f", &l.shadowColor[0], &l.shadowColor[1],
                       &l.shadowColor[2], &l.shadowColor[3]);
            else if (field == "shadowOffX") l.shadowOffsetX = (float)std::atof(v.c_str());
            else if (field == "shadowOffY") l.shadowOffsetY = (float)std::atof(v.c_str());
            else if (field == "outlineOn")  l.outlineEnabled = (v != "0");
            else if (field == "outlineCol")
                sscanf(v.c_str(), "%f,%f,%f,%f", &l.outlineColor[0], &l.outlineColor[1],
                       &l.outlineColor[2], &l.outlineColor[3]);
            else if (field == "outlineW")   l.outlineWidth = (float)std::atof(v.c_str());
            else if (field == "bgOn")       l.bgEnabled = (v != "0");
            else if (field == "bgCol")
                sscanf(v.c_str(), "%f,%f,%f,%f", &l.bgColor[0], &l.bgColor[1], &l.bgColor[2], &l.bgColor[3]);
            else if (field == "bgPadX")     l.bgPaddingX = (float)std::atof(v.c_str());
            else if (field == "bgPadY")     l.bgPaddingY = (float)std::atof(v.c_str());
            else if (field == "bgRound")    l.bgRounding = (float)std::atof(v.c_str());
        }
    }
    return true;
}

} // namespace ProyecThor::UI
