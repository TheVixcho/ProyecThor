#pragma once
#include <string>

namespace ProyecThor::Core {

// ── SystemStats ────────────────────────────────────────────────────────────
// Uso de CPU y RAM de TODA la maquina (no solo este proceso) — para el
// panel de Rendimiento (ver PerformancePanel), pensado como diagnostico de
// "esta la PC sobrecargada", no de cuanto gasta la app puntualmente.
//
// Meyer singleton, mismo estilo que PerformanceGovernor. Update() se llama
// una vez por frame desde main.cpp, pero la lectura real del SO se throttlea
// internamente a ~1 vez por segundo — nunca se parsea /proc o se llama a
// las APIs de Windows en cada uno de los 60 frames/seg.
class SystemStats {
public:
    static SystemStats& Get() {
        static SystemStats instance;
        return instance;
    }

    SystemStats(const SystemStats&)            = delete;
    SystemStats& operator=(const SystemStats&) = delete;

    void Update();

    float CpuPercent() const { return m_CpuPercent; }
    float RamUsedMB()  const { return m_RamUsedMB; }
    float RamTotalMB() const { return m_RamTotalMB; }

    // Nombre de la placa via glGetString(GL_RENDERER) — se lee y cachea una
    // sola vez (no cambia en runtime). Vacio hasta el primer Update() con
    // contexto GL activo.
    const std::string& GpuName() const { return m_GpuName; }

    // GL_VENDOR contiene "NVIDIA" (con fallback a buscarlo en GL_RENDERER
    // por si algun driver raro no lo repite en vendor) -- usado por
    // ShadersPanel para mostrar el escalador NIS solo en placas NVIDIA
    // reales (ver PostProcessorNIS.h). Cacheado junto con m_GpuName.
    bool IsNvidiaGpu() const { return m_IsNvidiaGpu; }

private:
    SystemStats() = default;

    void SampleCpuAndRam();
    void EnsureGpuName();

    double m_LastSampleTime = 0.0; // glfwGetTime() de la ultima muestra real

    float m_CpuPercent = 0.0f;
    float m_RamUsedMB  = 0.0f;
    float m_RamTotalMB = 0.0f;
    std::string m_GpuName;
    bool        m_IsNvidiaGpu = false;

    // Contadores crudos de la muestra anterior, para calcular el delta de
    // uso de CPU entre dos lecturas (un snapshot puntual de los contadores
    // acumulados del SO no dice nada por si solo).
#ifdef _WIN32
    unsigned long long m_PrevIdle = 0, m_PrevKernel = 0, m_PrevUser = 0;
#else
    unsigned long long m_PrevIdle = 0, m_PrevTotal = 0;
#endif
    bool m_HasPrevCpuSample = false;
};

} // namespace ProyecThor::Core
