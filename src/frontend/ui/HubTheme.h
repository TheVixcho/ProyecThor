#pragma once
#include <imgui.h>

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::HubTheme {

// Paleta del Hub de inicio. No son constexpr: se recalculan en Sync()
// a partir del tema activo, igual que DS:: y MonitorTheme::.
inline ImU32 BgSidebar  = IM_COL32(22, 22, 26, 255);
inline ImU32 BgMain     = IM_COL32(16, 16, 19, 255);
inline ImU32 AccentBlue = IM_COL32(35, 116, 225, 255);
inline ImU32 TextPri    = IM_COL32(230, 230, 230, 255);
inline ImU32 TextMuted  = IM_COL32(120, 120, 126, 255);
inline ImU32 Divider    = IM_COL32(45, 45, 52, 255);

// Botones "fantasma"/secundarios (ej. "Abrir configuración", accesos rápidos).
inline ImU32 Surface       = IM_COL32(38, 38, 46, 255);
inline ImU32 SurfaceHover  = IM_COL32(52, 52, 62, 255);
inline ImU32 SurfaceActive = IM_COL32(30, 30, 38, 255);

// Fondos opacos de tarjetas (tarjetas de actualizacion, metricas, canciones).
inline ImU32 Card    = IM_COL32(20, 20, 24, 230);
inline ImU32 CardAlt = IM_COL32(24, 24, 29, 230);

// Acentos secundarios / estados — derivados igual que el resto, para que
// badges, glow del logo y estados semanticos sigan el preset activo.
inline ImU32 AccentSoft  = IM_COL32(115, 244, 233, 255);
// Color de texto a usar sobre un fondo de AccentBlue (botones/badges),
// elegido segun la luminancia del acento para mantener contraste legible
// en presets con acentos claros (ej. Light) u oscuros.
inline ImU32 OnAccent    = IM_COL32(255, 255, 255, 255);
inline ImU32 Success     = IM_COL32( 82, 224, 160, 255);
inline ImU32 Danger      = IM_COL32(240,  90, 100, 255);
inline ImU32 BorderFaint = IM_COL32(255, 255, 255,  15);
inline ImU32 TextFaint   = IM_COL32(255, 255, 255,  70);

// Radios de esquina derivados de theme.windowRounding/frameRounding, para
// que el rounding de cards/modal/botones del Hub cambie con el preset en
// vez de estar hardcodeado (ver Sync()).
inline float RadiusSm = 6.0f;
inline float RadiusMd = 8.0f;
inline float RadiusLg = 10.0f;

// Paleta del fondo animado (grilla + particulas conectadas): dos tonos
// derivados del acento del tema en vez de cian/verde fijos, para que el
// canvas combine con cualquier preset.
inline ImU32 ParticleA = IM_COL32(0,   212, 232, 255);
inline ImU32 ParticleB = IM_COL32(115, 244, 205, 255);

// Alpha del overlay de la imagen de fondo (splash), derivado de la
// luminancia real de theme.base — en temas claros una imagen oscura
// superpuesta a alta opacidad ensucia el fondo, asi que se atenua.
inline float BgImageAlpha = 30.0f / 255.0f;

void Sync(const ProyecThor::Settings::ThemeSettings& theme);

} // namespace ProyecThor::UI::HubTheme