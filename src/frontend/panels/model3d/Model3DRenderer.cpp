#include "Model3DRenderer.h"
#include <iostream>
#include <vector>

namespace ProyecThor::UI {

// ── Shaders GLSL ────────────────────────────────────────────────────────────

static const char* kModelVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uView;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vUV;

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    // Matriz normal aproximada para transformaciones rígidas/uniformes
    vNormal = normalize(mat3(uModel) * aNormal);
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kModelFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vFragPos;
in vec2 vUV;

uniform vec4 uModelColor;
uniform vec3 uLightDir;
uniform vec3 uCameraPos;
uniform float uAmbientIntensity;
uniform float uLightIntensity;
uniform int uShadingMode; // 0 = SmoothLit, 1 = Flat, 2 = Wireframe

out vec4 FragColor;

void main() {
    if (uShadingMode == 2) {
        FragColor = uModelColor;
        return;
    }

    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N; // Soporte para geometría de doble cara

    vec3 L = normalize(uLightDir);
    vec3 V = normalize(uCameraPos - vFragPos);
    vec3 H = normalize(L + V);

    // Iluminación Difusa Blinn-Phong
    float diff = max(dot(N, L), 0.0);
    // Iluminación Especular suave
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.45;

    // Luz secundaria de relleno (Fill light) desde abajo/lateral
    vec3 fillDir = normalize(vec3(-uLightDir.x, -0.4, -uLightDir.z));
    float fillDiff = max(dot(N, fillDir), 0.0) * 0.22;

    vec3 ambient = uAmbientIntensity * vec3(1.0);
    vec3 lighting = ambient + (diff * uLightIntensity + fillDiff) * vec3(1.0) + spec * vec3(1.0);

    vec3 finalRgb = uModelColor.rgb * lighting;
    FragColor = vec4(finalRgb, uModelColor.a);
}
)";

static const char* kGridVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kGridFragmentShader = R"(
#version 330 core
uniform vec4 uGridColor;
out vec4 FragColor;

void main() {
    FragColor = uGridColor;
}
)";

static unsigned int CompileShader(unsigned int type, const char* src) {
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    int success = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(s, 512, nullptr, infoLog);
        std::cerr << "Shader compile error: " << infoLog << "\n";
    }
    return s;
}

