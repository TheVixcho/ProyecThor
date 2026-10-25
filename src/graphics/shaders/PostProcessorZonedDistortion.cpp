#include "PostProcessorZonedDistortion.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildZonedDistortionFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform float u_Speed;
uniform int   u_Zone;
uniform float u_Feather;
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

void main() {
    vec2 uv = v_UV;
    float t = u_Time * u_Speed;

    // Calcular máscara de zona
    float zoneMask = 0.0;
    float f = max(0.01, u_Feather);

    if (u_Zone == 0) {
        // Inferior / Suelo (Ondas de Calor / Fuego en la base)
        zoneMask = smoothstep(0.35 - f, 0.95 + f, uv.y);
    } else if (u_Zone == 1) {
        // Superior / Cielo (Turbulencia Atmosférica)
        zoneMask = 1.0 - smoothstep(0.05 - f, 0.65 + f, uv.y);
    } else if (u_Zone == 2) {
        // Centro Focal (Vórtice / Ondas Concéntricas)
        float d = length(uv - vec2(0.5, 0.5));
        zoneMask = 1.0 - smoothstep(0.15 - f, 0.55 + f, d);
    } else if (u_Zone == 3) {
        // Lateral Izquierdo
        zoneMask = 1.0 - smoothstep(0.05 - f, 0.50 + f, uv.x);
    } else if (u_Zone == 4) {
        // Lateral Derecho
        zoneMask = smoothstep(0.50 - f, 0.95 + f, uv.x);
    }

    // Turbulencia de calor y distorsión
    vec2 p = uv * 12.0;
    float nX = noise(p + vec2(0.0, -t * 2.5)) - 0.5;
    float nY = noise(p + vec2(5.2, -t * 3.0)) - 0.5;

    float wave = sin(uv.y * 25.0 - t * 4.0) * 0.015;

    vec2 offset = vec2(nX * 0.03 + wave, nY * 0.02) * u_Intensity * zoneMask;
    vec2 distortedUV = clamp(uv + offset, 0.0, 1.0);

    vec4 baseCol = texture(u_InputTex, distortedUV);

    // Sutil brillo de refracción en las ondas de calor
    float caustics = abs(nX + nY) * 0.08 * u_Intensity * zoneMask;
    baseCol.rgb += vec3(caustics);

    fragColor = baseCol;
}
)GLSL";
}

bool PostProcessorZonedDistortion::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildZonedDistortionFragSrc());
}

void PostProcessorZonedDistortion::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorZonedDistortion::SetSpeed(float v) {
    m_Speed = std::clamp(v, 0.1f, 4.0f);
}

void PostProcessorZonedDistortion::SetZone(int z) {
    m_Zone = std::clamp(z, 0, 4);
}

void PostProcessorZonedDistortion::SetFeather(float v) {
    m_Feather = std::clamp(v, 0.05f, 0.6f);
}

GLuint PostProcessorZonedDistortion::Process(GLuint srcTex, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [this, timeSec](GLuint program) {
        GLint locInt = glGetUniformLocation(program, "u_Intensity");
        if (locInt >= 0) glUniform1f(locInt, m_Intensity);

        GLint locSpeed = glGetUniformLocation(program, "u_Speed");
        if (locSpeed >= 0) glUniform1f(locSpeed, m_Speed);

        GLint locZone = glGetUniformLocation(program, "u_Zone");
        if (locZone >= 0) glUniform1i(locZone, m_Zone);

        GLint locFeather = glGetUniformLocation(program, "u_Feather");
        if (locFeather >= 0) glUniform1f(locFeather, m_Feather);

        GLint locTime = glGetUniformLocation(program, "u_Time");
        if (locTime >= 0) glUniform1f(locTime, static_cast<float>(timeSec));
    });
}

} // namespace ProyecThor::Shaders

