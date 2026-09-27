#include "PostProcessorWaves.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildWavesFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform float u_Speed;
uniform float u_Frequency;
uniform float u_Time;

void main() {
    float t = u_Time * u_Speed;
    vec2 uv = v_UV;

    // Distorsión sinusoidal compuesta bidireccional (tipo ondas en agua o calor)
    float waveX = sin(uv.y * u_Frequency + t * 2.0) * 0.02 * u_Intensity;
    float waveY = cos(uv.x * (u_Frequency * 0.8) + t * 1.5) * 0.02 * u_Intensity;

    // Micro-refracción diagonal
    float ripple = sin((uv.x + uv.y) * u_Frequency * 1.5 + t * 3.0) * 0.01 * u_Intensity;

    vec2 distortedUV = clamp(uv + vec2(waveX + ripple, waveY + ripple), 0.0, 1.0);

    vec4 color = texture(u_InputTex, distortedUV);

    // Sutil caústica/destello en las crestas de la ola
    float crest = smoothstep(0.7, 1.0, sin(uv.y * u_Frequency + t * 2.0));
    color.rgb += vec3(0.04, 0.06, 0.08) * crest * u_Intensity;

    fragColor = color;
}
)GLSL";
}

bool PostProcessorWaves::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildWavesFragSrc());
}

void PostProcessorWaves::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorWaves::SetSpeed(float v) {
    m_Speed = std::clamp(v, 0.1f, 3.0f);
}

void PostProcessorWaves::SetFrequency(float v) {
    m_Frequency = std::clamp(v, 1.0f, 25.0f);
}

GLuint PostProcessorWaves::Process(GLuint srcTex, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
        glUniform1f(glGetUniformLocation(prog, "u_Speed"),     m_Speed);
        glUniform1f(glGetUniformLocation(prog, "u_Frequency"), m_Frequency);
        glUniform1f(glGetUniformLocation(prog, "u_Time"),      (float)timeSec);
    });
}

} // namespace ProyecThor::Shaders

