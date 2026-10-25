#include "PostProcessorGrain.h"
#include <algorithm>

namespace ProyecThor::Shaders {

static std::string BuildGrainFragSrc() {
    return R"GLSL(
#version 330 core
in vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform float u_Time;
uniform float u_Intensity;

// Hash estable para cualquier magnitud de entrada. La version anterior
// (fract(sin(dot(p, ...)) * 43758.5453)) alimentada con gl_FragCoord.xy
// crudo (valores en los miles a 1080p/4K) se ve muy fea en GPU real: sin()
// pierde precision con argumentos grandes y el "ruido" sale bandeado/con
// patron en vez de aleatorio -- eso era el grano "feisimo" reportado, no
// la intensidad ni la animacion.
float Hash(vec2 p) {
    p = fract(p * vec2(443.8975, 397.2973));
    p += dot(p, p.yx + 19.19);
    return fract((p.x + p.y) * p.x);
}

void main() {
    vec4 src = texture(u_InputTex, v_UV);

    // Semilla que cambia cada frame (parpadeo tipo pelicula real) sin
    // meter un numero grande dentro de un seno -- solo un offset chico.
    vec2  seed = gl_FragCoord.xy + fract(u_Time) * 173.0;
    float n    = Hash(seed) - 0.5;

    // Menos grano en sombras y luces bien extremas, mas en tonos medios:
    // asi se lee como grano de pelicula real en vez de estatica pareja de
    // video superpuesta encima de todo.
    float luma     = dot(src.rgb, vec3(0.299, 0.587, 0.114));
    float falloff  = clamp(1.0 - abs(luma - 0.5) * 1.4, 0.25, 1.0);

    fragColor = vec4(src.rgb + n * u_Intensity * falloff, src.a);
}
)GLSL";
}

bool PostProcessorGrain::Init(int w, int h) {
    return m_Pipeline.Init(w, h, BuildGrainFragSrc());
}

void PostProcessorGrain::SetIntensity(float v) {
    m_Intensity = std::clamp(v, 0.0f, 1.0f);
}

GLuint PostProcessorGrain::Process(GLuint srcTex, double timeSeconds) {
    if (!m_Enabled || !m_Pipeline.IsInitialized()) return srcTex;

    return m_Pipeline.Process(srcTex, [&](GLuint prog) {
        glUniform1f(glGetUniformLocation(prog, "u_Time"), (float)timeSeconds);
        glUniform1f(glGetUniformLocation(prog, "u_Intensity"), m_Intensity);
    });
}

} // namespace ProyecThor::Shaders
