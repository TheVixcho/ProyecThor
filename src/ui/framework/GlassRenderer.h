#pragma once

#include <GL/glew.h>

namespace ProyecThor::UI {

class GlassRenderer {
public:
    GlassRenderer() = default;
    ~GlassRenderer();

    bool Initialize(int screenWidth, int screenHeight);
    void Resize(int screenWidth, int screenHeight);
    void Shutdown();

    void CaptureCurrentFrame();
    void Blur(float spread = 1.0f, int passes = 1);

    GLuint GetBlurredTexture() const { return m_BlurTexB; }
    int    GetScreenWidth()  const { return m_ScreenW; }
    int    GetScreenHeight() const { return m_ScreenH; }

private:
    void CreateSceneTarget(int w, int h);
    void CreateBlurTargets(int w, int h);
    void DestroyTargets();
    GLuint CompileProgram(const char* vsSrc, const char* fsSrc);
    void DrawFullscreenQuad();
    void BlurPass(GLuint srcTex, GLuint dstFBO, int dstW, int dstH,
                  float dirX, float dirY, float spread);

    int m_ScreenW = 0;
    int m_ScreenH = 0;
    int m_BlurW   = 0;
    int m_BlurH   = 0;

    GLuint m_SceneFBO     = 0;
    GLuint m_SceneTexture = 0;

    GLuint m_BlurFBOA = 0;
    GLuint m_BlurTexA = 0;
    GLuint m_BlurFBOB = 0;
    GLuint m_BlurTexB = 0;

    GLuint m_BlurProgram = 0;
    GLuint m_QuadVAO     = 0;
    GLuint m_QuadVBO     = 0;

    GLint m_UniformTexture  = -1;
    GLint m_UniformDirection = -1;
    GLint m_UniformTexelSize = -1;

    bool m_Initialized = false;
};

} // namespace ProyecThor::UI
