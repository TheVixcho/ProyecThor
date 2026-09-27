#include "PostProcessorCine.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildCineFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform vec3  u_TintColor;

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    vec3 c = src.rgb;

    // Curva de contraste filmica suave (smoothstep), independiente del
    // tinte -- da el "punch" tipico de una gradacion de cine.
    c = c * c * (3.0 - 2.0 * c);

    // Empuje de tinte hacia el canal elegido (rojo/verde/azul).
    c *= mix(vec3(1.0), u_TintColor, u_Intensity);

    fragColor = vec4(clamp(c, 0.0, 1.0), src.a);
}
)GLSL";
}

bool PostProcessorCine::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildCineFragSrc());
}

void PostProcessorCine::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorCine::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    float tr = 1.0f, tg = 1.0f, tb = 1.0f;
    switch (m_Tint) {
        case Tint::Red:   tr = 1.18f; tg = 0.94f; tb = 0.90f; break;
        case Tint::Green: tr = 0.90f; tg = 1.18f; tb = 0.90f; break;
        case Tint::Blue:  tr = 0.90f; tg = 0.94f; tb = 1.18f; break;
    }

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
        glUniform3f(glGetUniformLocation(prog, "u_TintColor"), tr, tg, tb);
    });
}

} // namespace ProyecThor::Shaders
