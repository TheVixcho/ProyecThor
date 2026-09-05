#include "PostProcessorVolumetricFog.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildVolumetricFogFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Density;
uniform float u_Speed;
uniform float u_Scale;
uniform int   u_ColorMode;
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
    vec2 shift = vec2(100.0);
    mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.5));
    for (int i = 0; i < 5; ++i) {
        v += a * noise(p);
        p = rot * p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = v_UV;
    float t = u_Time * u_Speed * 0.22;

    // Movimiento de convección ascendente y turbulencia de remolinos
    vec2 motion1 = vec2(t * 0.35, -t * 0.65);
    vec2 motion2 = vec2(-t * 0.20, -t * 0.45);

    vec2 q = vec2(0.0);
    q.x = fbm(uv * u_Scale + motion1);
    q.y = fbm(uv * u_Scale + motion2 + vec2(5.2, 1.3));

    vec2 r = vec2(0.0);
    r.x = fbm(uv * u_Scale + 4.0 * q + vec2(1.7, 9.2) + motion1 * 0.5);
    r.y = fbm(uv * u_Scale + 4.0 * q + vec2(8.3, 2.8) + motion2 * 0.5);

    float f = fbm(uv * u_Scale + 4.0 * r + t * 0.12);

    // Forma volumétrica y densidad
    float smoke = smoothstep(0.12, 0.88, f) * u_Density;

    // Color y tinte del humo
    vec3 smokeTint = vec3(0.85, 0.88, 0.93); // 0=Gris / Humo realista
    if (u_ColorMode == 1) smokeTint = vec3(0.20, 0.80, 1.00);  // 1=Místico / Cian
    else if (u_ColorMode == 2) smokeTint = vec3(1.00, 0.45, 0.15); // 2=Fuego / Cálido
    else if (u_ColorMode == 3) smokeTint = vec3(0.85, 0.25, 0.95); // 3=Cyber / Neón

    vec4 baseCol = texture(u_InputTex, uv);

    // Mezcla volumétrica con absorción de luz suave
    vec3 mixed = mix(baseCol.rgb, smokeTint * (0.65 + 0.35 * f), smoke * 0.85);
    mixed += smokeTint * pow(smoke, 2.2) * 0.3;

    fragColor = vec4(mixed, baseCol.a);
}
)GLSL";
}

bool PostProcessorVolumetricFog::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildVolumetricFogFragSrc());
}

void PostProcessorVolumetricFog::SetDensity(float v) {
    m_Density = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorVolumetricFog::SetSpeed(float v) {
    m_Speed = std::clamp(v, 0.1f, 4.0f);
}

void PostProcessorVolumetricFog::SetScale(float v) {
    m_Scale = std::clamp(v, 1.0f, 10.0f);
}

void PostProcessorVolumetricFog::SetColorMode(int mode) {
    m_ColorMode = std::clamp(mode, 0, 3);
}

GLuint PostProcessorVolumetricFog::Process(GLuint srcTex, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [this, timeSec](GLuint program) {
        GLint locDensity = glGetUniformLocation(program, "u_Density");
        if (locDensity >= 0) glUniform1f(locDensity, m_Density);

        GLint locSpeed = glGetUniformLocation(program, "u_Speed");
        if (locSpeed >= 0) glUniform1f(locSpeed, m_Speed);

        GLint locScale = glGetUniformLocation(program, "u_Scale");
        if (locScale >= 0) glUniform1f(locScale, m_Scale);

        GLint locMode = glGetUniformLocation(program, "u_ColorMode");
        if (locMode >= 0) glUniform1i(locMode, m_ColorMode);

        GLint locTime = glGetUniformLocation(program, "u_Time");
        if (locTime >= 0) glUniform1f(locTime, static_cast<float>(timeSec));
    });
}

} // namespace ProyecThor::Shaders

