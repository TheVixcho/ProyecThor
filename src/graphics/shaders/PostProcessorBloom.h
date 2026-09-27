#pragma once
#include "PostProcessSinglePass.h"

namespace ProyecThor::Shaders {

// Bloom (resplandor que se derrama desde zonas brillantes) sobre el
// composite completo (ver CompositePostChain). Aproximacion de un solo
// pase: muestrea un anillo de puntos alrededor de cada pixel, se queda
// solo con lo que supera un umbral de brillo, y lo suma de vuelta sobre la
// imagen original -- no es el bloom multi-pase "de verdad" (extraccion +
// blur separable + composite) que usarian un juego/ReShade, pero da el
// mismo efecto visual con un solo PostProcessSinglePass, mismo patron que
// el resto de los efectos de este panel.
class PostProcessorBloom {
public:
    bool Init(int w, int h);
    void Destroy() { m_Pipeline.Destroy(); }
    void Resize(int w, int h) { m_Pipeline.Resize(w, h); }
    void ForgetGLResources() { m_Pipeline.ForgetGLResources(); }
    bool IsInitialized() const { return m_Pipeline.IsInitialized(); }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = sin efecto, 1 = resplandor fuerte. Default 0.35.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    GLuint Process(GLuint srcTex, int w, int h);

private:
    PostProcessSinglePass m_Pipeline;
    bool  m_Enabled   = false;
    float m_Intensity = 0.35f;
};

} // namespace ProyecThor::Shaders
