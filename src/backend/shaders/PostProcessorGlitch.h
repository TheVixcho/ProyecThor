#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorGlitch {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    void SetSpeed(float v);
    float GetSpeed() const { return m_Speed; }

    void SetMode(int mode);
    int GetMode() const { return m_Mode; }

    GLuint Process(GLuint srcTex, int w, int h, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.40f;
    float m_Speed     = 1.0f;
    int   m_Mode      = 0; // 0=Sutil, 1=Cyberpunk RGB Split, 2=Cinta Analógica
};

} // namespace ProyecThor::Shaders

