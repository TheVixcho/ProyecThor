#include "GlassRenderer.h"
#include <cstdio>
#include <algorithm>

namespace ProyecThor::UI {

// ── Shaders ────────────────────────────────────────────────────────────────

static const char* kQuadVS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// Gaussian 9-tap separable con pesos normalizados (suma = 1.0)
static const char* kBlurFS = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2      uDirection;
uniform vec2      uTexelSize;

void main() {
    vec2 step = uDirection * uTexelSize;
    vec4 sum  = vec4(0.0);

    sum += texture(uTexture, vUV - step * 4.0) * 0.0162162162;
    sum += texture(uTexture, vUV - step * 3.0) * 0.0540540541;
    sum += texture(uTexture, vUV - step * 2.0) * 0.1216216216;
    sum += texture(uTexture, vUV - step * 1.0) * 0.1945945946;
    sum += texture(uTexture, vUV             ) * 0.2270270270;
    sum += texture(uTexture, vUV + step * 1.0) * 0.1945945946;
    sum += texture(uTexture, vUV + step * 2.0) * 0.1216216216;
    sum += texture(uTexture, vUV + step * 3.0) * 0.0540540541;
    sum += texture(uTexture, vUV + step * 4.0) * 0.0162162162;

    FragColor = sum;
}
)";

// ── Lifecycle ──────────────────────────────────────────────────────────────

GlassRenderer::~GlassRenderer() { Shutdown(); }

// ── Compile helpers ────────────────────────────────────────────────────────

GLuint GlassRenderer::CompileProgram(const char* vsSrc, const char* fsSrc)
{
    auto compileShader = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(s, 1024, nullptr, log);
            std::fprintf(stderr, "[GlassRenderer] Shader error: %s\n", log);
        }
        return s;
    };

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::fprintf(stderr, "[GlassRenderer] Program link error: %s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ── FBO / Texture helpers ──────────────────────────────────────────────────

static void MakeColorFBO(GLuint& fbo, GLuint& tex, int w, int h)
{
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlassRenderer::CreateSceneTarget(int w, int h)
{
    MakeColorFBO(m_SceneFBO, m_SceneTexture, w, h);
}

void GlassRenderer::CreateBlurTargets(int w, int h)
{
    MakeColorFBO(m_BlurFBOA, m_BlurTexA, w, h);
    MakeColorFBO(m_BlurFBOB, m_BlurTexB, w, h);
}

void GlassRenderer::DestroyTargets()
{
    auto delFBO = [](GLuint& id){ if (id){ glDeleteFramebuffers(1,&id); id=0; } };
    auto delTex = [](GLuint& id){ if (id){ glDeleteTextures(1,&id);     id=0; } };

    delFBO(m_SceneFBO);     delTex(m_SceneTexture);
    delFBO(m_BlurFBOA);     delTex(m_BlurTexA);
    delFBO(m_BlurFBOB);     delTex(m_BlurTexB);
}

// ── Initialize / Resize / Shutdown ────────────────────────────────────────

bool GlassRenderer::Initialize(int screenWidth, int screenHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0) return false;

    m_BlurProgram      = CompileProgram(kQuadVS, kBlurFS);
    m_UniformTexture   = glGetUniformLocation(m_BlurProgram, "uTexture");
    m_UniformDirection = glGetUniformLocation(m_BlurProgram, "uDirection");
    m_UniformTexelSize = glGetUniformLocation(m_BlurProgram, "uTexelSize");

    // Quad fullscreen en clip-space
    float verts[] = {
        -1.f,  1.f,  0.f, 1.f,
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
    };

    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);
    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    Resize(screenWidth, screenHeight);
    m_Initialized = true;
    return true;
}

void GlassRenderer::Resize(int screenWidth, int screenHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0) return;
    if (screenWidth == m_ScreenW && screenHeight == m_ScreenH && m_SceneFBO != 0)
        return;

    DestroyTargets();

    m_ScreenW = screenWidth;
    m_ScreenH = screenHeight;

    // Resolución de blur: 1/4 para rendimiento óptimo
    m_BlurW = std::max(1, screenWidth  / 4);
    m_BlurH = std::max(1, screenHeight / 4);

    CreateSceneTarget(m_ScreenW, m_ScreenH);
    CreateBlurTargets(m_BlurW, m_BlurH);
}

void GlassRenderer::Shutdown()
{
    DestroyTargets();
    if (m_BlurProgram) { glDeleteProgram(m_BlurProgram);       m_BlurProgram = 0; }
    if (m_QuadVAO)     { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO     = 0; }
    if (m_QuadVBO)     { glDeleteBuffers(1, &m_QuadVBO);      m_QuadVBO     = 0; }
    m_Initialized = false;
}

// ── Frame capture ──────────────────────────────────────────────────────────

void GlassRenderer::CaptureCurrentFrame()
{
    if (!m_Initialized) return;

    // Copia el backbuffer actual al SceneFBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_SceneFBO);
    glBlitFramebuffer(
        0, 0, m_ScreenW, m_ScreenH,
        0, 0, m_ScreenW, m_ScreenH,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── Quad / Blur internos ───────────────────────────────────────────────────

void GlassRenderer::DrawFullscreenQuad()
{
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void GlassRenderer::BlurPass(GLuint srcTex, GLuint dstFBO,
                              int dstW, int dstH,
                              float dirX, float dirY, float spread)
{
    glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);
    glViewport(0, 0, dstW, dstH);

    glUseProgram(m_BlurProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(m_UniformTexture,   0);
    glUniform2f(m_UniformDirection, dirX, dirY);
    glUniform2f(m_UniformTexelSize,
                spread / static_cast<float>(dstW),
                spread / static_cast<float>(dstH));

    DrawFullscreenQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── Blur público ───────────────────────────────────────────────────────────
//
//  Flujo de texturas:
//    SceneTexture (full-res)
//      → downsample blit → BlurTexA
//    Por cada pass:
//      BlurTexA → (H) → BlurTexB
//      BlurTexB → (V) → BlurTexA
//    Al salir del loop el resultado está en BlurTexA.
//    BlurTexA → (H final) → BlurTexB    (GetBlurredTexture devuelve BlurTexB)
//
//  De esta forma GetBlurredTexture() SIEMPRE devuelve BlurTexB con el
//  resultado final, independientemente del nº de passes (que debe ser ≥1).

void GlassRenderer::Blur(float spread, int passes)
{
    if (!m_Initialized) return;

    passes = std::max(1, passes);

    // ── 1. Downsample: SceneFBO → BlurFBOA ────────────────────────────────
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_SceneFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_BlurFBOA);
    glBlitFramebuffer(
        0, 0, m_ScreenW, m_ScreenH,
        0, 0, m_BlurW,   m_BlurH,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Guarda el viewport actual para restaurarlo al salir
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // ── 2. Passes ping-pong: A→B (H), B→A (V) ────────────────────────────
    //    Al final de los passes, el resultado queda en BlurTexA.
    for (int i = 0; i < passes; ++i) {
        BlurPass(m_BlurTexA, m_BlurFBOB, m_BlurW, m_BlurH, 1.f, 0.f, spread);
        BlurPass(m_BlurTexB, m_BlurFBOA, m_BlurW, m_BlurH, 0.f, 1.f, spread);
    }

    // ── 3. Pass final: A→B (H extra para suavidad) ────────────────────────
    //    Deja el resultado final en BlurTexB (== GetBlurredTexture()).
    BlurPass(m_BlurTexA, m_BlurFBOB, m_BlurW, m_BlurH, 1.f, 0.f, spread);

    // Restaurar viewport
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);
}

} // namespace ProyecThor::UI