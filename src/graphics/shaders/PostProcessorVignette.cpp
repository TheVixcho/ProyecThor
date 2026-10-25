#include "PostProcessorVignette.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildVignetteFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;

void main() {
    vec4 src = texture(u_InputTex, v_UV);

    // Distancia normalizada al centro (0 en el centro, ~1.0 en las
    // esquinas) -- radio/suavizado fijos, solo la intensidad es publica
    // (mismo enfoque "un solo slider" que Grain).
    vec2  uv   = v_UV - 0.5;
    float dist = length(uv) * 1.41421356;
    float vig  = 1.0 - smoothstep(0.65, 1.15, dist) * u_Intensity;

    fragColor = vec4(src.rgb * vig, src.a);
}
)GLSL";
}

bool PostProcessorVignette::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildVignetteFragSrc());
}

void PostProcessorVignette::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorVignette::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
