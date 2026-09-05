#pragma once
#include "PostProcessorCRT.h"
#include "PostProcessorGrain.h"
#include "PostProcessorFXAA.h"
#include "PostProcessorSaturation.h"
#include "PostProcessorVignette.h"
#include "PostProcessorBlur.h"
#include "PostProcessorSharpen.h"
#include "PostProcessorBloom.h"
#include "PostProcessorChromaticAberration.h"
#include "PostProcessorVHS.h"
#include "PostProcessorCine.h"
#include "PostProcessorContrast.h"
#include "PostProcessorLuminosity.h"
#include "PostProcessorTAA.h"
#include "PostProcessorGlitch.h"
#include "PostProcessorColorGrading.h"
#include "PostProcessorPixelate.h"
#include "PostProcessorRadialBlur.h"
#include "PostProcessorWaves.h"
#include "PostProcessorMirror.h"
#include "PostProcessorThermal.h"
#include "PostProcessorHalftone.h"
#include "PostProcessorVolumetricFog.h"
#include "PostProcessorVolumetricClouds.h"
#include "PostProcessorZonedDistortion.h"
#include <imgui.h>

namespace ProyecThor::Shaders {

// Cadena de post-proceso sobre el COMPOSITE completo de la ventana/viewport
// "ProjectorLive" (fondo + overlays + texto + anuncios + captura, ya
// dibujados por ImGui en su ImDrawData) — a diferencia de FSR, que sigue
// viviendo en BackgroundLayer y solo escala el fondo antes de componer.
class CompositePostChain {
public:
    CompositePostChain()  = default;
    ~CompositePostChain() { Destroy(); }

    CompositePostChain(const CompositePostChain&)            = delete;
    CompositePostChain& operator=(const CompositePostChain&) = delete;

    void SetCRTEnabled(bool e)              { m_CRT.SetEnabled(e); }
    bool GetCRTEnabled() const              { return m_CRT.IsEnabled(); }
    void SetCRTScanlineIntensity(float v)   { m_CRT.SetScanlineIntensity(v); }
    float GetCRTScanlineIntensity() const   { return m_CRT.GetScanlineIntensity(); }

    void SetGrainEnabled(bool e)            { m_Grain.SetEnabled(e); }
    bool GetGrainEnabled() const            { return m_Grain.IsEnabled(); }
    void SetGrainIntensity(float v)         { m_Grain.SetIntensity(v); }
    float GetGrainIntensity() const         { return m_Grain.GetIntensity(); }

    void SetFXAAEnabled(bool e)             { m_FXAA.SetEnabled(e); }
    bool GetFXAAEnabled() const             { return m_FXAA.IsEnabled(); }

    void SetSaturationEnabled(bool e)       { m_Saturation.SetEnabled(e); }
    bool GetSaturationEnabled() const       { return m_Saturation.IsEnabled(); }
    void SetSaturationAmount(float v)       { m_Saturation.SetAmount(v); }
    float GetSaturationAmount() const       { return m_Saturation.GetAmount(); }

    void SetVignetteEnabled(bool e)         { m_Vignette.SetEnabled(e); }
    bool GetVignetteEnabled() const         { return m_Vignette.IsEnabled(); }
    void SetVignetteIntensity(float v)      { m_Vignette.SetIntensity(v); }
    float GetVignetteIntensity() const      { return m_Vignette.GetIntensity(); }

    void SetBlurEnabled(bool e)             { m_Blur.SetEnabled(e); }
    bool GetBlurEnabled() const             { return m_Blur.IsEnabled(); }
    void SetBlurIntensity(float v)          { m_Blur.SetIntensity(v); }
    float GetBlurIntensity() const          { return m_Blur.GetIntensity(); }

    void SetSharpenEnabled(bool e)          { m_Sharpen.SetEnabled(e); }
    bool GetSharpenEnabled() const          { return m_Sharpen.IsEnabled(); }
    void SetSharpenIntensity(float v)       { m_Sharpen.SetIntensity(v); }
    float GetSharpenIntensity() const       { return m_Sharpen.GetIntensity(); }

    void SetBloomEnabled(bool e)            { m_Bloom.SetEnabled(e); }
    bool GetBloomEnabled() const            { return m_Bloom.IsEnabled(); }
    void SetBloomIntensity(float v)         { m_Bloom.SetIntensity(v); }
    float GetBloomIntensity() const         { return m_Bloom.GetIntensity(); }

    void SetChromaticAberrationEnabled(bool e)    { m_ChromaticAberration.SetEnabled(e); }
    bool GetChromaticAberrationEnabled() const    { return m_ChromaticAberration.IsEnabled(); }
    void SetChromaticAberrationIntensity(float v) { m_ChromaticAberration.SetIntensity(v); }
    float GetChromaticAberrationIntensity() const { return m_ChromaticAberration.GetIntensity(); }

