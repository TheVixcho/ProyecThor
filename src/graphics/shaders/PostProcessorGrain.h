#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Grano de pelicula: ruido procedural animado superpuesto sobre el
// composite completo del proyector (ver CompositePostChain). Filtro 1:1.
class PostProcessorGrain {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin ruido, 1 = ruido muy marcado. Default 0.15.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    // timeSeconds: reloj monotonico (ej. glfwGetTime()) para animar el
    // ruido frame a frame, sin lo cual se veria como una mancha estatica.
    GLuint Process(GLuint srcTex, double timeSeconds);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.15f;
};

} // namespace ProyecThor::Shaders
