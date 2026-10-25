#include "PostProcessorChromaticAberration.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildChromaticAberrationFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;

void main() {
    vec2  uv   = v_UV - 0.5;
    float dist = length(uv) * 1.41421356;

    // El desfase crece hacia los bordes (dist^2), como en el efecto real
    // de una lente -- casi nada en el centro, marcado en las esquinas.
    vec2  dir    = normalize(uv + 1e-6);
    float shift  = u_Intensity * 0.02 * dist * dist;

    float r = texture(u_InputTex, v_UV - dir * shift).r;
    float g = texture(u_InputTex, v_UV).g;
    float b = texture(u_InputTex, v_UV + dir * shift).b;
    float a = texture(u_InputTex, v_UV).a;

    fragColor = vec4(r, g, b, a);
}
)GLSL";
}

bool PostProcessorChromaticAberration::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildChromaticAberrationFragSrc());
}

void PostProcessorChromaticAberration::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorChromaticAberration::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
