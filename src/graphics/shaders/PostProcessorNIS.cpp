#include "PostProcessorNIS.h"
#include "NIS_CoefTables.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ProyecThor::Shaders {

static const float k_QuadVerts[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
};

static const char* k_VertSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
//  Fragment shader NIS -- puerto fiel de NVScaler (NIS_Scaler.h del SDK
//  oficial de NVIDIA, MIT License) a un unico pase de fragment shader.
//
//  El original carga un tile+mapa de bordes a memoria COMPARTIDA entre
//  threads de un compute-shader group para no releer la misma region de
//  memoria varias veces. Ese direccionamiento tile-relativo se redujo
//  (ver comentario largo en PostProcessorNIS.h) a esta formula absoluta,
//  independiente de cualquier nocion de "tile":
//
//    ix = floor(srcX), iy = floor(srcY)   (srcX/Y = posicion fuente continua)
//    ventana 6x6 de luma:  P[a][b] = Luma(ix + b - 2, iy + a - 2)
//    celda de borde (i,j) in {0,1}x{0,1}: ventana 3x3 de luma centrada en
//                                          (ix + j, iy + i)
//
//  Con esas dos ventanas, el resto (GetEdgeMap/CalcLTI/GetInterpEdgeMap/
//  EvalPoly6/FilterNormal/AddDirFilters) es un port linea por linea de
//  NIS_Scaler.h -- misma matematica, mismas tablas de coeficientes reales
//  (coef_scale/coef_usm, ver NIS_CoefTables.h), solo que cada fragmento
//  hace sus propios texture() en vez de leer un tile cacheado.
// ---------------------------------------------------------------------------
static const char* k_FragSrc = R"GLSL(
#version 330 core
in  vec2 v_UV;
out vec4 fragColor;

uniform sampler2D u_InputTex;
uniform sampler2D u_CoefScale; // 6 x 64, R32F -- kNisCoefScale[phase][tap]
uniform sampler2D u_CoefUSM;   // 6 x 64, R32F -- kNisCoefUSM[phase][tap]

uniform vec2  u_Scale;     // kScaleX, kScaleY (srcViewport / dstViewport)
uniform vec2  u_SrcNorm;   // 1/srcW, 1/srcH
uniform vec2  u_DstSize;   // outputW, outputH (para reconstruir dstX/dstY)

uniform float u_DetectRatio;
uniform float u_DetectThres;
uniform float u_MinContrastRatio;
uniform float u_RatioNorm;
uniform float u_ContrastBoost;
uniform float u_Eps;
uniform float u_SharpStartY;
uniform float u_SharpScaleY;
uniform float u_SharpStrengthMin;
uniform float u_SharpStrengthScale;
uniform float u_SharpLimitMin;
uniform float u_SharpLimitScale;

float getY(vec3 rgb) {
    return 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
}

float LumaAt(float ix, float iy) {
    vec2 uv = (vec2(ix, iy) + 0.5) * u_SrcNorm;
    return getY(texture(u_InputTex, uv).rgb);
}

float CoefScale(int phase, int tap) { return texelFetch(u_CoefScale, ivec2(tap, phase), 0).r; }
float CoefUSM(int phase, int tap)   { return texelFetch(u_CoefUSM,   ivec2(tap, phase), 0).r; }

// ── GetEdgeMap -- deteccion de gradiente en 4 direcciones sobre una
// ventana 3x3 (p[row][col], row=Y, col=X) ───────────────────────────────
vec4 GetEdgeMap3x3(float p00, float p01, float p02,
                    float p10, float p11, float p12,
                    float p20, float p21, float p22)
{
    float g_0   = abs(p00 + p01 + p02 - p20 - p21 - p22);
    float g_45  = abs(p10 + p00 + p01 - p21 - p22 - p12);
    float g_90  = abs(p00 + p10 + p20 - p02 - p12 - p22);
    float g_135 = abs(p10 + p20 + p21 - p01 - p02 - p12);

    float g_0_90_max = max(g_0, g_90);
    float g_0_90_min = min(g_0, g_90);
    float g_45_135_max = max(g_45, g_135);
    float g_45_135_min = min(g_45, g_135);

    if (g_0_90_max + g_45_135_max == 0.0)
        return vec4(0.0);

    float e_0_90   = min(g_0_90_max / (g_0_90_max + g_45_135_max), 1.0);
    float e_45_135 = 1.0 - e_0_90;

    bool c_0_90     = (g_0_90_max > (g_0_90_min * u_DetectRatio)) && (g_0_90_max > u_DetectThres) && (g_0_90_max > g_45_135_min);
    bool c_45_135   = (g_45_135_max > (g_45_135_min * u_DetectRatio)) && (g_45_135_max > u_DetectThres) && (g_45_135_max > g_0_90_min);
    bool c_g_0_90   = g_0_90_max == g_0;
    bool c_g_45_135 = g_45_135_max == g_45;

    float f_e_0_90   = (c_0_90 && c_45_135) ? e_0_90   : 1.0;
    float f_e_45_135 = (c_0_90 && c_45_135) ? e_45_135 : 1.0;

    float weight_0   = (c_0_90   && c_g_0_90)    ? f_e_0_90   : 0.0;
    float weight_90  = (c_0_90   && !c_g_0_90)   ? f_e_0_90   : 0.0;
    float weight_45  = (c_45_135 && c_g_45_135)  ? f_e_45_135 : 0.0;
    float weight_135 = (c_45_135 && !c_g_45_135) ? f_e_45_135 : 0.0;

    return vec4(weight_0, weight_90, weight_45, weight_135);
}

vec4 EdgeAt(float cx, float cy) {
    return GetEdgeMap3x3(
        LumaAt(cx - 1.0, cy - 1.0), LumaAt(cx, cy - 1.0), LumaAt(cx + 1.0, cy - 1.0),
        LumaAt(cx - 1.0, cy      ), LumaAt(cx, cy      ), LumaAt(cx + 1.0, cy      ),
        LumaAt(cx - 1.0, cy + 1.0), LumaAt(cx, cy + 1.0), LumaAt(cx + 1.0, cy + 1.0));
}

float CalcLTI(float p0, float p1, float p2, float p3, float p4, float p5, int phaseIndex)
{
    bool  selector = (phaseIndex <= 32);
    float sel  = selector ? p0 : p3;
    float a_min = min(min(p1, p2), sel);
    float a_max = max(max(p1, p2), sel);
    sel  = selector ? p2 : p5;
    float b_min = min(min(p3, p4), sel);
    float b_max = max(max(p3, p4), sel);

    float a_cont = a_max - a_min;
    float b_cont = b_max - b_min;

    float cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + u_Eps);
    return (1.0 - clamp((cont_ratio - u_MinContrastRatio) * u_RatioNorm, 0.0, 1.0)) * u_ContrastBoost;
}

vec4 GetInterpEdgeMap(vec4 e00, vec4 e01, vec4 e10, vec4 e11, float fx, float fy)
{
    vec4 h0 = mix(e00, e01, fx);
    vec4 h1 = mix(e10, e11, fx);
    return mix(h0, h1, fy);
}

float EvalPoly6(float pxl[6], int phaseInt)
{
    float y = 0.0;
    for (int i = 0; i < 6; ++i) y += CoefScale(phaseInt, i) * pxl[i];

    float y_usm = 0.0;
    for (int i = 0; i < 6; ++i) y_usm += CoefUSM(phaseInt, i) * pxl[i];

    float y_scale = 1.0 - clamp((y - u_SharpStartY) * u_SharpScaleY, 0.0, 1.0);
    float y_sharpness = y_scale * u_SharpStrengthScale + u_SharpStrengthMin;
    y_usm *= y_sharpness;

    float y_sharpness_limit = (y_scale * u_SharpLimitScale + u_SharpLimitMin) * y;
    y_usm = min(y_sharpness_limit, max(-y_sharpness_limit, y_usm));

    y_usm *= CalcLTI(pxl[0], pxl[1], pxl[2], pxl[3], pxl[4], pxl[5], phaseInt);

    return y + y_usm;
}

float FilterNormal(float p[6][6], int phaseXInt, int phaseYInt)
{
    float h_acc = 0.0;
    for (int j = 0; j < 6; ++j) {
        float v_acc = 0.0;
        for (int i = 0; i < 6; ++i) v_acc += p[i][j] * CoefScale(phaseYInt, i);
        h_acc += v_acc * CoefScale(phaseXInt, j);
    }
    return h_acc;
}

float AddDirFilters(float p[6][6], float phaseXFrac, float phaseYFrac, int phaseXInt, int phaseYInt, vec4 w)
{
    float f = 0.0;

    if (w.x > 0.0) {
        float interp0Deg[6];
        for (int i = 0; i < 6; ++i) interp0Deg[i] = mix(p[i][2], p[i][3], phaseXFrac);
        f += EvalPoly6(interp0Deg, phaseYInt) * w.x;
    }
    if (w.y > 0.0) {
        float interp90Deg[6];
        for (int i = 0; i < 6; ++i) interp90Deg[i] = mix(p[2][i], p[3][i], phaseYFrac);
        f += EvalPoly6(interp90Deg, phaseXInt) * w.y;
    }
    if (w.z > 0.0) {
        float pb45 = 0.5 + 0.5 * (phaseXFrac - phaseYFrac);
        float t1 = mix(p[2][1], p[1][2], pb45);
        float t3 = mix(p[3][2], p[2][3], pb45);
        float t5 = mix(p[4][3], p[3][4], pb45);
        float pb45b = pb45 - 0.5;
        float a = (pb45b >= 0.0) ? p[0][2] : p[2][0];
        float b = (pb45b >= 0.0) ? p[1][3] : p[3][1];
        float c = (pb45b >= 0.0) ? p[2][4] : p[4][2];
        float d = (pb45b >= 0.0) ? p[3][5] : p[5][3];
        float t0 = mix(p[1][1], a, abs(pb45b));
        float t2 = mix(p[2][2], b, abs(pb45b));
        float t4 = mix(p[3][3], c, abs(pb45b));
        float t6 = mix(p[4][4], d, abs(pb45b));

        float interp45Deg[6];
        float pp45 = phaseXFrac + phaseYFrac;
        if (pp45 >= 1.0) {
            interp45Deg[0]=t1; interp45Deg[1]=t2; interp45Deg[2]=t3;
            interp45Deg[3]=t4; interp45Deg[4]=t5; interp45Deg[5]=t6;
            pp45 -= 1.0;
        } else {
            interp45Deg[0]=t0; interp45Deg[1]=t1; interp45Deg[2]=t2;
            interp45Deg[3]=t3; interp45Deg[4]=t4; interp45Deg[5]=t5;
        }
        f += EvalPoly6(interp45Deg, int(pp45 * 64.0)) * w.z;
    }
    if (w.w > 0.0) {
        float pb135 = 0.5 * (phaseXFrac + phaseYFrac);
        float t1 = mix(p[3][1], p[4][2], pb135);
        float t3 = mix(p[2][2], p[3][3], pb135);
        float t5 = mix(p[1][3], p[2][4], pb135);
        float pb135b = pb135 - 0.5;
        float a = (pb135b >= 0.0) ? p[5][2] : p[3][0];
        float b = (pb135b >= 0.0) ? p[4][3] : p[2][1];
        float c = (pb135b >= 0.0) ? p[3][4] : p[1][2];
        float d = (pb135b >= 0.0) ? p[2][5] : p[0][3];
        float t0 = mix(p[4][1], a, abs(pb135b));
        float t2 = mix(p[3][2], b, abs(pb135b));
        float t4 = mix(p[2][3], c, abs(pb135b));
        float t6 = mix(p[1][4], d, abs(pb135b));

        float interp135Deg[6];
        float pp135 = 1.0 + (phaseXFrac - phaseYFrac);
        if (pp135 >= 1.0) {
            interp135Deg[0]=t1; interp135Deg[1]=t2; interp135Deg[2]=t3;
            interp135Deg[3]=t4; interp135Deg[4]=t5; interp135Deg[5]=t6;
            pp135 -= 1.0;
        } else {
            interp135Deg[0]=t0; interp135Deg[1]=t1; interp135Deg[2]=t2;
            interp135Deg[3]=t3; interp135Deg[4]=t4; interp135Deg[5]=t5;
        }
        f += EvalPoly6(interp135Deg, int(pp135 * 64.0)) * w.w;
    }
    return f;
}

