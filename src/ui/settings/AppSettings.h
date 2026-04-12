#pragma once
#include <string>
#include <array>

namespace ProyecThor::Settings {

    // ── Idiomas soportados ────────────────────────────────────────────────────
    enum class Language { Spanish = 0, English, Portuguese, COUNT };

    inline const char* LanguageName(Language l) {
        switch (l) {
            case Language::Spanish:    return "Español";
            case Language::English:    return "English";
            case Language::Portuguese: return "Português";
            default:                   return "Español";
        }
    }

    // ── Configuración de Proyección ───────────────────────────────────────────
    struct ProjectionSettings {
        int     targetMonitor   = 1;        // índice del monitor proyector
        float   textSize        = 72.0f;
        float   textColor[4]    = {1,1,1,1};
        float   bgColor[3]      = {0,0,0};
        float   margins[4]      = {80,60,80,60}; // L T R B en píxeles (base 1920)
        int     textAlignment   = 1;        // 0=left 1=center 2=right
        bool    autoScale       = true;
        bool    showClock       = false;
        bool    showVerseRef    = true;     // muestra "Juan 3:16" sobre el texto
        float   lineSpacing     = 1.2f;
        float   textOpacity     = 1.0f;
        bool    shadowEnabled   = true;
        float   shadowOffset[2] = {3.0f, 3.0f};
        float   shadowOpacity   = 0.86f;
    };

    // ── Configuración General ─────────────────────────────────────────────────
    struct GeneralSettings {
        Language language       = Language::Spanish;
        bool     autoSave       = true;
        int      autoSaveIntervalSec = 300;   // 5 minutos
        bool     startMinimized = false;
        bool     rememberLayout = true;
        bool     confirmOnExit  = true;
        std::string defaultBiblesFolder = "assets/bibles";
        std::string defaultMediaFolder  = "assets/videos";
        std::string lastOpenedBible     = "";
    };

    // ── Actualizaciones automáticas ───────────────────────────────────────────
    struct UpdateSettings {
        bool    checkOnStartup  = true;
        bool    autoDownload    = false;
        std::string updateChannel = "stable";  // "stable" | "beta"
        std::string lastChecked = "";
        std::string currentVersion = "0.1.0";
    };

    // ── Apariencia / Tema ─────────────────────────────────────────────────────
    struct ThemeSettings {
        // Todos los colores que ApplyTheme() necesita
        float base[4]         = {0.055f, 0.060f, 0.080f, 1.0f};
        float surface0[4]     = {0.075f, 0.082f, 0.108f, 1.0f};
        float surface1[4]     = {0.095f, 0.104f, 0.138f, 1.0f};
        float surface2[4]     = {0.120f, 0.130f, 0.170f, 1.0f};
        float surface3[4]     = {0.155f, 0.165f, 0.210f, 1.0f};
        float accent[4]       = {0.788f, 0.659f, 0.298f, 1.0f};
        float accentLight[4]  = {0.886f, 0.753f, 0.408f, 1.0f};
        float accentDim[4]    = {0.500f, 0.415f, 0.180f, 1.0f};
        float accentFaint[4]  = {0.240f, 0.200f, 0.085f, 1.0f};
        float border[4]       = {0.200f, 0.215f, 0.275f, 1.0f};
        float borderFaint[4]  = {0.140f, 0.150f, 0.195f, 1.0f};
        float textPrimary[4]  = {0.920f, 0.910f, 0.880f, 1.0f};
        float textDim[4]      = {0.550f, 0.550f, 0.540f, 1.0f};
        float textFaint[4]    = {0.320f, 0.320f, 0.310f, 1.0f};
    };

    // ── Audio ─────────────────────────────────────────────────────────────────
    struct AudioSettings {
        int     masterVolume    = 80;
        bool    muted           = false;
        std::string audioDevice = "";   // vacío = dispositivo por defecto
        bool    muteOnBlank     = true;
    };

    // ── Root: todas las configuraciones juntas ────────────────────────────────
    struct AppSettings {
        GeneralSettings   general;
        ProjectionSettings projection;
        ThemeSettings     theme;
        AudioSettings     audio;
        UpdateSettings    updates;
    };

} // namespace ProyecThor::Settings
