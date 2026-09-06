#include "PostProcessorRadialBlur.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildRadialBlurFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;

void main() {
    vec2 center = vec2(0.5, 0.5);
    vec2 toCenter = center - v_UV;
    float dist = length(toCenter);

    // Muestreo radial ponderado
    const int SAMPLES = 14;
    vec4 sum = vec4(0.0);
    float totalWeight = 0.0;
    float stepFactor = (u_Intensity * 0.12) / float(SAMPLES);

    for (int i = 0; i < SAMPLES; i++) {
        float scale = 1.0 + float(i) * stepFactor;
        vec2 sampleUV = clamp(center - toCenter * scale, 0.0, 1.0);
        float weight = 1.0 - (float(i) / float(SAMPLES)) * 0.5;
        sum += texture(u_InputTex, sampleUV) * weight;
        totalWeight += weight;
    }

    vec4 blurred = sum / totalWeight;
    vec4 original = texture(u_InputTex, v_UV);

    // El desenfoque radial se intensifica naturalmente en la periferia
    float edgeMask = smoothstep(0.1, 0.7, dist);
    fragColor = mix(original, blurred, edgeMask * clamp(u_Intensity * 1.5, 0.0, 1.0));
}
)GLSL";
}

bool PostProcessorRadialBlur::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildRadialBlurFragSrc());
}

void PostProcessorRadialBlur::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorRadialBlur::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders

