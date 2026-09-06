#include "PostProcessorGlitch.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildGlitchFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform float u_Speed;
uniform float u_Time;
uniform int   u_Mode; // 0=Sutil, 1=Cyberpunk RGB, 2=Cinta Analogica

// Generador de pseudo-ruido
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise1D(float x) {
    float i = floor(x);
    float f = fract(x);
    return mix(hash(vec2(i, 0.0)), hash(vec2(i + 1.0, 0.0)), smoothstep(0.0, 1.0, f));
}

void main() {
    float t = u_Time * u_Speed;
    vec2 uv = v_UV;

    // Desplazamiento horizontal por bloques / franjas
    float sliceBlock = floor(uv.y * 24.0);
    float sliceNoise = hash(vec2(sliceBlock, floor(t * 8.0)));
    float sliceThreshold = mix(0.92, 0.65, u_Intensity);

    float shiftX = 0.0;
    if (sliceNoise > sliceThreshold) {
        shiftX = (hash(vec2(sliceBlock * 1.5, floor(t * 12.0))) - 0.5) * 0.08 * u_Intensity;
    }

    // Pequeñas micro-franjas de jitter
    float microNoise = hash(vec2(floor(uv.y * 200.0), floor(t * 30.0)));
    if (microNoise > 0.96) {
        shiftX += (microNoise - 0.98) * 0.04 * u_Intensity;
    }

    vec2 shiftedUV = vec2(clamp(uv.x + shiftX, 0.0, 1.0), uv.y);

    // Separacion de canales cromaticos (RGB Split)
    float splitDist = (0.012 + shiftX * 0.5) * u_Intensity;
    if (u_Mode == 1) {
        // Cyberpunk: RGB Split mas amplio y constante
        splitDist = (0.02 + 0.01 * sin(t * 5.0 + uv.y * 10.0)) * u_Intensity;
    }

    vec4 colR = texture(u_InputTex, vec2(clamp(shiftedUV.x + splitDist, 0.0, 1.0), shiftedUV.y));
    vec4 colG = texture(u_InputTex, shiftedUV);
    vec4 colB = texture(u_InputTex, vec2(clamp(shiftedUV.x - splitDist, 0.0, 1.0), shiftedUV.y));

    vec4 color = vec4(colR.r, colG.g, colB.b, colG.a);

    // Modo 2: Cinta analogica (ruido de estatica y lineas de tracking)
    if (u_Mode == 2) {
        float staticNoise = (hash(uv * vec2(800.0, 600.0) + fract(t * 100.0)) - 0.5) * 0.18 * u_Intensity;
        color.rgb += staticNoise;

        float trackingBar = smoothstep(0.0, 0.06, abs(fract(uv.y * 1.5 - t * 0.4) - 0.5));
        color.rgb = mix(color.rgb * 1.25, color.rgb, trackingBar);
    }

    // Tintes y artefactos digitales
    if (sliceNoise > (sliceThreshold + 0.05) && u_Intensity > 0.2) {
        color.rgb = mix(color.rgb, vec3(color.g, color.b, color.r), 0.35 * u_Intensity);
    }

    vec4 original = texture(u_InputTex, v_UV);
    fragColor = mix(original, color, clamp(u_Intensity * 1.2, 0.0, 1.0));
}
)GLSL";
}

bool PostProcessorGlitch::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildGlitchFragSrc());
}

void PostProcessorGlitch::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorGlitch::SetSpeed(float v) {
    m_Speed = std::clamp(v, 0.1f, 3.0f);
}

void PostProcessorGlitch::SetMode(int mode) {
    m_Mode = std::clamp(mode, 0, 2);
}

GLuint PostProcessorGlitch::Process(GLuint srcTex, int w, int h, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
        glUniform1f(glGetUniformLocation(prog, "u_Speed"),     m_Speed);
        glUniform1f(glGetUniformLocation(prog, "u_Time"),      (float)timeSec);
        glUniform1i(glGetUniformLocation(prog, "u_Mode"),      m_Mode);
    });
}

} // namespace ProyecThor::Shaders

