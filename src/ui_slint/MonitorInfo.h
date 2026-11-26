#pragma once

#include <string>
#include <vector>

namespace ProyecThor::SlintUI {

// Geometría de un monitor físico en coordenadas globales del escritorio.
// x/y es la esquina superior izquierda del monitor dentro del espacio virtual
// (p. ej. el monitor secundario a la derecha del principal tendría x = ancho
// del principal). Se usa para posicionar la ventana de proyección sobre el
// monitor correcto antes de ponerla en pantalla completa.
struct MonitorInfo {
    int         x = 0;
    int         y = 0;
    int         width = 0;
    int         height = 0;
    bool        isPrimary = false;
    std::string name;
};

// Enumera los monitores conectados. Devuelve una lista vacía si no se puede
// detectar (p. ej. Wayland puro sin XWayland); en ese caso la proyección usa
// el comportamiento por defecto del compositor.
//
// El orden es estable: el monitor primario primero cuando se puede determinar.
std::vector<MonitorInfo> EnumerateMonitors();

// Índice del monitor recomendado por defecto para proyectar: el secundario si
// hay más de uno, o el único disponible. Devuelve 0 si no hay información.
int DefaultProjectionMonitorIndex(const std::vector<MonitorInfo>& monitors);

} // namespace ProyecThor::SlintUI
