#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Luminosidad/brillo sobre el composite completo (ver CompositePostChain).
// Filtro 1:1, mismo patron que PostProcessorSaturation/Contrast.
class PostProcessorLuminosity {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = negro, 1 = normal, 2 = doble de brillo. Default 1.2.
    void SetAmount(float v);
    float GetAmount() const { return m_Amount; }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled = false;
    float m_Amount  = 1.2f;
};

} // namespace ProyecThor::Shaders