static unsigned int CreateProgram(const char* vsSrc, const char* fsSrc) {
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ── Constructor / Destructor ────────────────────────────────────────────────

Model3DRenderer::Model3DRenderer() = default;

Model3DRenderer::~Model3DRenderer() {
    Shutdown();
}

bool Model3DRenderer::Initialize() {
    if (m_Initialized) return true;

    InitShaders();
    m_Initialized = true;
    return true;
}

void Model3DRenderer::Shutdown() {
    if (m_FBO)      { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_ColorTex) { glDeleteTextures(1, &m_ColorTex); m_ColorTex = 0; }
    if (m_DepthRBO) { glDeleteRenderbuffers(1, &m_DepthRBO); m_DepthRBO = 0; }

    if (m_ShaderProgram) { glDeleteProgram(m_ShaderProgram); m_ShaderProgram = 0; }
    if (m_GridProgram)   { glDeleteProgram(m_GridProgram); m_GridProgram = 0; }
    if (m_GridVAO)       { glDeleteVertexArrays(1, &m_GridVAO); m_GridVAO = 0; }
    if (m_GridVBO)       { glDeleteBuffers(1, &m_GridVBO); m_GridVBO = 0; }

    m_Initialized = false;
    m_Width = 0;
    m_Height = 0;
}

void Model3DRenderer::InitShaders() {
    m_ShaderProgram = CreateProgram(kModelVertexShader, kModelFragmentShader);
    m_GridProgram   = CreateProgram(kGridVertexShader, kGridFragmentShader);

    // Crear geometría de la grilla de suelo
    std::vector<float> gridLines;
    const float extent = 3.0f;
    const float step   = 0.30f;
    const float y      = -1.05f;

    for (float x = -extent; x <= extent + 1e-4f; x += step) {
        gridLines.push_back(x); gridLines.push_back(y); gridLines.push_back(-extent);
        gridLines.push_back(x); gridLines.push_back(y); gridLines.push_back(extent);
    }
    for (float z = -extent; z <= extent + 1e-4f; z += step) {
        gridLines.push_back(-extent); gridLines.push_back(y); gridLines.push_back(z);
        gridLines.push_back(extent);  gridLines.push_back(y); gridLines.push_back(z);
    }

    m_GridVertexCount = (int)gridLines.size() / 3;

    glGenVertexArrays(1, &m_GridVAO);
    glGenBuffers(1, &m_GridVBO);

    glBindVertexArray(m_GridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_GridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(float), gridLines.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Model3DRenderer::EnsureFBO(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_FBO != 0 && m_Width == width && m_Height == height) return;

    m_Width  = width;
    m_Height = height;

    if (m_FBO == 0) glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    if (m_ColorTex == 0) glGenTextures(1, &m_ColorTex);
    glBindTexture(GL_TEXTURE_2D, m_ColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorTex, 0);

    if (m_DepthRBO == 0) glGenRenderbuffers(1, &m_DepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Model3DRenderer::Update(float dt, Model3DRenderConfig& config) {
    if (config.autoRotate) {
        config.cameraYaw += config.autoRotateSpeed * dt;
        if (config.cameraYaw > 360.0f) config.cameraYaw -= 360.0f;
        if (config.cameraYaw < -360.0f) config.cameraYaw += 360.0f;
    }
}

void Model3DRenderer::ProcessMouseInput(Model3DRenderConfig& config, ImVec2 mouseDelta, bool isLeftDragging, bool isRightDragging, float wheelDelta) {
    // 1. Orbitación (Click izquierdo + arrastre)
    if (isLeftDragging) {
        config.cameraYaw   += mouseDelta.x * 0.45f;
        config.cameraPitch += mouseDelta.y * 0.45f;
        config.cameraPitch  = std::clamp(config.cameraPitch, -89.0f, 89.0f);
    }

    // 2. Desplazamiento / Paneo (Click derecho o botón central + arrastre)
    if (isRightDragging) {
        float radYaw = config.cameraYaw * 3.14159265f / 180.0f;
        float rightX = std::cos(radYaw), rightZ = -std::sin(radYaw);

        float panSpeed = config.cameraDistance * 0.0022f;
        config.cameraTarget.x -= rightX * mouseDelta.x * panSpeed;
        config.cameraTarget.z -= rightZ * mouseDelta.x * panSpeed;
        config.cameraTarget.y += mouseDelta.y * panSpeed;
    }

    // 3. Zoom (Rueda del ratón)
    if (std::abs(wheelDelta) > 0.01f) {
        config.cameraDistance -= wheelDelta * 0.28f;
        config.cameraDistance = std::clamp(config.cameraDistance, 0.6f, 15.0f);
    }
}

void Model3DRenderer::RenderGrid(const Mat4& view, const Mat4& proj) {
    if (!m_GridProgram || !m_GridVAO || m_GridVertexCount <= 0) return;

    Mat4 mvp = proj * view;

    glUseProgram(m_GridProgram);
    int locMVP = glGetUniformLocation(m_GridProgram, "uMVP");
    int locCol = glGetUniformLocation(m_GridProgram, "uGridColor");

    glUniformMatrix4fv(locMVP, 1, GL_FALSE, mvp.m);
    glUniform4f(locCol, 0.35f, 0.42f, 0.65f, 0.30f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    glBindVertexArray(m_GridVAO);
    glDrawArrays(GL_LINES, 0, m_GridVertexCount);
    glBindVertexArray(0);
    glUseProgram(0);
}

unsigned int Model3DRenderer::RenderToTexture(Model3DMesh& mesh, const Model3DRenderConfig& config, int width, int height) {
    if (!m_Initialized) Initialize();
    EnsureFBO(width, height);
    if (!m_FBO || m_Width <= 0 || m_Height <= 0) return 0;

    // Subir malla a GPU si no está lista
    if (!mesh.isGpuLoaded && !mesh.vertices.empty()) {
        mesh.UploadToGpu();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);

    // Fondo según configuración
    if (config.background == Model3DBackground::Transparent) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    } else if (config.background == Model3DBackground::DarkStudio) {
        glClearColor(0.07f, 0.08f, 0.11f, 1.0f);
    } else if (config.background == Model3DBackground::GradientBlue) {
        glClearColor(0.05f, 0.09f, 0.16f, 1.0f);
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Calcular Matrices de Cámara
    float radYaw   = config.cameraYaw * 3.14159265f / 180.0f;
    float radPitch = config.cameraPitch * 3.14159265f / 180.0f;

    Vec3 eye = {
        config.cameraTarget.x + config.cameraDistance * std::cos(radPitch) * std::sin(radYaw),
        config.cameraTarget.y + config.cameraDistance * std::sin(radPitch),
        config.cameraTarget.z + config.cameraDistance * std::cos(radPitch) * std::cos(radYaw)
    };

    Mat4 view = Mat4::LookAt(eye, config.cameraTarget, { 0.0f, 1.0f, 0.0f });
    float aspect = (float)m_Width / (float)std::max(1, m_Height);
    Mat4 proj = Mat4::Perspective(45.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f);

    // Dibujar suelo de referencia
    if (config.showGrid && config.background != Model3DBackground::Transparent) {
        RenderGrid(view, proj);
    }

    // Dibujar modelo 3D
    if (mesh.isGpuLoaded && !mesh.indices.empty()) {
        Mat4 model = Mat4::Identity();
        Mat4 mvp   = proj * view * model;

        glUseProgram(m_ShaderProgram);

        int locMVP    = glGetUniformLocation(m_ShaderProgram, "uMVP");
        int locModel  = glGetUniformLocation(m_ShaderProgram, "uModel");
        int locView   = glGetUniformLocation(m_ShaderProgram, "uView");
        int locColor  = glGetUniformLocation(m_ShaderProgram, "uModelColor");
        int locLDir   = glGetUniformLocation(m_ShaderProgram, "uLightDir");
        int locCPos   = glGetUniformLocation(m_ShaderProgram, "uCameraPos");
        int locAmb    = glGetUniformLocation(m_ShaderProgram, "uAmbientIntensity");
        int locLInt   = glGetUniformLocation(m_ShaderProgram, "uLightIntensity");
        int locShad   = glGetUniformLocation(m_ShaderProgram, "uShadingMode");

        glUniformMatrix4fv(locMVP, 1, GL_FALSE, mvp.m);
        glUniformMatrix4fv(locModel, 1, GL_FALSE, model.m);
        glUniformMatrix4fv(locView, 1, GL_FALSE, view.m);

        glUniform3f(locLDir, config.lightDir.x, config.lightDir.y, config.lightDir.z);
        glUniform3f(locCPos, eye.x, eye.y, eye.z);
        glUniform1f(locAmb, config.ambientIntensity);
        glUniform1f(locLInt, config.lightIntensity);

        glBindVertexArray(mesh.vao);

        if (config.shading == Model3DShading::Wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(config.wireframeWidth);
            glUniform4f(locColor, config.wireframeColor.x, config.wireframeColor.y, config.wireframeColor.z, config.wireframeColor.w);
            glUniform1i(locShad, 2);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else if (config.shading == Model3DShading::ShadedWithEdges) {
            // Pase 1: Sombreado
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform4f(locColor, config.modelColor.x, config.modelColor.y, config.modelColor.z, config.modelColor.w);
            glUniform1i(locShad, 0);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);

            // Pase 2: Malla sobrepuesta con Depth bias
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            glLineWidth(config.wireframeWidth);
            glUniform4f(locColor, config.wireframeColor.x, config.wireframeColor.y, config.wireframeColor.z, config.wireframeColor.w);
            glUniform1i(locShad, 2);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } else {
            // Sombreado normal
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glUniform4f(locColor, config.modelColor.x, config.modelColor.y, config.modelColor.z, config.modelColor.w);
            glUniform1i(locShad, (config.shading == Model3DShading::FlatNormals) ? 1 : 0);
            glDrawElements(GL_TRIANGLES, (GLsizei)mesh.indices.size(), GL_UNSIGNED_INT, 0);
        }

        glBindVertexArray(0);
        glUseProgram(0);
    }

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_ColorTex;
}

} // namespace ProyecThor::UI

