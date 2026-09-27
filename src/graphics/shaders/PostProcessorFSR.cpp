#include "PostProcessorFSR.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <cmath>

static std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "[FSR] No se pudo abrir: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

namespace ProyecThor::Shaders {

static const float k_QuadVerts[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
};

// ---------------------------------------------------------------------------
//  Vertex shader compartido — compatible con GLSL 330
// ---------------------------------------------------------------------------
std::string PostProcessorFSR::BuildVertexShaderSrc() const {
    return R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";
}

// ---------------------------------------------------------------------------
//  Fragment shader EASU
//
//  Macros de compilación condicional en ffx_a.h:
//    A_GPU  = 1  -> activa el bloque GPU (tipos GLSL, funciones GPU)
//    A_GLSL = 1  -> activa el bloque GLSL dentro de A_GPU
//  NO se define A_HALF porque usamos la variante 32-bit (FSR_EASU_F),
//  que es compatible con OpenGL 3.3 sin extensiones de float16.
//
//  El cast AU2(gl_FragCoord.xy) se hace via uvec2() que es GLSL estándar.
//  Los headers de AMD definen AU2 = uvec2 cuando A_GLSL está activo,
//  por lo que FsrEasuF puede usarlo directamente.
// ---------------------------------------------------------------------------
std::string PostProcessorFSR::BuildEASUFragSrc(int /*inputW*/, int /*inputH*/,
                                                int /*outputW*/, int /*outputH*/) const {
    std::string ffx_a   = ReadFileToString("shaders/ffx_a.h");
std::string ffx_fsr = ReadFileToString("shaders/ffx_fsr1.h");

    if (ffx_a.empty() || ffx_fsr.empty()) {
        std::cerr << "[FSR] ADVERTENCIA: No se encontraron los headers AMD FSR. "
                     "Asegurate de que src/shaders/ffx_a.h y src/shaders/ffx_fsr1.h existen.\n";
    }

    std::ostringstream ss;
    ss << "#version 450 core\n"; // Solo una vez
    ss << "#extension GL_ARB_gpu_shader5 : enable\n";
    ss << "#extension GL_ARB_shading_language_packing : enable\n";
    ss << "#define A_GPU  1\n";
    ss << "#define A_GLSL 1\n";
    // Inyección de los headers AMD FSR.
    ss << ffx_a   << "\n";
    // Activa la variante 32-bit de EASU en ffx_fsr1.h.
    ss << "#define FSR_EASU_F 1\n";
    ss << ffx_fsr << "\n";
    // Callbacks que FSR necesita: gather4 de cada canal del color.
    // textureGather es GLSL 1.30+ (disponible en 3.3 core).
    ss << R"GLSL(
uniform sampler2D u_InputTex;
uniform vec4      u_Const0;
uniform vec4      u_Const1;
uniform vec4      u_Const2;
uniform vec4      u_Const3;

AF4 FsrEasuRF(AF2 p) { return textureGather(u_InputTex, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(u_InputTex, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(u_InputTex, p, 2); }

in  vec2 v_UV;
out vec4 fragColor;

void main() {
    // AU2 = uvec2 en el contexto GLSL (definido por ffx_a.h con A_GLSL).
    // gl_FragCoord.xy da la posición del fragmento en pixels enteros.
    AU2 gxy    = AU2(gl_FragCoord.xy);
    AF3 color;
    FsrEasuF(color, gxy,
             AU4(floatBitsToUint(u_Const0.x), floatBitsToUint(u_Const0.y),
                 floatBitsToUint(u_Const0.z), floatBitsToUint(u_Const0.w)),
             AU4(floatBitsToUint(u_Const1.x), floatBitsToUint(u_Const1.y),
                 floatBitsToUint(u_Const1.z), floatBitsToUint(u_Const1.w)),
             AU4(floatBitsToUint(u_Const2.x), floatBitsToUint(u_Const2.y),
                 floatBitsToUint(u_Const2.z), floatBitsToUint(u_Const2.w)),
             AU4(floatBitsToUint(u_Const3.x), floatBitsToUint(u_Const3.y),
                 floatBitsToUint(u_Const3.z), floatBitsToUint(u_Const3.w)));
    fragColor = vec4(color, 1.0);
}
)GLSL";
    return ss.str();
}

// ---------------------------------------------------------------------------
//  Fragment shader RCAS — variante 32-bit (FSR_RCAS_F)
// ---------------------------------------------------------------------------
std::string PostProcessorFSR::BuildRCASFragSrc() const {
    std::string ffx_a   = ReadFileToString("shaders/ffx_a.h"); // Ruta corregida
    std::string ffx_fsr = ReadFileToString("shaders/ffx_fsr1.h");

    std::ostringstream ss;
    ss << "#version 450 core\n"; // Actualizado a 450
    ss << "#extension GL_ARB_gpu_shader5 : enable\n";
    ss << "#define A_GPU  1\n";
    ss << "#define A_GLSL 1\n";
    ss << ffx_a   << "\n";
    ss << "#define FSR_RCAS_F 1\n";
    ss << ffx_fsr << "\n";
    ss << R"GLSL(
uniform sampler2D u_InputTex;
uniform vec4      u_RCASConst;

// RCAS usa texelFetch (acceso por coordenadas enteras), disponible en GLSL 1.30+.
AF4 FsrRcasLoadF(ASU2 p) {
    return texelFetch(u_InputTex, ASU2(p), 0);
}
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

in  vec2 v_UV;
out vec4 fragColor;

void main() {
    AU2 gxy = AU2(gl_FragCoord.xy);
    AF3 color;
    // La constante RCAS se pasa como AU4 via floatBitsToUint,
    // igual que las constantes EASU.
    FsrRcasF(color.r, color.g, color.b, gxy,
             AU4(floatBitsToUint(u_RCASConst.x), floatBitsToUint(u_RCASConst.y),
                 floatBitsToUint(u_RCASConst.z), floatBitsToUint(u_RCASConst.w)));
    fragColor = vec4(color, 1.0);
}
)GLSL";
    return ss.str();
}

// ---------------------------------------------------------------------------
//  CompileShaderStage
// ---------------------------------------------------------------------------
GLuint PostProcessorFSR::CompileShaderStage(GLenum type, const std::string& src) {
    GLuint id = glCreateShader(type);
    const char* cstr = src.c_str();
    glShaderSource(id, 1, &cstr, nullptr);
    glCompileShader(id);

    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetShaderInfoLog(id, len, nullptr, log.data());
        const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        std::cerr << "[FSR] Error compilando shader " << typeName << ":\n"
                  << log.data() << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

// ---------------------------------------------------------------------------
//  LinkProgram
// ---------------------------------------------------------------------------
GLuint PostProcessorFSR::LinkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "[FSR] Error linkeando programa:\n" << log.data() << "\n";
        glDeleteProgram(prog);
        return 0;
    }

