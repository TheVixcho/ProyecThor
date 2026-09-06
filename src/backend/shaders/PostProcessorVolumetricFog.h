#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorVolumetricFog {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetDensity(float v);
    float GetDensity() const { return m_Density; }

    void SetSpeed(float v);
    float GetSpeed() const { return m_Speed; }

    void SetScale(float v);
    float GetScale() const { return m_Scale; }

    void SetColorMode(int mode);
    int  GetColorMode() const { return m_ColorMode; }

    GLuint Process(GLuint srcTex, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Density   = 0.50f;
    float m_Speed     = 1.0f;
    float m_Scale     = 3.5f;
    int   m_ColorMode = 0; // 0=Humo Gris/Realista, 1=Místico/Cian, 2=Fuego/Cálido, 3=Cyber/Neón
};

} // namespace ProyecThor::Shaders

