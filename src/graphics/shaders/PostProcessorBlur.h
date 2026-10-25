#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Desenfoque gaussiano sobre el composite completo (ver CompositePostChain).
// Separable en 2 pases (horizontal + vertical, cada uno un
// PostProcessSinglePass) en vez de un kernel 2D de un solo pase -- mismo
// costo visual, mucho mas barato en muestras de textura (2*N en vez de N^2).
class PostProcessorBlur {
public:
    bool Init(int w, int h);
    void Destroy();
    void Resize(int w, int h);
    void ForgetGLResources();
    bool IsInitialized() const { return m_PipelineH.IsInitialized() && m_PipelineV.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = casi sin efecto, 1 = desenfoque fuerte. Default 0.35.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_PipelineH;
    PostProcessSinglePass m_PipelineV;
    bool  m_Enabled   = false;
    float m_Intensity = 0.35f;
    int   m_W = 0, m_H = 0;
};

} // namespace ProyecThor::Shaders
