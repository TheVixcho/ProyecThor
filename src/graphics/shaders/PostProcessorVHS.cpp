#include "PostProcessorVHS.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildVHSFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2  u_TexelSize;
uniform float u_Time;
uniform float u_Intensity;

float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    vec2 uv = v_UV;

    // Bamboleo vertical leve -- toda la imagen se mueve un poco, como una
    // cinta floja en el mecanismo.
    float wobble = sin(u_Time * 1.3 + uv.y * 6.0) * 0.0015 * u_Intensity;
    uv.x += wobble;

    // Banda de "tracking" que se desliza de arriba a abajo y glitchea el
    // muestreo justo en esa franja -- el glitch clasico de VHS.
    float bandY    = fract(u_Time * 0.05);
    float bandDist = abs(uv.y - bandY);
    float band     = smoothstep(0.02, 0.0, bandDist) * u_Intensity;
    uv.x += band * (hash(floor(uv.y * 80.0) + floor(u_Time * 10.0)) - 0.5) * 0.05;

    // Sangrado de color horizontal (crosstalk de cabeza desalineada).
    float chromaOff = u_TexelSize.x * (2.0 + 2.0 * u_Intensity);
    float r = texture(u_InputTex, uv + vec2(chromaOff, 0.0)).r;
    float g = texture(u_InputTex, uv).g;
    float b = texture(u_InputTex, uv - vec2(chromaOff, 0.0)).b;
    float a = texture(u_InputTex, uv).a;
    vec3 col = vec3(r, g, b);

    // Scanlines.
    float scan = 0.90 + 0.10 * sin(uv.y * 900.0);
    col *= mix(1.0, scan, u_Intensity);

    // Ruido de estatica.
    float n = hash(uv.y * 1000.0 + u_Time * 60.0) - 0.5;
    col += n * 0.06 * u_Intensity;

    // Ligera desaturacion + cast verdoso, tipico de cinta vieja.
    float gray = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(col, vec3(gray) * vec3(0.96, 1.02, 0.96), 0.25 * u_Intensity);

    fragColor = vec4(clamp(col, 0.0, 1.0), a);
}
)GLSL";
}

bool PostProcessorVHS::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildVHSFragSrc());
}

void PostProcessorVHS::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorVHS::Process(GLuint srcTex, int w, int h, double timeSeconds) {
    if (!m_Enabled || !m_Pipeline.IsInitialized() || w <= 0 || h <= 0) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_TexelSize"), 1.0f / (float)w, 1.0f / (float)h);
        glUniform1f(glGetUniformLocation(prog, "u_Time"), (float)timeSeconds);
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
