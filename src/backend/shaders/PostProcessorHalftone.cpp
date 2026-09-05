#include "PostProcessorHalftone.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildHalftoneFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_Resolution;
uniform float u_DotScale;
uniform int   u_Mode; // 0=Color Pop-Art, 1=Monocromo, 2=Periodico

vec2 rotate(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float dotPattern(vec2 uv, float angle, float scale) {
    vec2 rotUV = rotate(uv * u_Resolution / scale, angle);
    vec2 grid = fract(rotUV) - 0.5;
    return length(grid) * 1.4142;
}

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    float lum = dot(src.rgb, vec3(0.299, 0.587, 0.114));

    vec3 col = src.rgb;

    if (u_Mode == 0) {
        // 0: Pop-Art Color CMYK simulado (canales con diferentes angulos de trama)
        float dotR = dotPattern(v_UV, 0.2618, u_DotScale); // 15 deg
        float dotG = dotPattern(v_UV, 1.3090, u_DotScale); // 75 deg
        float dotB = dotPattern(v_UV, 0.0,    u_DotScale); // 0 deg

        float r = step(dotR, src.r * 1.2);
        float g = step(dotG, src.g * 1.2);
        float b = step(dotB, src.b * 1.2);
        col = vec3(r, g, b) * 1.1;
    } else if (u_Mode == 1) {
        // 1: Monocromo B&W (trama a 45 grados estilo manga/comic)
        float d = dotPattern(v_UV, 0.785398, u_DotScale); // 45 deg
        float ink = step(d, lum * 1.25);
        col = vec3(ink);
    } else if (u_Mode == 2) {
        // 2: Papel periodico envejecido
        float d = dotPattern(v_UV, 0.785398, u_DotScale * 0.85);
        float ink = step(d, lum * 1.2);
        vec3 paper = vec3(0.92, 0.86, 0.74);
        vec3 printInk = vec3(0.12, 0.10, 0.15);
        col = mix(printInk, paper, ink);
    }

    fragColor = vec4(mix(src.rgb, col, 0.90), src.a);
}
)GLSL";
}

bool PostProcessorHalftone::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildHalftoneFragSrc());
}

void PostProcessorHalftone::SetDotScale(float v) {
    m_DotScale = std::clamp(v, 3.0f, 40.0f);
}

void PostProcessorHalftone::SetMode(int mode) {
    m_Mode = std::clamp(mode, 0, 2);
}

GLuint PostProcessorHalftone::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_Resolution"), (float)w, (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_DotScale"),   m_DotScale);
        glUniform1i(glGetUniformLocation(prog, "u_Mode"),       m_Mode);
    });
}

} // namespace ProyecThor::Shaders