    glDetachShader(prog, vert);
    glDetachShader(prog, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

// ---------------------------------------------------------------------------
//  CompileShaders
// ---------------------------------------------------------------------------
bool PostProcessorFSR::CompileShaders() {
    if (m_ProgramEASU) { glDeleteProgram(m_ProgramEASU); m_ProgramEASU = 0; }
    if (m_ProgramRCAS) { glDeleteProgram(m_ProgramRCAS); m_ProgramRCAS = 0; }

    std::string vertSrc = BuildVertexShaderSrc();

    // EASU
    {
        std::string fragSrc = BuildEASUFragSrc(m_LastSrcW, m_LastSrcH,
                                               m_OutputW,  m_OutputH);
        GLuint vert = CompileShaderStage(GL_VERTEX_SHADER,   vertSrc);
        GLuint frag = CompileShaderStage(GL_FRAGMENT_SHADER, fragSrc);
        if (!vert || !frag) {
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }
        m_ProgramEASU = LinkProgram(vert, frag);
        if (!m_ProgramEASU) return false;
    }

    // RCAS
    {
        std::string fragSrc = BuildRCASFragSrc();
        GLuint vert = CompileShaderStage(GL_VERTEX_SHADER,   vertSrc);
        GLuint frag = CompileShaderStage(GL_FRAGMENT_SHADER, fragSrc);
        if (!vert || !frag) {
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }
        m_ProgramRCAS = LinkProgram(vert, frag);
        if (!m_ProgramRCAS) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  CreateFramebuffers
// ---------------------------------------------------------------------------
bool PostProcessorFSR::CreateFramebuffers(int w, int h) {
    DestroyFramebuffers();

    auto makeColorFBO = [&](GLuint& fbo, GLuint& tex) -> bool {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        // GL_RGBA16F disponible en OpenGL 3.0+.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[FSR] FBO incompleto (status=0x"
                      << std::hex << status << std::dec << ")\n";
            return false;
        }
        return true;
    };

    if (!makeColorFBO(m_FBO_EASU, m_Tex_EASU)) return false;
    if (!makeColorFBO(m_FBO_RCAS, m_Tex_RCAS)) return false;
    return true;
}

// ---------------------------------------------------------------------------
//  DestroyFramebuffers
// ---------------------------------------------------------------------------
void PostProcessorFSR::DestroyFramebuffers() {
    if (m_FBO_EASU) { glDeleteFramebuffers(1, &m_FBO_EASU); m_FBO_EASU = 0; }
    if (m_Tex_EASU) { glDeleteTextures(1,    &m_Tex_EASU);  m_Tex_EASU = 0; }
    if (m_FBO_RCAS) { glDeleteFramebuffers(1, &m_FBO_RCAS); m_FBO_RCAS = 0; }
    if (m_Tex_RCAS) { glDeleteTextures(1,    &m_Tex_RCAS);  m_Tex_RCAS = 0; }
}

// ---------------------------------------------------------------------------
//  DrawFullscreenQuad
// ---------------------------------------------------------------------------
void PostProcessorFSR::DrawFullscreenQuad() {
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
//  UpdateEASUConstants
//
//  Calcula las 4 constantes AU4 que FsrEasuF() necesita, siguiendo
//  exactamente la función FsrEasuCon() de la especificación AMD FSR 1.0.
//  Se pasan al shader como vec4 y el shader las convierte via floatBitsToUint.
// ---------------------------------------------------------------------------
void PostProcessorFSR::UpdateEASUConstants(int inputW, int inputH,
                                           int outputW, int outputH) {
    if (!m_ProgramEASU) return;

    float iw = static_cast<float>(inputW);
    float ih = static_cast<float>(inputH);
    float ow = static_cast<float>(outputW);
    float oh = static_cast<float>(outputH);

    // Constantes según FsrEasuCon() de ffx_fsr1.h (lado CPU/GPU).
    // con0: escala de viewport y half-pixel offset
    float c0x = iw / ow;
    float c0y = ih / oh;
    float c0z = 0.5f * (iw / ow) - 0.5f;
    float c0w = 0.5f * (ih / oh) - 0.5f;

    // con1: tamaño de un texel en UV-space + offsets para gather4
    float c1x =  1.0f / iw;
    float c1y =  1.0f / ih;
    float c1z =  1.0f / iw;
    float c1w = -1.0f / ih;

    // con2: offsets adicionales para los taps del filtro
    float c2x = -1.0f / iw;
    float c2y =  2.0f / ih;
    float c2z =  1.0f / iw;
    float c2w =  2.0f / ih;

    // con3: offsets para los taps inferiores
    float c3x =  0.0f / iw;
    float c3y =  4.0f / ih;
    float c3z =  0.0f;
    float c3w =  0.0f;

    glUseProgram(m_ProgramEASU);
    glUniform4f(glGetUniformLocation(m_ProgramEASU, "u_Const0"), c0x, c0y, c0z, c0w);
    glUniform4f(glGetUniformLocation(m_ProgramEASU, "u_Const1"), c1x, c1y, c1z, c1w);
    glUniform4f(glGetUniformLocation(m_ProgramEASU, "u_Const2"), c2x, c2y, c2z, c2w);
    glUniform4f(glGetUniformLocation(m_ProgramEASU, "u_Const3"), c3x, c3y, c3z, c3w);
    glUniform1i(glGetUniformLocation(m_ProgramEASU, "u_InputTex"), 0);
    glUseProgram(0);
}

// ---------------------------------------------------------------------------
//  Init
// ---------------------------------------------------------------------------
bool PostProcessorFSR::Init(int outputW, int outputH) {
    if (m_Initialized) Destroy();

    m_OutputW  = outputW;
    m_OutputH  = outputH;
    m_LastSrcW = outputW;
    m_LastSrcH = outputH;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (!CreateFramebuffers(outputW, outputH)) {
        std::cerr << "[FSR] Fallo al crear framebuffers.\n";
        Destroy();
        return false;
    }

    if (!CompileShaders()) {
        std::cerr << "[FSR] Fallo al compilar shaders.\n";
        Destroy();
        return false;
    }

    UpdateEASUConstants(m_LastSrcW, m_LastSrcH, m_OutputW, m_OutputH);

    m_Initialized = true;
    std::cout << "[FSR] Inicializado: " << outputW << "x" << outputH << "\n";
    return true;
}

// ---------------------------------------------------------------------------
//  Destroy
// ---------------------------------------------------------------------------
void PostProcessorFSR::Destroy() {
    DestroyFramebuffers();

    if (m_ProgramEASU) { glDeleteProgram(m_ProgramEASU); m_ProgramEASU = 0; }
    if (m_ProgramRCAS) { glDeleteProgram(m_ProgramRCAS); m_ProgramRCAS = 0; }
    if (m_VAO)         { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO)         { glDeleteBuffers(1, &m_VBO);      m_VBO = 0; }

    m_Initialized = false;
    std::cout << "[FSR] Recursos destruidos.\n";
}

// ---------------------------------------------------------------------------
//  Resize
// ---------------------------------------------------------------------------
void PostProcessorFSR::Resize(int newOutputW, int newOutputH) {
    if (newOutputW == m_OutputW && newOutputH == m_OutputH) return;
    m_OutputW = newOutputW;
    m_OutputH = newOutputH;
    if (!m_Initialized) return;
    CreateFramebuffers(m_OutputW, m_OutputH);
    CompileShaders();
    UpdateEASUConstants(m_LastSrcW, m_LastSrcH, m_OutputW, m_OutputH);
    std::cout << "[FSR] Resize a " << newOutputW << "x" << newOutputH << "\n";
}

// ---------------------------------------------------------------------------
//  SetSharpness
// ---------------------------------------------------------------------------
void PostProcessorFSR::SetSharpness(float sharpness) {
    m_Sharpness = std::max(0.0f, std::min(2.0f, sharpness));
}

// ---------------------------------------------------------------------------
//  Process
//  Pase 1 (EASU): upscale de srcTex (srcW x srcH) → m_Tex_EASU (outputW x outputH)
//  Pase 2 (RCAS): sharpening de m_Tex_EASU → m_Tex_RCAS
//  Devuelve m_Tex_RCAS (textura final upscaleada y afilada).
// ---------------------------------------------------------------------------
GLuint PostProcessorFSR::Process(GLuint srcTex, int srcW, int srcH) {
    if (!m_Initialized || !m_Enabled) return srcTex;
    if (srcTex == 0)                  return 0;

    if (srcW != m_LastSrcW || srcH != m_LastSrcH) {
        m_LastSrcW = srcW;
        m_LastSrcH = srcH;
        CompileShaders();
        UpdateEASUConstants(srcW, srcH, m_OutputW, m_OutputH);
    }

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Pase 1: EASU
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_EASU);
    glViewport(0, 0, m_OutputW, m_OutputH);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ProgramEASU);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    DrawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    // Pase 2: RCAS
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_RCAS);
    glViewport(0, 0, m_OutputW, m_OutputH);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ProgramRCAS);

    // La constante RCAS es un float que representa la atenuación del sharpening.
    // Se empaqueta como AU4 en FsrRcasCon(). El shader lo recibe via floatBitsToUint.
    float rcasAttenuation = std::exp2(-m_Sharpness);
    glUniform4f(glGetUniformLocation(m_ProgramRCAS, "u_RCASConst"),
                rcasAttenuation, 0.0f, 0.0f, 0.0f);
    glUniform1i(glGetUniformLocation(m_ProgramRCAS, "u_InputTex"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_Tex_EASU);

    DrawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    return m_Tex_RCAS;
}

} // namespace ProyecThor::Shaders
