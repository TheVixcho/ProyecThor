#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <imgui.h>

namespace ProyecThor::UI {

class UIManager;

// ── Definición de una Función Matemática ────────────────────────────────────
struct LabFunction {
    std::string name       = "f(x)";
    char        expr[256]  = "sin(x)";
    ImVec4      color      = { 0.20f, 0.85f, 1.00f, 1.0f }; // Cian neón
    bool        enabled    = true;
    float       thickness  = 2.0f;
    std::string lastError  = "";
};

// ── Entrada de Fórmula de Referencia / Tablero ──────────────────────────────
struct LabFormulaEntry {
    std::string category;
    std::string title;
    std::string formula;
    std::string description;
    std::string parameters;
};

// ── LabPanel: Laboratorio de Matemáticas, Funciones y Fórmulas ───────────────
class LabPanel {
public:
    LabPanel();
    ~LabPanel();

    void SetUIManager(UIManager* manager) { m_UIManager = manager; }
    void Render();

    // Estado de proyección en vivo
    bool IsProjectingLive() const { return m_IsProjectingLive; }
    void SetProjectingLive(bool live);

    // Transmisión de Gráficas estilo GeoGebra a Pantalla Pública / Proyector
    void RenderLiveProjection(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH);

private:
    void RenderTopBar();
    void RenderGrapher(float w, float h);
    void RenderFormulaBoard(float w, float h);
    void RenderFunctionControls();
    void RenderPresetsPopup();
    void RenderGeoGebraLiveOptions();

    // Evaluador de expresiones matemáticas
    double EvaluateExpression(const char* expr, double x, double t, bool& hasError);

    UIManager* m_UIManager = nullptr;

    // Modo activo dentro del Lab: 0 = Graficador de Funciones, 1 = Tablero de Fórmulas
    int m_ActiveTab = 0;

    // Funciones graficables
    std::vector<LabFunction> m_Functions;

    // Rango y coordenadas del visor cartesiano
    float m_XMin = -10.0f;
    float m_XMax =  10.0f;
    float m_YMin =  -6.0f;
    float m_YMax =   6.0f;

    // Parámetros de animación y renderizado
    float m_Time        = 0.0f;
    bool  m_Animated    = true;
    float m_AnimSpeed   = 1.0f;
    bool  m_ShowGrid    = true;
    bool  m_ShowAxes    = true;
    bool  m_ShowLabels  = true;
    int   m_Resolution  = 400; // Número de muestras por función

    // Opciones de transmisión estilo GeoGebra a pantalla pública
    int   m_ProjectionTheme     = 0; // 0: Transparente (HUD), 1: Pizarra Oscura, 2: Pizarra GeoGebra Blanca
    bool  m_ShowLegendOnLive    = true;  // Rótulos flotantes de f(x), g(x), etc.
    bool  m_ShowTracerPoint     = true;  // Punto móvil con coordenadas P(x, y)
    bool  m_ShowIntegralShading = false; // Área bajo la curva translúcida
    bool  m_ShowAxisArrows      = true;  // Flechas en los extremos de los ejes

    // Tablero de Fórmulas
    char m_FormulaSearch[128] = "";
    char m_CustomFormulaTitle[128] = "Ecuación de Onda";
    char m_CustomFormulaText[512]  = "ψ(x, t) = A * sin(k*x - ω*t + φ)";
    char m_CustomFormulaDesc[512]  = "Propagación de una onda armónica unidimensional.";
    int  m_SelectedFormulaIdx = 0;
    std::vector<LabFormulaEntry> m_FormulaLibrary;

    // Proyección en vivo
    bool m_IsProjectingLive = false;
};

} // namespace ProyecThor::UI

