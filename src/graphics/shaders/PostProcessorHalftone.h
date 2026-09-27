#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorHalftone {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetDotScale(float v);
    float GetDotScale() const { return m_DotScale; }

    void SetMode(int mode);
    int GetMode() const { return m_Mode; }

    GLuint Process(GLuint srcTex, int w, int h);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled  = false;
    float m_DotScale = 10.0f; // 4.0 a 30.0
    int   m_Mode     = 0;     // 0=Color Pop-Art, 1=Monocromo Blanco y Negro, 2=Periódico Retro
};

} // namespace ProyecThor::Shaders

