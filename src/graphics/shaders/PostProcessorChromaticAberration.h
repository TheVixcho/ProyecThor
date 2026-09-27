#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Aberracion cromatica (desfase radial de los canales R/G/B, mas marcado
// hacia los bordes) sobre el composite completo (ver CompositePostChain).
// Filtro 1:1, mismo patron que PostProcessorVignette.
class PostProcessorChromaticAberration {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin efecto, 1 = desfase fuerte. Default 0.35.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.35f;
};

} // namespace ProyecThor::Shaders
