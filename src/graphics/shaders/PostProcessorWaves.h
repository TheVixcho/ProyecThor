#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorWaves {
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

    void SetFrequency(float v);
    float GetFrequency() const { return m_Frequency; }

    GLuint Process(GLuint srcTex, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.35f;
    float m_Speed     = 1.0f;
    float m_Frequency = 8.0f;
};

} // namespace ProyecThor::Shaders

