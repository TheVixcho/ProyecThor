#include "ImageView.h"
#include <imgui.h>
#include "stb_image.h"
#include <iostream>
#include <cstring>
#include <cmath>

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

static const char* k_FragSrc = R"GLSL(
#version 330 core
in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec2      u_TexSize;
uniform float     u_Brightness;
uniform float     u_Contrast;
uniform float     u_Saturation;
uniform float     u_Hue;          // en radianes
uniform float     u_Temperature;
uniform float     u_Sharpness;
uniform float     u_Gamma;
uniform float     u_Vignette;

// ── Rotacion de hue via matriz YIQ ──────────────────────────────────────────
vec3 applyHue(vec3 c, float angle) {
    float cosA = cos(angle);
    float sinA = sin(angle);
    mat3 m = mat3(
        0.299 + cosA*0.701 - sinA*0.168,
        0.299 - cosA*0.299 - sinA*0.328,
        0.299 - cosA*0.299 + sinA*1.250,

        0.587 - cosA*0.587 + sinA*0.330,
        0.587 + cosA*0.413 + sinA*0.035,
        0.587 - cosA*0.587 - sinA*1.050,

        0.114 - cosA*0.114 + sinA*0.886,
        0.114 - cosA*0.114 + sinA*0.292,
        0.114 + cosA*0.886 - sinA*0.203
    );
    return clamp(m * c, 0.0, 1.0);
}

// ── Luminancia ───────────────────────────────────────────────────────────────
float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 texel = 1.0 / u_TexSize;

    // ── Sharpness: unsharp mask con kernel 3x3 ───────────────────────────────
    vec3 col = texture(u_Texture, v_UV).rgb;
    if (u_Sharpness > 0.001) {
        vec3 blur =
            texture(u_Texture, v_UV + vec2(-texel.x, -texel.y)).rgb +
            texture(u_Texture, v_UV + vec2( 0.0,     -texel.y)).rgb +
            texture(u_Texture, v_UV + vec2( texel.x, -texel.y)).rgb +
            texture(u_Texture, v_UV + vec2(-texel.x,  0.0    )).rgb +
            texture(u_Texture, v_UV + vec2( texel.x,  0.0    )).rgb +
            texture(u_Texture, v_UV + vec2(-texel.x,  texel.y)).rgb +
            texture(u_Texture, v_UV + vec2( 0.0,      texel.y)).rgb +
            texture(u_Texture, v_UV + vec2( texel.x,  texel.y)).rgb;
        blur /= 8.0;
        col = clamp(col + (col - blur) * u_Sharpness * 2.0, 0.0, 1.0);
    }

    // ── Brillo ───────────────────────────────────────────────────────────────
    col = clamp(col + u_Brightness, 0.0, 1.0);

    // ── Contraste (pivote en 0.5) ─────────────────────────────────────────────
    col = clamp((col - 0.5) * u_Contrast + 0.5, 0.0, 1.0);

    // ── Saturacion ───────────────────────────────────────────────────────────
    float lum = luminance(col);
    col = clamp(mix(vec3(lum), col, u_Saturation), 0.0, 1.0);

    // ── Hue ──────────────────────────────────────────────────────────────────
    if (abs(u_Hue) > 0.001) {
        col = applyHue(col, u_Hue);
    }

    // ── Temperatura (frio/calido) ─────────────────────────────────────────────
    // positivo = calido (sube R, baja B), negativo = frio (baja R, sube B)
    col.r = clamp(col.r + u_Temperature * 0.15, 0.0, 1.0);
    col.g = clamp(col.g + u_Temperature * 0.05, 0.0, 1.0);
    col.b = clamp(col.b - u_Temperature * 0.15, 0.0, 1.0);

    // ── Gamma ─────────────────────────────────────────────────────────────────
    col = pow(max(col, vec3(0.0001)), vec3(1.0 / u_Gamma));

    // ── Viñeta ────────────────────────────────────────────────────────────────
    if (u_Vignette > 0.001) {
        vec2 uv     = v_UV - 0.5;
        float dist  = length(uv) * 1.414; // normalizado a 1.0 en esquinas
        float fade  = 1.0 - smoothstep(0.3, 1.0, dist * u_Vignette * 1.8);
        col *= fade;
    }

    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
)GLSL";

namespace ProyecThor::UI {

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, 1024, nullptr, log);
        std::cerr << "[ImageView] Shader compile error:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, 1024, nullptr, log);
        std::cerr << "[ImageView] Program link error:\n" << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

ImageView::ImageView() {
    InitShader();
    InitQuad();
}

ImageView::~ImageView() {
    Clear();
    DestroyShader();
    DestroyQuad();
}

void ImageView::InitShader() {
    GLuint vert = CompileShader(GL_VERTEX_SHADER,   k_VertSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, k_FragSrc);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return;
    }
    m_ShaderProgram = LinkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!m_ShaderProgram) return;

    m_uTexture     = glGetUniformLocation(m_ShaderProgram, "u_Texture");
    m_uBrightness  = glGetUniformLocation(m_ShaderProgram, "u_Brightness");
    m_uContrast    = glGetUniformLocation(m_ShaderProgram, "u_Contrast");
    m_uSaturation  = glGetUniformLocation(m_ShaderProgram, "u_Saturation");
    m_uHue         = glGetUniformLocation(m_ShaderProgram, "u_Hue");
    m_uTemperature = glGetUniformLocation(m_ShaderProgram, "u_Temperature");
    m_uSharpness   = glGetUniformLocation(m_ShaderProgram, "u_Sharpness");
    m_uGamma       = glGetUniformLocation(m_ShaderProgram, "u_Gamma");
    m_uVignette    = glGetUniformLocation(m_ShaderProgram, "u_Vignette");
    m_uTexSize     = glGetUniformLocation(m_ShaderProgram, "u_TexSize");

    m_ShaderReady = true;
    std::cout << "[ImageView] Shader listo. Program ID: " << m_ShaderProgram << "\n";
}

