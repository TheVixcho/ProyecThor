#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Emulacion de TV/monitor CRT: scanlines + curvatura leve + vineta. Filtro
// 1:1 (misma resolucion in/out) sobre el composite completo del proyector
// (ver CompositePostChain) — a diferencia de FSR, no hace upscale.
class PostProcessorCRT {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin scanlines, 1 = maximo oscurecimiento entre lineas. Default 0.5.
    void SetScanlineIntensity(float v);
    float GetScanlineIntensity() const { return m_ScanlineIntensity; }

    // Devuelve srcTex sin modificar si esta deshabilitado o no inicializado.
    GLuint Process(GLuint srcTex, int w, int h);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled            = false;
    float m_ScanlineIntensity  = 0.5f;
    float m_Curvature          = 0.05f;  // fijo v1, ver plan
    float m_VignetteIntensity  = 0.35f;  // fijo v1, ver plan
};

} // namespace ProyecThor::Shaders
