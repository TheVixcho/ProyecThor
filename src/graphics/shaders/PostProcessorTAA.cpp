#include "PostProcessorTAA.h"
#include <algorithm>
#include <iostream>
#include <vector>

namespace ProyecThor::Shaders {

static const float k_QuadVerts[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
};

static const char* k_VertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

static const char* k_BlendFragSrc = R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_CurrentTex;
uniform sampler2D u_HistoryTex;
uniform float u_Blend;
uniform float u_HasHistory;

void main() {
    vec4 cur  = texture(u_CurrentTex, v_UV);
    vec4 hist = texture(u_HistoryTex, v_UV);
    vec3 result = mix(cur.rgb, hist.rgb, u_HasHistory * u_Blend);
    fragColor = vec4(result, cur.a);
}
)GLSL";

static const char* k_BlitFragSrc = R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;
uniform sampler2D u_Tex;
void main() { fragColor = texture(u_Tex, v_UV); }
)GLSL";

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "[TAA] Error compilando shader: " << log << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

static GLuint LinkProgram(GLuint vert, GLuint fragSrcCompiled) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, fragSrcCompiled);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[TAA] Error linkeando programa: " << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool PostProcessorTAA::CompilePrograms() {
    GLuint vert = CompileStage(GL_VERTEX_SHADER, k_VertSrc);
    if (!vert) return false;

    GLuint blendFrag = CompileStage(GL_FRAGMENT_SHADER, k_BlendFragSrc);
    if (!blendFrag) { glDeleteShader(vert); return false; }
    m_Program = LinkProgram(vert, blendFrag);
    glDeleteShader(blendFrag);
    if (!m_Program) { glDeleteShader(vert); return false; }

    GLuint blitFrag = CompileStage(GL_FRAGMENT_SHADER, k_BlitFragSrc);
    if (!blitFrag) { glDeleteShader(vert); return false; }
    m_BlitProgram = LinkProgram(vert, blitFrag);
    glDeleteShader(blitFrag);
    glDeleteShader(vert);

    return m_BlitProgram != 0;
}

static bool MakeColorFBO(GLuint& fbo, GLuint& tex, int w, int h) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[TAA] FBO incompleto (status=0x" << std::hex << status << std::dec << ")\n";
        return false;
    }
    return true;
}

bool PostProcessorTAA::CreateFramebuffers(int w, int h) {
    DestroyFramebuffers();
    if (!MakeColorFBO(m_FBO, m_Tex, w, h)) return false;
    if (!MakeColorFBO(m_HistoryFBO, m_HistoryTex, w, h)) return false;
    return true;
}

void PostProcessorTAA::DestroyFramebuffers() {
    if (m_FBO)        { glDeleteFramebuffers(1, &m_FBO);        m_FBO = 0; }
    if (m_Tex)        { glDeleteTextures(1, &m_Tex);            m_Tex = 0; }
    if (m_HistoryFBO) { glDeleteFramebuffers(1, &m_HistoryFBO); m_HistoryFBO = 0; }
    if (m_HistoryTex) { glDeleteTextures(1, &m_HistoryTex);     m_HistoryTex = 0; }
}

void PostProcessorTAA::DrawFullscreenQuad() {
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

bool PostProcessorTAA::Init(int w, int h) {
    if (m_Initialized) Destroy();

    m_W = w; m_H = h;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!CreateFramebuffers(w, h)) { Destroy(); return false; }
    if (!CompilePrograms())        { Destroy(); return false; }

    m_HasHistory  = false;
    m_Initialized = true;
    return true;
}

void PostProcessorTAA::Destroy() {
    DestroyFramebuffers();
    if (m_Program)     { glDeleteProgram(m_Program);     m_Program = 0; }
    if (m_BlitProgram) { glDeleteProgram(m_BlitProgram); m_BlitProgram = 0; }
    if (m_VAO)         { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO)         { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }
    m_Initialized = false;
    m_HasHistory  = false;
}

void PostProcessorTAA::Resize(int w, int h) {
    if (w == m_W && h == m_H) return;
    m_W = w; m_H = h;
    if (!m_Initialized) return;
    CreateFramebuffers(w, h);
    m_HasHistory = false; // el historial viejo ya no es del tamaño correcto
}

void PostProcessorTAA::ForgetGLResources() {
    m_FBO = m_Tex = m_HistoryFBO = m_HistoryTex = 0;
    m_Program = m_BlitProgram = m_VAO = m_VBO = 0;
    m_Initialized = false;
    m_HasHistory  = false;
}

void PostProcessorTAA::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorTAA::CopyToHistory(GLuint tex, int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_HistoryFBO);
    glViewport(0, 0, w, h);
    glUseProgram(m_BlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(m_BlitProgram, "u_Tex"), 0);
    DrawFullscreenQuad();
}

GLuint PostProcessorTAA::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Initialized || srcTex == 0 || w <= 0 || h <= 0) return srcTex;

    if (w != m_W || h != m_H) Resize(w, h);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Pase 1: mezclar actual + historial -> m_Tex.
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_W, m_H);
    glUseProgram(m_Program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_CurrentTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_HistoryTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_HistoryTex"), 1);

    // El blend maximo se limita a 0.9 para que nunca "congele" del todo la
    // imagen aunque el usuario ponga el slider al maximo.
    glUniform1f(glGetUniformLocation(m_Program, "u_Blend"), m_Intensity * 0.9f);
    glUniform1f(glGetUniformLocation(m_Program, "u_HasHistory"), m_HasHistory ? 1.0f : 0.0f);

    DrawFullscreenQuad();

    // Pase 2: copiar m_Tex al historial para el proximo frame.
    CopyToHistory(m_Tex, m_W, m_H);
    m_HasHistory = true;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glUseProgram(0);

    return m_Tex;
}

} // namespace ProyecThor::Shaders
