#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Efecto VHS: sangrado de color horizontal (crosstalk de cabeza
// desalineada), scanlines, bamboleo/jitter vertical, una banda de
// "tracking" que se desliza y glitchea el muestreo, ruido de estatica, y
// un ligero cast verdoso/desaturado tipico de cinta vieja. Sobre el
// composite completo (ver CompositePostChain). Filtro 1:1, mismo patron
// que PostProcessorGrain (necesita el reloj para animarse).
class PostProcessorVHS {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = casi limpio, 1 = VHS bien maltratado. Default 0.5.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    // timeSeconds: reloj monotonico (ej. glfwGetTime()) para animar el
    // bamboleo/tracking/ruido frame a frame.
    GLuint Process(GLuint srcTex, int w, int h, double timeSeconds);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.5f;
};

} // namespace ProyecThor::Shaders
