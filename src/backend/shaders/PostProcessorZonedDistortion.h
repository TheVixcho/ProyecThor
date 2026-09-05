#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

class PostProcessorZonedDistortion {
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

    void SetZone(int z);
    int  GetZone() const { return m_Zone; }

    void SetFeather(float v);
    float GetFeather() const { return m_Feather; }

    GLuint Process(GLuint srcTex, double timeSec);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.45f;
    float m_Speed     = 1.20f;
    int   m_Zone      = 0; // 0=Inferior (Suelo/Calor), 1=Superior (Cielo), 2=Centro Focal, 3=Lateral Izq, 4=Lateral Der
    float m_Feather   = 0.35f;
};

} // namespace ProyecThor::Shaders

