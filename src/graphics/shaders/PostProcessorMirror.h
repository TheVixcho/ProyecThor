#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorMirror {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetMode(int mode);
    int GetMode() const { return m_Mode; }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_Pipeline;
    bool m_Enabled = false;
    int  m_Mode    = 0; // 0=Horizontal, 1=Vertical, 2=Caleidoscopio 4x, 3=Caleidoscopio 8x
};

} // namespace ProyecThor::Shaders

