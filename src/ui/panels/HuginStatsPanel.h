#pragma once

namespace ProyecThor::UI {

class HuginStatsPanel {
public:
    HuginStatsPanel() = default;
    ~HuginStatsPanel() = default;

    void Render(bool* pOpen);

private:
    int m_SelectedSection = 0; // 0: Resumen General, 1: Top Canciones, 2: Rendimiento FPS, 3: Sesión Activa
    bool m_ShowResetConfirm = false;
};

} // namespace ProyecThor::UI
