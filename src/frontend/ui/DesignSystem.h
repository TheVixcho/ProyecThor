#pragma once

#include <imgui.h>
#include <string>
#include "GlassRenderer.h"

namespace ProyecThor::Settings { struct ThemeSettings; }

namespace ProyecThor::UI::DS {

// Antes 8/12/16 (look "glass" redondeado) — se bajan a bordes sutiles, mas
// cuadrado, consistente con el resto de la app (ver BeginCard en
// ControlWidgets.cpp, que ya habia bajado a esto localmente).
constexpr float RadiusSmall  =  3.0f;
constexpr float RadiusMedium =  5.0f;
constexpr float RadiusLarge  =  6.0f;
// Antes 40px — filas mas compactas para mostrar mas items en pantalla.
constexpr float RowHeight    = 30.0f;
constexpr float ButtonHeight = 32.0f;

// Gris neutro tipo ProPresenter/OBS (ver MakeThemePreset(Dark) en
// SettingsManager.cpp para el preset real; esto es solo el valor antes del
// primer SyncFromTheme, que corre en el arranque via ApplyTheme()).
inline ImU32 TextPrimary   = IM_COL32(235, 235, 237, 255);
inline ImU32 TextSecondary = IM_COL32(148, 148, 153, 255);
inline ImU32 TextHint      = IM_COL32( 92,  92,  97, 255);

inline ImU32 AccentColor    = IM_COL32(140, 142, 148, 255);
inline ImU32 AccentLight    = IM_COL32(184, 186, 191, 255);
inline ImU32 AccentPastel   = IM_COL32(210, 211, 214, 255);
inline ImU32 AccentColorDim = IM_COL32( 97,  99, 105, 255);
inline ImU32 AccentColorHov = IM_COL32(160, 162, 167, 255);

inline ImU32 DangerColor    = IM_COL32(240,  90, 100, 255);
inline ImU32 DangerColorDim = IM_COL32(240,  90, 100, 255);
inline ImU32 SuccessColor   = IM_COL32( 82, 224, 160, 255);

// Fondo del panel — sólido (sin blur ni alpha), un solo tono parejo.
inline ImU32 GlassFillTop   = IM_COL32( 20,  20,  21, 255);
inline ImU32 GlassFillBot   = IM_COL32( 20,  20,  21, 255);
inline ImU32 GlassTint      = IM_COL32(  0,   0,   0,   0); // ya no se usa como overlay
inline ImU32 GlassBorder    = IM_COL32(255, 255, 255,  26);
inline ImU32 GlassHighlight = IM_COL32(235, 235, 237,  95);
inline ImU32 GlassShadow    = IM_COL32(  0,   0,   0, 255);

inline ImU32 RowSelectedFill = IM_COL32(140, 142, 148, 255);
inline ImU32 RowSelectedBar  = IM_COL32(184, 186, 191, 255);
inline ImU32 RowHoverFill    = IM_COL32( 42,  42,  44, 255);

inline ImU32 BtnDefaultFill  = IM_COL32( 33,  33,  35, 255);
inline ImU32 BtnDefaultBord  = IM_COL32(120, 122, 128, 255);
inline ImU32 BtnHoverFill    = IM_COL32( 46,  47,  50, 255);
inline ImU32 BtnHoverBord    = IM_COL32(150, 152, 158, 255);

inline ImU32 SepColor        = IM_COL32(120, 122, 128, 255);

void SyncFromTheme(const ProyecThor::Settings::ThemeSettings& theme);

bool BeginGlassPanel(const char* name, GlassRenderer& glass, bool* open = nullptr,
                     ImGuiWindowFlags flags = 0, ImVec2 windowPadding = ImVec2(14.0f, 12.0f));
void EndGlassPanel();
bool GlassButton(const char* label, const ImVec2& size = ImVec2(0.0f, ButtonHeight), ImU32 accent = AccentColor);

// Boton con icono de StyleGeneralApp (fallback a glifo corto si el icono no
// esta cargado). Extraido de LibrarySongs.cpp (donde vivia duplicado junto a
// variantes locales en LibraryVideos.cpp/LibraryDocuments.cpp) para que el
// editor de canciones (SongEditView) tambien pueda usarlo, ej. para
// Undo/Redo. El icono se recorta como cuadrado centrado a partir del lado
// menor del boton, para no estirarse en botones anchos y bajos.
bool GlassIconButton(const char* id,
                     const char* iconKey,
                     const char* fallbackGlyph,
                     const char* tooltip,
                     ImVec2      size,
                     ImVec4      tint = ImGui::ColorConvertU32ToFloat4(TextPrimary));
bool GlassListRow(const char* label, bool selected, float indent = 14.0f, float height = RowHeight);
void GlassSeparator(float thickness = 1.0f);
void GlassSectionHeader(const char* label);

// Slider "estilo HTML": track fino + thumb circular, con una leve animación
// de crecimiento en hover/drag (ver GetStateStorage()->GetFloatRef, mismo
// patron que LPHoverLerp en LayersTheme.h). Reemplaza el look de barra
// gruesa rellena por defecto de ImGui::SliderFloat en toda la app.
bool ModernSlider(const char* id, float* value, float minVal, float maxVal,
                  float width = -1.0f, ImU32 accentOverride = 0, ImU32 trackOverride = 0);

}