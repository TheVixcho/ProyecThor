#include "MonitorTheme.h"
#include "SettingsManager.h"

namespace ProyecThor::UI::MonitorTheme {

static ImVec4 V(const float* a, float alphaMul = 1.0f) {
    return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
}

void Sync(const ProyecThor::Settings::ThemeSettings& t) {
    k_Bg0 = V(t.base);
    k_Bg1 = V(t.surface0);
    k_Bg2 = V(t.surface1);
    k_Bg3 = V(t.base); // opaco -- antes 0.9 dejaba ver lo de atras (transparencia no pedida)

    k_BorderSubtle = V(t.border, 1.0f);
    k_BorderFocus  = V(t.accentDim);

    k_TextWhite     = ImVec4(1, 1, 1, 1);
    k_TextPrimary   = V(t.textPrimary);
    k_TextSecondary = V(t.textDim);
    k_TextDim       = V(t.textFaint);

    k_PrevAccent    = V(t.accent);
    k_PrevAccentDim = V(t.accentDim, 0.5f);
    k_LiveAccent    = V(t.danger);
    k_LiveAccentDim = V(t.danger, 0.45f);
    k_AmberAccent   = ImVec4(1.00f, 0.75f, 0.20f, 1.00f); // color de estado fijo (loop)

    k_LiveBtn    = V(t.danger, 0.55f);
    k_LiveBtnHov = V(t.danger, 0.70f);
    k_LiveBtnAct = V(t.danger, 0.40f);

    k_PrevBtn    = V(t.accentDim, 1.0f);
    k_PrevBtnHov = V(t.accent, 1.0f);
    k_PrevBtnAct = V(t.accentDim, 0.7f);

    k_AmberBtn    = ImVec4(0.40f, 0.28f, 0.04f, 1.00f);
    k_AmberBtnHov = ImVec4(0.52f, 0.38f, 0.06f, 1.00f);
    k_AmberBtnAct = ImVec4(0.30f, 0.20f, 0.02f, 1.00f);

    k_NeutBtn    = V(t.surface2);
    k_NeutBtnHov = V(t.surface3);
    k_NeutBtnAct = V(t.surface1);

    k_PrevTrack = V(t.accentDim, 0.6f);
    k_PrevGrab  = V(t.accent);

    k_LiveTrack = V(t.danger, 0.4f);
    k_LiveGrab  = V(t.danger);

    // Acento de la Cola = el acento del TEMA, no "success" (que casi
    // siempre es verde) -- pedido explicito: la Cola debe verse gris en un
    // tema oscuro neutro, no verde fijo sin importar el tema elegido. Verde
    // de verdad solo en el preset que efectivamente es verde (Deadlock).
    k_QueueAccent = V(t.accent);
    k_BtnGreen    = V(t.accent, 0.55f);
    k_BtnGreenH   = V(t.accent, 0.75f);
    k_BtnGreenA   = V(t.accent, 0.40f);
    k_BtnDel      = V(t.danger, 0.55f);
    k_BtnDelH     = V(t.danger, 0.75f);
    k_BtnDelA     = V(t.danger, 0.40f);
    k_BtnDelT     = V(t.danger, 1.0f);
    k_BtnNeutral  = V(t.surface1);
    k_BtnNeutralH = V(t.surface2);
    k_BtnNeutralA = V(t.surface0);
    k_BtnNeutralT = V(t.textDim);
}

} // namespace ProyecThor::UI::MonitorTheme