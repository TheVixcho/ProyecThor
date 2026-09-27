#pragma once
#include <imgui.h>

namespace ProyecThor::UI {

using DrawIconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

struct IconRailItem {
    int         index;     // valor logico a asignar a currentIndex al hacer click
    DrawIconFn  drawIcon;
    const char* label;
};

enum class IconRailOrientation {
    Vertical,   // columna apilada — usado tanto para rail izquierdo como derecho
                // (el orden de layout del panel que lo llama decide el lado)
    Horizontal, // fila — usado para barras arriba (ej. Home)
};

// Ancho fijo de columna (Vertical) / alto fijo de fila (Horizontal).
// Tamaños compactos tipo toolbar (Holyrics/ProPresenter) — no tarjetas grandes.
inline constexpr float kIconRailVerticalSize    = 56.0f;
inline constexpr float kIconRailHorizontalSize  = 50.0f;
inline constexpr float kIconRailHorizontalItemW = 64.0f;

// Variantes "solo icono" (sin título) — usadas cuando el usuario apaga
// Vista > Titulos en barras de iconos, para ocupar aun menos espacio.
inline constexpr float kIconRailVerticalSizeCompact   = 34.0f;
inline constexpr float kIconRailHorizontalSizeCompact = 30.0f;

// Grosor animado del rail (ancho si es vertical, alto si es horizontal),
// leyendo Settings.general.showRailLabels y suavizando la transicion entre
// el tamaño completo y el compacto. Los 4 rails (Biblioteca/Home/Control/
// Diseño) deben pedir su tamaño de contenedor con esta funcion en vez de
// usar las constantes de arriba directamente, para que la opcion de
// Vista > Titulos los afecte a todos por igual.
float IconRailThickness(bool vertical);

// Dibuja el rail completo (fondo, hover, barra de seleccion, icono+label) y
// actualiza currentIndex al click. categoryColor debe tener exactamente
// `count` entradas, mismo orden que items. Debe llamarse ya posicionado
// dentro del child/ventana que le corresponde (mismo patron que
// LibrarySidebar::RenderCategoryButtons / HomeSidebar::RenderHomeSidebar).
void RenderIconRail(const IconRailItem* items, int count, int& currentIndex,
                     IconRailOrientation orientation, const float (*categoryColor)[4]);

} // namespace ProyecThor::UI
