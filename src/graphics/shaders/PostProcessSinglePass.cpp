#include "PostProcessSinglePass.h"
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

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetShaderInfoLog(id, len, nullptr, log.data());
        const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::cerr << "[PostFX] Error compilando shader " << typeName << ":\n" << log.data() << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

bool PostProcessSinglePass::CompileProgram(const std::string& fragSrc) {
    if (m_Program) { glDeleteProgram(m_Program); m_Program = 0; }

    GLuint vert = CompileStage(GL_VERTEX_SHADER, k_VertSrc);
    GLuint frag = CompileStage(GL_FRAGMENT_SHADER, fragSrc.c_str());
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDetachShader(prog, vert);
    glDetachShader(prog, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "[PostFX] Error linkeando programa:\n" << log.data() << "\n";
        glDeleteProgram(prog);
        return false;
    }

    m_Program = prog;
    return true;
}

bool PostProcessSinglePass::CreateFBO(int w, int h) {
    DestroyFBO();

    glGenTextures(1, &m_Tex);
    glBindTexture(GL_TEXTURE_2D, m_Tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[PostFX] FBO incompleto (status=0x" << std::hex << status << std::dec << ")\n";
        return false;
    }
    return true;
}

void PostProcessSinglePass::DestroyFBO() {
    if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_Tex) { glDeleteTextures(1, &m_Tex); m_Tex = 0; }
}

void PostProcessSinglePass::DrawFullscreenQuad() {
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

bool PostProcessSinglePass::Init(int w, int h, const std::string& fragSrc) {
    if (m_Initialized) Destroy();

    m_W = w;
    m_H = h;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!CreateFBO(w, h)) { Destroy(); return false; }
    if (!CompileProgram(fragSrc)) { Destroy(); return false; }

    m_Initialized = true;
    return true;
}

void PostProcessSinglePass::Destroy() {
    DestroyFBO();
    if (m_Program) { glDeleteProgram(m_Program); m_Program = 0; }
    if (m_VAO)     { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO)     { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    m_Initialized = false;
}

void PostProcessSinglePass::ForgetGLResources() {
    m_Program = 0;
    m_FBO     = 0;
    m_Tex     = 0;
    m_VAO     = 0;
    m_VBO     = 0;
    m_Initialized = false;
    m_W = m_H = 0;
}

void PostProcessSinglePass::Resize(int w, int h) {
    if (w == m_W && h == m_H) return;
    m_W = w; m_H = h;
    if (!m_Initialized) return;
    CreateFBO(w, h);
}

GLuint PostProcessSinglePass::Process(GLuint srcTex, const std::function<void(GLuint)>& setUniforms) {
    if (!m_Initialized || srcTex == 0) return srcTex;

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_W, m_H);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_Program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_InputTex"), 0);

    if (setUniforms) setUniforms(m_Program);

    DrawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    return m_Tex;
}

} // namespace ProyecThor::Shaders
