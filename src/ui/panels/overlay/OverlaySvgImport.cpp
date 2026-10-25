#include "OverlaySvgImport.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#include "stb_image_write.h"
#include "stb_image.h"
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STBIR_DEFAULT_FILTER_UPSAMPLE   STBIR_FILTER_TRIANGLE
#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_TRIANGLE
#include "stb_image_resize2.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cfloat>
#include <functional>
#include <unordered_map>

namespace fs = std::filesystem;

namespace ProyecThor::UI {

namespace {

std::string SanitizeStem(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum((unsigned char)c) || c == '-' || c == '_') out += c;
        else out += '_';
    }
    if (out.empty()) out = "capa";
    if (out.size() > 40) out = out.substr(0, 40);
    return out;
}

// Devuelve el indice justo despues de la etiqueta que empieza en s[start]
// (que debe ser '<'), respetando comillas de atributos (para no confundir
// un '>' literal dentro de un valor con el cierre de la etiqueta), y marca
// si es autocerrada (".../>") en selfClosing.
size_t ScanTagEnd(const std::string& s, size_t start, bool& selfClosing) {
    selfClosing = false;
    size_t i = start + 1;
    char quote = 0;
    while (i < s.size()) {
        char c = s[i];
        if (quote) {
            if (c == quote) quote = 0;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '>') {
            selfClosing = (i > start && s[i - 1] == '/');
            return i + 1;
        }
        i++;
    }
    return s.size();
}

std::string TagNameAt(const std::string& s, size_t start) {
    size_t i = start + 1;
    if (i < s.size() && s[i] == '/') i++;
    size_t nameStart = i;
    while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '-' || s[i] == ':'))
        i++;
    return s.substr(nameStart, i - nameStart);
}

// Primer atributo id="..." (o '...') dentro de una etiqueta -- solo para
// nombrar el PNG generado de forma mas legible, no afecta el import en si.
std::string ExtractIdAttr(const std::string& markup) {
    size_t p = markup.find("id=");
    if (p == std::string::npos) return {};
    p += 3;
    if (p >= markup.size()) return {};
    char q = markup[p];
    if (q != '"' && q != '\'') return {};
    size_t end = markup.find(q, p + 1);
    if (end == std::string::npos) return {};
    return markup.substr(p + 1, end - p - 1);
}

// Atributo generico name="..." dentro de una etiqueta (o '...').
bool ExtractAttr(const std::string& markup, const std::string& name, std::string& out) {
    std::string key = name + "=";
    size_t p = markup.find(key);
    while (p != std::string::npos) {
        // Evitar matchear el sufijo de otro atributo (ej. "xlink:href" al
        // buscar "href").
        if (p == 0 || !(std::isalnum((unsigned char)markup[p - 1]) || markup[p - 1] == ':' || markup[p - 1] == '-'))
            break;
        p = markup.find(key, p + 1);
    }
    if (p == std::string::npos) return false;
    p += key.size();
    if (p >= markup.size()) return false;
    char q = markup[p];
    if (q != '"' && q != '\'') return false;
    size_t end = markup.find(q, p + 1);
    if (end == std::string::npos) return false;
    out = markup.substr(p + 1, end - p - 1);
    return true;
}

int Base64DecodedSize(const std::string& b64) {
    int len = (int)b64.size();
    int pad = 0;
    if (len >= 1 && b64[len - 1] == '=') pad++;
    if (len >= 2 && b64[len - 2] == '=') pad++;
    return (len / 4) * 3 - pad;
}

