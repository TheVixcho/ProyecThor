#include "MonitorDesign.h"
#include "SettingsManager.h"

namespace ProyecThor::UI::Design {

static ImVec4 V(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

void Sync(const ProyecThor::Settings::ThemeSettings& t) {
    k_Bg0 = V(t.base);
    k_Bg1 = V(t.surface0);
    k_Bg3 = V(t.base); // opaco -- ver mismo fix en MonitorTheme::Sync

    k_BorderSubtle = V(t.border);

    k_TextSecondary = V(t.textDim);
    k_TextDim       = V(t.textFaint);

    k_PrevAccent    = V(t.accent);
    k_PrevAccentDim = V(t.accentDim, 0.5f);
    k_PrevBtn       = V(t.accentDim, 1.0f);
    k_PrevBtnHov    = V(t.accent, 1.0f);
    k_PrevBtnAct    = V(t.accentDim, 0.7f);
    k_PrevGrab      = V(t.accent);
    k_PrevTrack     = V(t.accentDim, 0.6f);

    k_LiveAccent    = V(t.danger);
    k_LiveAccentDim = V(t.danger, 0.40f);
    k_LiveBtn       = V(t.danger, 0.55f);
    k_LiveBtnHov    = V(t.danger, 0.70f);
    k_LiveBtnAct    = V(t.danger, 0.40f);
    k_LiveGrab      = V(t.danger);
    k_LiveTrack     = V(t.danger, 0.40f);

    k_NeutBtn    = V(t.surface2);
    k_NeutBtnHov = V(t.surface3);
    k_NeutBtnAct = V(t.surface1);
}

} // namespace ProyecThor::UI::Design
