#include "LabPanel.h"
#include "frontend/ui/AppIcons.h"
#include "frontend/ui/DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include "backend/settings/SettingsManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::UI {

// ── Math Expression Parser & Evaluator ──────────────────────────────────────
namespace MathParser {

enum class TokenType {
    Number,
    VariableX,
    VariableT,
    Plus,
    Minus,
    Multiply,
    Divide,
    Power,
    Modulo,
    LParen,
    RParen,
    Identifier,
    End,
    Error
};

struct Token {
    TokenType   type;
    double      numValue = 0.0;
    std::string textValue;
};

class Lexer {
public:
    explicit Lexer(const char* src) : m_Src(src), m_Pos(0) {}

    Token NextToken() {
        while (m_Src[m_Pos] && std::isspace((unsigned char)m_Src[m_Pos])) {
            m_Pos++;
        }

        if (!m_Src[m_Pos]) return { TokenType::End };

        char c = m_Src[m_Pos];

        if (std::isdigit((unsigned char)c) || c == '.') {
            size_t start = m_Pos;
            while (m_Src[m_Pos] && (std::isdigit((unsigned char)m_Src[m_Pos]) || m_Src[m_Pos] == '.')) {
                m_Pos++;
            }
            std::string s = m_Src.substr(start, m_Pos - start);
            try {
                return { TokenType::Number, std::stod(s) };
            } catch (...) {
                return { TokenType::Error };
            }
        }

        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = m_Pos;
            while (m_Src[m_Pos] && (std::isalnum((unsigned char)m_Src[m_Pos]) || m_Src[m_Pos] == '_')) {
                m_Pos++;
            }
            std::string ident = m_Src.substr(start, m_Pos - start);
            std::string lower = ident;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            if (lower == "x")  return { TokenType::VariableX };
            if (lower == "t")  return { TokenType::VariableT };
            if (lower == "pi") return { TokenType::Number, 3.141592653589793 };
            if (lower == "e")  return { TokenType::Number, 2.718281828459045 };

            return { TokenType::Identifier, 0.0, lower };
        }

        m_Pos++;
        switch (c) {
            case '+': return { TokenType::Plus };
            case '-': return { TokenType::Minus };
            case '*': return { TokenType::Multiply };
            case '/': return { TokenType::Divide };
            case '^': return { TokenType::Power };
            case '%': return { TokenType::Modulo };
            case '(': return { TokenType::LParen };
            case ')': return { TokenType::RParen };
            default:  return { TokenType::Error };
        }
    }

private:
    std::string m_Src;
    size_t      m_Pos;
};

class Parser {
public:
    Parser(const char* expr, double x, double t)
        : m_Lexer(expr), m_X(x), m_T(t), m_HasError(false) {
        Advance();
    }

    double Parse() {
        double res = ParseExpression();
        if (m_Current.type != TokenType::End) {
            m_HasError = true;
        }
        return res;
    }

    bool HasError() const { return m_HasError; }

private:
    void Advance() {
        m_Current = m_Lexer.NextToken();
        if (m_Current.type == TokenType::Error) m_HasError = true;
    }

    double ParseExpression() {
        double val = ParseTerm();
        while (m_Current.type == TokenType::Plus || m_Current.type == TokenType::Minus) {
            TokenType op = m_Current.type;
            Advance();
            double rhs = ParseTerm();
            if (op == TokenType::Plus) val += rhs;
            else val -= rhs;
        }
        return val;
    }

    double ParseTerm() {
        double val = ParseFactor();
        while (m_Current.type == TokenType::Multiply || m_Current.type == TokenType::Divide || m_Current.type == TokenType::Modulo) {
            TokenType op = m_Current.type;
            Advance();
            double rhs = ParseFactor();
            if (op == TokenType::Multiply) {
                val *= rhs;
            } else if (op == TokenType::Divide) {
                if (std::abs(rhs) < 1e-12) {
                    val = (val >= 0) ? 1e6 : -1e6;
                } else {
                    val /= rhs;
                }
            } else if (op == TokenType::Modulo) {
                if (std::abs(rhs) > 1e-12) val = std::fmod(val, rhs);
            }
        }
        return val;
    }

    double ParseFactor() {
        double val = ParseUnary();
        if (m_Current.type == TokenType::Power) {
            Advance();
            double rhs = ParseFactor(); // asociatividad por derecha
            val = std::pow(val, rhs);
        }
        return val;
    }

    double ParseUnary() {
        if (m_Current.type == TokenType::Plus) {
            Advance();
            return ParseUnary();
        }
        if (m_Current.type == TokenType::Minus) {
            Advance();
            return -ParseUnary();
        }
        return ParsePrimary();
    }

    double ParsePrimary() {
        if (m_Current.type == TokenType::Number) {
            double v = m_Current.numValue;
            Advance();
            return v;
        }
        if (m_Current.type == TokenType::VariableX) {
            Advance();
            return m_X;
        }
        if (m_Current.type == TokenType::VariableT) {
            Advance();
            return m_T;
        }
        if (m_Current.type == TokenType::LParen) {
            Advance();
            double v = ParseExpression();
            if (m_Current.type == TokenType::RParen) {
                Advance();
            } else {
                m_HasError = true;
            }
            return v;
        }
        if (m_Current.type == TokenType::Identifier) {
            std::string fn = m_Current.textValue;
            Advance();
            if (m_Current.type == TokenType::LParen) {
                Advance();
                double arg = ParseExpression();
                if (m_Current.type == TokenType::RParen) {
                    Advance();
                } else {
                    m_HasError = true;
                }
                return CallMathFunction(fn, arg);
            }
            // Identificador sin paréntesis (ej. sin x)
            double arg = ParseFactor();
            return CallMathFunction(fn, arg);
        }

        m_HasError = true;
        return 0.0;
    }

