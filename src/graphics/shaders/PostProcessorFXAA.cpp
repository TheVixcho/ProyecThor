#include "PostProcessorFXAA.h"

namespace ProyecThor::Shaders {

static std::string BuildFXAAFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform vec2 u_RcpFrame;

#define FXAA_REDUCE_MIN (1.0/128.0)
#define FXAA_REDUCE_MUL (1.0/8.0)
#define FXAA_SPAN_MAX   8.0

vec3 FXAA(sampler2D tex, vec2 uv, vec2 rcpFrame) {
    vec3 rgbNW = texture(tex, uv + vec2(-1.0, -1.0) * rcpFrame).rgb;
    vec3 rgbNE = texture(tex, uv + vec2( 1.0, -1.0) * rcpFrame).rgb;
    vec3 rgbSW = texture(tex, uv + vec2(-1.0,  1.0) * rcpFrame).rgb;
    vec3 rgbSE = texture(tex, uv + vec2( 1.0,  1.0) * rcpFrame).rgb;
    vec3 rgbM  = texture(tex, uv).rgb;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * FXAA_REDUCE_MUL * 0.25, FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * rcpFrame;

    vec3 rgbA = 0.5 * (
        texture(tex, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(tex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(tex, uv + dir * -0.5).rgb +
        texture(tex, uv + dir *  0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax) return rgbA;
    return rgbB;
}

void main() {
    fragColor = vec4(FXAA(u_InputTex, v_UV, u_RcpFrame), 1.0);
}
)GLSL";
}

bool PostProcessorFXAA::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildFXAAFragSrc());
}

GLuint PostProcessorFXAA::Process(GLuint srcTex, int w, int h) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_RcpFrame"), 1.0f / (float)w, 1.0f / (float)h);
    });
}

} // namespace ProyecThor::Shaders
