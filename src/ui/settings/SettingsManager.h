#pragma once
#include "AppSettings.h"
#include <string>
#include <algorithm>  // std::clamp

namespace ProyecThor::Settings {

    // ─────────────────────────────────────────────────────────────────────────
    // SettingsManager  —  Singleton
    //
    // Uso:
    //   auto& cfg = SettingsManager::Get();
    //   cfg.Load();                       // al iniciar la app
    //   cfg.Get().projection.textSize = 80;
    //   cfg.Save();                       // al cerrar o cambiar ajuste
    //   cfg.ApplyTheme();                 // aplica colores a ImGui
    //   cfg.ApplyProjection();            // aplica al PresentationCore
    // ─────────────────────────────────────────────────────────────────────────
    class SettingsManager {
    public:
        static SettingsManager& Get();

        void Load(const std::string& path = "settings.json");
        void Save(const std::string& path = "settings.json");
        void ResetToDefaults();

        AppSettings&       GetSettings()       { return m_Settings; }
    const AppSettings& GetSettings() const { return m_Settings; }

        // Aplica el tema de color actual a ImGui (llama a Apply internamente)
        void ApplyTheme();

        // Empuja los ajustes de proyección al PresentationCore
        void ApplyProjection();

        const std::string& GetSettingsPath() const { return m_LastPath; }

    private:
        SettingsManager() = default;
        AppSettings m_Settings;
        std::string m_LastPath = "settings.json";
    };

} // namespace ProyecThor::Settings
