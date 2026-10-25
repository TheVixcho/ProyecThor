#pragma once

namespace ProyecThor::Core {

// ── PerformanceGovernor ───────────────────────────────────────────────────────
// Mide el frame time real (EMA continua, alimentada desde main.cpp junto al
// FrameProfiler existente) y sugiere recortar la resolucion del video de
// fondo cuando el rendimiento sostenido cae por debajo del presupuesto de
// frame — para que una degradacion (si hace falta en hardware debil) sea
// gradual y controlada en vez de que el publico vea stutter crudo.
//
// Standalone (no vive dentro de PresentationCore) para no acoplar medicion
// de rendimiento con estado de reproduccion. Solo SUGIERE un recorte: nunca
// pisa una eleccion manual del usuario — quien lo consulte debe aplicar
// ApplyCap() unicamente cuando OutputQualityMode == Auto.
class PerformanceGovernor {
public:
    static PerformanceGovernor& Get() {
        static PerformanceGovernor instance;
        return instance;
    }

    PerformanceGovernor(const PerformanceGovernor&)            = delete;
    PerformanceGovernor& operator=(const PerformanceGovernor&) = delete;

    // Llamar una vez por frame desde main.cpp con el tiempo total del frame
    // (ms, el mismo que ya alimenta FrameProfiler::s_FrameTotal). Actualiza
    // la EMA interna y, con histeresis temporal, sube o baja de nivel.
    void ReportFrame(double frameMs);

    // Nivel de degradacion actual: 0 = calidad completa (sin recorte).
    int Level() const { return m_Level; }

    // EMA de frame time (ms) / FPS derivado — usado por el panel de
    // Rendimiento (ver PerformancePanel). 0.0 antes de la primera muestra.
    double EmaFrameMs() const { return m_HasEma ? m_EmaFrameMs : 0.0; }
    double Fps() const { return (m_HasEma && m_EmaFrameMs > 0.0) ? (1000.0 / m_EmaFrameMs) : 0.0; }

    // Recorta w/h in-place al tope del nivel actual (preservando aspect
    // ratio). No hace nada si el nivel es 0 o si w/h ya son mas chicos que
    // el tope — nunca "sube" la resolucion por encima de lo ya resuelto por
    // Settings::ResolveQualityTarget.
    void ApplyCap(int& w, int& h) const;

private:
    PerformanceGovernor() = default;

    double m_EmaFrameMs      = 0.0;
    bool   m_HasEma          = false;
    int    m_Level           = 0;

    // Segundos acumulados sostenidos por encima/debajo de cada umbral.
    double m_TimeOverBudget  = 0.0;
    double m_TimeUnderBudget = 0.0;
};

} // namespace ProyecThor::Core
