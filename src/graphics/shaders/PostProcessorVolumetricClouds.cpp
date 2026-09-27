#include "PostProcessorVolumetricClouds.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildVolumetricCloudsFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Coverage;
uniform float u_Density;
uniform float u_Speed;
uniform float u_SunIntensity;
uniform float u_Time;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < 6; ++i) {
        v += a * noise(p);
        p = rot * p * 2.02 + vec2(100.0);
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = v_UV;
    float t = u_Time * u_Speed * 0.08;

    // Desplazamiento de viento en capas de altitud
    vec2 cloudUV1 = uv * 3.2 + vec2(t * 1.0, t * 0.2);
    vec2 cloudUV2 = uv * 6.4 + vec2(t * 1.7, t * 0.35);

    float n1 = fbm(cloudUV1);
    float n2 = fbm(cloudUV2 + n1 * 1.4);

    float cloud = n1 * 0.65 + n2 * 0.35;
    
    // Cobertura y umbral de nube
    float threshold = 1.0 - u_Coverage;
    float density = smoothstep(threshold - 0.25, threshold + 0.28, cloud) * u_Density;

    // Iluminación solar simulada (Silver Lining)
    vec2 sunPos = vec2(0.82, 0.15);
    float distToSun = length(uv - sunPos);
    float sunGlow = exp(-distToSun * 2.6) * u_SunIntensity;

    // Sombra propia y borde brillante
    float rim = smoothstep(0.25, 0.75, density) * (1.0 - smoothstep(0.75, 1.0, density));
    vec3 cloudColor = mix(vec3(0.72, 0.76, 0.84), vec3(0.98, 0.98, 1.0), density);
    cloudColor += vec3(1.0, 0.9, 0.7) * rim * u_SunIntensity;
    cloudColor += vec3(1.0, 0.85, 0.6) * sunGlow * 0.5;

    vec4 baseCol = texture(u_InputTex, uv);
    vec3 finalCol = mix(baseCol.rgb, cloudColor, clamp(density * 0.85, 0.0, 1.0));
    finalCol += vec3(1.0, 0.9, 0.7) * sunGlow * (1.0 - density * 0.5) * 0.35;

    fragColor = vec4(finalCol, baseCol.a);
}
)GLSL";
}

bool PostProcessorVolumetricClouds::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildVolumetricCloudsFragSrc());
}

void PostProcessorVolumetricClouds::SetCoverage(float v) {
    m_Coverage = std::clamp(v, 0.05f, 1.0f);
}

void PostProcessorVolumetricClouds::SetDensity(float v) {
    m_Density = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorVolumetricClouds::SetSpeed(float v) {
    m_Speed = std::clamp(v, 0.1f, 3.0f);
}

void PostProcessorVolumetricClouds::SetSunIntensity(float v) {
    m_SunIntensity = std::clamp(v, 0.0f, 1.5f);
}

GLuint PostProcessorVolumetricClouds::Process(GLuint srcTex, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [this, timeSec](GLuint program) {
        GLint locCoverage = glGetUniformLocation(program, "u_Coverage");
        if (locCoverage >= 0) glUniform1f(locCoverage, m_Coverage);

        GLint locDensity = glGetUniformLocation(program, "u_Density");
        if (locDensity >= 0) glUniform1f(locDensity, m_Density);

        GLint locSpeed = glGetUniformLocation(program, "u_Speed");
        if (locSpeed >= 0) glUniform1f(locSpeed, m_Speed);

        GLint locSun = glGetUniformLocation(program, "u_SunIntensity");
        if (locSun >= 0) glUniform1f(locSun, m_SunIntensity);

        GLint locTime = glGetUniformLocation(program, "u_Time");
        if (locTime >= 0) glUniform1f(locTime, static_cast<float>(timeSec));
    });
}

} // namespace ProyecThor::Shaders

