#pragma once
#include <string>
#include <vector>

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayLayerKind — de que tipo es una capa dentro del canvas.
// ─────────────────────────────────────────────────────────────────────────────
// Clock: cuadro-flag para el reloj/contador en vivo (ver OClock). No lleva
// contenido propio horneado -- solo posicion/estilo (reusa los mismos campos
// de texto de abajo); el texto real se dibuja en vivo sobre el overlay
// activo, nunca en el PNG exportado (ver OverlayCanvasEditor::RenderCanvas).
enum class OverlayLayerKind { Text, Image, Shape, Clock };

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayShapeKind — variante de forma para capas Shape.
// ─────────────────────────────────────────────────────────────────────────────
enum class OverlayShapeKind { Rectangle, Ellipse };

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayLayer — una capa (texto, imagen o forma) dentro de un Overlay
//  (canvas tipo Canva). La posicion es el CENTRO de la capa, normalizada
//  0..1 respecto al canvas (asi el overlay se ve igual sin importar la
//  resolucion de exportacion). Para imagenes/formas, sizeW/sizeH tambien son
//  fracciones 0..1 del canvas.
// ─────────────────────────────────────────────────────────────────────────────
struct OverlayLayer {
    OverlayLayerKind kind = OverlayLayerKind::Text;

    // -- Texto --
    std::string text     = "Texto";
    std::string fontName = "Predeterminada";
    float       fontSize = 72.0f;          // en px, referido a un canvas 1920x1080
    float       color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    bool  shadowEnabled  = true;           // sombra suave (look por defecto, ya existia)
    float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.63f };
    float shadowOffsetX  = 1.5f;           // px, referido a un canvas 1920x1080
    float shadowOffsetY  = 1.5f;

    bool  outlineEnabled  = false;         // contorno (stroke) de texto; para
                                            // formas dobla como su borde (ver abajo)
    float outlineColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float outlineWidth    = 2.0f;          // px, referido a un canvas 1920x1080

    bool  bgEnabled   = false;             // fondo de color detras del texto
    float bgColor[4]  = { 0.0f, 0.0f, 0.0f, 0.5f };
    float bgPaddingX  = 10.0f;             // px, referido a un canvas 1920x1080
    float bgPaddingY  = 6.0f;
    float bgRounding  = 6.0f;

    // -- Imagen --
    std::string imagePath;                 // ruta absoluta al archivo de imagen

    // -- Forma (Shape) --
    OverlayShapeKind shapeKind      = OverlayShapeKind::Rectangle;
    bool             shapeFilled    = true;   // si no, solo el borde (outlineColor/outlineWidth)
    float            shapeRounding  = 0.0f;   // solo Rectangle, px a 1920x1080

    // -- Comun a Imagen/Forma --
    float       sizeW = 0.3f;              // 0..1, ancho respecto al canvas
    float       sizeH = 0.3f;              // 0..1, alto respecto al canvas
    float       rotation = 0.0f;           // grados, sentido horario

    // -- Comun a todas --
    float posX = 0.5f;                     // 0..1, centro de la capa
    float posY = 0.5f;                     // 0..1, centro de la capa
    float opacity = 1.0f;                  // 0..1, multiplica TODO lo que dibuje la capa
                                            // (independiente del alpha de cada color propio
                                            // -- ej. desvanecer un texto entero con sombra+
                                            // contorno+fondo de un solo control, en vez de
                                            // ajustar cada color por separado).
};

// ─────────────────────────────────────────────────────────────────────────────
//  OverlayDoc — la "receta" editable de un Overlay guardado (junto al .png
//  ya rasterizado que se usa para proyectar).
// ─────────────────────────────────────────────────────────────────────────────
struct OverlayDoc {
    int   canvasW = 1920;
    int   canvasH = 1080;
    float bgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // overlay = sin fondo por defecto (PNG transparente)
    std::vector<OverlayLayer> layers;
};

// Primer layer de tipo Clock dentro del doc, o nullptr si no tiene ninguno.
// Solo se permite uno por overlay (ver OverlayCanvasEditor::RenderFloatingToolbar).
inline const OverlayLayer* FindClockLayer(const OverlayDoc& doc) {
    for (const auto& l : doc.layers)
        if (l.kind == OverlayLayerKind::Clock) return &l;
    return nullptr;
}

} // namespace ProyecThor::UI