void main() {
    vec2 dstXY = floor(v_UV * u_DstSize);

    vec2 srcXY = (dstXY + 0.5) * u_Scale - 0.5;
    vec2 iXY   = floor(srcXY);
    vec2 fXY   = srcXY - iXY;
    ivec2 phaseInt = ivec2(fXY * 64.0);

    // Ventana 6x6 de luma (P[a][b], a=fila/Y, b=columna/X).
    float P[6][6];
    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            P[a][b] = LumaAt(iXY.x + float(b) - 2.0, iXY.y + float(a) - 2.0);

    // Mapa de bordes 2x2 interpolado a la posicion exacta.
    vec4 e00 = EdgeAt(iXY.x,       iXY.y);
    vec4 e01 = EdgeAt(iXY.x + 1.0, iXY.y);
    vec4 e10 = EdgeAt(iXY.x,       iXY.y + 1.0);
    vec4 e11 = EdgeAt(iXY.x + 1.0, iXY.y + 1.0);
    vec4 w = GetInterpEdgeMap(e00, e01, e10, e11, fXY.x, fXY.y);

    float baseWeight = 1.0 - w.x - w.y - w.z - w.w;

    float opY = FilterNormal(P, phaseInt.x, phaseInt.y) * baseWeight;
    opY += AddDirFilters(P, fXY.x, fXY.y, phaseInt.x, phaseInt.y, w);

    vec2 uv = (srcXY + 0.5) * u_SrcNorm;
    vec4 op = texture(u_InputTex, uv);
    float y = getY(op.rgb);

    float corr = opY - y;
    op.rgb += corr;

    fragColor = vec4(clamp(op.rgb, 0.0, 1.0), op.a);
}
)GLSL";

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        std::cerr << "[NIS] Error compilando shader: " << log << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

bool PostProcessorNIS::CompileProgram() {
    GLuint vert = CompileStage(GL_VERTEX_SHADER, k_VertSrc);
    GLuint frag = CompileStage(GL_FRAGMENT_SHADER, k_FragSrc);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    m_Program = glCreateProgram();
    glAttachShader(m_Program, vert);
    glAttachShader(m_Program, frag);
    glLinkProgram(m_Program);

    GLint ok = 0;
    glGetProgramiv(m_Program, GL_LINK_STATUS, &ok);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(m_Program, sizeof(log), nullptr, log);
        std::cerr << "[NIS] Error linkeando programa: " << log << "\n";
        glDeleteProgram(m_Program);
        m_Program = 0;
        return false;
    }
    return true;
}

bool PostProcessorNIS::CreateFramebuffer(int w, int h) {
    DestroyFramebuffer();

    glGenTextures(1, &m_Tex);
    glBindTexture(GL_TEXTURE_2D, m_Tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[NIS] FBO incompleto (status=0x" << std::hex << status << std::dec << ")\n";
        return false;
    }
    return true;
}

void PostProcessorNIS::DestroyFramebuffer() {
    if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_Tex) { glDeleteTextures(1, &m_Tex);      m_Tex = 0; }
}

