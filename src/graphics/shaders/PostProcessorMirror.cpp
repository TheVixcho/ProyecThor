#include "PostProcessorMirror.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildMirrorFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform int u_Mode; // 0=Horizontal, 1=Vertical, 2=Caleidoscopio 4x, 3=Radial 8x

void main() {
    vec2 uv = v_UV;

    if (u_Mode == 0) {
        // 0: Espejo Horizontal (mitad izquierda reflejada a la derecha)
        uv.x = uv.x > 0.5 ? 1.0 - uv.x : uv.x;
    } else if (u_Mode == 1) {
        // 1: Espejo Vertical (mitad superior reflejada a la inferior)
        uv.y = uv.y > 0.5 ? 1.0 - uv.y : uv.y;
    } else if (u_Mode == 2) {
        // 2: Cuadrante Caleidoscopio 4x
        uv.x = uv.x > 0.5 ? 1.0 - uv.x : uv.x;
        uv.y = uv.y > 0.5 ? 1.0 - uv.y : uv.y;
    } else if (u_Mode == 3) {
        // 3: Caleidoscopio Radial 8 facetas
        vec2 p = uv - 0.5;
        float r = length(p);
        float a = atan(p.y, p.x);
        float pi = 3.14159265;
        float segment = pi / 4.0; // 8 segmentos
        a = mod(a, segment);
        a = abs(a - segment * 0.5);
        p = vec2(cos(a), sin(a)) * r;
        uv = clamp(p + 0.5, 0.0, 1.0);
    }

    fragColor = texture(u_InputTex, uv);
}
)GLSL";
}

bool PostProcessorMirror::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildMirrorFragSrc());
}

void PostProcessorMirror::SetMode(int mode) {
    m_Mode = std::clamp(mode, 0, 3);
}

GLuint PostProcessorMirror::Process(GLuint srcTex) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1i(glGetUniformLocation(prog, "u_Mode"), m_Mode);
    });
}

} // namespace ProyecThor::Shaders

