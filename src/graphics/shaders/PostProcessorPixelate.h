#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorPixelate {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetPixelSize(float v);
    float GetPixelSize() const { return m_PixelSize; }

    void SetColorDepth(int depth);
    int GetColorDepth() const { return m_ColorDepth; }

    GLuint Process(GLuint srcTex, int w, int h);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled    = false;
    float m_PixelSize  = 12.0f; // 2.0 a 48.0
    int   m_ColorDepth = 0;     // 0=Color Real, 1=16-Bit, 2=8-Bit Retro
};

} // namespace ProyecThor::Shaders