// Sube las tablas reales de NVIDIA (NIS_CoefTables.h) como texturas R32F de
// 6x64 -- se hace una sola vez, las tablas son constantes.
void PostProcessorNIS::EnsureCoefTextures() {
    if (m_CoefScaleTex && m_CoefUSMTex) return;

    auto upload = [](GLuint& tex, const float table[64][6]) {
        std::vector<float> flat(64 * 6);
        for (int phase = 0; phase < 64; ++phase)
            for (int tap = 0; tap < 6; ++tap)
                flat[phase * 6 + tap] = table[phase][tap];

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 6, 64, 0, GL_RED, GL_FLOAT, flat.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    };

    upload(m_CoefScaleTex, kNisCoefScale);
    upload(m_CoefUSMTex,   kNisCoefUSM);
}

void PostProcessorNIS::DrawFullscreenQuad() {
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

bool PostProcessorNIS::Init(int outputW, int outputH) {
    if (m_Initialized) Destroy();

    m_OutputW = outputW;
    m_OutputH = outputH;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!CreateFramebuffer(outputW, outputH)) { Destroy(); return false; }
    if (!CompileProgram())                    { Destroy(); return false; }
    EnsureCoefTextures();

    m_Initialized = true;
    std::cout << "[NIS] Inicializado: " << outputW << "x" << outputH << "\n";
    return true;
}

void PostProcessorNIS::Destroy() {
    DestroyFramebuffer();
    if (m_Program)      { glDeleteProgram(m_Program);  m_Program = 0; }
    if (m_CoefScaleTex) { glDeleteTextures(1, &m_CoefScaleTex); m_CoefScaleTex = 0; }
    if (m_CoefUSMTex)   { glDeleteTextures(1, &m_CoefUSMTex);   m_CoefUSMTex = 0; }
    if (m_VAO)          { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO)          { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }
    m_Initialized = false;
}

void PostProcessorNIS::Resize(int newOutputW, int newOutputH) {
    if (newOutputW == m_OutputW && newOutputH == m_OutputH) return;
    m_OutputW = newOutputW;
    m_OutputH = newOutputH;
    if (!m_Initialized) return;
    CreateFramebuffer(m_OutputW, m_OutputH);
}

void PostProcessorNIS::SetSharpness(float sharpness) {
    m_Sharpness = std::clamp(sharpness, 0.0f, 1.0f);
}

// Port directo de NVScalerUpdateConfig (NIS_Config.h), rama SDR (sin
// HDR) -- calcula las mismas constantes que el algoritmo real a partir
// del slider de sharpness 0..1.
struct NISConfigCPU {
    float scaleX, scaleY, srcNormX, srcNormY;
    float detectRatio, detectThres, minContrastRatio, ratioNorm;
    float contrastBoost, eps;
    float sharpStartY, sharpScaleY;
    float sharpStrengthMin, sharpStrengthScale;
    float sharpLimitMin, sharpLimitScale;
    bool  valid;
};

