#include "PostProcessorBloom.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildBloomFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_TexelSize;
uniform float u_Radius;
uniform float u_Intensity;

const float kThreshold = 0.6;

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

void main() {
    vec4 base = texture(u_InputTex, v_UV);

    vec3  glow = vec3(0.0);
    const int kTaps = 8;
    for (int i = 0; i < kTaps; ++i) {
        float a = (6.2831853 / float(kTaps)) * float(i);
        vec2  off = vec2(cos(a), sin(a)) * u_TexelSize * u_Radius;
        vec3  s   = texture(u_InputTex, v_UV + off).rgb;
        float bright = max(luma(s) - kThreshold, 0.0);
        glow += s * bright;
    }
    glow /= float(kTaps);

    fragColor = vec4(base.rgb + glow * u_Intensity * 2.5, base.a);
}
)GLSL";
}

bool PostProcessorBloom::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildBloomFragSrc());
}

void PostProcessorBloom::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorBloom::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized() || w <= 0 || h <= 0) return srcTex;

    float radiusPx = 2.0f + m_Intensity * 6.0f;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_TexelSize"), 1.0f / (float)w, 1.0f / (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_Radius"), radiusPx);
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
