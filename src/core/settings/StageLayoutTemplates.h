#pragma once

namespace ProyecThor::Settings {

// Tipo de contenido que puede mostrar una celda del Stage Display.
enum class StageWidgetType { Empty = 0, Clock = 1, LiveText = 2, NextLine = 3 };

inline const char* StageWidgetTypeName(StageWidgetType t) {
    switch (t) {
        case StageWidgetType::Clock:    return "Reloj";
        case StageWidgetType::LiveText: return "Texto en vivo";
        case StageWidgetType::NextLine: return "Proxima linea";
        default:                        return "Vacio";
    }
}

inline constexpr int kStageMaxCells = 4;

// Plantilla de grid: cada celda es un rectangulo en fracciones 0..1 del
// monitor destino (x, y, w, h). Plantillas fijas en vez de un editor libre de
// arrastrar/redimensionar, para mantener el alcance simple.
struct StageLayoutTemplate {
    const char* label;
    int cellCount;
    float rect[kStageMaxCells][4];
};

inline constexpr StageLayoutTemplate kStageLayoutTemplates[] = {
    { "1 celda",    1, { {0.0f, 0.0f, 1.0f, 1.0f} } },
    { "2 filas",    2, { {0.0f, 0.0f, 1.0f, 0.5f}, {0.0f, 0.5f, 1.0f, 0.5f} } },
    { "2 columnas", 2, { {0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f} } },
    { "3 celdas",   3, { {0.0f, 0.0f, 1.0f, 0.6f}, {0.0f, 0.6f, 0.5f, 0.4f}, {0.5f, 0.6f, 0.5f, 0.4f} } },
    { "4 celdas",   4, { {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
                         {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f} } },
};
inline constexpr int kStageLayoutTemplateCount =
    static_cast<int>(sizeof(kStageLayoutTemplates) / sizeof(kStageLayoutTemplates[0]));

} // namespace ProyecThor::Settings
