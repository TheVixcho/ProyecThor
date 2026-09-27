#include "LayersTheme.h"
#include "SettingsManager.h"

namespace ProyecThor::UI {

using ProyecThor::Settings::ThemeSettings;

static ImVec4 V(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

static ImVec4 WithAlpha(const float* a, float alpha) {
    return ImVec4(a[0], a[1], a[2], alpha);
}

void LP::Sync(const ThemeSettings& t) {
    Base     = V(t.base);
    Surface0 = V(t.surface0);
    Surface1 = V(t.surface1);
    Surface2 = V(t.surface2);
    Surface3 = V(t.surface3);

    Border    = V(t.border);
    BorderHov = WithAlpha(t.accentLight, 0.8f);

    Accent       = V(t.accent);
    AccentHov    = V(t.accentLight);
    AccentDim    = WithAlpha(t.accent, 0.18f);
    AccentActive = V(t.accentDim);

    // Gold se deja fijo a proposito: no hay token "warning" en ThemeSettings
    // y ya es legible sobre fondos claros y oscuros.
    Green    = V(t.success);
    GreenDim = WithAlpha(t.success, 0.15f);
    Red      = V(t.danger);
    RedDim   = WithAlpha(t.danger, 0.15f);

    Text      = V(t.textPrimary);
    TextSub   = V(t.textDim);
    TextMuted = V(t.textFaint);

    TabBar      = V(t.base);
    TabActive   = V(t.surface1);
    TabInactive = V(t.surface0);
}

} // namespace ProyecThor::UI
