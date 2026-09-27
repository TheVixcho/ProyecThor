#include "PostProcessorSharpen.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildSharpenFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_TexelSize;
uniform float u_Amount;

void main() {
    vec4 c  = texture(u_InputTex, v_UV);
    vec4 up    = texture(u_InputTex, v_UV + vec2(0.0, -u_TexelSize.y));
    vec4 down  = texture(u_InputTex, v_UV + vec2(0.0,  u_TexelSize.y));
    vec4 left  = texture(u_InputTex, v_UV + vec2(-u_TexelSize.x, 0.0));
    vec4 right = texture(u_InputTex, v_UV + vec2( u_TexelSize.x, 0.0));

    vec4 sharpened = c * (1.0 + 4.0 * u_Amount) - (up + down + left + right) * u_Amount;
    fragColor = vec4(clamp(sharpened.rgb, 0.0, 1.0), c.a);
}
)GLSL";
}

bool PostProcessorSharpen::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildSharpenFragSrc());
}

void PostProcessorSharpen::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorSharpen::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized() || w <= 0 || h <= 0) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_TexelSize"), 1.0f / (float)w, 1.0f / (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_Amount"), m_Intensity * 0.8f);
    });
}

} // namespace ProyecThor::Shaders