    double CallMathFunction(const std::string& fn, double arg) {
        if (fn == "sin")   return std::sin(arg);
        if (fn == "cos")   return std::cos(arg);
        if (fn == "tan")   return std::tan(arg);
        if (fn == "asin")  return (arg >= -1.0 && arg <= 1.0) ? std::asin(arg) : 0.0;
        if (fn == "acos")  return (arg >= -1.0 && arg <= 1.0) ? std::acos(arg) : 0.0;
        if (fn == "atan")  return std::atan(arg);
        if (fn == "sinh")  return std::sinh(arg);
        if (fn == "cosh")  return std::cosh(arg);
        if (fn == "tanh")  return std::tanh(arg);
        if (fn == "sqrt")  return (arg >= 0.0) ? std::sqrt(arg) : 0.0;
        if (fn == "cbrt")  return std::cbrt(arg);
        if (fn == "abs")   return std::abs(arg);
        if (fn == "exp")   return std::exp(arg);
        if (fn == "ln")    return (arg > 0.0) ? std::log(arg) : -1e6;
        if (fn == "log")   return (arg > 0.0) ? std::log10(arg) : -1e6;
        if (fn == "floor") return std::floor(arg);
        if (fn == "ceil")  return std::ceil(arg);
        if (fn == "round") return std::round(arg);
        if (fn == "sign" || fn == "sgn") return (arg > 0.0) ? 1.0 : ((arg < 0.0) ? -1.0 : 0.0);

        m_HasError = true;
        return 0.0;
    }

    Lexer  m_Lexer;
    double m_X;
    double m_T;
    Token  m_Current;
    bool   m_HasError;
};

} // namespace MathParser

// ── Constructor ─────────────────────────────────────────────────────────────
LabPanel::LabPanel() {
    // Inicializar 3 funciones por defecto
    {
        LabFunction f1;
        f1.name = "f(x)";
        snprintf(f1.expr, sizeof(f1.expr), "2.5 * sin(x - t * 1.5)");
        f1.color = { 0.15f, 0.85f, 1.00f, 1.0f }; // Cian
        f1.enabled = true;
        f1.thickness = 2.4f;
        m_Functions.push_back(f1);
    }
    {
        LabFunction f2;
        f2.name = "g(x)";
        snprintf(f2.expr, sizeof(f2.expr), "0.15 * x^2 - 3.0");
        f2.color = { 1.00f, 0.78f, 0.20f, 1.0f }; // Ámbar / Oro
        f2.enabled = true;
        f2.thickness = 2.0f;
        m_Functions.push_back(f2);
    }
    {
        LabFunction f3;
        f3.name = "h(x)";
        snprintf(f3.expr, sizeof(f3.expr), "3.0 * exp(-0.4 * abs(x)) * cos(3 * x - t)");
        f3.color = { 0.95f, 0.35f, 0.75f, 1.0f }; // Magenta
        f3.enabled = false;
        f3.thickness = 2.0f;
        m_Functions.push_back(f3);
    }

    // Inicializar biblioteca de fórmulas de referencia
    m_FormulaLibrary = {
        // Cálculo
        { "Cálculo", "Límite Fundamental", "\\lim_{x \\to 0} \\frac{\\sin x}{x} = 1", "Comportamiento infinitesimal de la función seno alrededor del origen.", "x -> variable real" },
        { "Cálculo", "Derivada Exponencial", "\\frac{d}{dx}[e^{kx}] = k \\cdot e^{kx}", "Razón de cambio de funciones de crecimiento exponencial continuo.", "k -> constante de tasa" },
        { "Cálculo", "Integral Gaussiana", "\\int_{-\\infty}^{\\infty} e^{-x^2} dx = \\sqrt{\\pi}", "Área total bajo la curva de distribución normal estándar.", "x -> variable aleatoria" },
        { "Cálculo", "Serie de Fourier", "f(x) = \\frac{a_0}{2} + \\sum_{n=1}^\\infty [a_n \\cos(nx) + b_n \\sin(nx)]", "Descomposición de funciones periódicas en armónicos.", "a_n, b_n -> coeficientes" },

        // Física & Ciencias
        { "Física", "Equivalencia Masa-Energía", "E = m \\cdot c^2", "Principio fundamental de relatividad especial postulado por Einstein.", "E=Energía, m=Masa, c=Vel. Luz" },
        { "Física", "Gravitación Universal", "F = G \\frac{m_1 \\cdot m_2}{r^2}", "Fuerza atractiva entre dos cuerpos con masa separados por distancia r.", "G = 6.674e-11 N m²/kg²" },
        { "Física", "Ecuación de Onda", "\\frac{\\partial^2 u}{\\partial t^2} = v^2 \\frac{\\partial^2 u}{\\partial x^2}", "Ecuación diferencial hiperbólica que describe ondas sonoras y lumínicas.", "v -> velocidad de propagación" },
        { "Física", "Ecuación de Schrödinger", "i\\hbar \\frac{\\partial}{\\partial t}\\Psi = \\hat{H}\\Psi", "Ecuación fundamental de la mecánica cuántica no relativista.", "\\hbar -> cte Planck reducida" },

        // Álgebra & Geometría
        { "Álgebra", "Identidad de Euler", "e^{i\\pi} + 1 = 0", "Conecta las 5 constantes matemáticas fundamentales (e, i, pi, 1, 0).", "i -> unidad imaginaria" },
        { "Álgebra", "Fórmula Cuadrática", "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}", "Raíces exactas del polinomio ax² + bx + c = 0.", "b²-4ac -> discriminante" },
        { "Geometría", "Teorema de Pitágoras", "a^2 + b^2 = c^2", "Relación métrica fundamental en triángulos rectángulos euclidianos.", "c -> hipotenusa" },
        { "Geometría", "Área y Perímetro de Círculo", "A = \\pi r^2, \\quad C = 2\\pi r", "Dimensiones geométricas del círculo en función de su radio r.", "r -> radio del círculo" },
    };

    // Registrar referencia en PresentationCore para transmisión en vivo
    Core::PresentationCore::Get().SetLabPanelRef(this);
}

