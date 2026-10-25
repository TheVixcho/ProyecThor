#include "PostProcessorCRT.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildCRTFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_Resolution;
uniform float u_ScanlineIntensity;
uniform float u_Curvature;
uniform float u_VignetteIntensity;

vec2 CurveUV(vec2 uv) {
    vec2 c = uv * 2.0 - 1.0;
    vec2 offset = c.yx * u_Curvature;
    c += c * offset * offset;
    return c * 0.5 + 0.5;
}

void main() {
    vec2 uv = CurveUV(v_UV);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 color = texture(u_InputTex, uv).rgb;

    float scanline  = sin(uv.y * u_Resolution.y * 3.14159265);
    float scanShade = 0.5 + 0.5 * scanline;
    color *= mix(1.0, scanShade, u_ScanlineIntensity);

    vec2  vc  = uv - 0.5;
    float vig = 1.0 - dot(vc, vc) * u_VignetteIntensity;
    color *= clamp(vig, 0.0, 1.0);

    fragColor = vec4(color, 1.0);
}
)GLSL";
}

bool PostProcessorCRT::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildCRTFragSrc());
}

void PostProcessorCRT::SetScanlineIntensity(float v) {
    m_ScanlineIntensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorCRT::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_Resolution"), (float)w, (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_ScanlineIntensity"), m_ScanlineIntensity);
        glUniform1f(glGetUniformLocation(prog, "u_Curvature"), m_Curvature);
        glUniform1f(glGetUniformLocation(prog, "u_VignetteIntensity"), m_VignetteIntensity);
    });
}

} // namespace ProyecThor::Shaders
