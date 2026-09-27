#pragma once
#include "OverlayTypes.h"
#include <string>
#include <vector>

namespace ProyecThor::UI {

// Importa un SVG como VARIAS capas Image, una por cada grupo <g id="..."> de
// primer nivel (o por cada forma suelta sin grupo) -- pensado para traer
// archivos ya diagramados en otros programas (Illustrator/Figma/Inkscape,
// que exportan cada capa como su propio grupo con id) y poder seguir
// moviendo/editando cada parte por separado en el editor de Overlays, en
// vez de importar el SVG entero como una sola imagen plana.
//
// Cada grupo se rasteriza a su propio PNG con transparencia (nanosvg no
// soporta filtros/gradientes CSS avanzados, pero cubre fill/stroke/opacidad/
// paths/formas basicas, que es la mayoria de lo que exportan esas apps).
//
// outImagesDir: carpeta donde se escriben los PNG generados (uno por capa).
// Devuelve un layer por grupo, en el mismo orden de pintado del SVG original
// (el primero va mas atras) listos para agregar a OverlayDoc::layers,
// posicionados/escalados para llenar proporcionalmente un canvas de
// canvasW x canvasH (el mismo tamano que target del overlay activo).
// Devuelve vacio si el archivo no pudo leerse/parsearse.
std::vector<OverlayLayer> ImportSvgAsLayers(const std::string& svgPath,
                                             int canvasW, int canvasH,
                                             const std::string& outImagesDir);

// Importa el SVG entero como UNA sola capa Image (todo rasterizado junto,
// tal cual se veria en cualquier visor). Pensado para archivos que no
// agrupan de forma util para separar en capas -- en la practica, Canva
// exporta cada forma/glifo suelto como su propio grupo de primer nivel sin
// jerarquia real de "capas de diseño", así que ImportSvgAsLayers termina
// fragmentando en decenas de pedazos irreconocibles. Esta funcion es el
// resultado seguro/garantizado: se ve exactamente igual que el archivo
// original, a costa de no poder mover/editar cada parte por separado.
// Devuelve un layer "vacio" (imagePath="") si no se pudo leer/rasterizar.
OverlayLayer ImportSvgAsSingleImage(const std::string& svgPath,
                                     int canvasW, int canvasH,
                                     const std::string& outImagesDir);

} // namespace ProyecThor::UI
