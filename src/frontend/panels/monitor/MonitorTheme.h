#pragma once
#include <imgui.h>
#include <algorithm>

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::MonitorTheme {

inline ImVec4 k_Bg0           = { 0.055f, 0.060f, 0.075f, 1.00f };
inline ImVec4 k_Bg1           = { 0.075f, 0.082f, 0.100f, 1.00f };
inline ImVec4 k_Bg2           = { 0.090f, 0.098f, 0.120f, 1.00f };
inline ImVec4 k_Bg3           = { 0.040f, 0.044f, 0.055f, 1.00f };

inline ImVec4 k_BorderSubtle  = { 0.16f, 0.18f, 0.25f, 0.55f };
inline ImVec4 k_BorderFocus   = { 0.22f, 0.26f, 0.40f, 0.80f };

inline ImVec4 k_TextWhite     = { 1.00f, 1.00f, 1.00f, 1.00f };
inline ImVec4 k_TextPrimary   = { 0.88f, 0.90f, 0.96f, 1.00f };
inline ImVec4 k_TextSecondary = { 0.55f, 0.58f, 0.70f, 1.00f };
inline ImVec4 k_TextDim       = { 0.32f, 0.35f, 0.44f, 1.00f };

inline ImVec4 k_PrevAccent    = { 0.35f, 0.70f, 1.00f, 1.00f };
inline ImVec4 k_PrevAccentDim = { 0.20f, 0.42f, 0.70f, 0.45f };
inline ImVec4 k_LiveAccent    = { 1.00f, 0.26f, 0.32f, 1.00f };
inline ImVec4 k_LiveAccentDim = { 0.70f, 0.16f, 0.20f, 0.40f };
inline ImVec4 k_AmberAccent   = { 1.00f, 0.75f, 0.20f, 1.00f };

inline ImVec4 k_LiveBtn       = { 0.55f, 0.10f, 0.14f, 1.00f };
inline ImVec4 k_LiveBtnHov    = { 0.68f, 0.14f, 0.18f, 1.00f };
inline ImVec4 k_LiveBtnAct    = { 0.40f, 0.07f, 0.10f, 1.00f };

inline ImVec4 k_PrevBtn       = { 0.10f, 0.26f, 0.52f, 1.00f };
inline ImVec4 k_PrevBtnHov    = { 0.14f, 0.34f, 0.64f, 1.00f };
inline ImVec4 k_PrevBtnAct    = { 0.07f, 0.18f, 0.40f, 1.00f };

inline ImVec4 k_AmberBtn      = { 0.40f, 0.28f, 0.04f, 1.00f };
inline ImVec4 k_AmberBtnHov   = { 0.52f, 0.38f, 0.06f, 1.00f };
inline ImVec4 k_AmberBtnAct   = { 0.30f, 0.20f, 0.02f, 1.00f };

inline ImVec4 k_NeutBtn       = { 0.12f, 0.14f, 0.18f, 1.00f };
inline ImVec4 k_NeutBtnHov    = { 0.18f, 0.20f, 0.26f, 1.00f };
inline ImVec4 k_NeutBtnAct    = { 0.08f, 0.09f, 0.12f, 1.00f };

inline ImVec4 k_PrevTrack     = { 0.10f, 0.22f, 0.45f, 1.00f };
inline ImVec4 k_PrevGrab      = { 0.35f, 0.68f, 1.00f, 1.00f };

inline ImVec4 k_LiveTrack     = { 0.40f, 0.07f, 0.10f, 1.00f };
inline ImVec4 k_LiveGrab      = { 1.00f, 0.26f, 0.32f, 1.00f };

inline ImVec4 k_QueueAccent   = { 0.22f, 0.88f, 0.52f, 1.00f };
inline ImVec4 k_BtnGreen      = { 0.08f, 0.36f, 0.16f, 1.00f };
inline ImVec4 k_BtnGreenH     = { 0.11f, 0.48f, 0.22f, 1.00f };
inline ImVec4 k_BtnGreenA     = { 0.05f, 0.24f, 0.11f, 1.00f };
inline ImVec4 k_BtnDel        = { 0.42f, 0.06f, 0.08f, 1.00f };
inline ImVec4 k_BtnDelH       = { 0.58f, 0.10f, 0.12f, 1.00f };
inline ImVec4 k_BtnDelA       = { 0.30f, 0.04f, 0.05f, 1.00f };
inline ImVec4 k_BtnDelT       = { 1.00f, 0.56f, 0.56f, 1.00f };
inline ImVec4 k_BtnNeutral    = { 0.12f, 0.13f, 0.17f, 1.00f };
inline ImVec4 k_BtnNeutralH   = { 0.18f, 0.20f, 0.26f, 1.00f };
inline ImVec4 k_BtnNeutralA   = { 0.08f, 0.09f, 0.12f, 1.00f };
inline ImVec4 k_BtnNeutralT   = { 0.60f, 0.63f, 0.78f, 1.00f };

inline constexpr float k_R          = 7.0f;
inline constexpr float k_RLg        = 10.0f;
inline constexpr float k_Pad        = 6.0f;
inline constexpr float k_PadLg      = 10.0f;
inline constexpr float k_Gap        = 5.0f;
inline constexpr float k_ControlsH  = 150.0f;
inline constexpr float k_TransportH = 26.0f;
inline constexpr float k_VolumeH    = 24.0f;
inline constexpr float k_Meters_H   = 48.0f;
inline constexpr float k_CenterW    = 86.0f;

inline ImU32 ColAf(const ImVec4& c, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, std::clamp(c.w * a, 0.0f, 1.0f)));
}

// Recalcula la paleta de MonitorView a partir del tema activo.
void Sync(const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::UI::MonitorTheme