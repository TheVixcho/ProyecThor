#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Vinetado (oscurecido de bordes) sobre el composite completo del proyector
// (ver CompositePostChain). Filtro 1:1, mismo patron que PostProcessorGrain.
// Independiente del vinetado fijo/interno que ya trae PostProcessorCRT (ese
// solo se ve si el modo CRT esta activo) -- este se puede prender solo,
// sin scanlines ni curvatura.
class PostProcessorVignette {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin efecto, 1 = bordes muy oscurecidos. Default 0.45.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.45f;
};

} // namespace ProyecThor::Shaders