LabPanel::~LabPanel() {
    Core::PresentationCore::Get().SetLabPanelRef(nullptr);
    if (m_IsProjectingLive) {
        Core::PresentationCore::Get().SetLiveLabActive(false);
    }
}

double LabPanel::EvaluateExpression(const char* expr, double x, double t, bool& hasError) {
    if (!expr || !*expr) {
        hasError = true;
        return 0.0;
    }
    MathParser::Parser parser(expr, x, t);
    double res = parser.Parse();
    hasError = parser.HasError() || std::isnan(res) || std::isinf(res);
    return res;
}

void LabPanel::SetProjectingLive(bool live) {
    m_IsProjectingLive = live;
    Core::PresentationCore::Get().SetLiveLabActive(live);

    if (m_IsProjectingLive) {
        // Enviar contenido a la salida en vivo
        Core::LibrarySelection sel;
        sel.type  = Core::ItemType::Documents;
        sel.title = (m_ActiveTab == 0) ? "Lab: Gráfica GeoGebra 2D" : m_CustomFormulaTitle;
        Core::PresentationCore::Get().SetSelection(sel);

        // Si es pestaña de fórmulas estáticas generamos texto para la nota rápida
        if (m_ActiveTab == 1) {
            std::string slideContent = std::string(m_CustomFormulaTitle) + "\n\n" +
                                       std::string(m_CustomFormulaText) + "\n\n" +
                                       std::string(m_CustomFormulaDesc);
            Core::PresentationCore::Get().SetLiveQuickNote(slideContent);
        } else {
            Core::PresentationCore::Get().ClearQuickNote();
        }
    } else {
        Core::PresentationCore::Get().ClearQuickNote();
    }
}

