#pragma once
#include <GL/glew.h>

namespace ProyecThor::Shaders {

// Escalador + sharpening NVIDIA Image Scaling (NIS) -- alternativa
// exclusiva de NVIDIA a FSR (ver PostProcessorFSR.h), mutuamente
// excluyente con el (ver ShadersPanel.cpp / PresentationCore::SetNISEnabled:
// activar uno apaga el otro, no tiene sentido correr los dos upscalers de
// la misma etapa a la vez).
//
// El algoritmo real de NVIDIA (NVScaler, ver NIS_Scaler.h del SDK oficial
// github.com/NVIDIAGameWorks/NVIDIAImageScaling) esta escrito como compute
// shader: carga un tile de la imagen fuente + un "mapa de bordes" a memoria
// COMPARTIDA entre los threads de un mismo grupo, para no releer la misma
// region varias veces. Este renderer corre en un contexto OpenGL 3.3 sin
// compute shaders (ver main.cpp), asi que se porto la MISMA formula
// matematica y las MISMAS tablas de coeficientes reales (ver
// NIS_CoefTables.h) a un fragment shader de un solo pase: cada fragmento
// recalcula su propia ventana de vecinos via texture() en vez de leer de
// un tile compartido (redunda algo de ancho de banda entre pixeles
// vecinos, sin GPU NVIDIA a mano en este entorno para medir cuanto, pero
// el resultado visual es equivalente ya que es exactamente la misma
// matematica). Las coordenadas absolutas usadas abajo se derivaron
// analizando el direccionamiento tile-relativo de NIS_Scaler.h hasta
// reducirlo a su equivalente independiente de tile (ver PostProcessorNIS.cpp).
//
// Uso: igual que PostProcessorFSR (mismo lugar, BackgroundLayer, antes de
// componer -- ver CompositePostChain.h para por que FSR no vive ahi).
class PostProcessorNIS {
public:
    PostProcessorNIS()  = default;
    ~PostProcessorNIS() { Destroy(); }

    PostProcessorNIS(const PostProcessorNIS&)            = delete;
    PostProcessorNIS& operator=(const PostProcessorNIS&) = delete;

    // outputW/H: resolucion final del proyector.
    bool Init(int outputW, int outputH);
    void Destroy();
    void Resize(int newOutputW, int newOutputH);

    // Ejecuta el pase NIS sobre srcTex (resolucion srcW x srcH) y devuelve
    // el ID de la textura OpenGL escalada a outputW x outputH. Devuelve
    // srcTex si esta desactivado o no inicializado.
    GLuint Process(GLuint srcTex, int srcW, int srcH);

    // 0.0 = sin sharpening extra, 1.0 = maximo. Default 0.5 (equivalente al
    // "sharpness slider" de 50% de NVIDIA). Ver NVScalerUpdateConfig.
    void  SetSharpness(float sharpness);
    float GetSharpness() const { return m_Sharpness; }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled()        const { return m_Enabled; }

    bool IsInitialized() const { return m_Initialized; }

    int GetOutputW() const { return m_OutputW; }
    int GetOutputH() const { return m_OutputH; }

private:
    bool CompileProgram();
    bool CreateFramebuffer(int w, int h);
    void DestroyFramebuffer();
    void EnsureCoefTextures();
    void DrawFullscreenQuad();

    bool  m_Initialized = false;
    bool  m_Enabled     = false;
    float m_Sharpness   = 0.5f;

    int m_OutputW = 0, m_OutputH = 0;

    GLuint m_Program = 0;
    GLuint m_FBO = 0, m_Tex = 0;
    GLuint m_CoefScaleTex = 0, m_CoefUSMTex = 0;
    GLuint m_VAO = 0, m_VBO = 0;
};

} // namespace ProyecThor::Shaders