void ImageView::DestroyShader() {
    if (m_ShaderProgram) { glDeleteProgram(m_ShaderProgram); m_ShaderProgram = 0; }
    if (m_FBO)           { glDeleteFramebuffers(1, &m_FBO);  m_FBO = 0; }
    if (m_OutputTex)     { glDeleteTextures(1, &m_OutputTex); m_OutputTex = 0; }
    m_ShaderReady = false;
}

void ImageView::InitQuad() {
    float verts[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void ImageView::DestroyQuad() {
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO);       m_VBO = 0; }
}

void ImageView::Clear() {
    if (m_TextureID != 0) {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
    m_Width  = 0;
    m_Height = 0;
}

bool ImageView::LoadImageFromFile(const std::string& path) {
    Clear();
    int channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &m_Width, &m_Height, &channels, 4);
    if (!data) return false;

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    return true;
}

void ImageView::ApplyRenderWithShader(float renderWidth, float renderHeight,
                                      float offsetX,    float offsetY)
{
    int iW = static_cast<int>(renderWidth);
    int iH = static_cast<int>(renderHeight);

    if (m_FBO == 0 || m_FBOWidth != iW || m_FBOHeight != iH) {
        if (m_FBO)       { glDeleteFramebuffers(1, &m_FBO);   m_FBO = 0; }
        if (m_OutputTex) { glDeleteTextures(1, &m_OutputTex); m_OutputTex = 0; }

        glGenTextures(1, &m_OutputTex);
        glBindTexture(GL_TEXTURE_2D, m_OutputTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iW, iH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_OutputTex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_FBOWidth  = iW;
        m_FBOHeight = iH;
    }

    GLint prevFBO      = 0;
    GLint prevViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, iW, iH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glUniform1i(m_uTexture,     0);
    glUniform2f(m_uTexSize,     static_cast<float>(m_Width), static_cast<float>(m_Height));
    glUniform1f(m_uBrightness,  m_Adj.brightness);
    glUniform1f(m_uContrast,    m_Adj.contrast);
    glUniform1f(m_uSaturation,  m_Adj.saturation);
    glUniform1f(m_uHue,         m_Adj.hue * (3.14159265f / 180.0f));
    glUniform1f(m_uTemperature, m_Adj.temperature);
    glUniform1f(m_uSharpness,   m_Adj.sharpness);
    glUniform1f(m_uGamma,       m_Adj.gamma);
    glUniform1f(m_uVignette,    m_Adj.vignette);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);
}

void ImageView::Render(float maxWidth, float maxHeight) {
    if (m_TextureID == 0) return;

    float aspect      = static_cast<float>(m_Width) / static_cast<float>(m_Height);
    float renderWidth = maxWidth;
    float renderHeight = renderWidth / aspect;
    if (renderHeight > maxHeight) {
        renderHeight = maxHeight;
        renderWidth  = renderHeight * aspect;
    }

    float offsetX = (maxWidth - renderWidth) * 0.5f;

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursorPos.x + offsetX, cursorPos.y));

    if (m_ShaderReady) {
        ApplyRenderWithShader(renderWidth, renderHeight, offsetX, cursorPos.y);
        ImGui::Image(static_cast<ImTextureID>(m_OutputTex),
                     ImVec2(renderWidth, renderHeight));
    } else {
        ImGui::Image(static_cast<ImTextureID>(m_TextureID),
                     ImVec2(renderWidth, renderHeight));
    }
}

void ImageView::RenderAdjustmentsPanel() {
    if (m_TextureID == 0) return;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

    ImGui::SeparatorText("Ajustes de imagen");

    ImGui::SliderFloat("Brillo",      &m_Adj.brightness,  -1.0f,  1.0f, "%.2f");
    ImGui::SliderFloat("Contraste",   &m_Adj.contrast,     0.0f,  3.0f, "%.2f");
    ImGui::SliderFloat("Saturación",  &m_Adj.saturation,   0.0f,  3.0f, "%.2f");
    ImGui::SliderFloat("Hue",         &m_Adj.hue,        -180.0f, 180.0f, "%.1f deg");
    ImGui::SliderFloat("Temperatura", &m_Adj.temperature, -1.0f,  1.0f, "%.2f");
    ImGui::SliderFloat("Nitidez",     &m_Adj.sharpness,    0.0f,  1.0f, "%.2f");
    ImGui::SliderFloat("Gamma",       &m_Adj.gamma,        0.1f,  3.0f, "%.2f");
    ImGui::SliderFloat("Viñeta",      &m_Adj.vignette,     0.0f,  1.0f, "%.2f");

    ImGui::Spacing();
    if (ImGui::Button("Restaurar valores", ImVec2(-1.0f, 0.0f))) {
        ResetAdjustments();
    }

    ImGui::PopStyleVar();
}

void ImageView::ResetAdjustments() {
    m_Adj = ImageAdjustments{};
}

}

