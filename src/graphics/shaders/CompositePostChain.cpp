#include "CompositePostChain.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <vector>

namespace ProyecThor::Shaders {

static const float k_QuadVerts[] = {
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,
};

static const char* k_BlitVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

static const char* k_BlitFrag = R"GLSL(
#version 330 core
in  vec2 v_UV;
out vec4 fragColor;
uniform sampler2D u_Tex;
void main() { fragColor = texture(u_Tex, v_UV); }
)GLSL";

static GLuint CompileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        std::cerr << "[CompositePostChain] Shader error: " << log << "\n";
        glDeleteShader(id);
        return 0;
    }
    return id;
}

void CompositePostChain::EnsureBlitProgram() {
    if (m_BlitProgram) return;

    GLuint vert = CompileStage(GL_VERTEX_SHADER, k_BlitVert);
    GLuint frag = CompileStage(GL_FRAGMENT_SHADER, k_BlitFrag);
    m_BlitProgram = glCreateProgram();
    glAttachShader(m_BlitProgram, vert);
    glAttachShader(m_BlitProgram, frag);
    glLinkProgram(m_BlitProgram);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &m_BlitVAO);
    glGenBuffers(1, &m_BlitVBO);
    glBindVertexArray(m_BlitVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_BlitVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void CompositePostChain::BlitToCurrentFramebuffer(unsigned int tex, int w, int h) {
    glViewport(0, 0, w, h);
    glUseProgram(m_BlitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(m_BlitProgram, "u_Tex"), 0);
    glBindVertexArray(m_BlitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void CompositePostChain::CreateCaptureFBO(int w, int h) {
    glGenTextures(1, &m_CaptureTex);
    glBindTexture(GL_TEXTURE_2D, m_CaptureTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_CaptureFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_CaptureTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[CompositePostChain] Capture FBO incompleto (status=0x" << std::hex << status << std::dec << ")\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CompositePostChain::DestroyCaptureFBO() {
    if (m_CaptureFBO) { glDeleteFramebuffers(1, &m_CaptureFBO); m_CaptureFBO = 0; }
    if (m_CaptureTex) { glDeleteTextures(1, &m_CaptureTex); m_CaptureTex = 0; }
}

void CompositePostChain::EnsureSized(int w, int h, void* platformHandle) {
    bool contextChanged = (platformHandle != m_LastPlatformHandle);
    bool sizeChanged    = (w != m_W || h != m_H);

    if (!contextChanged && !sizeChanged && m_SubEffectsInitialized)
        return;

    if (contextChanged) {
        // La ventana nativa de "ProjectorLive" se recreo (ej. Audiencia
        // apagada/prendida) -- el contexto GL viejo, y todo lo que tenia,
        // ya fue destruido por GLFW. Los IDs numericos que teniamos
        // cacheados son ajenos al contexto nuevo: "olvidarlos" sin
        // glDelete* evita el riesgo de borrar, por coincidencia de
        // numero, un objeto legitimo recien creado en el contexto nuevo.
        m_CaptureFBO = m_CaptureTex = 0;
        m_BlitProgram = m_BlitVAO = m_BlitVBO = 0;
        m_CRT.ForgetGLResources();
        m_Grain.ForgetGLResources();
        m_FXAA.ForgetGLResources();
        m_Saturation.ForgetGLResources();
        m_Vignette.ForgetGLResources();
        m_Blur.ForgetGLResources();
        m_Sharpen.ForgetGLResources();
        m_Bloom.ForgetGLResources();
        m_ChromaticAberration.ForgetGLResources();
        m_VHS.ForgetGLResources();
        m_Cine.ForgetGLResources();
        m_Contrast.ForgetGLResources();
        m_Luminosity.ForgetGLResources();
        m_TAA.ForgetGLResources();
        m_Glitch.ForgetGLResources();
        m_ColorGrading.ForgetGLResources();
        m_Pixelate.ForgetGLResources();
        m_RadialBlur.ForgetGLResources();
        m_Waves.ForgetGLResources();
        m_Mirror.ForgetGLResources();
        m_Thermal.ForgetGLResources();
        m_Halftone.ForgetGLResources();
        m_SubEffectsInitialized = false;
    } else {
        // Mismo contexto: el resize normal, con glDelete* real, es seguro.
        DestroyCaptureFBO();
    }

    m_W = w;
    m_H = h;
    m_LastPlatformHandle = platformHandle;

    CreateCaptureFBO(w, h);
    EnsureBlitProgram();

    if (!m_SubEffectsInitialized) {
        m_CRT.Init(w, h);
        m_Grain.Init(w, h);
        m_FXAA.Init(w, h);
        m_Saturation.Init(w, h);
        m_Vignette.Init(w, h);
        m_Blur.Init(w, h);
        m_Sharpen.Init(w, h);
        m_Bloom.Init(w, h);
        m_ChromaticAberration.Init(w, h);
        m_VHS.Init(w, h);
        m_Cine.Init(w, h);
        m_Contrast.Init(w, h);
        m_Luminosity.Init(w, h);
        m_TAA.Init(w, h);
        m_Glitch.Init(w, h);
        m_ColorGrading.Init(w, h);
        m_Pixelate.Init(w, h);
        m_RadialBlur.Init(w, h);
        m_Waves.Init(w, h);
        m_Mirror.Init(w, h);
        m_Thermal.Init(w, h);
        m_Halftone.Init(w, h);
        m_VolumetricFog.Init(w, h);
        m_VolumetricClouds.Init(w, h);
        m_ZonedDistortion.Init(w, h);
        m_SubEffectsInitialized = true;
    } else {
        m_CRT.Resize(w, h);
        m_Grain.Resize(w, h);
        m_FXAA.Resize(w, h);
        m_Saturation.Resize(w, h);
        m_Vignette.Resize(w, h);
        m_Blur.Resize(w, h);
        m_Sharpen.Resize(w, h);
        m_Bloom.Resize(w, h);
        m_ChromaticAberration.Resize(w, h);
        m_VHS.Resize(w, h);
        m_Cine.Resize(w, h);
        m_Contrast.Resize(w, h);
        m_Luminosity.Resize(w, h);
        m_TAA.Resize(w, h);
        m_Glitch.Resize(w, h);
        m_ColorGrading.Resize(w, h);
        m_Pixelate.Resize(w, h);
        m_RadialBlur.Resize(w, h);
        m_Waves.Resize(w, h);
        m_Mirror.Resize(w, h);
        m_Thermal.Resize(w, h);
        m_Halftone.Resize(w, h);
        m_VolumetricFog.Resize(w, h);
        m_VolumetricClouds.Resize(w, h);
        m_ZonedDistortion.Resize(w, h);
    }
}

void CompositePostChain::RenderViewport(ImGuiViewport* viewport,
                                        void (*defaultRenderFn)(ImGuiViewport*, void*))
{
    if (!AnyEnabled()) {
        if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
        return;
    }

    int w = (int)(viewport->Size.x * viewport->FramebufferScale.x);
    int h = (int)(viewport->Size.y * viewport->FramebufferScale.y);
    if (w <= 0 || h <= 0) {
        if (defaultRenderFn) defaultRenderFn(viewport, nullptr);
        return;
    }

    EnsureSized(w, h, viewport->PlatformHandle);

    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // 1) Captura: todo el drawList de "ProjectorLive"
    glBindFramebuffer(GL_FRAMEBUFFER, m_CaptureFBO);
    glViewport(0, 0, m_W, m_H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(viewport->DrawData);

    // 2) Cadena de efectos sobre el composite capturado
    double curTime = glfwGetTime();
    GLuint tex = m_CaptureTex;
    if (m_ColorGrading.IsEnabled())        tex = m_ColorGrading.Process(tex);
    if (m_Saturation.IsEnabled())          tex = m_Saturation.Process(tex);
    if (m_Contrast.IsEnabled())            tex = m_Contrast.Process(tex);
    if (m_Luminosity.IsEnabled())          tex = m_Luminosity.Process(tex);
    if (m_Cine.IsEnabled())                tex = m_Cine.Process(tex);
    if (m_Thermal.IsEnabled())             tex = m_Thermal.Process(tex, curTime);
    if (m_Bloom.IsEnabled())               tex = m_Bloom.Process(tex, m_W, m_H);
    if (m_Sharpen.IsEnabled())             tex = m_Sharpen.Process(tex, m_W, m_H);
    if (m_Blur.IsEnabled())                tex = m_Blur.Process(tex);
    if (m_Waves.IsEnabled())               tex = m_Waves.Process(tex, curTime);
    if (m_ZonedDistortion.IsEnabled())     tex = m_ZonedDistortion.Process(tex, curTime);
    if (m_VolumetricClouds.IsEnabled())    tex = m_VolumetricClouds.Process(tex, curTime);
    if (m_VolumetricFog.IsEnabled())       tex = m_VolumetricFog.Process(tex, curTime);
    if (m_RadialBlur.IsEnabled())          tex = m_RadialBlur.Process(tex);
    if (m_Pixelate.IsEnabled())            tex = m_Pixelate.Process(tex, m_W, m_H);
    if (m_Halftone.IsEnabled())            tex = m_Halftone.Process(tex, m_W, m_H);
    if (m_Mirror.IsEnabled())              tex = m_Mirror.Process(tex);
    if (m_Glitch.IsEnabled())              tex = m_Glitch.Process(tex, m_W, m_H, curTime);
    if (m_VHS.IsEnabled())                 tex = m_VHS.Process(tex, m_W, m_H, curTime);
    if (m_CRT.IsEnabled())                 tex = m_CRT.Process(tex, m_W, m_H);
    if (m_Grain.IsEnabled())               tex = m_Grain.Process(tex, curTime);
    if (m_Vignette.IsEnabled())            tex = m_Vignette.Process(tex);
    if (m_ChromaticAberration.IsEnabled()) tex = m_ChromaticAberration.Process(tex);
    if (m_TAA.IsEnabled())                 tex = m_TAA.Process(tex, m_W, m_H);
    if (m_FXAA.IsEnabled())                tex = m_FXAA.Process(tex, m_W, m_H);

    // 3) Blit final al framebuffer real de la ventana.
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    BlitToCurrentFramebuffer(tex, m_W, m_H);
}

void CompositePostChain::Destroy() {
    DestroyCaptureFBO();
    if (m_BlitProgram) { glDeleteProgram(m_BlitProgram); m_BlitProgram = 0; }
    if (m_BlitVAO)     { glDeleteVertexArrays(1, &m_BlitVAO); m_BlitVAO = 0; }
    if (m_BlitVBO)     { glDeleteBuffers(1, &m_BlitVBO); m_BlitVBO = 0; }
    m_CRT.Destroy();
    m_Grain.Destroy();
    m_FXAA.Destroy();
    m_Saturation.Destroy();
    m_Vignette.Destroy();
    m_Blur.Destroy();
    m_Sharpen.Destroy();
    m_Bloom.Destroy();
    m_ChromaticAberration.Destroy();
    m_VHS.Destroy();
    m_Cine.Destroy();
    m_Contrast.Destroy();
    m_Luminosity.Destroy();
    m_TAA.Destroy();
    m_Glitch.Destroy();
    m_ColorGrading.Destroy();
    m_Pixelate.Destroy();
    m_RadialBlur.Destroy();
    m_Waves.Destroy();
    m_Mirror.Destroy();
    m_Thermal.Destroy();
    m_Halftone.Destroy();
    m_VolumetricFog.Destroy();
    m_VolumetricClouds.Destroy();
    m_ZonedDistortion.Destroy();
    m_SubEffectsInitialized = false;

    m_PreviewCRT.Destroy();
    m_PreviewGrain.Destroy();
    m_PreviewFXAA.Destroy();
    m_PreviewSaturation.Destroy();
    m_PreviewVignette.Destroy();
    m_PreviewBlur.Destroy();
    m_PreviewSharpen.Destroy();
    m_PreviewBloom.Destroy();
    m_PreviewChromaticAberration.Destroy();
    m_PreviewVHS.Destroy();
    m_PreviewCine.Destroy();
    m_PreviewContrast.Destroy();
    m_PreviewLuminosity.Destroy();
    m_PreviewTAA.Destroy();
    m_PreviewGlitch.Destroy();
    m_PreviewColorGrading.Destroy();
    m_PreviewPixelate.Destroy();
    m_PreviewRadialBlur.Destroy();
    m_PreviewWaves.Destroy();
    m_PreviewMirror.Destroy();
    m_PreviewThermal.Destroy();
    m_PreviewHalftone.Destroy();
    m_PreviewVolumetricFog.Destroy();
    m_PreviewVolumetricClouds.Destroy();
    m_PreviewZonedDistortion.Destroy();
    m_PreviewInitialized = false;
}

GLuint CompositePostChain::ProcessBackgroundForPreview(GLuint srcTex, int w, int h) {
    if (srcTex == 0 || w <= 0 || h <= 0) return srcTex;
    if (!AnyEnabled()) return srcTex;

    if (!m_PreviewInitialized || w != m_PreviewW || h != m_PreviewH) {
        m_PreviewCRT.Destroy();        m_PreviewCRT.Init(w, h);
        m_PreviewGrain.Destroy();      m_PreviewGrain.Init(w, h);
        m_PreviewFXAA.Destroy();       m_PreviewFXAA.Init(w, h);
        m_PreviewSaturation.Destroy(); m_PreviewSaturation.Init(w, h);
        m_PreviewVignette.Destroy();   m_PreviewVignette.Init(w, h);
        m_PreviewBlur.Destroy();       m_PreviewBlur.Init(w, h);
        m_PreviewSharpen.Destroy();    m_PreviewSharpen.Init(w, h);
        m_PreviewBloom.Destroy();      m_PreviewBloom.Init(w, h);
        m_PreviewChromaticAberration.Destroy(); m_PreviewChromaticAberration.Init(w, h);
        m_PreviewVHS.Destroy();         m_PreviewVHS.Init(w, h);
        m_PreviewCine.Destroy();        m_PreviewCine.Init(w, h);
        m_PreviewContrast.Destroy();    m_PreviewContrast.Init(w, h);
        m_PreviewLuminosity.Destroy();  m_PreviewLuminosity.Init(w, h);
        m_PreviewTAA.Destroy();         m_PreviewTAA.Init(w, h);
        m_PreviewGlitch.Destroy();      m_PreviewGlitch.Init(w, h);
        m_PreviewColorGrading.Destroy(); m_PreviewColorGrading.Init(w, h);
        m_PreviewPixelate.Destroy();    m_PreviewPixelate.Init(w, h);
        m_PreviewRadialBlur.Destroy();  m_PreviewRadialBlur.Init(w, h);
        m_PreviewWaves.Destroy();       m_PreviewWaves.Init(w, h);
        m_PreviewMirror.Destroy();      m_PreviewMirror.Init(w, h);
        m_PreviewThermal.Destroy();     m_PreviewThermal.Init(w, h);
        m_PreviewHalftone.Destroy();    m_PreviewHalftone.Init(w, h);
        m_PreviewVolumetricFog.Destroy();    m_PreviewVolumetricFog.Init(w, h);
        m_PreviewVolumetricClouds.Destroy(); m_PreviewVolumetricClouds.Init(w, h);
        m_PreviewZonedDistortion.Destroy();  m_PreviewZonedDistortion.Init(w, h);
        m_PreviewW = w;
        m_PreviewH = h;
        m_PreviewInitialized = true;
    }

    m_PreviewCRT.SetEnabled(m_CRT.IsEnabled());
    m_PreviewCRT.SetScanlineIntensity(m_CRT.GetScanlineIntensity());
    m_PreviewGrain.SetEnabled(m_Grain.IsEnabled());
    m_PreviewGrain.SetIntensity(m_Grain.GetIntensity());
    m_PreviewFXAA.SetEnabled(m_FXAA.IsEnabled());
    m_PreviewSaturation.SetEnabled(m_Saturation.IsEnabled());
    m_PreviewSaturation.SetAmount(m_Saturation.GetAmount());
    m_PreviewVignette.SetEnabled(m_Vignette.IsEnabled());
    m_PreviewVignette.SetIntensity(m_Vignette.GetIntensity());
    m_PreviewBlur.SetEnabled(m_Blur.IsEnabled());
    m_PreviewBlur.SetIntensity(m_Blur.GetIntensity());
    m_PreviewSharpen.SetEnabled(m_Sharpen.IsEnabled());
    m_PreviewSharpen.SetIntensity(m_Sharpen.GetIntensity());
    m_PreviewBloom.SetEnabled(m_Bloom.IsEnabled());
    m_PreviewBloom.SetIntensity(m_Bloom.GetIntensity());
    m_PreviewChromaticAberration.SetEnabled(m_ChromaticAberration.IsEnabled());
    m_PreviewChromaticAberration.SetIntensity(m_ChromaticAberration.GetIntensity());
    m_PreviewVHS.SetEnabled(m_VHS.IsEnabled());
    m_PreviewVHS.SetIntensity(m_VHS.GetIntensity());
    m_PreviewCine.SetEnabled(m_Cine.IsEnabled());
    m_PreviewCine.SetIntensity(m_Cine.GetIntensity());
    m_PreviewCine.SetTintInt(m_Cine.GetTintInt());
    m_PreviewContrast.SetEnabled(m_Contrast.IsEnabled());
    m_PreviewContrast.SetAmount(m_Contrast.GetAmount());
    m_PreviewLuminosity.SetEnabled(m_Luminosity.IsEnabled());
    m_PreviewLuminosity.SetAmount(m_Luminosity.GetAmount());
    m_PreviewTAA.SetEnabled(m_TAA.IsEnabled());
    m_PreviewTAA.SetIntensity(m_TAA.GetIntensity());
    m_PreviewGlitch.SetEnabled(m_Glitch.IsEnabled());
    m_PreviewGlitch.SetIntensity(m_Glitch.GetIntensity());
    m_PreviewGlitch.SetSpeed(m_Glitch.GetSpeed());
    m_PreviewGlitch.SetMode(m_Glitch.GetMode());
    m_PreviewColorGrading.SetEnabled(m_ColorGrading.IsEnabled());
    m_PreviewColorGrading.SetIntensity(m_ColorGrading.GetIntensity());
    m_PreviewColorGrading.SetPreset(m_ColorGrading.GetPreset());
    m_PreviewPixelate.SetEnabled(m_Pixelate.IsEnabled());
    m_PreviewPixelate.SetPixelSize(m_Pixelate.GetPixelSize());
    m_PreviewPixelate.SetColorDepth(m_Pixelate.GetColorDepth());
    m_PreviewRadialBlur.SetEnabled(m_RadialBlur.IsEnabled());
    m_PreviewRadialBlur.SetIntensity(m_RadialBlur.GetIntensity());
    m_PreviewWaves.SetEnabled(m_Waves.IsEnabled());
    m_PreviewWaves.SetIntensity(m_Waves.GetIntensity());
    m_PreviewWaves.SetSpeed(m_Waves.GetSpeed());
    m_PreviewWaves.SetFrequency(m_Waves.GetFrequency());
    m_PreviewMirror.SetEnabled(m_Mirror.IsEnabled());
    m_PreviewMirror.SetMode(m_Mirror.GetMode());
    m_PreviewThermal.SetEnabled(m_Thermal.IsEnabled());
    m_PreviewThermal.SetIntensity(m_Thermal.GetIntensity());
    m_PreviewThermal.SetMode(m_Thermal.GetMode());
    m_PreviewHalftone.SetEnabled(m_Halftone.IsEnabled());
    m_PreviewHalftone.SetDotScale(m_Halftone.GetDotScale());
    m_PreviewHalftone.SetMode(m_Halftone.GetMode());

    m_PreviewVolumetricFog.SetEnabled(m_VolumetricFog.IsEnabled());
    m_PreviewVolumetricFog.SetDensity(m_VolumetricFog.GetDensity());
    m_PreviewVolumetricFog.SetSpeed(m_VolumetricFog.GetSpeed());
    m_PreviewVolumetricFog.SetScale(m_VolumetricFog.GetScale());
    m_PreviewVolumetricFog.SetColorMode(m_VolumetricFog.GetColorMode());

    m_PreviewVolumetricClouds.SetEnabled(m_VolumetricClouds.IsEnabled());
    m_PreviewVolumetricClouds.SetCoverage(m_VolumetricClouds.GetCoverage());
    m_PreviewVolumetricClouds.SetDensity(m_VolumetricClouds.GetDensity());
    m_PreviewVolumetricClouds.SetSpeed(m_VolumetricClouds.GetSpeed());
    m_PreviewVolumetricClouds.SetSunIntensity(m_VolumetricClouds.GetSunIntensity());

    m_PreviewZonedDistortion.SetEnabled(m_ZonedDistortion.IsEnabled());
    m_PreviewZonedDistortion.SetIntensity(m_ZonedDistortion.GetIntensity());
    m_PreviewZonedDistortion.SetSpeed(m_ZonedDistortion.GetSpeed());
    m_PreviewZonedDistortion.SetZone(m_ZonedDistortion.GetZone());
    m_PreviewZonedDistortion.SetFeather(m_ZonedDistortion.GetFeather());

    double curTime = glfwGetTime();
    GLuint tex = srcTex;
    if (m_PreviewColorGrading.IsEnabled())        tex = m_PreviewColorGrading.Process(tex);
    if (m_PreviewSaturation.IsEnabled())          tex = m_PreviewSaturation.Process(tex);
    if (m_PreviewContrast.IsEnabled())            tex = m_PreviewContrast.Process(tex);
    if (m_PreviewLuminosity.IsEnabled())          tex = m_PreviewLuminosity.Process(tex);
    if (m_PreviewCine.IsEnabled())                tex = m_PreviewCine.Process(tex);
    if (m_PreviewThermal.IsEnabled())             tex = m_PreviewThermal.Process(tex, curTime);
    if (m_PreviewBloom.IsEnabled())               tex = m_PreviewBloom.Process(tex, w, h);
    if (m_PreviewSharpen.IsEnabled())             tex = m_PreviewSharpen.Process(tex, w, h);
    if (m_PreviewBlur.IsEnabled())                tex = m_PreviewBlur.Process(tex);
    if (m_PreviewWaves.IsEnabled())               tex = m_PreviewWaves.Process(tex, curTime);
    if (m_PreviewZonedDistortion.IsEnabled())     tex = m_PreviewZonedDistortion.Process(tex, curTime);
    if (m_PreviewVolumetricClouds.IsEnabled())    tex = m_PreviewVolumetricClouds.Process(tex, curTime);
    if (m_PreviewVolumetricFog.IsEnabled())       tex = m_PreviewVolumetricFog.Process(tex, curTime);
    if (m_PreviewRadialBlur.IsEnabled())          tex = m_PreviewRadialBlur.Process(tex);
    if (m_PreviewPixelate.IsEnabled())            tex = m_PreviewPixelate.Process(tex, w, h);
    if (m_PreviewHalftone.IsEnabled())            tex = m_PreviewHalftone.Process(tex, w, h);
    if (m_PreviewMirror.IsEnabled())              tex = m_PreviewMirror.Process(tex);
    if (m_PreviewGlitch.IsEnabled())              tex = m_PreviewGlitch.Process(tex, w, h, curTime);
    if (m_PreviewVHS.IsEnabled())                 tex = m_PreviewVHS.Process(tex, w, h, curTime);
    if (m_PreviewCRT.IsEnabled())                 tex = m_PreviewCRT.Process(tex, w, h);
    if (m_PreviewGrain.IsEnabled())               tex = m_PreviewGrain.Process(tex, curTime);
    if (m_PreviewVignette.IsEnabled())            tex = m_PreviewVignette.Process(tex);
    if (m_PreviewChromaticAberration.IsEnabled()) tex = m_PreviewChromaticAberration.Process(tex);
    if (m_PreviewTAA.IsEnabled())                 tex = m_PreviewTAA.Process(tex, w, h);
    if (m_PreviewFXAA.IsEnabled())                tex = m_PreviewFXAA.Process(tex, w, h);

    return tex;
}

} // namespace ProyecThor::Shaders
