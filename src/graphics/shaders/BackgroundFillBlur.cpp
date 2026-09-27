#include "BackgroundFillBlur.h"
#include <algorithm>
#include <cstdio>

namespace ProyecThor::Shaders {

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

// Gaussiano 9-tap separable, mismos pesos que GlassRenderer (ver ahi el
// comentario: suman 1.0).
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

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::fprintf(stderr, "[BackgroundFillBlur] Shader error: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

static void MakeColorFBO(GLuint& fbo, GLuint& tex, int w, int h) {
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BackgroundFillBlur::EnsureProgram() {
    if (m_Program) return;

    GLuint vs = CompileStage(GL_VERTEX_SHADER, kQuadVS);
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, kBlurFS);
    m_Program = glCreateProgram();
    glAttachShader(m_Program, vs);
    glAttachShader(m_Program, fs);
    glLinkProgram(m_Program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    m_UniformTexture   = glGetUniformLocation(m_Program, "uTexture");
    m_UniformDirection = glGetUniformLocation(m_Program, "uDirection");
    m_UniformTexelSize = glGetUniformLocation(m_Program, "uTexelSize");

    float verts[] = {
        -1.f,  1.f,  0.f, 1.f,
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
        -1.f,  1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
    };
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void BackgroundFillBlur::CreateTargets(int w, int h) {
    MakeColorFBO(m_FBOA, m_TexA, w, h);
    MakeColorFBO(m_FBOB, m_TexB, w, h);
}

void BackgroundFillBlur::DestroyTargets() {
    auto delFBO = [](GLuint& id){ if (id) { glDeleteFramebuffers(1, &id); id = 0; } };
    auto delTex = [](GLuint& id){ if (id) { glDeleteTextures(1, &id);     id = 0; } };
    delFBO(m_FBOA); delTex(m_TexA);
    delFBO(m_FBOB); delTex(m_TexB);
}

bool BackgroundFillBlur::Init(int workW, int workH) {
    if (workW <= 0 || workH <= 0) return false;
    EnsureProgram();
    Resize(workW, workH);
    m_Initialized = true;
    return true;
}

void BackgroundFillBlur::Resize(int workW, int workH) {
    workW = std::max(1, workW);
    workH = std::max(1, workH);
    if (workW == m_W && workH == m_H && m_FBOA != 0) return;

    DestroyTargets();
    m_W = workW;
    m_H = workH;
    CreateTargets(m_W, m_H);
}

void BackgroundFillBlur::Destroy() {
    DestroyTargets();
    if (m_Program) { glDeleteProgram(m_Program);      m_Program = 0; }
    if (m_VAO)     { glDeleteVertexArrays(1, &m_VAO); m_VAO     = 0; }
    if (m_VBO)     { glDeleteBuffers(1, &m_VBO);      m_VBO     = 0; }
    m_Initialized = false;
}

void BackgroundFillBlur::DrawFullscreenQuad() {
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void BackgroundFillBlur::BlurPass(GLuint srcTex, GLuint dstFBO, int dstW, int dstH,
                                   float dirX, float dirY, float spread) {
    glBindFramebuffer(GL_FRAMEBUFFER, dstFBO);
    glViewport(0, 0, dstW, dstH);

    glUseProgram(m_Program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(m_UniformTexture, 0);
    glUniform2f(m_UniformDirection, dirX, dirY);
    glUniform2f(m_UniformTexelSize, spread / (float)dstW, spread / (float)dstH);

    DrawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

GLuint BackgroundFillBlur::Process(GLuint srcTex, float spread, int passes) {
    if (!m_Initialized || srcTex == 0) return srcTex;
    passes = std::max(1, passes);

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Primer pase: srcTex (resolucion nativa) -> TexA, hace de downsample Y
    // de primer blur horizontal en el mismo dibujo (sin blit previo).
    BlurPass(srcTex, m_FBOA, m_W, m_H, 1.0f, 0.0f, spread);
    BlurPass(m_TexA, m_FBOB, m_W, m_H, 0.0f, 1.0f, spread);

    for (int i = 1; i < passes; ++i) {
        BlurPass(m_TexB, m_FBOA, m_W, m_H, 1.0f, 0.0f, spread);
        BlurPass(m_TexA, m_FBOB, m_W, m_H, 0.0f, 1.0f, spread);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    return m_TexB;
}

} // namespace ProyecThor::Shaders
