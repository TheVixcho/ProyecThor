#include "PerformanceGovernor.h"
#include <algorithm>
#include <iostream>

namespace ProyecThor::Core {

namespace {
    constexpr double kEmaAlpha            = 0.05;  // suaviza picos puntuales
    constexpr double kFrameBudgetMs       = 20.0;  // ~50 FPS: umbral de "va lento"
    constexpr double kUpgradeThresholdMs  = kFrameBudgetMs * 0.7; // 14ms: umbral de "sobra margen"
    constexpr double kSustainToDowngradeS = 3.0;   // bajar rapido si se pone lento
    constexpr double kSustainToUpgradeS   = 20.0;  // subir solo tras un buen rato estable (evita flapping)
    constexpr int    kMaxLevel            = 3;

    struct Cap { int w, h; };
    constexpr Cap kCaps[kMaxLevel] = {
        { 1280, 720 },
        {  960, 540 },
        {  640, 360 },
    };
}

void PerformanceGovernor::ReportFrame(double frameMs)
{
    if (frameMs <= 0.0)
        return;

    if (!m_HasEma) {
        m_EmaFrameMs = frameMs;
        m_HasEma     = true;
        return;
    }
    m_EmaFrameMs += (frameMs - m_EmaFrameMs) * kEmaAlpha;

    const double dtSeconds = frameMs / 1000.0;

    if (m_EmaFrameMs > kFrameBudgetMs) {
        m_TimeOverBudget  += dtSeconds;
        m_TimeUnderBudget  = 0.0;
    } else if (m_EmaFrameMs < kUpgradeThresholdMs) {
        m_TimeUnderBudget += dtSeconds;
        m_TimeOverBudget   = 0.0;
    } else {
        // Zona intermedia (entre los dos umbrales): no acumula hacia
        // ningun lado, para no oscilar justo en el borde.
        m_TimeOverBudget  = 0.0;
        m_TimeUnderBudget = 0.0;
    }

    if (m_TimeOverBudget >= kSustainToDowngradeS && m_Level < kMaxLevel) {
        ++m_Level;
        m_TimeOverBudget = 0.0;
        std::cerr << "[PerfGovernor] Rendimiento sostenido bajo el presupuesto ("
                  << m_EmaFrameMs << "ms/frame) -> bajando calidad a nivel " << m_Level << ".\n";
    } else if (m_TimeUnderBudget >= kSustainToUpgradeS && m_Level > 0) {
        --m_Level;
        m_TimeUnderBudget = 0.0;
        std::cerr << "[PerfGovernor] Rendimiento estable (" << m_EmaFrameMs
                  << "ms/frame) -> subiendo calidad a nivel " << m_Level << ".\n";
    }
}

void PerformanceGovernor::ApplyCap(int& w, int& h) const
{
    if (m_Level <= 0 || w <= 0 || h <= 0)
        return;

    const Cap& cap = kCaps[m_Level - 1];
    if (w <= cap.w && h <= cap.h)
        return; // ya esta por debajo del tope: no hay nada que recortar

    // Recorta por el eje mas restrictivo, preservando aspect ratio (evita
    // distorsion en pantallas ultrawide o verticales).
    const double scale = std::min(static_cast<double>(cap.w) / w, static_cast<double>(cap.h) / h);
    w = std::max(160, static_cast<int>(w * scale));
    h = std::max(90,  static_cast<int>(h * scale));
}

} // namespace ProyecThor::Core
