#include "PostProcessorBlur.h"
#include <algorithm>

namespace ProyecThor::Shaders {

// Kernel gaussiano de 9 taps (5 pesos, simetrico), separable en 1
// dimension por pase -- u_TexelStep trae la direccion (horizontal o
// vertical) ya multiplicada por el tamano de paso en pixeles.
static std::string BuildBlurFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2 u_TexelStep;

void main() {
    float w0 = 0.2270270270;
    float w1 = 0.1945945946;
    float w2 = 0.1216216216;
    float w3 = 0.0540540541;
    float w4 = 0.0162162162;

    vec4 sum = texture(u_InputTex, v_UV) * w0;
    sum += texture(u_InputTex, v_UV + u_TexelStep * 1.0) * w1;
    sum += texture(u_InputTex, v_UV - u_TexelStep * 1.0) * w1;
    sum += texture(u_InputTex, v_UV + u_TexelStep * 2.0) * w2;
    sum += texture(u_InputTex, v_UV - u_TexelStep * 2.0) * w2;
    sum += texture(u_InputTex, v_UV + u_TexelStep * 3.0) * w3;
    sum += texture(u_InputTex, v_UV - u_TexelStep * 3.0) * w3;
    sum += texture(u_InputTex, v_UV + u_TexelStep * 4.0) * w4;
    sum += texture(u_InputTex, v_UV - u_TexelStep * 4.0) * w4;

    fragColor = sum;
}
)GLSL";
}

bool PostProcessorBlur::Init(int w, int h) {
    m_W = w; m_H = h;
    bool ok = m_PipelineH.Init(w, h, BuildBlurFragSrc());
    ok = m_PipelineV.Init(w, h, BuildBlurFragSrc()) && ok;
    return ok;
}

void PostProcessorBlur::Destroy() {
    m_PipelineH.Destroy();
    m_PipelineV.Destroy();
}

void PostProcessorBlur::Resize(int w, int h) {
    m_W = w; m_H = h;
    m_PipelineH.Resize(w, h);
    m_PipelineV.Resize(w, h);
}

void PostProcessorBlur::ForgetGLResources() {
    m_PipelineH.ForgetGLResources();
    m_PipelineV.ForgetGLResources();
}

void PostProcessorBlur::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorBlur::Process(GLuint srcTex) {
    if (!m_Enabled || !IsInitialized() || m_W <= 0 || m_H <= 0) return srcTex;

    // Radio en pixeles entre taps: de ~1px (casi imperceptible) a ~6px
    // (desenfoque marcado) segun la intensidad 0..1.
    float radiusPx = 1.0f + m_Intensity * 5.0f;

    GLuint mid = m_PipelineH.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_TexelStep"), radiusPx / (float)m_W, 0.0f);
    });

    return m_PipelineV.Process(mid, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_TexelStep"), 0.0f, radiusPx / (float)m_H);
    });
}

} // namespace ProyecThor::Shaders