    void SetVHSEnabled(bool e)              { m_VHS.SetEnabled(e); }
    bool GetVHSEnabled() const              { return m_VHS.IsEnabled(); }
    void SetVHSIntensity(float v)           { m_VHS.SetIntensity(v); }
    float GetVHSIntensity() const           { return m_VHS.GetIntensity(); }

    void SetCineEnabled(bool e)             { m_Cine.SetEnabled(e); }
    bool GetCineEnabled() const             { return m_Cine.IsEnabled(); }
    void SetCineIntensity(float v)          { m_Cine.SetIntensity(v); }
    float GetCineIntensity() const          { return m_Cine.GetIntensity(); }
    void SetCineTint(int t)                 { m_Cine.SetTintInt(t); }
    int  GetCineTint() const                { return m_Cine.GetTintInt(); }

    void SetContrastEnabled(bool e)         { m_Contrast.SetEnabled(e); }
    bool GetContrastEnabled() const         { return m_Contrast.IsEnabled(); }
    void SetContrastAmount(float v)         { m_Contrast.SetAmount(v); }
    float GetContrastAmount() const         { return m_Contrast.GetAmount(); }

    void SetLuminosityEnabled(bool e)       { m_Luminosity.SetEnabled(e); }
    bool GetLuminosityEnabled() const       { return m_Luminosity.IsEnabled(); }
    void SetLuminosityAmount(float v)       { m_Luminosity.SetAmount(v); }
    float GetLuminosityAmount() const       { return m_Luminosity.GetAmount(); }

    void SetTAAEnabled(bool e)              { m_TAA.SetEnabled(e); }
    bool GetTAAEnabled() const              { return m_TAA.IsEnabled(); }
    void SetTAAIntensity(float v)           { m_TAA.SetIntensity(v); }
    float GetTAAIntensity() const           { return m_TAA.GetIntensity(); }

    // ── Nuevos Efectos de Shaders ──────────────────────────────────────────
    void SetGlitchEnabled(bool e)           { m_Glitch.SetEnabled(e); }
    bool GetGlitchEnabled() const           { return m_Glitch.IsEnabled(); }
    void SetGlitchIntensity(float v)        { m_Glitch.SetIntensity(v); }
    float GetGlitchIntensity() const        { return m_Glitch.GetIntensity(); }
    void SetGlitchSpeed(float v)            { m_Glitch.SetSpeed(v); }
    float GetGlitchSpeed() const            { return m_Glitch.GetSpeed(); }
    void SetGlitchMode(int m)               { m_Glitch.SetMode(m); }
    int  GetGlitchMode() const              { return m_Glitch.GetMode(); }

    void SetColorGradingEnabled(bool e)     { m_ColorGrading.SetEnabled(e); }
    bool GetColorGradingEnabled() const     { return m_ColorGrading.IsEnabled(); }
    void SetColorGradingIntensity(float v)  { m_ColorGrading.SetIntensity(v); }
    float GetColorGradingIntensity() const  { return m_ColorGrading.GetIntensity(); }
    void SetColorGradingPreset(int p)       { m_ColorGrading.SetPreset(p); }
    int  GetColorGradingPreset() const      { return m_ColorGrading.GetPreset(); }

    void SetPixelateEnabled(bool e)         { m_Pixelate.SetEnabled(e); }
    bool GetPixelateEnabled() const         { return m_Pixelate.IsEnabled(); }
    void SetPixelateSize(float v)           { m_Pixelate.SetPixelSize(v); }
    float GetPixelateSize() const           { return m_Pixelate.GetPixelSize(); }
    void SetPixelateColorDepth(int d)       { m_Pixelate.SetColorDepth(d); }
    int  GetPixelateColorDepth() const      { return m_Pixelate.GetColorDepth(); }

    void SetRadialBlurEnabled(bool e)       { m_RadialBlur.SetEnabled(e); }
    bool GetRadialBlurEnabled() const       { return m_RadialBlur.IsEnabled(); }
    void SetRadialBlurIntensity(float v)    { m_RadialBlur.SetIntensity(v); }
    float GetRadialBlurIntensity() const    { return m_RadialBlur.GetIntensity(); }

    void SetWavesEnabled(bool e)            { m_Waves.SetEnabled(e); }
    bool GetWavesEnabled() const            { return m_Waves.IsEnabled(); }
    void SetWavesIntensity(float v)         { m_Waves.SetIntensity(v); }
    float GetWavesIntensity() const         { return m_Waves.GetIntensity(); }
    void SetWavesSpeed(float v)             { m_Waves.SetSpeed(v); }
    float GetWavesSpeed() const             { return m_Waves.GetSpeed(); }
    void SetWavesFrequency(float v)         { m_Waves.SetFrequency(v); }
    float GetWavesFrequency() const         { return m_Waves.GetFrequency(); }

