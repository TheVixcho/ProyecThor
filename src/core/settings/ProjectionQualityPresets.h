#pragma once
#include <algorithm>

namespace ProyecThor::Settings {

// Presets estandar de calidad de salida para el video de fondo del proyector.
// w/h = 0,0 (unicamente el ultimo, "Nativo") es un sentinela: usa la
// resolucion real del monitor destino en vez de un tamano fijo.
struct QualityPreset {
    const char* label;
    int w;
    int h;
    int fps;
};

inline constexpr QualityPreset kQualityPresets[] = {
    { "720p / 30 FPS",  1280, 720,  30 },
    { "720p / 60 FPS",  1280, 720,  60 },
    { "1080p / 30 FPS", 1920, 1080, 30 },
    { "1080p / 60 FPS", 1920, 1080, 60 },
    { "Nativo",         0,    0,    0  },
};
inline constexpr int kQualityPresetCount = static_cast<int>(sizeof(kQualityPresets) / sizeof(kQualityPresets[0]));

enum class OutputQualityMode { Auto = 0, Preset = 1, Custom = 2 };

// Heuristica de modo Auto: entrega un objetivo de reescalado FSR razonable
// segun la resolucion real del monitor. Pensada para el caso de uso actual
// del FSR de este proyecto (upscale de video de baja resolucion), no para
// reducir carga de decodificacion. Facil de ajustar mas adelante.
inline void ResolveAutoQualityTarget(int monitorW, int monitorH, int& outW, int& outH) {
    const long long pixels = static_cast<long long>(monitorW) * monitorH;

    if (monitorW <= 0 || monitorH <= 0 || pixels <= 1280LL * 720) {
        outW = monitorW; outH = monitorH; // ya es chico: nada que ganar reescalando
    } else if (pixels <= 1920LL * 1080) {
        outW = 1280; outH = 720;
    } else {
        outW = 1920; outH = 1080; // 1440p/4K/ultrawide: tope en 1080p
    }
}

// Punto unico de resolucion del target de calidad, usado por Settings,
// ControlPanel y UIManager para no duplicar el switch en 3 lugares.
inline void ResolveQualityTarget(OutputQualityMode mode, int presetIndex,
                                  int customW, int customH,
                                  int monitorW, int monitorH,
                                  int& outW, int& outH)
{
    switch (mode) {
        case OutputQualityMode::Preset: {
            int idx = std::clamp(presetIndex, 0, kQualityPresetCount - 1);
            const QualityPreset& p = kQualityPresets[idx];
            if (p.w <= 0 || p.h <= 0) { outW = monitorW; outH = monitorH; }
            else { outW = std::min(p.w, monitorW); outH = std::min(p.h, monitorH); }
            break;
        }
        case OutputQualityMode::Custom: {
            int w = customW > 0 ? customW : monitorW;
            int h = customH > 0 ? customH : monitorH;
            outW = std::clamp(w, 320, std::max(320, monitorW));
            outH = std::clamp(h, 180, std::max(180, monitorH));
            break;
        }
        case OutputQualityMode::Auto:
        default:
            ResolveAutoQualityTarget(monitorW, monitorH, outW, outH);
            break;
    }
}

} // namespace ProyecThor::Settings
