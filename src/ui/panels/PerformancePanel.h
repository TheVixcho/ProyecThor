#pragma once

namespace ProyecThor::UI {

// ── PerformancePanel ─────────────────────────────────────────────────────────
// Ventana de diagnostico opcional (menu Vista > Rendimiento): FPS, CPU, RAM
// y nombre de GPU en vivo. Sin estado propio — solo lee PerformanceGovernor
// y SystemStats (ambos ya actualizados una vez por frame desde main.cpp) y
// los muestra como texto simple.
class PerformancePanel {
public:
    void Render(bool* isOpen);
};

} // namespace ProyecThor::UI
