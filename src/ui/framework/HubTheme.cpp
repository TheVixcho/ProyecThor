#include "HubTheme.h"
#include "SettingsManager.h"

namespace ProyecThor::UI::HubTheme {

static ImU32 U32(const float* c, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3] * a));
}

void Sync(const ProyecThor::Settings::ThemeSettings& t) {
    BgSidebar  = U32(t.surface0);
    BgMain     = U32(t.base);
    AccentBlue = U32(t.accent);
    TextPri    = U32(t.textPrimary);
    TextMuted  = U32(t.textDim);
    Divider    = U32(t.border);

    Surface       = U32(t.surface2);
    SurfaceHover  = U32(t.surface3);
    SurfaceActive = U32(t.surface1);

    Card    = U32(t.surface1); // opaco -- antes 0.9 dejaba ver lo de atras
    CardAlt = U32(t.surface2);

    AccentSoft  = U32(t.accentLight);
    {
        const float accentLuma = 0.299f * t.accent[0] + 0.587f * t.accent[1] + 0.114f * t.accent[2];
        OnAccent = (accentLuma > 0.6f) ? IM_COL32(20, 20, 24, 255) : IM_COL32(255, 255, 255, 255);
    }
    Success     = U32(t.success);
    Danger      = U32(t.danger);
    BorderFaint = U32(t.borderFaint);
    TextFaint   = U32(t.textFaint);

    // Proporciones fijas sobre el rounding del preset: cuadrado y moderno
    RadiusSm = std::clamp(t.frameRounding * 0.65f, 0.0f, 2.0f);
    RadiusMd = std::clamp(t.frameRounding * 0.9f, 0.0f, 3.0f);
    RadiusLg = std::clamp(t.windowRounding * 0.7f, 0.0f, 4.0f);

    ParticleA = U32(t.accentLight);
    ParticleB = U32(t.accent);

    // Luminancia de theme.base (Rec. 601): temas claros bajan la opacidad
    // del splash de fondo para que no ensucie el contenido.
    const float luma = 0.299f * t.base[0] + 0.587f * t.base[1] + 0.114f * t.base[2];
    BgImageAlpha = (luma > 0.5f) ? (12.0f / 255.0f) : (34.0f / 255.0f);
}

} // namespace ProyecThor::UI::HubTheme