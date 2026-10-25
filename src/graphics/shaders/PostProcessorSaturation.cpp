#include "PostProcessorSaturation.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildSaturationFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Amount;

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    float gray = dot(src.rgb, vec3(0.299, 0.587, 0.114));
    vec3 result = mix(vec3(gray), src.rgb, u_Amount);
    fragColor = vec4(result, src.a);
}
)GLSL";
}

bool PostProcessorSaturation::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildSaturationFragSrc());
}

void PostProcessorSaturation::SetAmount(float v) {
    m_Amount = std::clamp(v, 0.0f, 2.0f);
}

GLuint PostProcessorSaturation::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Amount"), m_Amount);
    });
}

} // namespace ProyecThor::Shaders
