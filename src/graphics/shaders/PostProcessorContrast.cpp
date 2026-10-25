#include "PostProcessorContrast.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildContrastFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Amount;

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    vec3 result = (src.rgb - 0.5) * u_Amount + 0.5;
    fragColor = vec4(clamp(result, 0.0, 1.0), src.a);
}
)GLSL";
}

bool PostProcessorContrast::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildContrastFragSrc());
}

void PostProcessorContrast::SetAmount(float v) {
    m_Amount = std::clamp(v, 0.0f, 2.0f);
}

GLuint PostProcessorContrast::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Amount"), m_Amount);
    });
}

} // namespace ProyecThor::Shaders
