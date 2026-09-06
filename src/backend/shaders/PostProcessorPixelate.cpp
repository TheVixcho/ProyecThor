#include "PostProcessorPixelate.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildPixelateFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_Resolution;
uniform float u_PixelSize;
uniform int   u_ColorDepth; // 0=Real, 1=16-bit, 2=8-bit

void main() {
    // Calculo del grid de pixeles
    vec2 pixelCount = max(u_Resolution / max(u_PixelSize, 1.0), vec2(1.0));
    vec2 quantizedUV = floor(v_UV * pixelCount) / pixelCount + (0.5 / pixelCount);

    vec4 src = texture(u_InputTex, quantizedUV);
    vec3 col = src.rgb;

    if (u_ColorDepth == 1) {
        // Cuantizacion 16-bit (32 niveles por canal)
        col = floor(col * 31.0 + 0.5) / 31.0;
    } else if (u_ColorDepth == 2) {
        // Cuantizacion 8-bit retro (8 niveles por canal)
        col = floor(col * 7.0 + 0.5) / 7.0;
    }

    // Micro-rejilla sutil de pixel border
    vec2 gridPos = fract(v_UV * pixelCount);
    float gridLine = smoothstep(0.0, 0.05, gridPos.x) * smoothstep(1.0, 0.95, gridPos.x) *
                     smoothstep(0.0, 0.05, gridPos.y) * smoothstep(1.0, 0.95, gridPos.y);
    col *= mix(0.92, 1.0, gridLine);

    fragColor = vec4(col, src.a);
}
)GLSL";
}

bool PostProcessorPixelate::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildPixelateFragSrc());
}

void PostProcessorPixelate::SetPixelSize(float v) {
    m_PixelSize = std::clamp(v, 2.0f, 64.0f);
}

void PostProcessorPixelate::SetColorDepth(int depth) {
    m_ColorDepth = std::clamp(depth, 0, 2);
}

GLuint PostProcessorPixelate::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_Resolution"), (float)w, (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_PixelSize"),  m_PixelSize);
        glUniform1i(glGetUniformLocation(prog, "u_ColorDepth"), m_ColorDepth);
    });
}

} // namespace ProyecThor::Shaders

