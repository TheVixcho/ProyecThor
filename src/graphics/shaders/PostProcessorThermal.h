#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorThermal {
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

    void SetMode(int mode);
    int GetMode() const { return m_Mode; }

    GLuint Process(GLuint srcTex, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.85f;
    int   m_Mode      = 0; // 0=Térmico, 1=Visión Nocturna Verde, 2=Solarizado
};

} // namespace ProyecThor::Shaders