// Decodificador base64 minimo -- suficiente para el data URI embebido en
// <image href="data:image/png;base64,...">, no hace falta una libreria
// aparte para esto.
std::vector<unsigned char> Base64Decode(const std::string& b64) {
    static int table[256];
    static bool init = false;
    if (!init) {
        std::fill(std::begin(table), std::end(table), -1);
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) table[(unsigned char)chars[i]] = i;
        init = true;
    }

    std::vector<unsigned char> out;
    out.reserve(std::max(0, Base64DecodedSize(b64)));
    int val = 0, bits = -8;
    for (unsigned char c : b64) {
        if (c == '=' || std::isspace(c)) continue;
        if (table[c] == -1) continue;
        val = (val << 6) + table[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back((unsigned char)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// nanosvg no dibuja <image> (fotos/patrones embebidos como data URI base64
// -- muy comun como fondo en exports de Canva): sin esto, esa parte del
// diseño queda directamente invisible, aunque el resto (texto/formas
// vectoriales) se vea bien -- de ahi el reporte de "sigue en blanco y
// negro" incluso ya con los colores de fill/stroke funcionando OK.
//
// Busca TODAS las etiquetas <image> dentro de xmlFragment (a cualquier
// profundidad) y las compone DEBAJO de lo ya rasterizado en "pixels"
// (que ya tiene el contenido vectorial pintado encima, con huecos
// transparentes donde estaban las <image> que nanosvg se salteo).
// tx,ty,scale: misma transformacion usada para rasterizar el vector (de
// unidades SVG a pixeles de "pixels").
void CompositeEmbeddedImages(const std::string& xmlFragment,
                              std::vector<unsigned char>& pixels, int w, int h,
                              float tx, float ty, float scale) {
    size_t pos = 0;
    while (true) {
        size_t tagPos = xmlFragment.find("<image", pos);
        if (tagPos == std::string::npos) break;
        size_t tagEndSearch = xmlFragment.find('>', tagPos);
        if (tagEndSearch == std::string::npos) break;
        std::string tagMarkup = xmlFragment.substr(tagPos, tagEndSearch - tagPos + 1);
        pos = tagEndSearch + 1;

        std::string href;
        if (!ExtractAttr(tagMarkup, "href", href) && !ExtractAttr(tagMarkup, "xlink:href", href))
            continue;
        size_t dataPos = href.find("base64,");
        if (href.rfind("data:", 0) != 0 || dataPos == std::string::npos) continue; // solo data URI embebidos

        std::string xStr, yStr, wStr, hStr;
        float ix = ExtractAttr(tagMarkup, "x", xStr) ? (float)std::atof(xStr.c_str()) : 0.0f;
        float iy = ExtractAttr(tagMarkup, "y", yStr) ? (float)std::atof(yStr.c_str()) : 0.0f;
        if (!ExtractAttr(tagMarkup, "width", wStr) || !ExtractAttr(tagMarkup, "height", hStr)) continue;
        float iw = (float)std::atof(wStr.c_str());
        float ih = (float)std::atof(hStr.c_str());
        if (iw <= 0.0f || ih <= 0.0f) continue;

        std::vector<unsigned char> raw = Base64Decode(href.substr(dataPos + 7));
        int srcW = 0, srcH = 0, srcN = 0;
        unsigned char* srcPixels = stbi_load_from_memory(raw.data(), (int)raw.size(), &srcW, &srcH, &srcN, 4);
        if (!srcPixels) continue;

        int dstW = std::max(1, (int)std::lround(iw * scale));
        int dstH = std::max(1, (int)std::lround(ih * scale));
        std::vector<unsigned char> resized((size_t)dstW * dstH * 4);
        stbir_resize_uint8_linear(srcPixels, srcW, srcH, 0,
                                   resized.data(), dstW, dstH, 0, STBIR_RGBA);
        stbi_image_free(srcPixels);

        // Posicion en pixeles del buffer destino (misma transformacion que
        // nsvgRasterize: (svgUnit + t) * scale).
        int destX = (int)std::lround((ix + tx / scale) * scale);
        int destY = (int)std::lround((iy + ty / scale) * scale);

        for (int sy = 0; sy < dstH; sy++) {
            int py = destY + sy;
            if (py < 0 || py >= h) continue;
            for (int sx = 0; sx < dstW; sx++) {
                int px = destX + sx;
                if (px < 0 || px >= w) continue;

                unsigned char* photo  = &resized[((size_t)sy * dstW + sx) * 4];
                unsigned char* dstPix = &pixels[((size_t)py * w + px) * 4];

                // "Vector sobre foto": el pixel ya pintado (contenido
                // vectorial, puede estar vacio/transparente si ahi habia
                // justamente esta <image>) queda por encima; la foto se ve
                // donde el vector no cubre nada.
                float srcA = dstPix[3] / 255.0f;
                float phoA = photo[3] / 255.0f;
                float outA = srcA + phoA * (1.0f - srcA);
                if (outA <= 0.0001f) { dstPix[0]=dstPix[1]=dstPix[2]=dstPix[3]=0; continue; }
                for (int c = 0; c < 3; c++) {
                    float blended = (dstPix[c] * srcA + photo[c] * phoA * (1.0f - srcA)) / outA;
                    dstPix[c] = (unsigned char)std::clamp((int)std::lround(blended), 0, 255);
                }
                dstPix[3] = (unsigned char)std::clamp((int)std::lround(outA * 255.0f), 0, 255);
            }
        }
    }
}

// Bounding box (union) de todas las etiquetas <image> dentro de un
// fragmento -- nanosvg no genera shapes para ellas, asi que un grupo que
// sea SOLO una foto embebida (sin nada vectorial) necesita este calculo
// aparte para no quedar sin bbox valido y perderse del import.
bool FindImageBBoxUnion(const std::string& xmlFragment, float& minx, float& miny, float& maxx, float& maxy) {
    bool any = false;
    size_t pos = 0;
    while (true) {
        size_t tagPos = xmlFragment.find("<image", pos);
        if (tagPos == std::string::npos) break;
        size_t tagEnd = xmlFragment.find('>', tagPos);
        if (tagEnd == std::string::npos) break;
        std::string tagMarkup = xmlFragment.substr(tagPos, tagEnd - tagPos + 1);
        pos = tagEnd + 1;

        std::string xStr, yStr, wStr, hStr;
        float ix = ExtractAttr(tagMarkup, "x", xStr) ? (float)std::atof(xStr.c_str()) : 0.0f;
        float iy = ExtractAttr(tagMarkup, "y", yStr) ? (float)std::atof(yStr.c_str()) : 0.0f;
        if (!ExtractAttr(tagMarkup, "width", wStr) || !ExtractAttr(tagMarkup, "height", hStr)) continue;
        float iw = (float)std::atof(wStr.c_str()), ih = (float)std::atof(hStr.c_str());
        if (iw <= 0.0f || ih <= 0.0f) continue;

        minx = any ? std::min(minx, ix) : ix;
        miny = any ? std::min(miny, iy) : iy;
        maxx = any ? std::max(maxx, ix + iw) : ix + iw;
        maxy = any ? std::max(maxy, iy + ih) : iy + ih;
        any = true;
    }
    return any;
}

struct TopLevelChild { std::string tag; std::string markup; };

// Separa los hijos de PRIMER NIVEL dentro de "content" (el texto entre la
// etiqueta raiz <svg ...> y su </svg> de cierre) como bloques de markup
// completos -- esto es lo que realmente define "una capa" al importar
// (tipicamente cada <g id="..."> de Illustrator/Figma/Inkscape). No es un
// parser XML completo (no maneja CDATA anidado en casos raros ni
// namespaces), pero cubre bien lo que exportan esas herramientas.
//
// IMPORTANTE: no se puede agrupar usando NSVGshape::id (lo que se probo
// primero) porque nanosvg le asigna a cada shape el id de la etiqueta MAS
// especifica que lo define -- si el <path> tiene su propio id (como hacen
// Illustrator/Figma con CASI todos los paths), ese id pisa al del grupo
// contenedor, y cada path termina con un id distinto: agrupar por id daba
// una capa por PATH en vez de una por grupo (de ahi las "mil capas").
std::vector<TopLevelChild> SplitTopLevelChildren(const std::string& content) {
    std::vector<TopLevelChild> out;
    size_t i = 0;
    while (i < content.size()) {
        size_t lt = content.find('<', i);
        if (lt == std::string::npos) break;

        if (content.compare(lt, 4, "<!--") == 0) {
            size_t end = content.find("-->", lt);
            i = (end == std::string::npos) ? content.size() : end + 3;
            continue;
        }
        if (lt + 1 < content.size() && (content[lt + 1] == '?' || content[lt + 1] == '!')) {
            size_t end = content.find('>', lt);
            i = (end == std::string::npos) ? content.size() : end + 1;
            continue;
        }

        std::string tag = TagNameAt(content, lt);
        bool selfClosing = false;
        size_t tagEnd = ScanTagEnd(content, lt, selfClosing);

        if (selfClosing) {
            out.push_back({ tag, content.substr(lt, tagEnd - lt) });
            i = tagEnd;
            continue;
        }

        // Buscar el cierre correspondiente contando profundidad generica
        // (no hace falta que coincidan nombres de etiqueta: un documento
        // bien formado no se cruza).
        int depth = 1;
        size_t j = tagEnd;
        size_t blockEnd = content.size();
        while (j < content.size()) {
            size_t nlt = content.find('<', j);
            if (nlt == std::string::npos) break;

            if (content.compare(nlt, 4, "<!--") == 0) {
                size_t end = content.find("-->", nlt);
                j = (end == std::string::npos) ? content.size() : end + 3;
                continue;
            }
            if (nlt + 1 < content.size() && content[nlt + 1] == '/') {
                size_t end = content.find('>', nlt);
                j = (end == std::string::npos) ? content.size() : end + 1;
                depth--;
                if (depth == 0) { blockEnd = j; break; }
                continue;
            }
            if (nlt + 1 < content.size() && (content[nlt + 1] == '?' || content[nlt + 1] == '!')) {
                size_t end = content.find('>', nlt);
                j = (end == std::string::npos) ? content.size() : end + 1;
                continue;
            }

            bool innerSelfClosing = false;
            size_t innerEnd = ScanTagEnd(content, nlt, innerSelfClosing);
            if (!innerSelfClosing) depth++;
            j = innerEnd;
        }

        out.push_back({ tag, content.substr(lt, blockEnd - lt) });
        i = blockEnd;
    }
    return out;
}

} // namespace

std::vector<OverlayLayer> ImportSvgAsLayers(const std::string& svgPath,
                                             int canvasW, int canvasH,
                                             const std::string& outImagesDir) {
    std::vector<OverlayLayer> result;

    // Parseo de referencia SOLO para el tamano del canvas SVG completo (para
    // normalizar posicion/tamano de cada capa mas abajo) -- cada capa se
    // vuelve a parsear/rasterizar por separado, ver mas abajo.
    NSVGimage* refImage = nsvgParseFromFile(svgPath.c_str(), "px", 96.0f);
    if (!refImage || refImage->width <= 0.0f || refImage->height <= 0.0f) {
        if (refImage) nsvgDelete(refImage);
        return result;
    }
    float refW = refImage->width, refH = refImage->height;
    nsvgDelete(refImage);

    std::ifstream in(svgPath, std::ios::binary);
    if (!in) return result;
    std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    size_t svgTagStart = xml.find("<svg");
    if (svgTagStart == std::string::npos) return result;
    bool rootSelfClosing = false;
    size_t rootTagEnd = ScanTagEnd(xml, svgTagStart, rootSelfClosing);
    if (rootSelfClosing) return result; // svg vacio

    std::string rootOpenTag = xml.substr(svgTagStart, rootTagEnd - svgTagStart);

    size_t closeTag = xml.rfind("</svg>");
    if (closeTag == std::string::npos || closeTag < rootTagEnd) return result;
    std::string rootContent = xml.substr(rootTagEnd, closeTag - rootTagEnd);

    auto children = SplitTopLevelChildren(rootContent);
    if (children.empty()) return result;

    // <defs>/<style> de primer nivel se comparten con TODAS las capas
    // generadas (gradientes/clases CSS referenciadas desde cualquier grupo).
    std::string sharedPreamble;
    std::vector<TopLevelChild> layerChildren;
    for (auto& c : children) {
        if (c.tag == "defs" || c.tag == "style") sharedPreamble += c.markup;
        else if (c.tag == "title" || c.tag == "desc" || c.tag == "metadata") continue;
        else layerChildren.push_back(std::move(c));
    }
    if (layerChildren.empty()) return result;

    std::error_code ec;
    fs::create_directories(outImagesDir, ec);
    std::string baseStem = SanitizeStem(fs::path(svgPath).stem().string());
    NSVGrasterizer* rast = nsvgCreateRasterizer();

    // El SVG completo (refW x refH) se ubica en el canvas destino ocupando
    // como maximo el kImportFootprint de su dimension mas chica,
    // preservando su aspect ratio original y centrado -- asi un icono
    // importado no queda estirado a todo el canvas de 1920x1080 solo porque
    // su "mundo" SVG original era pequeño (ej. 64x64).
    constexpr float kImportFootprint = 0.6f;
    float svgAspect = refW / refH;
    float maxBoxPx  = kImportFootprint * std::min((float)canvasW, (float)canvasH);
    float dispW, dispH;
    if (svgAspect >= 1.0f) { dispW = maxBoxPx; dispH = maxBoxPx / svgAspect; }
    else                   { dispH = maxBoxPx; dispW = maxBoxPx * svgAspect; }
    float footX0 = ((float)canvasW - dispW) * 0.5f / (float)canvasW;
    float footY0 = ((float)canvasH - dispH) * 0.5f / (float)canvasH;
    float footW  = dispW / (float)canvasW;
    float footH  = dispH / (float)canvasH;

    // Nitidez: px SVG -> px de rasterizado, para que el PNG no se vea
    // borroso al escalarlo dentro del overlay (que trabaja a resoluciones
    // de 1920x1080 o mas).
    constexpr float kRasterScale = 3.0f;
    constexpr int   kMaxDim      = 4096;

    // ── Paso 1: bbox de cada hijo de primer nivel, SIN rasterizar todavia ──
    // Necesario para decidir que fusionar antes de generar ningun PNG (ver
    // paso 2). Herramientas como Canva exportan cada glifo/forma suelta
    // como su propio grupo de primer nivel sin jerarquia real de "capas de
    // diseño" -- sin este paso, cada glifo terminaria como su propia capa
    // (el bug reportado: "mil capas", texto fragmentado letra por letra).
    struct ChildBox { float minx = FLT_MAX, miny = FLT_MAX, maxx = -FLT_MAX, maxy = -FLT_MAX; bool valid = false; };
    std::vector<ChildBox> boxes(layerChildren.size());
    for (size_t idx = 0; idx < layerChildren.size(); idx++) {
        std::string miniSvg = rootOpenTag + sharedPreamble + layerChildren[idx].markup + "</svg>";
        NSVGimage* img = nsvgParse(miniSvg.data(), "px", 96.0f);
        if (!img) continue;
        ChildBox& b = boxes[idx];
        for (NSVGshape* s = img->shapes; s; s = s->next) {
            if (!(s->flags & NSVG_FLAGS_VISIBLE)) continue;
            b.minx = std::min(b.minx, s->bounds[0]);
            b.miny = std::min(b.miny, s->bounds[1]);
            b.maxx = std::max(b.maxx, s->bounds[2]);
            b.maxy = std::max(b.maxy, s->bounds[3]);
            b.valid = true;
        }
        nsvgDelete(img);
        if (b.valid && !(b.maxx > b.minx && b.maxy > b.miny)) b.valid = false;

        // <image> (foto/patron embebido) no genera shapes -- sin esto, un
        // grupo que sea SOLO una foto quedaria sin bbox y se perderia.
        float imgMinX, imgMinY, imgMaxX, imgMaxY;
        if (FindImageBBoxUnion(layerChildren[idx].markup, imgMinX, imgMinY, imgMaxX, imgMaxY)) {
            if (b.valid) {
                b.minx = std::min(b.minx, imgMinX); b.miny = std::min(b.miny, imgMinY);
                b.maxx = std::max(b.maxx, imgMaxX); b.maxy = std::max(b.maxy, imgMaxY);
            } else {
                b.minx = imgMinX; b.miny = imgMinY; b.maxx = imgMaxX; b.maxy = imgMaxY;
                b.valid = true;
            }
        }
    }

    // ── Paso 2: agrupar por cercania espacial los elementos "chicos" ──────
    // (candidatos a fragmentos de texto -- glifos sueltos, sombras/efectos
    // de un mismo diseño) cuyas cajas, agrandadas por un margen, se tocan:
    // asi un renglon de letras sueltas se une en UNA capa de texto en vez
    // de una por letra. Los elementos grandes (fondo, formas ya agrupadas,
    // logos) NO se tocan -- cada uno queda en su propia capa, para que
    // texto/fondo/entre distintos textos no se mezclen entre si.
    constexpr float kSmallHeightFrac   = 0.12f; // alto relativo al canvas -> candidato a fragmento de texto
    constexpr float kClusterMarginFrac = 0.02f; // margen de union, relativo a la diagonal del canvas
    float canvasDiag    = std::sqrt(refW * refW + refH * refH);
    float clusterMargin = canvasDiag * kClusterMarginFrac;

    std::vector<int> parent(boxes.size());
    for (size_t i = 0; i < parent.size(); i++) parent[i] = (int)i;
    std::function<int(int)> Find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto Unite = [&](int a, int b) { a = Find(a); b = Find(b); if (a != b) parent[a] = b; };

    for (size_t i = 0; i < boxes.size(); i++) {
        if (!boxes[i].valid || (boxes[i].maxy - boxes[i].miny) > refH * kSmallHeightFrac) continue;
        for (size_t j = i + 1; j < boxes.size(); j++) {
            if (!boxes[j].valid || (boxes[j].maxy - boxes[j].miny) > refH * kSmallHeightFrac) continue;
            bool disjoint = boxes[i].maxx + clusterMargin < boxes[j].minx ||
                            boxes[j].maxx + clusterMargin < boxes[i].minx ||
                            boxes[i].maxy + clusterMargin < boxes[j].miny ||
                            boxes[j].maxy + clusterMargin < boxes[i].miny;
            if (!disjoint) Unite((int)i, (int)j);
        }
    }

    // Clusters en orden de aparicion (por el menor indice original de cada
    // grupo), para conservar el orden de pintado del SVG original.
    std::vector<std::vector<size_t>> clusters;
    {
        std::unordered_map<int, size_t> rootToCluster;
        for (size_t i = 0; i < boxes.size(); i++) {
            if (!boxes[i].valid) continue;
            int root = Find((int)i);
            auto it = rootToCluster.find(root);
            if (it == rootToCluster.end()) {
                rootToCluster[root] = clusters.size();
                clusters.push_back({ i });
            } else {
                clusters[it->second].push_back(i);
            }
        }
    }

    // ── Paso 3: un PNG + OverlayLayer por cluster ─────────────────────────
    int layerIdx = 0;
    for (auto& cluster : clusters) {
        float minx = FLT_MAX, miny = FLT_MAX, maxx = -FLT_MAX, maxy = -FLT_MAX;
        std::string mergedMarkup;
        for (size_t idx : cluster) {
            minx = std::min(minx, boxes[idx].minx);
            miny = std::min(miny, boxes[idx].miny);
            maxx = std::max(maxx, boxes[idx].maxx);
            maxy = std::max(maxy, boxes[idx].maxy);
            mergedMarkup += layerChildren[idx].markup;
        }

        std::string miniSvg = rootOpenTag + sharedPreamble + mergedMarkup + "</svg>";
        NSVGimage* img = nsvgParse(miniSvg.data(), "px", 96.0f);
        if (!img) { layerIdx++; continue; }

        int w = std::clamp((int)std::ceil((maxx - minx) * kRasterScale), 1, kMaxDim);
        int h = std::clamp((int)std::ceil((maxy - miny) * kRasterScale), 1, kMaxDim);

        std::vector<unsigned char> pixels((size_t)w * h * 4, 0);
        nsvgRasterize(rast, img, -minx * kRasterScale, -miny * kRasterScale,
                      kRasterScale, pixels.data(), w, h, w * 4);
        nsvgDelete(img);
        CompositeEmbeddedImages(mergedMarkup, pixels, w, h,
                                 -minx * kRasterScale, -miny * kRasterScale, kRasterScale);

        std::string idAttr = ExtractIdAttr(layerChildren[cluster[0]].markup);
        std::string label  = !idAttr.empty() ? SanitizeStem(idAttr) : layerChildren[cluster[0]].tag;
        fs::path pngPath = fs::path(outImagesDir) /
            (baseStem + "_" + label + "_" + std::to_string(layerIdx) + ".png");

        if (stbi_write_png(pngPath.string().c_str(), w, h, 4, pixels.data(), w * 4)) {
            float fx0 = minx / refW, fx1 = maxx / refW;
            float fy0 = miny / refH, fy1 = maxy / refH;

            OverlayLayer layer;
            layer.kind      = OverlayLayerKind::Image;
            layer.imagePath = pngPath.string();
            layer.sizeW     = (fx1 - fx0) * footW;
            layer.sizeH     = (fy1 - fy0) * footH;
            layer.posX      = footX0 + ((fx0 + fx1) * 0.5f) * footW;
            layer.posY      = footY0 + ((fy0 + fy1) * 0.5f) * footH;
            result.push_back(layer);
        }

        layerIdx++;
    }

    nsvgDeleteRasterizer(rast);
    return result;
}

OverlayLayer ImportSvgAsSingleImage(const std::string& svgPath,
                                     int canvasW, int canvasH,
                                     const std::string& outImagesDir) {
    OverlayLayer layer; // imagePath vacio = fallo, ver comentario en el header

    NSVGimage* image = nsvgParseFromFile(svgPath.c_str(), "px", 96.0f);
    if (!image || image->width <= 0.0f || image->height <= 0.0f) {
        if (image) nsvgDelete(image);
        return layer;
    }

    std::error_code ec;
    fs::create_directories(outImagesDir, ec);

    constexpr float kRasterScale = 3.0f;
    constexpr int   kMaxDim      = 4096;
    int w = std::clamp((int)std::ceil(image->width  * kRasterScale), 1, kMaxDim);
    int h = std::clamp((int)std::ceil(image->height * kRasterScale), 1, kMaxDim);

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    std::vector<unsigned char> pixels((size_t)w * h * 4, 0);
    nsvgRasterize(rast, image, 0.0f, 0.0f, kRasterScale, pixels.data(), w, h, w * 4);
    nsvgDeleteRasterizer(rast);

    // nanosvg no dibuja <image> (fotos/patrones embebidos, muy comunes como
    // fondo en exports de Canva) -- se componen aparte, ver comentario en
    // CompositeEmbeddedImages.
    {
        std::ifstream in(svgPath, std::ios::binary);
        if (in) {
            std::string xml((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CompositeEmbeddedImages(xml, pixels, w, h, 0.0f, 0.0f, kRasterScale);
        }
    }

    std::string baseStem = SanitizeStem(fs::path(svgPath).stem().string());
    fs::path pngPath = fs::path(outImagesDir) / (baseStem + "_completo.png");

    if (stbi_write_png(pngPath.string().c_str(), w, h, 4, pixels.data(), w * 4)) {
        // Mismo footprint (ocupa como maximo kImportFootprint del canvas, en
        // su dimension mas chica, preservando aspect ratio) que usa
        // ImportSvgAsLayers, para que ambos modos de import se vean
        // consistentes en tamano.
        constexpr float kImportFootprint = 0.6f;
        float svgAspect = image->width / image->height;
        float maxBoxPx  = kImportFootprint * std::min((float)canvasW, (float)canvasH);
        float dispW, dispH;
        if (svgAspect >= 1.0f) { dispW = maxBoxPx; dispH = maxBoxPx / svgAspect; }
        else                   { dispH = maxBoxPx; dispW = maxBoxPx * svgAspect; }

        layer.kind      = OverlayLayerKind::Image;
        layer.imagePath = pngPath.string();
        layer.sizeW     = dispW / (float)canvasW;
        layer.sizeH     = dispH / (float)canvasH;
        layer.posX      = 0.5f;
        layer.posY      = 0.5f;
    }

    nsvgDelete(image);
    return layer;
}

} // namespace ProyecThor::UI
