#pragma once
#include <imgui.h>
#include <functional>

struct GLFWmonitor;

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  ControlWidgets — piezas de UI compartidas entre ControlPanel y
//  StageDisplayPanel (antes vivian duplicadas/enterradas dentro de
//  ControlPanel.cpp; ahora que StageDisplayPanel tambien necesita tarjetas y
//  el selector de monitor/LAN, se comparten desde aca).
// ─────────────────────────────────────────────────────────────────────────────

using DrawIconFn = void(*)(ImDrawList*, ImVec2 center, float radius, ImU32 col);

// ── Utilidades de color ─────────────────────────────────────────────────────
ImVec4 ToVec4(ImU32 col);
ImU32  ColA(ImU32 col, int a);
ImVec4 Brighten(const ImVec4& c, float amount);
ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t);

// ── Punto de estado con halo pulsante (para indicadores "en vivo") ─────────
void DrawStatusDot(ImDrawList* dl, ImVec2 center, float r, ImU32 col, bool pulse = false);

// ── Boton de icono vectorial (reemplaza al viejo ThemeIconButton basado en
//    texturas: los iconos se dibujan directo con ImDrawList, nitidos a
//    cualquier tamano/DPI). Anima el hover/press con un lerp suave. ────────
bool VectorIconButton(const char* id, DrawIconFn drawIcon, const char* tooltip,
                      ImVec2 size, ImVec4 bgColor, ImVec4 hoverColor,
                      ImVec4 activeColor, ImVec4 iconColor, bool toggledOn = false);

// ── Boton grande de accion: icono + label centrados en una capsula, con
//    hover animado y brillo pulsante opcional (halo) para estados "activos"
//    — usado para "Iniciar/Detener Proyección", "Activar/Detener Stage". ───
bool IconLabelButton(const char* id, const char* label, DrawIconFn icon, ImVec2 size,
                     ImVec4 bgColor, ImVec4 hoverColor, ImVec4 activeColor, ImVec4 textColor,
                     bool pulseGlow = false);

// ── Iconos (convencion centro+radio, igual que LPDrawIconFn en LayersTheme.h) ─
namespace ControlIcons {
    void DrawPlay(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawStop(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawChevronLeft(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawChevronRight(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawBroadcast(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawScreenCast(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawRoute(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawQuality(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
    void DrawStageMonitor(ImDrawList* dl, ImVec2 c, float r, ImU32 col);
}

// ── Tarjeta tipo "glass" (fill + borde + highlight), igual lenguaje visual
//    que DS::BeginGlassPanel pero anidada como child dentro del panel ───────
bool BeginCard(const char* id, float minHeight = 0.0f);
void EndCard();

// ── Detecta que pantalla fisica ocupa la ventana principal de la app ───────
int DetectCurrentMonitorIndex();

// ── Selector de monitor (+ opcion LAN) con flechas prev/next ───────────────
void MonitorSelector(const char* idPrefix, int selected, int monitorCountOverride,
                     bool includeLAN, int currentAppMonitor,
                     std::function<void(int)> onCycle,
                     std::function<void(int)> onPick);

} // namespace ProyecThor::UI
