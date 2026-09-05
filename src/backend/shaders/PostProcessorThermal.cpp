#include "PostProcessorThermal.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildThermalFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform int   u_Mode; // 0=Termico, 1=Vision Nocturna, 2=Solarizado
uniform float u_Time;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Rampa de color termico: Negro -> Azul -> Magenta -> Rojo -> Amarillo -> Blanco
vec3 thermalHeatmap(float val) {
    val = clamp(val, 0.0, 1.0);
    vec3 c0 = vec3(0.0, 0.0, 0.2);
    vec3 c1 = vec3(0.0, 0.2, 0.8);
    vec3 c2 = vec3(0.6, 0.0, 0.7);
    vec3 c3 = vec3(0.9, 0.2, 0.0);
    vec3 c4 = vec3(1.0, 0.9, 0.1);
    vec3 c5 = vec3(1.0, 1.0, 1.0);

    if (val < 0.2) return mix(c0, c1, val / 0.2);
    if (val < 0.4) return mix(c1, c2, (val - 0.2) / 0.2);
    if (val < 0.6) return mix(c2, c3, (val - 0.4) / 0.2);
    if (val < 0.8) return mix(c3, c4, (val - 0.6) / 0.2);
    return mix(c4, c5, (val - 0.8) / 0.2);
}

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    float lum = dot(src.rgb, vec3(0.299, 0.587, 0.114));
    vec3 effectCol = src.rgb;

    if (u_Mode == 0) {
        // 0: Mapa termico infrarrojo
        effectCol = thermalHeatmap(lum);
    } else if (u_Mode == 1) {
        // 1: Vision nocturna verde fosforo + ruido de sensor + viñetado circular
        float noise = (hash(v_UV * 400.0 + fract(u_Time * 50.0)) - 0.5) * 0.15;
        float nvLum = clamp(pow(lum, 0.8) * 1.35 + noise, 0.0, 1.0);
        vec3 greenGlow = vec3(0.15, 1.0, 0.25);
        effectCol = nvLum * greenGlow;

        // Vinetado de lente de visor
        vec2 dist = v_UV - 0.5;
        float vig = 1.0 - smoothstep(0.4, 0.75, length(dist));
        effectCol *= vig;
    } else if (u_Mode == 2) {
        // 2: Solarizado / Inversion cromatica de curvas
        vec3 sol = abs(src.rgb - 0.5) * 2.0;
        sol = sin(sol * 3.14159265);
        effectCol = mix(src.rgb, sol, 0.85);
    }

    fragColor = vec4(mix(src.rgb, effectCol, u_Intensity), src.a);
}
)GLSL";
}

bool PostProcessorThermal::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildThermalFragSrc());
}

void PostProcessorThermal::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorThermal::SetMode(int mode) {
    m_Mode = std::clamp(mode, 0, 2);
}

GLuint PostProcessorThermal::Process(GLuint srcTex, double timeSec) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
        glUniform1i(glGetUniformLocation(prog, "u_Mode"),      m_Mode);
        glUniform1f(glGetUniformLocation(prog, "u_Time"),      (float)timeSec);
    });
}

} // namespace ProyecThor::Shaders

