#pragma once
#include <GL/glew.h>

namespace ProyecThor::Shaders {

// TAA (Temporal Anti-Aliasing) simplificado: mezcla el frame actual con un
// "historial" del resultado del frame anterior (promedio movil
// exponencial) -- el mismo truco que usan los motores de juegos para
// suavizar bordes/shimmer, a costa de perder algo de nitidez y ganar un
// desenfoque de movimiento leve en escenas con cambio (justo lo que se
// pidio: "pierda nitidez pero se vea mejor").
//
// A diferencia del resto de los efectos de este panel (que solo necesitan
// el frame actual y por eso usan PostProcessSinglePass, sin estado entre
// llamadas), este necesita mantener su PROPIA textura de historial viva
// entre frames. Por eso no usa PostProcessSinglePass: gestiona su propio
// FBO de salida + FBO de historial, y un segundo programa minimo ("blit")
// para copiar el resultado de cada frame al historial del siguiente --
// mismo patron que CompositePostChain::BlitToCurrentFramebuffer. Todo en
// OpenGL 3.3 core (no usa glCopyImageSubData, que recien esta en GL 4.3).
class PostProcessorTAA {
public:
    PostProcessorTAA()  = default;
    ~PostProcessorTAA() { Destroy(); }

    PostProcessorTAA(const PostProcessorTAA&)            = delete;
    PostProcessorTAA& operator=(const PostProcessorTAA&) = delete;

    bool Init(int w, int h);
    void Destroy();
    void Resize(int w, int h);

    // Ver comentario de ForgetGLResources en PostProcessSinglePass.h --
    // mismo motivo (contexto GL recreado, ej. Audiencia apagada/prendida).
    void ForgetGLResources();

    bool IsInitialized() const { return m_Initialized; }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // 0 = casi sin mezcla (nitido, poco anti-aliasing), 1 = mezcla fuerte
    // (mas estable/suave pero mas desenfoque de movimiento). Default 0.5.
    void SetIntensity(float v);
    float GetIntensity() const { return m_Intensity; }

    GLuint Process(GLuint srcTex, int w, int h);

private:
    bool CompilePrograms();
    bool CreateFramebuffers(int w, int h);
    void DestroyFramebuffers();
    void CopyToHistory(GLuint tex, int w, int h);
    void DrawFullscreenQuad();

    bool  m_Initialized = false;
    bool  m_Enabled     = false;
    float m_Intensity   = 0.5f;
    bool  m_HasHistory  = false;

    int m_W = 0, m_H = 0;

    GLuint m_Program     = 0; // mezcla actual+historial -> m_Tex
    GLuint m_BlitProgram = 0; // copia m_Tex -> m_HistoryTex (passthrough)

    GLuint m_FBO = 0,        m_Tex = 0;        // salida de este pase
    GLuint m_HistoryFBO = 0, m_HistoryTex = 0;  // resultado del frame anterior

    GLuint m_VAO = 0, m_VBO = 0;
};

} // namespace ProyecThor::Shaders
