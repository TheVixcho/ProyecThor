#pragma once
#include "PostProcessSinglePass.h"
#include <algorithm>

namespace ProyecThor::Shaders {

// Gradiente de color "tipo cine": curva de contraste filmica suave +
// empuje de tinte hacia un canal (rojo/verde/azul), como el grading
// clasico de cine (ej. "orange & teal" pero simplificado a un solo canal
// elegible en vez de un LUT completo). Sobre el composite completo, ver
// CompositePostChain. Filtro 1:1, mismo patron que PostProcessorSaturation.
class PostProcessorCine {
public:
    enum class Tint { Red = 0, Green = 1, Blue = 2 };

    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin tinte (solo la curva filmica), 1 = tinte fuerte. Default 0.5.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    void SetTint(Tint t) { m_Tint = t; }
    Tint GetTint() const { return m_Tint; }
    void SetTintInt(int t) { m_Tint = static_cast<Tint>(std::clamp(t, 0, 2)); }
    int  GetTintInt() const { return static_cast<int>(m_Tint); }

    GLuint Process(GLuint srcTex);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.5f;
    Tint  m_Tint      = Tint::Red;
};

} // namespace ProyecThor::Shaders
