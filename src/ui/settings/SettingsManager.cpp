#include "SettingsManager.h"
#include "../../PresentationCore.h"
#include <imgui.h>
#include <fstream>
#include <iostream>

// ── JSON (nlohmann/json, header-only) ────────────────────────────────────────
// Asegúrate de tener nlohmann/json.hpp en tu árbol de dependencias.
// Con vcpkg:  vcpkg install nlohmann-json
// Con CMake:  find_package(nlohmann_json CONFIG REQUIRED)
//             target_link_libraries(ProyecThor PRIVATE nlohmann_json::nlohmann_json)
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ProyecThor::Settings {

    // ── Helpers ───────────────────────────────────────────────────────────────
    static json Vec4ToJson(const float v[4]) {
        return { v[0], v[1], v[2], v[3] };
    }
    static void JsonToVec4(const json& j, float v[4]) {
        if (j.is_array() && j.size() == 4)
            for (int i = 0; i < 4; i++) v[i] = j[i].get<float>();
    }
    static json Vec3ToJson(const float v[3]) {
        return { v[0], v[1], v[2] };
    }
    static void JsonToVec3(const json& j, float v[3]) {
        if (j.is_array() && j.size() == 3)
            for (int i = 0; i < 3; i++) v[i] = j[i].get<float>();
    }

    // ─────────────────────────────────────────────────────────────────────────
    SettingsManager& SettingsManager::Get() {
        static SettingsManager instance;
        return instance;
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsManager::Load(const std::string& path) {
        m_LastPath = path;
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cout << "[Settings] No se encontró '" << path << "', usando valores por defecto.\n";
            return;
        }

        try {
            json root = json::parse(f);

            // ── General ──
            if (root.contains("general")) {
                auto& g  = root["general"];
                auto& gs = m_Settings.general;
                if (g.contains("language"))            gs.language = static_cast<Language>(g["language"].get<int>());
                if (g.contains("autoSave"))            gs.autoSave = g["autoSave"];
                if (g.contains("autoSaveIntervalSec")) gs.autoSaveIntervalSec = g["autoSaveIntervalSec"];
                if (g.contains("startMinimized"))      gs.startMinimized = g["startMinimized"];
                if (g.contains("rememberLayout"))      gs.rememberLayout = g["rememberLayout"];
                if (g.contains("confirmOnExit"))       gs.confirmOnExit = g["confirmOnExit"];
                if (g.contains("defaultBiblesFolder")) gs.defaultBiblesFolder = g["defaultBiblesFolder"];
                if (g.contains("defaultMediaFolder"))  gs.defaultMediaFolder  = g["defaultMediaFolder"];
                if (g.contains("lastOpenedBible"))     gs.lastOpenedBible     = g["lastOpenedBible"];
            }

            // ── Projection ──
            if (root.contains("projection")) {
                auto& p  = root["projection"];
                auto& ps = m_Settings.projection;
                if (p.contains("targetMonitor"))  ps.targetMonitor  = p["targetMonitor"];
                if (p.contains("textSize"))        ps.textSize       = p["textSize"];
                if (p.contains("textAlignment"))   ps.textAlignment  = p["textAlignment"];
                if (p.contains("autoScale"))       ps.autoScale      = p["autoScale"];
                if (p.contains("showClock"))       ps.showClock      = p["showClock"];
                if (p.contains("showVerseRef"))    ps.showVerseRef   = p["showVerseRef"];
                if (p.contains("lineSpacing"))     ps.lineSpacing    = p["lineSpacing"];
                if (p.contains("textOpacity"))     ps.textOpacity    = p["textOpacity"];
                if (p.contains("shadowEnabled"))   ps.shadowEnabled  = p["shadowEnabled"];
                if (p.contains("shadowOpacity"))   ps.shadowOpacity  = p["shadowOpacity"];
                if (p.contains("textColor"))  JsonToVec4(p["textColor"],  ps.textColor);
                if (p.contains("bgColor"))    JsonToVec3(p["bgColor"],    ps.bgColor);
                if (p.contains("margins"))    JsonToVec4(p["margins"],    ps.margins);
                if (p.contains("shadowOffset")) {
                    auto& so = p["shadowOffset"];
                    if (so.is_array() && so.size() == 2) {
                        ps.shadowOffset[0] = so[0]; ps.shadowOffset[1] = so[1];
                    }
                }
            }

            // ── Theme ──
            if (root.contains("theme")) {
                auto& t  = root["theme"];
                auto& ts = m_Settings.theme;
                if (t.contains("base"))         JsonToVec4(t["base"],        ts.base);
                if (t.contains("surface0"))     JsonToVec4(t["surface0"],    ts.surface0);
                if (t.contains("surface1"))     JsonToVec4(t["surface1"],    ts.surface1);
                if (t.contains("surface2"))     JsonToVec4(t["surface2"],    ts.surface2);
                if (t.contains("surface3"))     JsonToVec4(t["surface3"],    ts.surface3);
                if (t.contains("accent"))       JsonToVec4(t["accent"],      ts.accent);
                if (t.contains("accentLight"))  JsonToVec4(t["accentLight"], ts.accentLight);
                if (t.contains("accentDim"))    JsonToVec4(t["accentDim"],   ts.accentDim);
                if (t.contains("accentFaint"))  JsonToVec4(t["accentFaint"], ts.accentFaint);
                if (t.contains("border"))       JsonToVec4(t["border"],      ts.border);
                if (t.contains("borderFaint"))  JsonToVec4(t["borderFaint"], ts.borderFaint);
                if (t.contains("textPrimary"))  JsonToVec4(t["textPrimary"], ts.textPrimary);
                if (t.contains("textDim"))      JsonToVec4(t["textDim"],     ts.textDim);
                if (t.contains("textFaint"))    JsonToVec4(t["textFaint"],   ts.textFaint);
            }

            // ── Audio ──
            if (root.contains("audio")) {
                auto& a  = root["audio"];
                auto& as = m_Settings.audio;
                if (a.contains("masterVolume")) as.masterVolume = a["masterVolume"];
                if (a.contains("muted"))        as.muted        = a["muted"];
                if (a.contains("audioDevice"))  as.audioDevice  = a["audioDevice"];
                if (a.contains("muteOnBlank"))  as.muteOnBlank  = a["muteOnBlank"];
            }

            // ── Updates ──
            if (root.contains("updates")) {
                auto& u  = root["updates"];
                auto& us = m_Settings.updates;
                if (u.contains("checkOnStartup"))  us.checkOnStartup  = u["checkOnStartup"];
                if (u.contains("autoDownload"))    us.autoDownload    = u["autoDownload"];
                if (u.contains("updateChannel"))   us.updateChannel   = u["updateChannel"];
                if (u.contains("lastChecked"))     us.lastChecked     = u["lastChecked"];
            }

            std::cout << "[Settings] Cargado desde '" << path << "'.\n";

        } catch (const std::exception& e) {
            std::cerr << "[Settings] Error al parsear JSON: " << e.what() << "\n";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsManager::Save(const std::string& path) {
        m_LastPath = path;
        const auto& s = m_Settings;

        json root;

        // ── General ──
        root["general"] = {
            {"language",            static_cast<int>(s.general.language)},
            {"autoSave",            s.general.autoSave},
            {"autoSaveIntervalSec", s.general.autoSaveIntervalSec},
            {"startMinimized",      s.general.startMinimized},
            {"rememberLayout",      s.general.rememberLayout},
            {"confirmOnExit",       s.general.confirmOnExit},
            {"defaultBiblesFolder", s.general.defaultBiblesFolder},
            {"defaultMediaFolder",  s.general.defaultMediaFolder},
            {"lastOpenedBible",     s.general.lastOpenedBible}
        };

        // ── Projection ──
        root["projection"] = {
            {"targetMonitor",  s.projection.targetMonitor},
            {"textSize",       s.projection.textSize},
            {"textAlignment",  s.projection.textAlignment},
            {"autoScale",      s.projection.autoScale},
            {"showClock",      s.projection.showClock},
            {"showVerseRef",   s.projection.showVerseRef},
            {"lineSpacing",    s.projection.lineSpacing},
            {"textOpacity",    s.projection.textOpacity},
            {"shadowEnabled",  s.projection.shadowEnabled},
            {"shadowOpacity",  s.projection.shadowOpacity},
            {"textColor",      Vec4ToJson(s.projection.textColor)},
            {"bgColor",        Vec3ToJson(s.projection.bgColor)},
            {"margins",        Vec4ToJson(s.projection.margins)},
            {"shadowOffset",   {s.projection.shadowOffset[0], s.projection.shadowOffset[1]}}
        };

        // ── Theme ──
        root["theme"] = {
            {"base",        Vec4ToJson(s.theme.base)},
            {"surface0",    Vec4ToJson(s.theme.surface0)},
            {"surface1",    Vec4ToJson(s.theme.surface1)},
            {"surface2",    Vec4ToJson(s.theme.surface2)},
            {"surface3",    Vec4ToJson(s.theme.surface3)},
            {"accent",      Vec4ToJson(s.theme.accent)},
            {"accentLight", Vec4ToJson(s.theme.accentLight)},
            {"accentDim",   Vec4ToJson(s.theme.accentDim)},
            {"accentFaint", Vec4ToJson(s.theme.accentFaint)},
            {"border",      Vec4ToJson(s.theme.border)},
            {"borderFaint", Vec4ToJson(s.theme.borderFaint)},
            {"textPrimary", Vec4ToJson(s.theme.textPrimary)},
            {"textDim",     Vec4ToJson(s.theme.textDim)},
            {"textFaint",   Vec4ToJson(s.theme.textFaint)}
        };

        // ── Audio ──
        root["audio"] = {
            {"masterVolume", s.audio.masterVolume},
            {"muted",        s.audio.muted},
            {"audioDevice",  s.audio.audioDevice},
            {"muteOnBlank",  s.audio.muteOnBlank}
        };

        // ── Updates ──
        root["updates"] = {
            {"checkOnStartup", s.updates.checkOnStartup},
            {"autoDownload",   s.updates.autoDownload},
            {"updateChannel",  s.updates.updateChannel},
            {"lastChecked",    s.updates.lastChecked},
            {"currentVersion", s.updates.currentVersion}
        };

        std::ofstream f(path);
        if (!f.is_open()) {
            std::cerr << "[Settings] No se pudo escribir '" << path << "'.\n";
            return;
        }
        f << root.dump(4);
        std::cout << "[Settings] Guardado en '" << path << "'.\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsManager::ResetToDefaults() {
        m_Settings = AppSettings{};
        std::cout << "[Settings] Valores restablecidos a por defecto.\n";
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsManager::ApplyTheme() {
        const auto& t = m_Settings.theme;
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* c = style.Colors;

        auto V4 = [](const float v[4]) { return ImVec4(v[0], v[1], v[2], v[3]); };

        c[ImGuiCol_Text]                  = V4(t.textPrimary);
        c[ImGuiCol_TextDisabled]          = V4(t.textFaint);
        c[ImGuiCol_WindowBg]              = V4(t.base);
        c[ImGuiCol_ChildBg]               = V4(t.surface0);
        c[ImGuiCol_PopupBg]               = ImVec4(0.065f, 0.070f, 0.094f, 0.98f);
        c[ImGuiCol_Border]                = V4(t.border);
        c[ImGuiCol_BorderShadow]          = ImVec4(0,0,0,0.40f);
        c[ImGuiCol_FrameBg]               = V4(t.surface1);
        c[ImGuiCol_FrameBgHovered]        = V4(t.surface2);
        c[ImGuiCol_FrameBgActive]         = V4(t.surface3);
        c[ImGuiCol_TitleBg]               = V4(t.base);
        c[ImGuiCol_TitleBgActive]         = V4(t.surface0);
        c[ImGuiCol_TitleBgCollapsed]      = V4(t.base);
        c[ImGuiCol_MenuBarBg]             = ImVec4(0.048f, 0.052f, 0.070f, 1.0f);
        c[ImGuiCol_ScrollbarBg]           = V4(t.surface0);
        c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.23f, 0.295f, 1.0f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.31f, 0.325f, 0.40f, 1.0f);
        c[ImGuiCol_ScrollbarGrabActive]   = V4(t.accentDim);
        c[ImGuiCol_CheckMark]             = V4(t.accent);
        c[ImGuiCol_SliderGrab]            = V4(t.accent);
        c[ImGuiCol_SliderGrabActive]      = V4(t.accentLight);
        c[ImGuiCol_Button]                = V4(t.surface2);
        c[ImGuiCol_ButtonHovered]         = ImVec4(t.accentFaint[0]+0.05f, t.accentFaint[1]+0.04f, t.accentFaint[2]+0.01f, 1.0f);
        c[ImGuiCol_ButtonActive]          = V4(t.accentFaint);
        c[ImGuiCol_Header]                = ImVec4(t.accentFaint[0], t.accentFaint[1], t.accentFaint[2], 0.80f);
        c[ImGuiCol_HeaderHovered]         = ImVec4(t.accentDim[0]*0.65f, t.accentDim[1]*0.65f, t.accentDim[2]*0.65f, 1.0f);
        c[ImGuiCol_HeaderActive]          = ImVec4(t.accentDim[0]*0.80f, t.accentDim[1]*0.80f, t.accentDim[2]*0.80f, 1.0f);
        c[ImGuiCol_Separator]             = V4(t.borderFaint);
        c[ImGuiCol_SeparatorHovered]      = V4(t.accentDim);
        c[ImGuiCol_SeparatorActive]       = V4(t.accent);
        c[ImGuiCol_ResizeGrip]            = ImVec4(t.accentDim[0], t.accentDim[1], t.accentDim[2], 0.30f);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(t.accent[0], t.accent[1], t.accent[2], 0.60f);
        c[ImGuiCol_ResizeGripActive]      = V4(t.accent);
        c[ImGuiCol_Tab]                   = V4(t.surface1);
        c[ImGuiCol_TabHovered]            = V4(t.surface3);
        c[ImGuiCol_TabActive]             = ImVec4(t.accentFaint[0]+0.04f, t.accentFaint[1]+0.03f, t.accentFaint[2], 1.0f);
        c[ImGuiCol_TabUnfocused]          = V4(t.surface0);
        c[ImGuiCol_TabUnfocusedActive]    = V4(t.surface2);
        c[ImGuiCol_DockingPreview]        = ImVec4(t.accent[0], t.accent[1], t.accent[2], 0.28f);
        c[ImGuiCol_DockingEmptyBg]        = V4(t.base);
        c[ImGuiCol_NavHighlight]          = V4(t.accent);
        c[ImGuiCol_PlotLines]             = V4(t.accent);
        c[ImGuiCol_PlotLinesHovered]      = V4(t.accentLight);
        c[ImGuiCol_PlotHistogram]         = V4(t.accent);
        c[ImGuiCol_PlotHistogramHovered]  = V4(t.accentLight);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0,0,0,0.72f);

        // Geometría (invariante)
        style.WindowPadding     = ImVec2(10,10); style.FramePadding   = ImVec2(8,5);
        style.CellPadding       = ImVec2(7,5);   style.ItemSpacing    = ImVec2(8,6);
        style.ItemInnerSpacing  = ImVec2(6,6);   style.IndentSpacing  = 18;
        style.ScrollbarSize     = 12;            style.GrabMinSize    = 10;
        style.WindowBorderSize  = 1;             style.ChildBorderSize= 1;
        style.PopupBorderSize   = 1;             style.FrameBorderSize= 0;
        style.WindowRounding    = 6;             style.ChildRounding  = 5;
        style.FrameRounding     = 4;             style.PopupRounding  = 6;
        style.ScrollbarRounding = 12;            style.GrabRounding   = 4;
        style.TabRounding       = 5;
    }

    // ─────────────────────────────────────────────────────────────────────────
    void SettingsManager::ApplyProjection() {
        const auto& p = m_Settings.projection;
        auto& core    = Core::PresentationCore::Get();

        core.SetTargetMonitor(p.targetMonitor);
        core.UpdateTextStyle(
            p.textSize,
            p.textColor,
            p.textAlignment,
            p.margins,
            p.autoScale
        );
        core.SetLayer0_Color(p.bgColor[0], p.bgColor[1], p.bgColor[2]);
    }

} // namespace ProyecThor::Settings
