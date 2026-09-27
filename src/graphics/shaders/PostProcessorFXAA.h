#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// FXAA (fast approximate anti-aliasing), preset unico estandar sin
// parametros de usuario — mismo criterio que la mayoria de motores/juegos
// que lo exponen como un simple on/off. Filtro 1:1 sobre el composite
// completo del proyector (ver CompositePostChain).
class PostProcessorFXAA {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    GLuint Process(GLuint srcTex, int w, int h);

private:
    PostProcessSinglePass m_Pipeline;
    bool m_Enabled = false;
};

} // namespace ProyecThor::Shaders
