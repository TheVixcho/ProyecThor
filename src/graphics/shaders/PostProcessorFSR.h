#pragma once
#include <GL/glew.h>
#include <string>

namespace ProyecThor::Shaders {

    // Encapsula el pipeline FSR 1.0 de AMD en dos pases:
    //   Pase 1 - EASU (Edge Adaptive Spatial Upsampling): upscale con deteccion de bordes.
    //   Pase 2 - RCAS (Robust Contrast Adaptive Sharpening): sharpening post-escala.
    //
    // Uso tipico:
    //   postFSR.Init(outputW, outputH);
    //   postFSR.SetSharpness(0.2f);
    //   GLuint upscaledTex = postFSR.Process(srcTexture, srcW, srcH);
    //   // Luego usar upscaledTex como textura final del proyector.

    class PostProcessorFSR {
    public:
        PostProcessorFSR()  = default;
        ~PostProcessorFSR() { Destroy(); }

        PostProcessorFSR(const PostProcessorFSR&)            = delete;
        PostProcessorFSR& operator=(const PostProcessorFSR&) = delete;

        // Inicializa los shaders GLSL, FBOs y texturas de trabajo.
        // outputW/H: resolución final del proyector (ej: 1920x1080).
        bool Init(int outputW, int outputH);

        // Libera todos los recursos OpenGL.
        void Destroy();

        // Getters de resolución de salida.
        int GetOutputW() const { return m_OutputW; }
        int GetOutputH() const { return m_OutputH; }

        // Ejecuta los dos pases FSR sobre srcTex (resolución srcW x srcH)
        // y devuelve el ID de la textura OpenGL upscaleada a outputW x outputH.
        // Devuelve srcTex si FSR está desactivado o no inicializado.
        GLuint Process(GLuint srcTex, int srcW, int srcH);

        // Sharpness RCAS: 0.0 = máximo sharpening, 2.0 = mínimo. Default 0.2.
        void  SetSharpness(float sharpness);
        float GetSharpness() const { return m_Sharpness; }

        // Permite activar/desactivar FSR en tiempo real sin destruir recursos.
        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool IsEnabled()        const { return m_Enabled; }

        bool IsInitialized()    const { return m_Initialized; }

        // Redimensiona los FBOs si cambia la resolución de salida.
        void Resize(int newOutputW, int newOutputH);

    private:
        bool CompileShaders();
        bool CreateFramebuffers(int w, int h);
        void DestroyFramebuffers();
        GLuint CompileShaderStage(GLenum type, const std::string& src);
        GLuint LinkProgram(GLuint vert, GLuint frag);

        // Genera el fuente GLSL del vertex shader (quad fullscreen).
        std::string BuildVertexShaderSrc() const;

        // Genera el fuente GLSL del fragment shader EASU.
        std::string BuildEASUFragSrc(int inputW, int inputH,
                                     int outputW, int outputH) const;

        // Genera el fuente GLSL del fragment shader RCAS.
        std::string BuildRCASFragSrc() const;

        // Actualiza los uniforms de EASU cuando cambia la resolución fuente.
        void UpdateEASUConstants(int inputW, int inputH, int outputW, int outputH);

        // Dibuja un quad fullscreen usando el programa activo.
        void DrawFullscreenQuad();

        bool  m_Initialized = false;
        bool  m_Enabled     = true;
        float m_Sharpness   = 0.2f;

        int m_OutputW = 0;   // FIX: declaración única (estaban duplicadas)
        int m_OutputH = 0;

        // Programas GLSL compilados
        GLuint m_ProgramEASU = 0;
        GLuint m_ProgramRCAS = 0;

        // FBO pase 1 (EASU → textura intermedia)
        GLuint m_FBO_EASU    = 0;
        GLuint m_Tex_EASU    = 0;

        // FBO pase 2 (RCAS → textura final)
        GLuint m_FBO_RCAS    = 0;
        GLuint m_Tex_RCAS    = 0;

        // Quad VAO/VBO
        GLuint m_VAO = 0;
        GLuint m_VBO = 0;

        // Cache de resolución fuente para detectar cambios y recompilar EASU
        int m_LastSrcW = 0;
        int m_LastSrcH = 0;
    };

} // namespace ProyecThor::Shaders