    void SetMirrorEnabled(bool e)           { m_Mirror.SetEnabled(e); }
    bool GetMirrorEnabled() const           { return m_Mirror.IsEnabled(); }
    void SetMirrorMode(int m)               { m_Mirror.SetMode(m); }
    int  GetMirrorMode() const              { return m_Mirror.GetMode(); }

    void SetThermalEnabled(bool e)          { m_Thermal.SetEnabled(e); }
    bool GetThermalEnabled() const          { return m_Thermal.IsEnabled(); }
    void SetThermalIntensity(float v)       { m_Thermal.SetIntensity(v); }
    float GetThermalIntensity() const       { return m_Thermal.GetIntensity(); }
    void SetThermalMode(int m)              { m_Thermal.SetMode(m); }
    int  GetThermalMode() const             { return m_Thermal.GetMode(); }

    void SetHalftoneEnabled(bool e)         { m_Halftone.SetEnabled(e); }
    bool GetHalftoneEnabled() const         { return m_Halftone.IsEnabled(); }
    void SetHalftoneDotScale(float v)       { m_Halftone.SetDotScale(v); }
    float GetHalftoneDotScale() const       { return m_Halftone.GetDotScale(); }
    void SetHalftoneMode(int m)             { m_Halftone.SetMode(m); }
    int  GetHalftoneMode() const            { return m_Halftone.GetMode(); }

    // ── Efectos Volumétricos y por Zonas ────────────────────────────────────
    void SetVolumetricFogEnabled(bool e)         { m_VolumetricFog.SetEnabled(e); }
    bool GetVolumetricFogEnabled() const         { return m_VolumetricFog.IsEnabled(); }
    void SetVolumetricFogDensity(float v)        { m_VolumetricFog.SetDensity(v); }
    float GetVolumetricFogDensity() const        { return m_VolumetricFog.GetDensity(); }
    void SetVolumetricFogSpeed(float v)          { m_VolumetricFog.SetSpeed(v); }
    float GetVolumetricFogSpeed() const          { return m_VolumetricFog.GetSpeed(); }
    void SetVolumetricFogScale(float v)          { m_VolumetricFog.SetScale(v); }
    float GetVolumetricFogScale() const          { return m_VolumetricFog.GetScale(); }
    void SetVolumetricFogColorMode(int m)        { m_VolumetricFog.SetColorMode(m); }
    int  GetVolumetricFogColorMode() const       { return m_VolumetricFog.GetColorMode(); }

    void SetVolumetricCloudsEnabled(bool e)      { m_VolumetricClouds.SetEnabled(e); }
    bool GetVolumetricCloudsEnabled() const      { return m_VolumetricClouds.IsEnabled(); }
    void SetVolumetricCloudsCoverage(float v)    { m_VolumetricClouds.SetCoverage(v); }
    float GetVolumetricCloudsCoverage() const    { return m_VolumetricClouds.GetCoverage(); }
    void SetVolumetricCloudsDensity(float v)     { m_VolumetricClouds.SetDensity(v); }
    float GetVolumetricCloudsDensity() const     { return m_VolumetricClouds.GetDensity(); }
    void SetVolumetricCloudsSpeed(float v)       { m_VolumetricClouds.SetSpeed(v); }
    float GetVolumetricCloudsSpeed() const       { return m_VolumetricClouds.GetSpeed(); }
    void SetVolumetricCloudsSunIntensity(float v){ m_VolumetricClouds.SetSunIntensity(v); }
    float GetVolumetricCloudsSunIntensity() const{ return m_VolumetricClouds.GetSunIntensity(); }

    void SetZonedDistortionEnabled(bool e)       { m_ZonedDistortion.SetEnabled(e); }
    bool GetZonedDistortionEnabled() const       { return m_ZonedDistortion.IsEnabled(); }
    void SetZonedDistortionIntensity(float v)    { m_ZonedDistortion.SetIntensity(v); }
    float GetZonedDistortionIntensity() const    { return m_ZonedDistortion.GetIntensity(); }
    void SetZonedDistortionSpeed(float v)        { m_ZonedDistortion.SetSpeed(v); }
    float GetZonedDistortionSpeed() const        { return m_ZonedDistortion.GetSpeed(); }
    void SetZonedDistortionZone(int z)           { m_ZonedDistortion.SetZone(z); }
    int  GetZonedDistortionZone() const          { return m_ZonedDistortion.GetZone(); }
    void SetZonedDistortionFeather(float v)      { m_ZonedDistortion.SetFeather(v); }
    float GetZonedDistortionFeather() const      { return m_ZonedDistortion.GetFeather(); }