// ── Render Top Bar ──────────────────────────────────────────────────────────
void LabPanel::RenderTopBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

    // Icono y Título
    {
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        AppIcons::DrawIcon_Formula(dl, { p0.x, p0.y + 2.0f }, 22.0f, DS::AccentColor);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, DS::AccentColor);
        ImGui::TextUnformatted("LAB MATEMÁTICO");
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 16.0f);

    // Selector de Modo (Pills)
    {
        const char* tabNames[] = { "📈 Gráficas GeoGebra", "📜 Fórmulas" };
        for (int i = 0; i < 2; ++i) {
            bool active = (m_ActiveTab == i);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.90f, 0.60f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.65f, 1.00f, 0.90f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.16f, 0.22f, 0.50f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.35f, 0.45f, 0.40f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            if (ImGui::Button(tabNames[i], ImVec2(0.0f, 24.0f))) {
                m_ActiveTab = i;
            }

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0.0f, 6.0f);
        }
    }

    // Botón de Proyección en Vivo
    {
        bool isLive = m_IsProjectingLive;
        ImVec4 btnBg  = isLive ? ImVec4(0.85f, 0.15f, 0.20f, 0.35f) : ImVec4(0.12f, 0.65f, 0.35f, 0.25f);
        ImVec4 btnBdr = isLive ? ImVec4(0.95f, 0.25f, 0.30f, 0.90f) : ImVec4(0.20f, 0.85f, 0.45f, 0.80f);

        ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isLive ? ImVec4(0.95f, 0.25f, 0.30f, 0.50f) : ImVec4(0.20f, 0.85f, 0.45f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Border, btnBdr);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        const char* liveText = isLive ? "[ ⏹ DETENER PROYECCIÓN ]" : "[ 🚀 PROYECTAR A PÚBLICO (GEOGEBRA) ]";
        if (ImGui::Button(liveText, ImVec2(0.0f, 24.0f))) {
            SetProjectingLive(!isLive);
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleVar(2);
}

// ── Render Function Grapher ─────────────────────────────────────────────────
void LabPanel::RenderGrapher(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cMin = ImGui::GetCursorScreenPos();
    ImVec2 cMax = { cMin.x + w, cMin.y + h };

    // Actualizar tiempo de animación
    if (m_Animated) {
        m_Time += ImGui::GetIO().DeltaTime * m_AnimSpeed;
        if (m_Time > 10000.0f) m_Time = 0.0f;
    }

    // Fondo del lienzo cartesiano
    dl->AddRectFilled(cMin, cMax, IM_COL32(10, 13, 20, 255), 6.0f);
    dl->AddRect(cMin, cMax, IM_COL32(50, 60, 85, 200), 6.0f, 0, 1.0f);

    // Área recortada para el dibujo dentro del marco
    dl->PushClipRect(cMin, cMax, true);

    // Factores de conversión: Plano Cartesiano <-> Pantalla
    float xSpan = std::max(0.1f, m_XMax - m_XMin);
    float ySpan = std::max(0.1f, m_YMax - m_YMin);

    auto ToScreen = [&](float x, float y) -> ImVec2 {
        float sx = cMin.x + ((x - m_XMin) / xSpan) * w;
        float sy = cMax.y - ((y - m_YMin) / ySpan) * h;
        return { sx, sy };
    };

    auto ToMath = [&](ImVec2 screenPos) -> ImVec2 {
        float mx = m_XMin + ((screenPos.x - cMin.x) / w) * xSpan;
        float my = m_YMin + ((cMax.y - screenPos.y) / h) * ySpan;
        return { mx, my };
    };

    // 1. Dibujar Cuadrícula y Marcas
    if (m_ShowGrid) {
        // Calcular paso óptimo de rejilla
        float targetStepX = xSpan / 10.0f;
        float pX = std::pow(10.0f, std::floor(std::log10(targetStepX)));
        float stepX = pX;
        if (targetStepX / pX >= 5.0f) stepX = pX * 5.0f;
        else if (targetStepX / pX >= 2.0f) stepX = pX * 2.0f;

        float firstX = std::floor(m_XMin / stepX) * stepX;
        for (float x = firstX; x <= m_XMax + 1e-4f; x += stepX) {
            ImVec2 p0 = ToScreen(x, m_YMin);
            ImVec2 p1 = ToScreen(x, m_YMax);
            dl->AddLine(p0, p1, IM_COL32(255, 255, 255, 18), 1.0f);

            if (m_ShowLabels && std::abs(x) > 1e-4f) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.2g", x);
                ImVec2 pt = ToScreen(x, 0.0f);
                pt.y = std::clamp(pt.y + 3.0f, cMin.y + 4.0f, cMax.y - 18.0f);
                dl->AddText(pt, IM_COL32(140, 160, 195, 170), lbl);
            }
        }

        float targetStepY = ySpan / 8.0f;
        float pY = std::pow(10.0f, std::floor(std::log10(targetStepY)));
        float stepY = pY;
        if (targetStepY / pY >= 5.0f) stepY = pY * 5.0f;
        else if (targetStepY / pY >= 2.0f) stepY = pY * 2.0f;

        float firstY = std::floor(m_YMin / stepY) * stepY;
        for (float y = firstY; y <= m_YMax + 1e-4f; y += stepY) {
            ImVec2 p0 = ToScreen(m_XMin, y);
            ImVec2 p1 = ToScreen(m_XMax, y);
            dl->AddLine(p0, p1, IM_COL32(255, 255, 255, 18), 1.0f);

            if (m_ShowLabels && std::abs(y) > 1e-4f) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.2g", y);
                ImVec2 pt = ToScreen(0.0f, y);
                pt.x = std::clamp(pt.x + 4.0f, cMin.x + 4.0f, cMax.x - 40.0f);
                dl->AddText(pt, IM_COL32(140, 160, 195, 170), lbl);
            }
        }
    }

    // 2. Dibujar Ejes X e Y Principales
    if (m_ShowAxes) {
        ImVec2 y0 = ToScreen(0.0f, m_YMin);
        ImVec2 y1 = ToScreen(0.0f, m_YMax);
        dl->AddLine(y0, y1, IM_COL32(130, 170, 230, 180), 1.6f);

        ImVec2 x0 = ToScreen(m_XMin, 0.0f);
        ImVec2 x1 = ToScreen(m_XMax, 0.0f);
        dl->AddLine(x0, x1, IM_COL32(130, 170, 230, 180), 1.6f);

        // Origen (0,0)
        ImVec2 origin = ToScreen(0.0f, 0.0f);
        dl->AddCircleFilled(origin, 3.5f, IM_COL32(100, 200, 255, 220));
    }

    // 3. Evaluar y Dibujar las Curvas de Funciones
    int samples = std::clamp(m_Resolution, 50, 1200);
    float dx = xSpan / (float)(samples - 1);

    for (auto& fn : m_Functions) {
        if (!fn.enabled) continue;

        ImU32 col = ImGui::ColorConvertFloat4ToU32(fn.color);
        ImVec2 prevScreen;
        bool hasPrev = false;
        bool anyError = false;

        for (int i = 0; i < samples; ++i) {
            float x = m_XMin + i * dx;
            bool err = false;
            double y = EvaluateExpression(fn.expr, (double)x, (double)m_Time, err);

            if (err) {
                anyError = true;
                hasPrev = false;
                continue;
            }

            ImVec2 sc = ToScreen(x, (float)y);

            // Filtrar asíntotas y saltos discontinuos extremos
            if (hasPrev) {
                float dyScreen = std::abs(sc.y - prevScreen.y);
                if (dyScreen < h * 2.0f) {
                    dl->AddLine(prevScreen, sc, col, fn.thickness);
                }
            }

            prevScreen = sc;
            hasPrev = true;
        }

        fn.lastError = anyError ? "Expresión inválida" : "";
    }

    // 4. Procesar Interacción del Ratón (Paneo y Zoom)
    ImGui::SetCursorScreenPos(cMin);
    ImGui::InvisibleButton("##canvas_interaction", ImVec2(w, h));
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    // Paneo (Arrastre)
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        float dXMath = (delta.x / w) * xSpan;
        float dYMath = (delta.y / h) * ySpan;

        m_XMin -= dXMath;
        m_XMax -= dXMath;
        m_YMin += dYMath;
        m_YMax += dYMath;
    }

    // Zoom (Rueda del ratón)
    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.01f) {
            float factor = (wheel > 0) ? 0.85f : 1.15f;
            ImVec2 mPos = ImGui::GetMousePos();
            ImVec2 mMath = ToMath(mPos);

            m_XMin = mMath.x + (m_XMin - mMath.x) * factor;
            m_XMax = mMath.x + (m_XMax - mMath.x) * factor;
            m_YMin = mMath.y + (m_YMin - mMath.y) * factor;
            m_YMax = mMath.y + (m_YMax - mMath.y) * factor;
        }

        // Inspección y cursor de coordenadas en hover
        ImVec2 mPos = ImGui::GetMousePos();
        ImVec2 mMath = ToMath(mPos);
        char coords[64];
        snprintf(coords, sizeof(coords), "X: %.3f | Y: %.3f", mMath.x, mMath.y);

        ImVec2 cSz = ImGui::CalcTextSize(coords);
        ImVec2 b0 = { mPos.x + 12.0f, mPos.y - 20.0f };
        ImVec2 b1 = { b0.x + cSz.x + 12.0f, b0.y + cSz.y + 6.0f };
        dl->AddRectFilled(b0, b1, IM_COL32(15, 20, 30, 220), 4.0f);
        dl->AddRect(b0, b1, IM_COL32(80, 110, 160, 200), 4.0f);
        dl->AddText({ b0.x + 6.0f, b0.y + 3.0f }, IM_COL32(220, 240, 255, 255), coords);

        // Cruz de mira fina
        dl->AddLine({ cMin.x, mPos.y }, { cMax.x, mPos.y }, IM_COL32(255, 255, 255, 45), 1.0f);
        dl->AddLine({ mPos.x, cMin.y }, { mPos.x, cMax.y }, IM_COL32(255, 255, 255, 45), 1.0f);
    }

    dl->PopClipRect();
}