static NISConfigCPU ComputeNISConfig(float sharpness, int srcW, int srcH, int dstW, int dstH) {
    NISConfigCPU c{};
    sharpness = std::clamp(sharpness, 0.0f, 1.0f);
    float sharpenSlider = sharpness - 0.5f;

    const float maxScale   = (sharpenSlider >= 0.0f) ? 1.25f : 1.75f;
    const float minScale   = (sharpenSlider >= 0.0f) ? 1.25f : 1.0f;
    const float limitScale = (sharpenSlider >= 0.0f) ? 1.25f : 1.0f;

    const float kDetectRatio = 2.0f * 1127.0f / 1024.0f;
    const float kDetectThres = 64.0f / 1024.0f;
    const float kMinContrastRatio = 2.0f;
    const float kMaxContrastRatio = 10.0f;

    const float kSharpStartY = 0.45f;
    const float kSharpEndY   = 0.9f;
    const float kSharpStrengthMin = std::max(0.0f, 0.4f + sharpenSlider * minScale * 1.2f);
    const float kSharpStrengthMax = 1.6f + sharpenSlider * maxScale * 1.8f;
    const float kSharpLimitMin = std::max(0.1f, 0.14f + sharpenSlider * limitScale * 0.32f);
    const float kSharpLimitMax = 0.5f + sharpenSlider * limitScale * 0.6f;

    c.ratioNorm          = 1.0f / (kMaxContrastRatio - kMinContrastRatio);
    c.sharpScaleY        = 1.0f / (kSharpEndY - kSharpStartY);
    c.sharpStrengthMin   = kSharpStrengthMin;
    c.sharpStrengthScale = kSharpStrengthMax - kSharpStrengthMin;
    c.sharpLimitMin      = kSharpLimitMin;
    c.sharpLimitScale    = kSharpLimitMax - kSharpLimitMin;
    c.sharpStartY        = kSharpStartY;

    c.detectRatio      = kDetectRatio;
    c.detectThres      = kDetectThres;
    c.minContrastRatio = kMinContrastRatio;
    c.contrastBoost    = 1.0f;
    c.eps              = 1.0f / 255.0f;

    c.srcNormX = 1.0f / static_cast<float>(srcW);
    c.srcNormY = 1.0f / static_cast<float>(srcH);
    c.scaleX   = static_cast<float>(srcW) / static_cast<float>(dstW);
    c.scaleY   = static_cast<float>(srcH) / static_cast<float>(dstH);

    // NVScalerUpdateConfig solo admite achicar la escala fuente/destino a
    // como mucho 2x (kScaleX/Y en [0.5, 1.0]) -- fuera de ese rango NVIDIA
    // considera el pedido invalido y no corre el escalador.
    c.valid = (c.scaleX >= 0.5f && c.scaleX <= 1.0f && c.scaleY >= 0.5f && c.scaleY <= 1.0f);
    return c;
}

GLuint PostProcessorNIS::Process(GLuint srcTex, int srcW, int srcH) {
    if (!m_Initialized || !m_Enabled) return srcTex;
    if (srcTex == 0 || srcW <= 0 || srcH <= 0) return srcTex;

    NISConfigCPU cfg = ComputeNISConfig(m_Sharpness, srcW, srcH, m_OutputW, m_OutputH);
    if (!cfg.valid) return srcTex; // relacion de escala fuera de lo que NIS soporta

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_OutputW, m_OutputH);

    glUseProgram(m_Program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_InputTex"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_CoefScaleTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_CoefScale"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_CoefUSMTex);
    glUniform1i(glGetUniformLocation(m_Program, "u_CoefUSM"), 2);

    glUniform2f(glGetUniformLocation(m_Program, "u_Scale"),   cfg.scaleX, cfg.scaleY);
    glUniform2f(glGetUniformLocation(m_Program, "u_SrcNorm"), cfg.srcNormX, cfg.srcNormY);
    glUniform2f(glGetUniformLocation(m_Program, "u_DstSize"), (float)m_OutputW, (float)m_OutputH);

    glUniform1f(glGetUniformLocation(m_Program, "u_DetectRatio"),      cfg.detectRatio);
    glUniform1f(glGetUniformLocation(m_Program, "u_DetectThres"),      cfg.detectThres);
    glUniform1f(glGetUniformLocation(m_Program, "u_MinContrastRatio"), cfg.minContrastRatio);
    glUniform1f(glGetUniformLocation(m_Program, "u_RatioNorm"),        cfg.ratioNorm);
    glUniform1f(glGetUniformLocation(m_Program, "u_ContrastBoost"),    cfg.contrastBoost);
    glUniform1f(glGetUniformLocation(m_Program, "u_Eps"),              cfg.eps);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpStartY"),      cfg.sharpStartY);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpScaleY"),      cfg.sharpScaleY);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpStrengthMin"),   cfg.sharpStrengthMin);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpStrengthScale"), cfg.sharpStrengthScale);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpLimitMin"),      cfg.sharpLimitMin);
    glUniform1f(glGetUniformLocation(m_Program, "u_SharpLimitScale"),    cfg.sharpLimitScale);

    DrawFullscreenQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glUseProgram(0);

    return m_Tex;
}

} // namespace ProyecThor::Shaders