    bool AnyEnabled() const {
        return m_CRT.IsEnabled() || m_Grain.IsEnabled() || m_FXAA.IsEnabled() ||
               m_Saturation.IsEnabled() || m_Vignette.IsEnabled() ||
               m_Blur.IsEnabled() || m_Sharpen.IsEnabled() || m_Bloom.IsEnabled() ||
               m_ChromaticAberration.IsEnabled() ||
               m_VHS.IsEnabled() || m_Cine.IsEnabled() || m_Contrast.IsEnabled() ||
               m_Luminosity.IsEnabled() || m_TAA.IsEnabled() ||
               m_Glitch.IsEnabled() || m_ColorGrading.IsEnabled() ||
               m_Pixelate.IsEnabled() || m_RadialBlur.IsEnabled() ||
               m_Waves.IsEnabled() || m_Mirror.IsEnabled() ||
               m_Thermal.IsEnabled() || m_Halftone.IsEnabled() ||
               m_VolumetricFog.IsEnabled() || m_VolumetricClouds.IsEnabled() ||
               m_ZonedDistortion.IsEnabled();
    }

    GLuint ProcessBackgroundForPreview(GLuint srcTex, int w, int h);

    void RenderViewport(ImGuiViewport* viewport,
                        void (*defaultRenderFn)(ImGuiViewport*, void*));

    void Destroy();

private:
    void EnsureSized(int w, int h, void* platformHandle);
    void CreateCaptureFBO(int w, int h);
    void DestroyCaptureFBO();
    void EnsureBlitProgram();
    void BlitToCurrentFramebuffer(unsigned int tex, int w, int h);

    PostProcessorCRT        m_CRT;
    PostProcessorGrain      m_Grain;
    PostProcessorFXAA       m_FXAA;
    PostProcessorSaturation m_Saturation;
    PostProcessorVignette   m_Vignette;
    PostProcessorBlur       m_Blur;
    PostProcessorSharpen    m_Sharpen;
    PostProcessorBloom      m_Bloom;
    PostProcessorChromaticAberration m_ChromaticAberration;
    PostProcessorVHS         m_VHS;
    PostProcessorCine        m_Cine;
    PostProcessorContrast    m_Contrast;
    PostProcessorLuminosity  m_Luminosity;
    PostProcessorTAA         m_TAA;
    PostProcessorGlitch      m_Glitch;
    PostProcessorColorGrading m_ColorGrading;
    PostProcessorPixelate    m_Pixelate;
    PostProcessorRadialBlur  m_RadialBlur;
    PostProcessorWaves       m_Waves;
    PostProcessorMirror      m_Mirror;
    PostProcessorThermal     m_Thermal;
    PostProcessorHalftone    m_Halftone;
    PostProcessorVolumetricFog    m_VolumetricFog;
    PostProcessorVolumetricClouds m_VolumetricClouds;
    PostProcessorZonedDistortion  m_ZonedDistortion;

    // Segundo juego de las mismas, a resolucion de PREVIEW
    PostProcessorCRT        m_PreviewCRT;
    PostProcessorGrain      m_PreviewGrain;
    PostProcessorFXAA       m_PreviewFXAA;
    PostProcessorSaturation m_PreviewSaturation;
    PostProcessorVignette   m_PreviewVignette;
    PostProcessorBlur       m_PreviewBlur;
    PostProcessorSharpen    m_PreviewSharpen;
    PostProcessorBloom      m_PreviewBloom;
    PostProcessorChromaticAberration m_PreviewChromaticAberration;
    PostProcessorVHS         m_PreviewVHS;
    PostProcessorCine        m_PreviewCine;
    PostProcessorContrast    m_PreviewContrast;
    PostProcessorLuminosity  m_PreviewLuminosity;
    PostProcessorTAA         m_PreviewTAA;
    PostProcessorGlitch      m_PreviewGlitch;
    PostProcessorColorGrading m_PreviewColorGrading;
    PostProcessorPixelate    m_PreviewPixelate;
    PostProcessorRadialBlur  m_PreviewRadialBlur;
    PostProcessorWaves       m_PreviewWaves;
    PostProcessorMirror      m_PreviewMirror;
    PostProcessorThermal     m_PreviewThermal;
    PostProcessorHalftone    m_PreviewHalftone;
    PostProcessorVolumetricFog    m_PreviewVolumetricFog;
    PostProcessorVolumetricClouds m_PreviewVolumetricClouds;
    PostProcessorZonedDistortion  m_PreviewZonedDistortion;
    int  m_PreviewW = 0, m_PreviewH = 0;
    bool m_PreviewInitialized = false;

    unsigned int m_CaptureFBO = 0, m_CaptureTex = 0;
    unsigned int m_BlitProgram = 0, m_BlitVAO = 0, m_BlitVBO = 0;

    int   m_W = 0, m_H = 0;
    void* m_LastPlatformHandle    = nullptr;
    bool  m_SubEffectsInitialized = false;
};

} // namespace ProyecThor::Shaders

