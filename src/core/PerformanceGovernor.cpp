#include "PerformanceGovernor.h"
#include "core/settings/SettingsManager.h"
#include <algorithm>
#include <iostream>

namespace ProyecThor::Core {

namespace {
    constexpr double kEmaAlpha            = 0.05;  // suaviza picos puntuales
    constexpr double kFrameBudgetMs       = 33.3;  // ~30 FPS: presupuesto objetivo para hardware debil
    constexpr double kUpgradeThresholdMs  = kFrameBudgetMs * 0.6; // ~20ms: hay que ir claramente mas rapido para subir de nuevo
    constexpr double kSustainToDowngradeS = 2.0;   // bajar rapido si se pone lento
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
    const int targetFps = Settings::SettingsManager::Get().GetSettings().performance.targetFPS;
    const double budgetMs = (targetFps > 0)
        ? std::max(kFrameBudgetMs, (1000.0 / static_cast<double>(targetFps)) * 1.18)
        : kFrameBudgetMs;
    const double upgradeMs = budgetMs * 0.6;

    if (m_EmaFrameMs > budgetMs) {
        m_TimeOverBudget  += dtSeconds;
        m_TimeUnderBudget  = 0.0;
    } else if (m_EmaFrameMs < upgradeMs) {
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