// ── Render Function Controls ────────────────────────────────────────────────
void LabPanel::RenderFunctionControls() {
    ImGui::BeginChild("##funcControlsChild", ImVec2(0, 0), false);

    // Controles de Vista y Animación
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    {
        if (ImGui::Button("⊙ Centrar Vista", ImVec2(110.0f, 24.0f))) {
            m_XMin = -10.0f; m_XMax = 10.0f;
            m_YMin =  -6.0f; m_YMax =  6.0f;
        }
        ImGui::SameLine();

        if (ImGui::Button(m_Animated ? "⏸ Pausar" : "▶ Animar", ImVec2(80.0f, 24.0f))) {
            m_Animated = !m_Animated;
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(100.0f);
        ImGui::SliderFloat("Vel.", &m_AnimSpeed, 0.1f, 5.0f, "%.1fx");
        ImGui::SameLine();

        ImGui::Checkbox("Rejilla", &m_ShowGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Ejes", &m_ShowAxes);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Opciones de Transmisión estilo GeoGebra
    RenderGeoGebraLiveOptions();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Lista de Funciones Matemáticas
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "FUNCIONES ACTIVAS");
    ImGui::SameLine();
    ImGui::TextDisabled("(Usa 'x' para variable y 't' para tiempo)");

    for (int i = 0; i < (int)m_Functions.size(); ++i) {
        auto& fn = m_Functions[i];
        ImGui::PushID(i);

        ImGui::Checkbox("##enabled", &fn.enabled);
        ImGui::SameLine();

        ImGui::ColorEdit4("##color", (float*)&fn.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(60.0f);
        ImGui::Text("%s =", fn.name.c_str());
        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
        ImGui::InputText("##expr", fn.expr, sizeof(fn.expr));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::SliderFloat("##thick", &fn.thickness, 1.0f, 6.0f, "%.1f");

        if (!fn.lastError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  ⚠ %s", fn.lastError.c_str());
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    // Preajustes rápidos de funciones
    ImGui::Spacing();
    ImGui::TextUnformatted("Preajustes Rápidos:");
    auto PresetFnBtn = [&](const char* label, const char* exprStr) {
        if (ImGui::Button(label, ImVec2(0, 22.0f))) {
            if (!m_Functions.empty()) {
                snprintf(m_Functions[0].expr, sizeof(m_Functions[0].expr), "%s", exprStr);
                m_Functions[0].enabled = true;
            }
        }
        ImGui::SameLine();
    };

    PresetFnBtn("Seno", "2.5 * sin(x - t * 2)");
    PresetFnBtn("Parábola", "0.2 * x^2 - 3");
    PresetFnBtn("Gauss", "3.5 * exp(-0.35 * x^2)");
    PresetFnBtn("Onda Amortiguada", "3.0 * exp(-0.25 * abs(x)) * cos(3 * x - t)");
    PresetFnBtn("Polinomio Cúbico", "0.04 * x^3 - 0.6 * x");

    ImGui::NewLine();
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

// ── Render GeoGebra Live Options ────────────────────────────────────────────
void LabPanel::RenderGeoGebraLiveOptions() {
    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1.0f), "TRANSMISIÓN GEOGEBRA A PANTALLA");

    // Selector de Tema de Fondo para la Transmisión
    ImGui::TextDisabled("Fondo de Salida:");
    ImGui::SameLine();

    const char* themeNames[] = { "Transparente (HUD)", "Pizarra Oscura", "GeoGebra Blanca" };
    for (int t = 0; t < 3; ++t) {
        bool sel = (m_ProjectionTheme == t);
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.85f, 0.70f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.40f, 0.70f, 1.00f, 0.90f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.20f, 0.50f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.30f, 0.40f, 0.40f));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

        if (ImGui::Button(themeNames[t], ImVec2(0, 20.0f))) {
            m_ProjectionTheme = t;
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }
    ImGui::NewLine();

    // Checkboxes de elementos visuales GeoGebra
    ImGui::Checkbox("Rótulos / Leyenda f(x)", &m_ShowLegendOnLive);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("Punto Móvil P(x, y)", &m_ShowTracerPoint);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("Flechas Ejes", &m_ShowAxisArrows);
    ImGui::SameLine(0.0f, 12.0f);
    ImGui::Checkbox("Área Integral", &m_ShowIntegralShading);
}

// ── Render Formula Board ────────────────────────────────────────────────────
void LabPanel::RenderFormulaBoard(float w, float h) {
    ImGui::BeginChild("##formulaBoardChild", ImVec2(w, h), false);

    // Barra de Búsqueda
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(std::min(w - 20.0f, 320.0f));
    ImGui::InputTextWithHint("##searchFormula", "🔍 Buscar fórmula (Cálculo, Física, Álgebra...)", m_FormulaSearch, sizeof(m_FormulaSearch));
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    std::string searchFilter = m_FormulaSearch;
    std::transform(searchFilter.begin(), searchFilter.end(), searchFilter.begin(), ::tolower);

    // Editor y Previsualización de Fórmula Seleccionada
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.15f, 0.65f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.50f, 0.85f, 0.40f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);

        if (ImGui::BeginChild("##formulaCardPreview", ImVec2(0, 140), true)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();

            dl->AddText(p, IM_COL32(100, 200, 255, 255), m_CustomFormulaTitle);
            dl->AddText({ p.x, p.y + 24.0f }, IM_COL32(255, 255, 255, 240), m_CustomFormulaText);
            dl->AddText({ p.x, p.y + 60.0f }, IM_COL32(160, 180, 210, 200), m_CustomFormulaDesc);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 92.0f);
            if (ImGui::Button(" 🚀 Proyectar Esta Fórmula ")) {
                SetProjectingLive(true);
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "BIBLIOTECA DE FÓRMULAS");
    ImGui::Spacing();

    // Grilla de Fórmulas Disponibles
    for (int i = 0; i < (int)m_FormulaLibrary.size(); ++i) {
        const auto& item = m_FormulaLibrary[i];

        if (!searchFilter.empty()) {
            std::string text = item.title + " " + item.category + " " + item.formula + " " + item.description;
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            if (text.find(searchFilter) == std::string::npos) continue;
        }

        ImGui::PushID(i);
        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        float cardW = ImGui::GetContentRegionAvail().x;
        float cardH = 58.0f;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool isSel = (m_SelectedFormulaIdx == i);
        ImU32 bg = isSel ? IM_COL32(40, 70, 140, 80) : IM_COL32(20, 25, 36, 120);
        ImU32 bdr = isSel ? IM_COL32(80, 140, 255, 220) : IM_COL32(50, 60, 80, 100);

        dl->AddRectFilled(cardPos, { cardPos.x + cardW, cardPos.y + cardH }, bg, 6.0f);
        dl->AddRect(cardPos, { cardPos.x + cardW, cardPos.y + cardH }, bdr, 6.0f, 0, 1.0f);

        // Badge de Categoría
        dl->AddText({ cardPos.x + 10.0f, cardPos.y + 6.0f }, IM_COL32(100, 180, 255, 220), item.category.c_str());
        dl->AddText({ cardPos.x + 80.0f, cardPos.y + 6.0f }, IM_COL32_WHITE, item.title.c_str());
        dl->AddText({ cardPos.x + 10.0f, cardPos.y + 26.0f }, IM_COL32(220, 230, 255, 200), item.formula.c_str());

        if (ImGui::InvisibleButton("##cardSelect", ImVec2(cardW, cardH))) {
            m_SelectedFormulaIdx = i;
            snprintf(m_CustomFormulaTitle, sizeof(m_CustomFormulaTitle), "%s", item.title.c_str());
            snprintf(m_CustomFormulaText, sizeof(m_CustomFormulaText), "%s", item.formula.c_str());
            snprintf(m_CustomFormulaDesc, sizeof(m_CustomFormulaDesc), "%s", item.description.c_str());
        }

        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ── Transmisión de Gráficas estilo GeoGebra a Pantalla Pública ──────────────
void LabPanel::RenderLiveProjection(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float drawW, float drawH) {
    if (drawW <= 10.0f || drawH <= 10.0f) return;

    const float uiScale = std::clamp(drawW / 1920.0f, 0.35f, 2.5f);
    const float xSpan = std::max(0.1f, m_XMax - m_XMin);
    const float ySpan = std::max(0.1f, m_YMax - m_YMin);

    auto ToScreen = [&](float x, float y) -> ImVec2 {
        float sx = p0.x + ((x - m_XMin) / xSpan) * drawW;
        float sy = p1.y - ((y - m_YMin) / ySpan) * drawH;
        return { sx, sy };
    };

    // ── 1. Fondo según Tema GeoGebra ──
    if (m_ProjectionTheme == 1) {
        // Pizarra Oscura (Dark Board)
        dl->AddRectFilled(p0, p1, IM_COL32(10, 14, 22, 255));
    } else if (m_ProjectionTheme == 2) {
        // Pizarra GeoGebra Blanca (Classic Whiteboard)
        dl->AddRectFilled(p0, p1, IM_COL32(248, 250, 254, 255));
    } else {
        // Transparente (HUD Overlay sobre video/fondo)
        dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 35));
    }

    const bool isLightBg = (m_ProjectionTheme == 2);
    const ImU32 gridCol = isLightBg ? IM_COL32(200, 215, 230, 160) : IM_COL32(255, 255, 255, 26);
    const ImU32 textGridCol = isLightBg ? IM_COL32(90, 105, 130, 220) : IM_COL32(160, 185, 225, 200);
    const ImU32 axisCol = isLightBg ? IM_COL32(30, 45, 70, 240) : IM_COL32(120, 170, 245, 220);

    dl->PushClipRect(p0, p1, true);

    // ── 2. Rejilla y Marcas Numéricas GeoGebra ──
    if (m_ShowGrid) {
        float targetStepX = xSpan / 12.0f;
        float pX = std::pow(10.0f, std::floor(std::log10(targetStepX)));
        float stepX = pX;
        if (targetStepX / pX >= 5.0f) stepX = pX * 5.0f;
        else if (targetStepX / pX >= 2.0f) stepX = pX * 2.0f;

        float firstX = std::floor(m_XMin / stepX) * stepX;
        for (float x = firstX; x <= m_XMax + 1e-4f; x += stepX) {
            ImVec2 s0 = ToScreen(x, m_YMin);
            ImVec2 s1 = ToScreen(x, m_YMax);
            dl->AddLine(s0, s1, gridCol, 1.0f * uiScale);

            if (m_ShowLabels && std::abs(x) > 1e-4f) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.2g", x);
                ImVec2 pt = ToScreen(x, 0.0f);
                pt.y = std::clamp(pt.y + 4.0f * uiScale, p0.y + 6.0f, p1.y - 24.0f * uiScale);
                dl->AddText(pt, textGridCol, lbl);
            }
        }

        float targetStepY = ySpan / 8.0f;
        float pY = std::pow(10.0f, std::floor(std::log10(targetStepY)));
        float stepY = pY;
        if (targetStepY / pY >= 5.0f) stepY = pY * 5.0f;
        else if (targetStepY / pY >= 2.0f) stepY = pY * 2.0f;

        float firstY = std::floor(m_YMin / stepY) * stepY;
        for (float y = firstY; y <= m_YMax + 1e-4f; y += stepY) {
            ImVec2 s0 = ToScreen(m_XMin, y);
            ImVec2 s1 = ToScreen(m_XMax, y);
            dl->AddLine(s0, s1, gridCol, 1.0f * uiScale);

            if (m_ShowLabels && std::abs(y) > 1e-4f) {
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%.2g", y);
                ImVec2 pt = ToScreen(0.0f, y);
                pt.x = std::clamp(pt.x + 6.0f * uiScale, p0.x + 6.0f, p1.x - 45.0f * uiScale);
                dl->AddText(pt, textGridCol, lbl);
            }
        }
    }

    // ── 3. Ejes X e Y Principales con Flechas y Rótulos ──
    if (m_ShowAxes) {
        ImVec2 y0 = ToScreen(0.0f, m_YMin);
        ImVec2 y1 = ToScreen(0.0f, m_YMax);
        dl->AddLine(y0, y1, axisCol, 2.0f * uiScale);

        ImVec2 x0 = ToScreen(m_XMin, 0.0f);
        ImVec2 x1 = ToScreen(m_XMax, 0.0f);
        dl->AddLine(x0, x1, axisCol, 2.0f * uiScale);

        // Flechas en los extremos
        if (m_ShowAxisArrows) {
            float arrSz = 8.0f * uiScale;
            // Flecha X+
            dl->AddTriangleFilled(
                ImVec2(x1.x, x1.y),
                ImVec2(x1.x - arrSz * 1.5f, x1.y - arrSz * 0.7f),
                ImVec2(x1.x - arrSz * 1.5f, x1.y + arrSz * 0.7f),
                axisCol);
            dl->AddText(ImVec2(x1.x - arrSz * 2.5f, x1.y - arrSz * 2.2f), axisCol, "x");

            // Flecha Y+
            dl->AddTriangleFilled(
                ImVec2(y1.x, y1.y),
                ImVec2(y1.x - arrSz * 0.7f, y1.y + arrSz * 1.5f),
                ImVec2(y1.x + arrSz * 0.7f, y1.y + arrSz * 1.5f),
                axisCol);
            dl->AddText(ImVec2(y1.x + arrSz * 1.2f, y1.y + arrSz * 0.2f), axisCol, "y");
        }

        // Origen (0,0)
        ImVec2 origin = ToScreen(0.0f, 0.0f);
        dl->AddCircleFilled(origin, 4.0f * uiScale, axisCol);
    }

    // ── 4. Evaluar y Dibujar Curvas Matemáticas ──
    int samples = std::clamp(m_Resolution * 2, 200, 1600);
    float dx = xSpan / (float)(samples - 1);

    for (const auto& fn : m_Functions) {
        if (!fn.enabled) continue;

        ImU32 col = ImGui::ColorConvertFloat4ToU32(fn.color);
        ImVec2 prevScreen;
        bool hasPrev = false;

        // Opcional: Integral / Sombreado de área bajo la curva
        if (m_ShowIntegralShading) {
            ImU32 areaCol = (col & 0x00FFFFFF) | (0x35 << IM_COL32_A_SHIFT);
            ImVec2 pZeroPrev = ToScreen(m_XMin, 0.0f);
            for (int i = 0; i < samples; i += 2) {
                float x = m_XMin + i * dx;
                bool err = false;
                double y = EvaluateExpression(fn.expr, (double)x, (double)m_Time, err);
                if (err) continue;

                ImVec2 sc = ToScreen(x, (float)y);
                ImVec2 pZero = ToScreen(x, 0.0f);
                if (hasPrev) {
                    dl->AddQuadFilled(prevScreen, sc, pZero, pZeroPrev, areaCol);
                }
                prevScreen = sc;
                pZeroPrev = pZero;
                hasPrev = true;
            }
            hasPrev = false;
        }

        // Trazado de la curva continua
        for (int i = 0; i < samples; ++i) {
            float x = m_XMin + i * dx;
            bool err = false;
            double y = EvaluateExpression(fn.expr, (double)x, (double)m_Time, err);
            if (err) {
                hasPrev = false;
                continue;
            }

            ImVec2 sc = ToScreen(x, (float)y);
            if (hasPrev) {
                float dyScreen = std::abs(sc.y - prevScreen.y);
                if (dyScreen < drawH * 1.5f) {
                    dl->AddLine(prevScreen, sc, col, std::max(1.5f, fn.thickness * uiScale));
                }
            }
            prevScreen = sc;
            hasPrev = true;
        }
    }

    // ── 5. Punto Móvil / Glider con Coordenadas en Vivo (GeoGebra Tracer) ──
    if (m_ShowTracerPoint && !m_Functions.empty() && m_Functions[0].enabled) {
        float xGlider = m_XMin + (0.5f + 0.45f * std::sin(m_Time * 0.85f)) * xSpan;
        bool err = false;
        double yGlider = EvaluateExpression(m_Functions[0].expr, (double)xGlider, (double)m_Time, err);

        if (!err) {
            ImVec2 gPos = ToScreen(xGlider, (float)yGlider);
            ImU32 gCol = ImGui::ColorConvertFloat4ToU32(m_Functions[0].color);

            // Halo pulsante y punto central
            float pulse = 0.5f + 0.5f * std::sin(m_Time * 6.0f);
            dl->AddCircleFilled(gPos, (10.0f + pulse * 4.0f) * uiScale, (gCol & 0x00FFFFFF) | (0x45 << IM_COL32_A_SHIFT));
            dl->AddCircleFilled(gPos, 5.0f * uiScale, IM_COL32_WHITE);
            dl->AddCircle(gPos, 5.0f * uiScale, gCol, 16, 1.8f * uiScale);

            // Badge flotante con coordenadas: P = (x, y)
            char pCoords[64];
            snprintf(pCoords, sizeof(pCoords), "P(%.2f, %.2f)", xGlider, (float)yGlider);
            ImVec2 txtSz = ImGui::CalcTextSize(pCoords);
            ImVec2 b0 = { gPos.x - txtSz.x * 0.5f - 8.0f * uiScale, gPos.y - 28.0f * uiScale };
            ImVec2 b1 = { b0.x + txtSz.x + 16.0f * uiScale, b0.y + txtSz.y + 6.0f * uiScale };

            ImU32 pillBg = isLightBg ? IM_COL32(255, 255, 255, 235) : IM_COL32(15, 22, 35, 230);
            ImU32 pillBdr = gCol;
            ImU32 pillTxt = isLightBg ? IM_COL32(20, 30, 45, 255) : IM_COL32(230, 245, 255, 255);

            dl->AddRectFilled(b0, b1, pillBg, 4.0f * uiScale);
            dl->AddRect(b0, b1, pillBdr, 4.0f * uiScale, 0, 1.2f * uiScale);
            dl->AddText({ b0.x + 8.0f * uiScale, b0.y + 3.0f * uiScale }, pillTxt, pCoords);
        }
    }

    // ── 6. Leyenda Flotante de Ecuaciones GeoGebra (Top-Left) ──
    if (m_ShowLegendOnLive) {
        float legPadX = 18.0f * uiScale, legPadY = 16.0f * uiScale;
        ImVec2 legPos = { p0.x + legPadX, p0.y + legPadY };

        std::vector<const LabFunction*> activeFns;
        for (const auto& fn : m_Functions) {
            if (fn.enabled) activeFns.push_back(&fn);
        }

        if (!activeFns.empty()) {
            float cardW = 280.0f * uiScale;
            float cardH = (32.0f + activeFns.size() * 24.0f) * uiScale;

            ImVec2 l0 = legPos;
            ImVec2 l1 = { legPos.x + cardW, legPos.y + cardH };

            ImU32 cardBg = isLightBg ? IM_COL32(255, 255, 255, 230) : IM_COL32(12, 17, 26, 225);
            ImU32 cardBdr = isLightBg ? IM_COL32(200, 215, 235, 200) : IM_COL32(50, 75, 120, 180);
            dl->AddRectFilled(l0, l1, cardBg, 6.0f * uiScale);
            dl->AddRect(l0, l1, cardBdr, 6.0f * uiScale, 0, 1.2f * uiScale);

            // Título de la leyenda
            dl->AddText({ l0.x + 10.0f * uiScale, l0.y + 6.0f * uiScale },
                isLightBg ? IM_COL32(40, 60, 90, 255) : IM_COL32(100, 180, 255, 240), "GEOGEBRA LIVE");

            float curY = l0.y + 26.0f * uiScale;
            for (const auto* fn : activeFns) {
                ImU32 fnCol = ImGui::ColorConvertFloat4ToU32(fn->color);
                dl->AddRectFilled({ l0.x + 10.0f * uiScale, curY + 4.0f * uiScale },
                                  { l0.x + 18.0f * uiScale, curY + 12.0f * uiScale }, fnCol, 2.0f * uiScale);

                char fnLabel[300];
                snprintf(fnLabel, sizeof(fnLabel), "%s = %s", fn->name.c_str(), fn->expr);
                dl->AddText({ l0.x + 24.0f * uiScale, curY },
                    isLightBg ? IM_COL32(25, 30, 40, 255) : IM_COL32(235, 240, 250, 240), fnLabel);

                curY += 22.0f * uiScale;
            }
        }
    }

    dl->PopClipRect();
}

// ── Main Render Entry Point ─────────────────────────────────────────────────
void LabPanel::Render() {
    RenderTopBar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;

    if (m_ActiveTab == 0) {
        // Modo Graficador 2D: Canvas Cartesiano Arriba + Controles Abajo (Vertical Stack)
        float graphH = std::clamp(totalH * 0.52f, 210.0f, 400.0f);
        RenderGrapher(totalW, graphH);

        ImGui::Spacing();
        RenderFunctionControls();
    } else {
        // Modo Tablero de Fórmulas
        RenderFormulaBoard(totalW, totalH);
    }
}

} // namespace ProyecThor::UI

