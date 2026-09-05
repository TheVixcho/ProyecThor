#include "PostProcessorColorGrading.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildColorGradingFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Intensity;
uniform int   u_Preset;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec4 src = texture(u_InputTex, v_UV);
    vec3 col = src.rgb;
    float lum = dot(col, vec3(0.299, 0.587, 0.114));

    vec3 graded = col;

    if (u_Preset == 0) {
        // 0: Cálido Dorado (Golden Hour)
        vec3 warmShadow = vec3(0.35, 0.20, 0.08);
        vec3 warmHigh   = vec3(1.05, 0.92, 0.70);
        graded = mix(warmShadow, warmHigh, col);
        graded.r *= 1.12;
        graded.b *= 0.82;
    } else if (u_Preset == 1) {
        // 1: Teal & Orange (Hollywood)
        vec3 shadowTeal  = vec3(0.05, 0.28, 0.38);
        vec3 highlightOr = vec3(1.10, 0.78, 0.45);
        vec3 split = mix(shadowTeal, highlightOr, smoothstep(0.15, 0.85, lum));
        graded = col * split * 1.35;
    } else if (u_Preset == 2) {
        // 2: Cyberpunk / Noche Neón (Magenta & Cyan)
        vec3 cyanShad = vec3(0.0, 0.45, 0.65);
        vec3 pinkHigh = vec3(1.2, 0.25, 0.75);
        graded = mix(cyanShad, pinkHigh, smoothstep(0.1, 0.9, lum));
        graded = mix(graded, col, 0.3);
    } else if (u_Preset == 3) {
        // 3: Sepia / Vintage
        vec3 sepia = vec3(
            dot(col, vec3(0.393, 0.769, 0.189)),
            dot(col, vec3(0.349, 0.686, 0.168)),
            dot(col, vec3(0.272, 0.534, 0.131))
        );
        graded = sepia * 1.05;
    } else if (u_Preset == 4) {
        // 4: Noir B&W Alto Contraste
        float noir = smoothstep(0.12, 0.90, lum);
        graded = vec3(pow(noir, 1.25));
    } else if (u_Preset == 5) {
        // 5: Matrix Verde
        vec3 greenTint = vec3(0.2, 1.05, 0.35) * lum;
        graded = mix(vec3(lum * 0.4), greenTint, smoothstep(0.1, 0.8, lum));
    } else if (u_Preset == 6) {
        // 6: Pastel Soft Aesthetic
        vec3 hsv = rgb2hsv(col);
        hsv.y = clamp(hsv.y * 0.75, 0.0, 1.0); // menor saturación
        hsv.z = pow(hsv.z, 0.85) * 1.05;      // sombras levantadas
        graded = hsv2rgb(hsv) + vec3(0.04, 0.02, 0.06);
    }

    fragColor = vec4(mix(src.rgb, graded, u_Intensity), src.a);
}
)GLSL";
}

bool PostProcessorColorGrading::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildColorGradingFragSrc());
}

void PostProcessorColorGrading::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

void PostProcessorColorGrading::SetPreset(int preset) {
    m_Preset = std::clamp(preset, 0, 6);
}

GLuint PostProcessorColorGrading::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
        glUniform1i(glGetUniformLocation(prog, "u_Preset"),    m_Preset);
    });
}

} // namespace ProyecThor::Shaders

