#pragma once
#include <GL/glew.h>
#include <string>
#include <functional>

namespace ProyecThor::Shaders {

// Pipeline compartido para efectos de post-proceso 1:1 (misma resolucion de
// entrada y salida, sin upscale): un FBO+textura de salida, un programa, un
// quad fullscreen. Lo reusan PostProcessorCRT/Grain/FXAA para no triplicar
// el boilerplate de compilacion/FBO — no se puede reusar PostProcessorFSR
// directamente porque su pipeline es de dos pases (EASU+RCAS) pensado para
// upscale real, una forma distinta de problema (ver PostProcessorFSR.h).
class PostProcessSinglePass {
public:
    PostProcessSinglePass()  = default;
    ~PostProcessSinglePass() { Destroy(); }

    PostProcessSinglePass(const PostProcessSinglePass&)            = delete;
    PostProcessSinglePass& operator=(const PostProcessSinglePass&) = delete;

    // Compila el programa (vertex fijo interno + fragSrc) y crea el FBO de
    // trabajo a w x h.
    bool Init(int w, int h, const std::string& fragSrc);
    void Destroy();

    // Recrea el FBO (y recompila, ya que el shader no depende de w/h salvo
    // via uniforms) si cambio el tamano.
    void Resize(int w, int h);

    // Olvida los IDs de GL SIN llamar glDelete*: usar solo cuando el
    // contexto GL que los creo ya no es el actual (ej. la ventana nativa de
    // "ProjectorLive" se recreo). Un glDelete* en el contexto nuevo podria
    // borrar, por coincidencia de numero de ID, un objeto legitimo de ESE
    // contexto — el contexto viejo y todo lo que tenia ya fue destruido por
    // GLFW, asi que no hace falta (ni es seguro) limpiar explicitamente.
    void ForgetGLResources();

    bool IsInitialized() const { return m_Initialized; }

    // Bindea srcTex en la unidad de textura 0 (uniform "u_InputTex" ya
    // seteado a 0), activa el programa y llama setUniforms(program) para que
    // el caller cargue sus propios uniforms antes de dibujar el quad.
    // Devuelve la textura de salida (o srcTex si no esta inicializado).
    GLuint Process(GLuint srcTex, const std::function<void(GLuint)>& setUniforms);

private:
    bool CompileProgram(const std::string& fragSrc);
    bool CreateFBO(int w, int h);
    void DestroyFBO();
    void DrawFullscreenQuad();

    bool   m_Initialized = false;
    int    m_W = 0, m_H = 0;
    GLuint m_Program = 0;
    GLuint m_FBO = 0, m_Tex = 0;
    GLuint m_VAO = 0, m_VBO = 0;
};

} // namespace ProyecThor::Shaders
