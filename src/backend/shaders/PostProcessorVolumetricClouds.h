#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorVolumetricClouds {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetCoverage(float v);
    float GetCoverage() const { return m_Coverage; }

    void SetDensity(float v);
    float GetDensity() const { return m_Density; }

    void SetSpeed(float v);
    float GetSpeed() const { return m_Speed; }

    void SetSunIntensity(float v);
    float GetSunIntensity() const { return m_SunIntensity; }

    GLuint Process(GLuint srcTex, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled       = false;
    float m_Coverage      = 0.55f;
    float m_Density       = 0.60f;
    float m_Speed         = 0.80f;
    float m_SunIntensity  = 0.65f;
};

} // namespace ProyecThor::Shaders

