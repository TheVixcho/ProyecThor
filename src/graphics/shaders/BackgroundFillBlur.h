#pragma once
#include <GL/glew.h>

namespace ProyecThor::Shaders {

// Blur pesado y barato para el "Rellenado" de las barras de letterbox/
// pillarbox del fondo (ver BackgroundLayer::GetBlurredFillTexture): en vez
// de dejar las barras negras, se llenan con una copia MUY desenfocada del
// mismo contenido, estirada a pantalla completa, detras del contenido
// nitido -- el efecto tipico de Spotify Canvas/YouTube/TVs "smart".
//
// Mismo enfoque separable con downsample que usa GlassRenderer (ver
// src/ui/framework/GlassRenderer.h) pero partiendo de una TEXTURA DE ORIGEN
// arbitraria (el fondo ya procesado) en vez de capturar el backbuffer --
// no se puede reusar GlassRenderer directo porque ese vive atado al
// contexto/tamano de la ventana principal de UI, no al del fondo del
// proyector (que puede ser otra resolucion/monitor).
class BackgroundFillBlur {
public:
    bool Init(int workW, int workH);
    void Destroy();
    void Resize(int workW, int workH);
    bool IsInitialized() const { return m_Initialized; }

    // srcTex: textura de origen (el fondo ya procesado, a su resolucion
    // nativa). El primer paso de blur hace de downsample a la resolucion
    // de trabajo (ver Resize) en el mismo pase, asi que no hace falta un
    // blit previo. Devuelve la textura resultante, ya muy desenfocada.
    GLuint Process(GLuint srcTex, float spread = 3.0f, int passes = 3);

private:
    void CreateTargets(int w, int h);
    void DestroyTargets();
    void EnsureProgram();
    void DrawFullscreenQuad();
    void BlurPass(GLuint srcTex, GLuint dstFBO, int dstW, int dstH, float dirX, float dirY, float spread);

    int m_W = 0, m_H = 0; // resolucion de trabajo (baja a proposito)

    GLuint m_FBOA = 0, m_TexA = 0;
    GLuint m_FBOB = 0, m_TexB = 0;

    GLuint m_Program = 0;
    GLuint m_VAO = 0, m_VBO = 0;
    GLint  m_UniformTexture   = -1;
    GLint  m_UniformDirection = -1;
    GLint  m_UniformTexelSize = -1;

    bool m_Initialized = false;
};

} // namespace ProyecThor::Shaders
