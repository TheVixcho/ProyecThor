#include "SettingsManager.h"
#include "DesignSystem.h"
#include "backend/core/PresentationCore.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <algorithm>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#include <cstdio>
#include <array>
#else
#include <windows.h>
#endif
#include "MonitorTheme.h"
#include "backend/monitors/MonitorDesign.h"
#include "HubTheme.h"
#include "LayersTheme.h"
#include "CanvaStyleEditor.h"
#include <cstdint>

using json = nlohmann::json;

namespace ProyecThor::Settings {

// Valida el archivo de fuente leyendo el directorio de tablas sfnt a mano,
// SIN pasar por stb_truetype: ImGui compila su copia con STBTT_STATIC (ver
// imgui_draw.cpp), asi que sus simbolos quedan ocultos a esta unidad de
// compilacion, y compilar una copia propia de imstb_truetype.h aca choca
// con el forward-declare de 'stbrp_node' que ya trae imgui_internal.h
// (mismo nombre de tipo, structs incompatibles). Como AddFontFromFileTTF
// llama IM_ASSERT ante un archivo invalido/corrupto -- en build Debug eso
// aborta el proceso entero, bug real que golpeo la propia fuente por
// defecto de este repo -- esta es la unica forma de saber de antemano si
// una fuente "sirve" sin arriesgar ese crash al cargarla de verdad.
bool IsValidFontFile(const std::string& path) {
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() < 12) return false;

    auto readU16 = [&](size_t off) -> uint16_t {
        return (uint16_t(data[off]) << 8) | uint16_t(data[off + 1]);
    };
    auto readU32 = [&](size_t off) -> uint32_t {
        return (uint32_t(data[off])     << 24) | (uint32_t(data[off + 1]) << 16) |
               (uint32_t(data[off + 2]) <<  8) |  uint32_t(data[off + 3]);
    };

    size_t   dirOffset = 0;
    uint32_t tag       = readU32(0);
    if (tag == 0x74746366u) { // 'ttcf' -- TrueType Collection: usa la primera fuente del set
        if (data.size() < 16) return false;
        dirOffset = readU32(12);
    } else if (tag != 0x00010000u && tag != 0x4F54544Fu /*'OTTO'*/ &&
               tag != 0x74727565u /*'true'*/ && tag != 0x74797031u /*'typ1'*/) {
        return false; // no es un sfnt reconocible
    }

    if (dirOffset + 12 > data.size()) return false;
    uint16_t numTables = readU16(dirOffset + 4);
    if (numTables == 0 || numTables > 128) return false; // sanity

    bool hasGlyf = false, hasLoca = false, hasHead = false, hasCFF = false;
    size_t recBase = dirOffset + 12;

    for (uint16_t i = 0; i < numTables; i++) {
        size_t rec = recBase + (size_t)i * 16;
        if (rec + 16 > data.size()) return false; // directorio trunco

        uint32_t tableTag    = readU32(rec);
        uint32_t tableOffset = readU32(rec + 8);
        uint32_t tableLength = readU32(rec + 12);
        if ((size_t)tableOffset + tableLength > data.size()) return false; // tabla fuera de rango -> archivo trunco/corrupto

        switch (tableTag) {
            case 0x676C7966u: hasGlyf = true; break; // 'glyf'
            case 0x6C6F6361u: hasLoca = true; break; // 'loca'
            case 0x68656164u: hasHead = true; break; // 'head'
            case 0x43464620u: hasCFF  = true; break; // 'CFF '
            default: break;
        }
    }

    // Hace falta el esqueleto TrueType (glyf+loca) o PostScript (CFF) para
    // tener contornos que rasterizar, mas la tabla 'head' que ImGui/
    // stb_truetype siempre esperan poder leer.
    return hasHead && (hasCFF || (hasGlyf && hasLoca));
}

void RestartApplication() {
#ifdef _WIN32
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return;

    STARTUPINFOA        si = {};
    si.cb                  = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#else
    char exePath[4096] = {};
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len <= 0) return;
    exePath[len] = '\0';

    // fork(): el hijo se convierte en la nueva instancia (execl); el padre
    // (este mismo proceso) NO llama exit() aca -- ya viene de un shutdown
    // limpio (ver comentario en el .h) y simplemente sigue su propio
    // return normal de main().
    pid_t pid = fork();
    if (pid == 0) {
        execl(exePath, exePath, (char*)nullptr);
        _exit(127); // solo se llega aca si execl fallo
    }
#endif
}

// Nunca debe resolver a una ruta relativa dependiente del cwd (podria
// terminar escrito dentro del propio repo si la app se lanza desde ahi).
// Windows usa %APPDATA%, el resto sigue la convencion XDG ($XDG_CONFIG_HOME
// o $HOME/.config), igual que LayersBgTab::GetAppDataDir().
static std::string GetSettingsPath() {
    std::filesystem::path dir;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    dir = std::filesystem::path(appData ? appData : ".") / "ProyecThor";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;
    if (xdgConfig && *xdgConfig) {
        base = xdgConfig;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = std::filesystem::path(home) / ".config";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = std::filesystem::path(pw->pw_dir) / ".config";
    } else {
        base = std::filesystem::current_path();
    }
    dir = base / "ProyecThor";
#endif
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "settings.json").string();
}

const char* ThemePresetName(ThemePreset preset) {
    switch (preset) {
        case ThemePreset::Dark:        return "Oscuro";
        case ThemePreset::Light:       return "Claro";
        case ThemePreset::OrangeBlack: return "Naranja y Negro";
        case ThemePreset::Jazz:        return "Jazz";
        case ThemePreset::Kofi:        return "Ko-fi";
        case ThemePreset::Deadlock:    return "Deadlock";
        case ThemePreset::Galaxy:      return "Galaxia";
        case ThemePreset::Mek:         return "Mek";
        default:                       return "Personalizado";
    }
}

ThemePreset ThemePresetFromString(const std::string& s) {
    if (s == "dark")        return ThemePreset::Dark;
    if (s == "light")       return ThemePreset::Light;
    if (s == "orangeblack") return ThemePreset::OrangeBlack;
    if (s == "jazz")        return ThemePreset::Jazz;
    if (s == "kofi")        return ThemePreset::Kofi;
    if (s == "deadlock")    return ThemePreset::Deadlock;
    if (s == "galaxy")      return ThemePreset::Galaxy;
    if (s == "mek")         return ThemePreset::Mek;
    return ThemePreset::Custom;
}

static std::string ThemePresetToKey(ThemePreset preset) {
    switch (preset) {
        case ThemePreset::Dark:        return "dark";
        case ThemePreset::Light:       return "light";
        case ThemePreset::OrangeBlack: return "orangeblack";
        case ThemePreset::Jazz:        return "jazz";
        case ThemePreset::Kofi:        return "kofi";
        case ThemePreset::Deadlock:    return "deadlock";
        case ThemePreset::Galaxy:      return "galaxy";
        case ThemePreset::Mek:         return "mek";
        default:                       return "custom";
    }
}

const char* WorkspaceLayoutPresetName(WorkspaceLayoutPreset preset) {
    switch (preset) {
        case WorkspaceLayoutPreset::Simple:    return "Simple";
        case WorkspaceLayoutPreset::Broadcast: return "Transmisión";
        case WorkspaceLayoutPreset::Library:   return "Biblioteca";
        case WorkspaceLayoutPreset::Render:    return "Render";
        case WorkspaceLayoutPreset::Audio:     return "Audio";
        case WorkspaceLayoutPreset::Video:     return "Video";
        case WorkspaceLayoutPreset::Image:     return "Imagen";
        default:                               return "Clásico";
    }
}

WorkspaceLayoutPreset WorkspaceLayoutPresetFromString(const std::string& s) {
    if (s == "simple")    return WorkspaceLayoutPreset::Simple;
    if (s == "broadcast") return WorkspaceLayoutPreset::Broadcast;
    if (s == "library")   return WorkspaceLayoutPreset::Library;
    if (s == "render")    return WorkspaceLayoutPreset::Render;
    if (s == "audio")     return WorkspaceLayoutPreset::Audio;
    if (s == "video")     return WorkspaceLayoutPreset::Video;
    if (s == "image")     return WorkspaceLayoutPreset::Image;
    return WorkspaceLayoutPreset::Classic;
}

static std::string WorkspaceLayoutPresetToKey(WorkspaceLayoutPreset preset) {
    switch (preset) {
        case WorkspaceLayoutPreset::Simple:    return "simple";
        case WorkspaceLayoutPreset::Broadcast: return "broadcast";
        case WorkspaceLayoutPreset::Library:   return "library";
        case WorkspaceLayoutPreset::Render:    return "render";
        case WorkspaceLayoutPreset::Audio:     return "audio";
        case WorkspaceLayoutPreset::Video:     return "video";
        case WorkspaceLayoutPreset::Image:     return "image";
        default:                               return "classic";
    }
}

// ── Presets ──────────────────────────────────────────────────────────────
// Nota: de momento todos los presets usan colores mas apagados/oscuros
// que lo habitual (menos saturacion, menos brillo) para evitar problemas
// de contraste mientras varios paneles todavia no estan terminados.
ThemeSettings MakeThemePreset(ThemePreset preset) {
    ThemeSettings t;
    t.preset = preset;

    switch (preset) {

    case ThemePreset::Dark: {
        // Gris neutro tipo ProPresenter/OBS: antes esto era un dorado/ambar
        // saturado (t.accent 0.60/0.52/0.30) que no pegaba con el resto de
        // la app. Ahora base/surfaces son gris puro (R=G=B, sin tinte de
        // color) y el accent es un gris claro "plata" en vez de un color
        // saturado, que es el look que se pidio como default.
        t.base[0]=0.078f; t.base[1]=0.078f; t.base[2]=0.082f; t.base[3]=1.0f;
        t.surface0[0]=0.098f; t.surface0[1]=0.098f; t.surface0[2]=0.102f; t.surface0[3]=1.0f;
        t.surface1[0]=0.130f; t.surface1[1]=0.130f; t.surface1[2]=0.136f; t.surface1[3]=1.0f;
        t.surface2[0]=0.165f; t.surface2[1]=0.165f; t.surface2[2]=0.172f; t.surface2[3]=1.0f;
        t.surface3[0]=0.205f; t.surface3[1]=0.205f; t.surface3[2]=0.213f; t.surface3[3]=1.0f;
        t.accent[0]=0.55f; t.accent[1]=0.56f; t.accent[2]=0.58f; t.accent[3]=1.0f;
        t.accentLight[0]=0.72f; t.accentLight[1]=0.73f; t.accentLight[2]=0.75f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.38f; t.accentDim[1]=0.39f; t.accentDim[2]=0.41f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.55f; t.accentFaint[1]=0.56f; t.accentFaint[2]=0.58f; t.accentFaint[3]=0.15f;
        t.border[0]=1; t.border[1]=1; t.border[2]=1; t.border[3]=0.10f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.92f; t.textPrimary[1]=0.92f; t.textPrimary[2]=0.93f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.58f; t.textDim[1]=0.58f; t.textDim[2]=0.60f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.75f; t.danger[1]=0.25f; t.danger[2]=0.25f; t.danger[3]=1.0f;
        t.success[0]=0.35f; t.success[1]=0.60f; t.success[2]=0.35f; t.success[3]=1.0f;
        t.windowRounding=8.0f; t.frameRounding=6.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::Light: {
        t.base[0]=0.86f; t.base[1]=0.87f; t.base[2]=0.89f; t.base[3]=1.0f;
        t.surface0[0]=0.92f; t.surface0[1]=0.93f; t.surface0[2]=0.94f; t.surface0[3]=1.0f;
        t.surface1[0]=0.95f; t.surface1[1]=0.95f; t.surface1[2]=0.96f; t.surface1[3]=1.0f;
        t.surface2[0]=0.82f; t.surface2[1]=0.83f; t.surface2[2]=0.85f; t.surface2[3]=1.0f;
        t.surface3[0]=0.76f; t.surface3[1]=0.77f; t.surface3[2]=0.80f; t.surface3[3]=1.0f;
        t.accent[0]=0.35f; t.accent[1]=0.45f; t.accent[2]=0.58f; t.accent[3]=1.0f;
        t.accentLight[0]=0.48f; t.accentLight[1]=0.58f; t.accentLight[2]=0.70f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.30f; t.accentDim[1]=0.38f; t.accentDim[2]=0.48f; t.accentDim[3]=0.5f;
        t.accentFaint[0]=0.30f; t.accentFaint[1]=0.38f; t.accentFaint[2]=0.48f; t.accentFaint[3]=0.15f;
        t.border[0]=0; t.border[1]=0; t.border[2]=0; t.border[3]=0.10f;
        t.borderFaint[0]=0; t.borderFaint[1]=0; t.borderFaint[2]=0; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.15f; t.textPrimary[1]=0.16f; t.textPrimary[2]=0.18f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.40f; t.textDim[1]=0.41f; t.textDim[2]=0.44f; t.textDim[3]=1.0f;
        t.textFaint[0]=0; t.textFaint[1]=0; t.textFaint[2]=0; t.textFaint[3]=0.32f;
        t.danger[0]=0.75f; t.danger[1]=0.25f; t.danger[2]=0.28f; t.danger[3]=1.0f;
        t.success[0]=0.25f; t.success[1]=0.55f; t.success[2]=0.38f; t.success[3]=1.0f;
        t.windowRounding=8.0f; t.frameRounding=6.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::OrangeBlack: {
        t.base[0]=0.050f; t.base[1]=0.045f; t.base[2]=0.040f; t.base[3]=1.0f;
        t.surface0[0]=0.080f; t.surface0[1]=0.065f; t.surface0[2]=0.045f; t.surface0[3]=1.0f;
        t.surface1[0]=0.110f; t.surface1[1]=0.085f; t.surface1[2]=0.060f; t.surface1[3]=1.0f;
        t.surface2[0]=0.140f; t.surface2[1]=0.105f; t.surface2[2]=0.070f; t.surface2[3]=1.0f;
        t.surface3[0]=0.170f; t.surface3[1]=0.130f; t.surface3[2]=0.090f; t.surface3[3]=1.0f;
        t.accent[0]=0.75f; t.accent[1]=0.42f; t.accent[2]=0.10f; t.accent[3]=1.0f;
        t.accentLight[0]=0.85f; t.accentLight[1]=0.55f; t.accentLight[2]=0.25f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.50f; t.accentDim[1]=0.28f; t.accentDim[2]=0.05f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.75f; t.accentFaint[1]=0.42f; t.accentFaint[2]=0.10f; t.accentFaint[3]=0.15f;
        t.border[0]=0.75f; t.border[1]=0.42f; t.border[2]=0.10f; t.border[3]=0.15f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.92f; t.textPrimary[1]=0.88f; t.textPrimary[2]=0.82f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.70f; t.textDim[1]=0.55f; t.textDim[2]=0.40f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.85f; t.danger[1]=0.28f; t.danger[2]=0.24f; t.danger[3]=1.0f;
        t.success[0]=0.50f; t.success[1]=0.75f; t.success[2]=0.28f; t.success[3]=1.0f;
        t.windowRounding=6.0f; t.frameRounding=4.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::Jazz: {
        t.base[0]=0.09f; t.base[1]=0.05f; t.base[2]=0.07f; t.base[3]=1.0f;
        t.surface0[0]=0.13f; t.surface0[1]=0.07f; t.surface0[2]=0.10f; t.surface0[3]=1.0f;
        t.surface1[0]=0.17f; t.surface1[1]=0.09f; t.surface1[2]=0.13f; t.surface1[3]=1.0f;
        t.surface2[0]=0.22f; t.surface2[1]=0.12f; t.surface2[2]=0.17f; t.surface2[3]=1.0f;
        t.surface3[0]=0.27f; t.surface3[1]=0.15f; t.surface3[2]=0.20f; t.surface3[3]=1.0f;
        t.accent[0]=0.68f; t.accent[1]=0.52f; t.accent[2]=0.24f; t.accent[3]=1.0f;
        t.accentLight[0]=0.80f; t.accentLight[1]=0.66f; t.accentLight[2]=0.38f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.46f; t.accentDim[1]=0.34f; t.accentDim[2]=0.14f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.68f; t.accentFaint[1]=0.52f; t.accentFaint[2]=0.24f; t.accentFaint[3]=0.16f;
        t.border[0]=0.68f; t.border[1]=0.52f; t.border[2]=0.24f; t.border[3]=0.20f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.90f; t.textPrimary[1]=0.86f; t.textPrimary[2]=0.80f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.70f; t.textDim[1]=0.56f; t.textDim[2]=0.51f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.80f; t.danger[1]=0.25f; t.danger[2]=0.30f; t.danger[3]=1.0f;
        t.success[0]=0.50f; t.success[1]=0.70f; t.success[2]=0.42f; t.success[3]=1.0f;
        t.windowRounding=14.0f; t.frameRounding=9.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Kofi: {
        t.base[0]=0.93f; t.base[1]=0.90f; t.base[2]=0.85f; t.base[3]=1.0f;
        t.surface0[0]=0.95f; t.surface0[1]=0.92f; t.surface0[2]=0.87f; t.surface0[3]=1.0f;
        t.surface1[0]=0.96f; t.surface1[1]=0.96f; t.surface1[2]=0.94f; t.surface1[3]=1.0f;
        t.surface2[0]=0.88f; t.surface2[1]=0.82f; t.surface2[2]=0.74f; t.surface2[3]=1.0f;
        t.surface3[0]=0.83f; t.surface3[1]=0.75f; t.surface3[2]=0.66f; t.surface3[3]=1.0f;
        t.accent[0]=0.85f; t.accent[1]=0.42f; t.accent[2]=0.42f; t.accent[3]=1.0f;
        t.accentLight[0]=0.90f; t.accentLight[1]=0.55f; t.accentLight[2]=0.55f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.68f; t.accentDim[1]=0.30f; t.accentDim[2]=0.30f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.85f; t.accentFaint[1]=0.42f; t.accentFaint[2]=0.42f; t.accentFaint[3]=0.15f;
        t.border[0]=0; t.border[1]=0; t.border[2]=0; t.border[3]=0.10f;
        t.borderFaint[0]=0; t.borderFaint[1]=0; t.borderFaint[2]=0; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.20f; t.textPrimary[1]=0.13f; t.textPrimary[2]=0.10f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.45f; t.textDim[1]=0.35f; t.textDim[2]=0.30f; t.textDim[3]=1.0f;
        t.textFaint[0]=0; t.textFaint[1]=0; t.textFaint[2]=0; t.textFaint[3]=0.35f;
        t.danger[0]=0.72f; t.danger[1]=0.20f; t.danger[2]=0.20f; t.danger[3]=1.0f;
        t.success[0]=0.28f; t.success[1]=0.58f; t.success[2]=0.35f; t.success[3]=1.0f;
        t.windowRounding=16.0f; t.frameRounding=10.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Deadlock: {
        t.base[0]=0.045f; t.base[1]=0.065f; t.base[2]=0.055f; t.base[3]=1.0f;
        t.surface0[0]=0.060f; t.surface0[1]=0.095f; t.surface0[2]=0.080f; t.surface0[3]=1.0f;
        t.surface1[0]=0.080f; t.surface1[1]=0.125f; t.surface1[2]=0.105f; t.surface1[3]=1.0f;
        t.surface2[0]=0.100f; t.surface2[1]=0.155f; t.surface2[2]=0.130f; t.surface2[3]=1.0f;
        t.surface3[0]=0.130f; t.surface3[1]=0.195f; t.surface3[2]=0.160f; t.surface3[3]=1.0f;
        t.accent[0]=0.42f; t.accent[1]=0.70f; t.accent[2]=0.32f; t.accent[3]=1.0f;
        t.accentLight[0]=0.55f; t.accentLight[1]=0.80f; t.accentLight[2]=0.45f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.26f; t.accentDim[1]=0.44f; t.accentDim[2]=0.20f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.42f; t.accentFaint[1]=0.70f; t.accentFaint[2]=0.32f; t.accentFaint[3]=0.15f;
        t.border[0]=0.42f; t.border[1]=0.70f; t.border[2]=0.32f; t.border[3]=0.15f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.85f; t.textPrimary[1]=0.90f; t.textPrimary[2]=0.86f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.58f; t.textDim[1]=0.70f; t.textDim[2]=0.62f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.26f;
        t.danger[0]=0.85f; t.danger[1]=0.28f; t.danger[2]=0.28f; t.danger[3]=1.0f;
        t.success[0]=0.42f; t.success[1]=0.70f; t.success[2]=0.32f; t.success[3]=1.0f;
        t.windowRounding=10.0f; t.frameRounding=6.0f; t.scrollbarSize=7.0f;
        break;
    }

    case ThemePreset::Galaxy: {
        t.base[0]=0.045f; t.base[1]=0.038f; t.base[2]=0.075f; t.base[3]=1.0f;
        t.surface0[0]=0.065f; t.surface0[1]=0.050f; t.surface0[2]=0.125f; t.surface0[3]=1.0f;
        t.surface1[0]=0.090f; t.surface1[1]=0.068f; t.surface1[2]=0.170f; t.surface1[3]=1.0f;
        t.surface2[0]=0.120f; t.surface2[1]=0.092f; t.surface2[2]=0.220f; t.surface2[3]=1.0f;
        t.surface3[0]=0.155f; t.surface3[1]=0.118f; t.surface3[2]=0.280f; t.surface3[3]=1.0f;
        t.accent[0]=0.48f; t.accent[1]=0.32f; t.accent[2]=0.70f; t.accent[3]=1.0f;
        t.accentLight[0]=0.60f; t.accentLight[1]=0.45f; t.accentLight[2]=0.80f; t.accentLight[3]=1.0f;
        t.accentDim[0]=0.32f; t.accentDim[1]=0.20f; t.accentDim[2]=0.46f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.48f; t.accentFaint[1]=0.32f; t.accentFaint[2]=0.70f; t.accentFaint[3]=0.15f;
        t.border[0]=0.48f; t.border[1]=0.32f; t.border[2]=0.70f; t.border[3]=0.18f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.86f; t.textPrimary[1]=0.84f; t.textPrimary[2]=0.90f; t.textPrimary[3]=1.0f;
        t.textDim[0]=0.62f; t.textDim[1]=0.58f; t.textDim[2]=0.75f; t.textDim[3]=1.0f;
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.85f; t.danger[1]=0.32f; t.danger[2]=0.45f; t.danger[3]=1.0f;
        t.success[0]=0.32f; t.success[1]=0.75f; t.success[2]=0.62f; t.success[3]=1.0f;
        t.windowRounding=16.0f; t.frameRounding=10.0f; t.scrollbarSize=8.0f;
        break;
    }

    case ThemePreset::Mek: {
        // Catppuccin Mocha -- paleta por defecto de Omarchy (ver
        // ~/.config/omarchy/current/theme/colors.toml). Progresion de
        // superficies Base -> Surface0 -> Surface1 -> Surface2 -> Overlay0,
        // mismo criterio de brillo creciente que el resto de los presets.
        t.base[0]=0.118f; t.base[1]=0.118f; t.base[2]=0.180f; t.base[3]=1.0f;             // Base #1e1e2e
        t.surface0[0]=0.192f; t.surface0[1]=0.196f; t.surface0[2]=0.267f; t.surface0[3]=1.0f; // Surface0 #313244
        t.surface1[0]=0.271f; t.surface1[1]=0.278f; t.surface1[2]=0.353f; t.surface1[3]=1.0f; // Surface1 #45475a
        t.surface2[0]=0.345f; t.surface2[1]=0.357f; t.surface2[2]=0.439f; t.surface2[3]=1.0f; // Surface2 #585b70
        t.surface3[0]=0.424f; t.surface3[1]=0.439f; t.surface3[2]=0.525f; t.surface3[3]=1.0f; // Overlay0 #6c7086
        t.accent[0]=0.537f; t.accent[1]=0.706f; t.accent[2]=0.980f; t.accent[3]=1.0f;          // Blue #89b4fa
        t.accentLight[0]=0.706f; t.accentLight[1]=0.745f; t.accentLight[2]=0.996f; t.accentLight[3]=1.0f; // Lavender #b4befe
        t.accentDim[0]=0.376f; t.accentDim[1]=0.494f; t.accentDim[2]=0.686f; t.accentDim[3]=1.0f;
        t.accentFaint[0]=0.537f; t.accentFaint[1]=0.706f; t.accentFaint[2]=0.980f; t.accentFaint[3]=0.16f;
        t.border[0]=0.537f; t.border[1]=0.706f; t.border[2]=0.980f; t.border[3]=0.18f;
        t.borderFaint[0]=1; t.borderFaint[1]=1; t.borderFaint[2]=1; t.borderFaint[3]=0.05f;
        t.textPrimary[0]=0.804f; t.textPrimary[1]=0.839f; t.textPrimary[2]=0.957f; t.textPrimary[3]=1.0f; // Text #cdd6f4
        t.textDim[0]=0.651f; t.textDim[1]=0.678f; t.textDim[2]=0.784f; t.textDim[3]=1.0f;                 // Subtext0 #a6adc8
        t.textFaint[0]=1; t.textFaint[1]=1; t.textFaint[2]=1; t.textFaint[3]=0.28f;
        t.danger[0]=0.953f; t.danger[1]=0.545f; t.danger[2]=0.659f; t.danger[3]=1.0f;   // Red #f38ba8
        t.success[0]=0.651f; t.success[1]=0.890f; t.success[2]=0.631f; t.success[3]=1.0f; // Green #a6e3a1
        t.windowRounding=14.0f; t.frameRounding=9.0f; t.scrollbarSize=8.0f;
        break;
    }

    default:
        // Cae aca solo si llega ThemePreset::Custom, que no genera colores
        // desde codigo (se cargan desde settings.json en LoadSettings).
        break;
    }

    return t;
}

#ifndef _WIN32
// Busca la ruta absoluta de "JetBrainsMono Nerd Font" via fontconfig (mismo
// popen+parseo defensivo que ya usa CategoryTheme::OpenFontFileDialogUnix
// para zenity/kdialog) -- es la fuente que usa waybar en Omarchy por
// defecto, y la idea del preset Mek es que la interfaz combine con el resto
// del escritorio. Devuelve "" si fc-match no esta instalado o no encuentra
// la fuente (Mek simplemente se queda con la fuente default de la app).
static std::string ResolveWaybarFontPath() {
    std::array<char, 512> buffer{};
    std::string result;

    FILE* pipe = popen("fc-match -f \"%{file}\" \"JetBrainsMono Nerd Font\" 2>/dev/null", "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
        result += buffer.data();
    pclose(pipe);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return result;
}
#endif

void SettingsManager::ApplyPreset(ThemePreset preset) {
    m_Settings.theme = MakeThemePreset(preset);

#ifndef _WIN32
    // Mek: ademas de los colores de Catppuccin Mocha, usa la misma fuente
    // que waybar (JetBrainsMono Nerd Font) si esta instalada -- coherente
    // con el resto del escritorio Omarchy. Como la fuente se carga una sola
    // vez al arrancar (ver main.cpp), el cambio recien se ve tras reiniciar
    // -- mismo comportamiento que ya tiene elegir una fuente a mano en
    // Ajustes > Apariencia.
    if (preset == ThemePreset::Mek) {
        std::string fontPath = ResolveWaybarFontPath();
        if (!fontPath.empty() && IsValidFontFile(fontPath))
            m_Settings.theme.customFontPath = fontPath;
    }
#endif

    ApplyTheme();
}

// ── Proyección ───────────────────────────────────────────────────────────
void SettingsManager::ApplyProjection() {
    const auto& p    = m_Settings.projection;
    auto&       core = ::ProyecThor::Core::PresentationCore::Get();

    core.SetTargetMonitor(p.targetMonitor);

    {
        // Convierte el default plano de Ajustes > Proyeccion (margenes L,T,R,B
        // en px @1920x1080) a la caja centro-relativa que espera
        // UpdateLyricsBoxStyle -- de paso corrige un bug preexistente donde
        // este armado pasaba los margenes en orden Top,Bottom,Left,Right en
        // vez de L,T,R,B.
        Core::TextBoxStyle box;
        float margins[4] = { p.marginLeft, p.marginTop, p.marginRight, p.marginBottom };
        box.sizeW = std::max(0.02f, (1920.0f - margins[0] - margins[2]) / 1920.0f);
        box.sizeH = std::max(0.02f, (1080.0f - margins[1] - margins[3]) / 1080.0f);
        box.posX  = margins[0] / 1920.0f + box.sizeW * 0.5f;
        box.posY  = margins[1] / 1080.0f + box.sizeH * 0.5f;
        box.fontName  = p.selectedFont;
        box.color[0]  = p.textColorR; box.color[1] = p.textColorG;
        box.color[2]  = p.textColorB; box.color[3] = p.textColorA;
        box.textSize  = p.textSize;
        box.hAlign    = p.textAlignment;
        box.vAlign    = p.vAlignment;
        box.autoScale = p.autoScale;
        Core::UnpackTextEffects(p.textEffectsPacked, box.effects);

        core.UpdateLyricsBoxStyle(box);
    }
    core.SetLayer0_Color(p.defaultBgR, p.defaultBgG, p.defaultBgB);
    core.SetLoadingLogoPath(p.loadingLogoPath);
    core.SetBackgroundPingPongLoop(p.bgPingPongLoop);

    core.SetFSREnabled(p.fsrEnabled);
    core.SetFSRSharpness(p.fsrSharpness);
    core.SetNISEnabled(p.nisEnabled);
    core.SetNISSharpness(p.nisSharpness);
    core.SetCRTEnabled(p.crtEnabled);
    core.SetCRTScanlineIntensity(p.crtScanlineIntensity);
    core.SetGrainEnabled(p.grainEnabled);
    core.SetGrainIntensity(p.grainIntensity);
    core.SetFXAAEnabled(p.fxaaEnabled);
    core.SetSaturationEnabled(p.saturationEnabled);
    core.SetSaturationAmount(p.saturationAmount);
    core.SetVignetteEnabled(p.vignetteEnabled);
    core.SetVignetteIntensity(p.vignetteIntensity);
    core.SetBlurEnabled(p.blurEnabled);
    core.SetBlurIntensity(p.blurIntensity);
    core.SetSharpenEnabled(p.sharpenEnabled);
    core.SetSharpenIntensity(p.sharpenIntensity);
    core.SetBloomEnabled(p.bloomEnabled);
    core.SetBloomIntensity(p.bloomIntensity);
    core.SetChromaticAberrationEnabled(p.chromaticAberrationEnabled);
    core.SetChromaticAberrationIntensity(p.chromaticAberrationIntensity);
    core.SetVHSEnabled(p.vhsEnabled);
    core.SetVHSIntensity(p.vhsIntensity);
    core.SetCineEnabled(p.cineEnabled);
    core.SetCineIntensity(p.cineIntensity);
    core.SetCineTint(p.cineTint);
    core.SetContrastEnabled(p.contrastEnabled);
    core.SetContrastAmount(p.contrastAmount);
    core.SetLuminosityEnabled(p.luminosityEnabled);
    core.SetLuminosityAmount(p.luminosityAmount);
    core.SetTAAEnabled(p.taaEnabled);
    core.SetTAAIntensity(p.taaIntensity);
    core.SetFillBlurEnabled(p.fillBlurEnabled);
    core.SetFillBlurBrightness(p.fillBlurBrightness);
    core.SetVideoRenderEngine(p.videoRenderEngine);
}

// ── Tema ─────────────────────────────────────────────────────────────────
// Expande los tokens de ThemeSettings a todo el estilo de ImGui y
// sincroniza DesignSystem (paneles glass) para que el tema alcance
// también a los widgets custom dibujados con ImDrawList.
void SettingsManager::ApplyTheme() {
    const auto& t = m_Settings.theme;
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4*     c = s.Colors;

    auto V = [](const float* a, float alphaMul = 1.0f) {
        return ImVec4(a[0], a[1], a[2], a[3] * alphaMul);
    };

    // Fondos de panel 100% opacos (pedido explicito: "quita las
    // transparencias de los paneles, queda muy feo") -- ChildBg en
    // particular estaba al 70% alpha, dejando ver lo que hubiera detras de
    // CUALQUIER BeginChild() sin override propio (la mayoria de los
    // paneles: Biblioteca, Cola de Monitor, listas, etc), lo que se veia
    // como un lavado/neblina inconsistente segun que hubiera de fondo.
    c[ImGuiCol_WindowBg]         = V(t.base);
    c[ImGuiCol_ChildBg]          = V(t.surface0);
    c[ImGuiCol_PopupBg]          = V(t.surface1);
    c[ImGuiCol_Border]           = V(t.border);
    c[ImGuiCol_BorderShadow]     = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]          = V(t.surface1);
    c[ImGuiCol_FrameBgHovered]   = V(t.surface2);
    c[ImGuiCol_FrameBgActive]    = V(t.surface3);

    c[ImGuiCol_TitleBg]          = V(t.base);
    c[ImGuiCol_TitleBgActive]    = V(t.surface0);
    c[ImGuiCol_TitleBgCollapsed] = V(t.base);
    c[ImGuiCol_MenuBarBg]        = V(t.base);

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = V(t.surface3, 0.7f);
    c[ImGuiCol_ScrollbarGrabHovered] = V(t.accentDim);
    c[ImGuiCol_ScrollbarGrabActive]  = V(t.accent);

    c[ImGuiCol_CheckMark]        = V(t.accent);
    c[ImGuiCol_SliderGrab]       = V(t.accent);
    c[ImGuiCol_SliderGrabActive] = V(t.accentLight);

    c[ImGuiCol_Button]        = V(t.surface2);
    c[ImGuiCol_ButtonHovered] = V(t.accentDim);
    c[ImGuiCol_ButtonActive]  = V(t.accent);

    c[ImGuiCol_Header]        = V(t.accentFaint);
    c[ImGuiCol_HeaderHovered] = V(t.accentDim);
    c[ImGuiCol_HeaderActive]  = V(t.accent);

    c[ImGuiCol_Separator]        = V(t.border, 0.6f);
    c[ImGuiCol_SeparatorHovered] = V(t.accentDim);
    c[ImGuiCol_SeparatorActive]  = V(t.accent);

    c[ImGuiCol_ResizeGrip]        = V(t.accentFaint, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = V(t.accentDim);
    c[ImGuiCol_ResizeGripActive]  = V(t.accent);

    c[ImGuiCol_Tab]                = V(t.surface1);
    c[ImGuiCol_TabHovered]         = V(t.accentDim);
    c[ImGuiCol_TabActive]          = V(t.accent);
    c[ImGuiCol_TabUnfocused]       = V(t.surface0);
    c[ImGuiCol_TabUnfocusedActive] = V(t.surface2);

    c[ImGuiCol_DockingPreview] = V(t.accent, 0.35f);
    c[ImGuiCol_DockingEmptyBg] = V(t.base);

    c[ImGuiCol_PlotLines]            = V(t.accent);
    c[ImGuiCol_PlotLinesHovered]     = V(t.accentLight);
    c[ImGuiCol_PlotHistogram]        = V(t.accent);
    c[ImGuiCol_PlotHistogramHovered] = V(t.accentLight);

    c[ImGuiCol_TableHeaderBg]     = V(t.surface1);
    c[ImGuiCol_TableBorderStrong] = V(t.border);
    c[ImGuiCol_TableBorderLight]  = V(t.borderFaint);
    c[ImGuiCol_TableRowBg]        = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]     = V(t.surface0, 0.4f);

    c[ImGuiCol_TextSelectedBg]        = V(t.accent, 0.35f);
    c[ImGuiCol_DragDropTarget]        = V(t.accent, 0.9f);
    c[ImGuiCol_NavHighlight]          = V(t.accent);
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.6f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.45f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.55f);

    c[ImGuiCol_Text]         = V(t.textPrimary);
    c[ImGuiCol_TextDisabled] = V(t.textFaint);

    s.WindowRounding    = t.windowRounding;
    s.ChildRounding     = t.windowRounding * 0.75f;
    s.FrameRounding     = t.frameRounding;
    s.PopupRounding     = t.windowRounding * 0.9f;
    s.ScrollbarRounding = t.windowRounding * 0.8f;
    s.GrabRounding      = t.frameRounding * 0.7f;
    s.TabRounding       = t.frameRounding;
    s.ScrollbarSize     = t.scrollbarSize;

    // Con viewports, cada ventana OS debe quedar cuadrada y opaca.
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        s.WindowRounding = 0.0f;
        c[ImGuiCol_WindowBg].w = 1.0f;
    }

    ProyecThor::UI::DS::SyncFromTheme(t);
    ProyecThor::UI::MonitorTheme::Sync(t);
    ProyecThor::UI::Design::Sync(t);
    ProyecThor::UI::HubTheme::Sync(t);
    ProyecThor::UI::LP::Sync(t);
    ProyecThor::UI::CanvaPalette::Sync(t);
}

// ── Persistencia ─────────────────────────────────────────────────────────
void SettingsManager::SaveSettings() {
    json j;
    const auto& p = m_Settings.projection;
    const auto& t = m_Settings.theme;

    j["projection"]["targetMonitor"] = p.targetMonitor;
    j["projection"]["extraMonitors"] = p.extraMonitors;
    j["projection"]["textSize"]      = p.textSize;
    j["projection"]["textColorR"]    = p.textColorR;
    j["projection"]["textColorG"]    = p.textColorG;
    j["projection"]["textColorB"]    = p.textColorB;
    j["projection"]["textColorA"]    = p.textColorA;
    j["projection"]["textAlignment"] = p.textAlignment;
    j["projection"]["vAlignment"]    = p.vAlignment;
    j["projection"]["marginTop"]     = p.marginTop;
    j["projection"]["marginBottom"]  = p.marginBottom;
    j["projection"]["marginLeft"]    = p.marginLeft;
    j["projection"]["marginRight"]   = p.marginRight;
    j["projection"]["autoScale"]     = p.autoScale;
    j["projection"]["selectedFont"]  = p.selectedFont;
    j["projection"]["textEffectsPacked"] = p.textEffectsPacked;
    j["projection"]["defaultBgR"]    = p.defaultBgR;
    j["projection"]["defaultBgG"]    = p.defaultBgG;
    j["projection"]["defaultBgB"]    = p.defaultBgB;
    j["projection"]["outputWidth"]        = p.outputWidth;
    j["projection"]["outputHeight"]       = p.outputHeight;
    j["projection"]["targetFPS"]          = p.targetFPS;
    j["projection"]["outputQualityMode"]  = p.outputQualityMode;
    j["projection"]["outputPresetIndex"]  = p.outputPresetIndex;
    j["projection"]["loadingLogoPath"]    = p.loadingLogoPath;
    j["projection"]["fsrEnabled"]           = p.fsrEnabled;
    j["projection"]["fsrSharpness"]         = p.fsrSharpness;
    j["projection"]["nisEnabled"]           = p.nisEnabled;
    j["projection"]["nisSharpness"]         = p.nisSharpness;
    j["projection"]["crtEnabled"]           = p.crtEnabled;
    j["projection"]["crtScanlineIntensity"] = p.crtScanlineIntensity;
    j["projection"]["grainEnabled"]         = p.grainEnabled;
    j["projection"]["grainIntensity"]       = p.grainIntensity;
    j["projection"]["fxaaEnabled"]          = p.fxaaEnabled;
    j["projection"]["saturationEnabled"]    = p.saturationEnabled;
    j["projection"]["saturationAmount"]     = p.saturationAmount;
    j["projection"]["vignetteEnabled"]      = p.vignetteEnabled;
    j["projection"]["vignetteIntensity"]    = p.vignetteIntensity;
    j["projection"]["blurEnabled"]          = p.blurEnabled;
    j["projection"]["blurIntensity"]        = p.blurIntensity;
    j["projection"]["sharpenEnabled"]       = p.sharpenEnabled;
    j["projection"]["sharpenIntensity"]     = p.sharpenIntensity;
    j["projection"]["bloomEnabled"]         = p.bloomEnabled;
    j["projection"]["bloomIntensity"]       = p.bloomIntensity;
    j["projection"]["chromaticAberrationEnabled"]   = p.chromaticAberrationEnabled;
    j["projection"]["chromaticAberrationIntensity"] = p.chromaticAberrationIntensity;
    j["projection"]["vhsEnabled"]           = p.vhsEnabled;
    j["projection"]["vhsIntensity"]         = p.vhsIntensity;
    j["projection"]["cineEnabled"]          = p.cineEnabled;
    j["projection"]["cineIntensity"]        = p.cineIntensity;
    j["projection"]["cineTint"]             = p.cineTint;
    j["projection"]["contrastEnabled"]      = p.contrastEnabled;
    j["projection"]["contrastAmount"]       = p.contrastAmount;
    j["projection"]["luminosityEnabled"]    = p.luminosityEnabled;
    j["projection"]["luminosityAmount"]     = p.luminosityAmount;
    j["projection"]["taaEnabled"]           = p.taaEnabled;
    j["projection"]["taaIntensity"]         = p.taaIntensity;
    j["projection"]["fillBlurEnabled"]      = p.fillBlurEnabled;
    j["projection"]["fillBlurBrightness"]   = p.fillBlurBrightness;
    j["projection"]["videoRenderEngine"]    = p.videoRenderEngine;

    const auto& sd = m_Settings.stageDisplay;
    j["stageDisplay"]["layoutTemplateIndex"] = sd.layoutTemplateIndex;
    for (int i = 0; i < kStageMaxCells; i++)
        j["stageDisplay"]["cellWidget"][i] = sd.cellWidget[i];
    j["stageDisplay"]["extraMonitors"] = sd.extraMonitors;

    const auto& lsb = m_Settings.librarySidebar;
    for (int i = 0; i < 10; i++)
        for (int c = 0; c < 4; c++)
            j["librarySidebar"]["categoryColor"][i][c] = lsb.categoryColor[i][c];

    const auto& hsb = m_Settings.homeSidebar;
    for (int i = 0; i < 6; i++)
        for (int c = 0; c < 4; c++)
            j["homeSidebar"]["categoryColor"][i][c] = hsb.categoryColor[i][c];

    const auto& chs = m_Settings.controlHub;
    for (int i = 0; i < 2; i++)
        for (int c = 0; c < 4; c++)
            j["controlHub"]["categoryColor"][i][c] = chs.categoryColor[i][c];

    const auto& shs = m_Settings.stylesHub;
    for (int i = 0; i < 6; i++)
        for (int c = 0; c < 4; c++)
            j["stylesHub"]["categoryColor"][i][c] = shs.categoryColor[i][c];

    const auto& vts = m_Settings.viewTools;
    for (int i = 0; i < 4; i++)
        for (int c = 0; c < 4; c++)
            j["viewTools"]["categoryColor"][i][c] = vts.categoryColor[i][c];

    for (int i = 0; i < kCaptureSceneCount; i++) {
        const auto& sc = m_Settings.capture.scenes[i];
        auto& js = j["capture"]["scenes"][i];
        js["assigned"]     = sc.assigned;
        js["sourceType"]   = sc.sourceType;
        js["sourceIndex"]  = sc.sourceIndex;
        js["sourceHandle"] = sc.sourceHandle;
        js["sourceName"]   = sc.sourceName;
        js["x0"] = sc.x0; js["y0"] = sc.y0; js["x1"] = sc.x1; js["y1"] = sc.y1;
        js["opacity"] = sc.opacity;
    }

    for (int i = 0; i < kPadCount; i++) {
        const auto& p  = m_Settings.pads.pads[i];
        auto&       jp = j["pads"]["pads"][i];
        jp["assigned"]  = p.assigned;
        jp["iconIndex"] = p.iconIndex;

        jp["hasCapture"] = p.hasCapture;
        auto& jc = jp["capture"];
        jc["assigned"]     = p.capture.assigned;
        jc["sourceType"]   = p.capture.sourceType;
        jc["sourceIndex"]  = p.capture.sourceIndex;
        jc["sourceHandle"] = p.capture.sourceHandle;
        jc["sourceName"]   = p.capture.sourceName;
        jc["x0"] = p.capture.x0; jc["y0"] = p.capture.y0;
        jc["x1"] = p.capture.x1; jc["y1"] = p.capture.y1;
        jc["opacity"] = p.capture.opacity;

        jp["hasStyle"]       = p.hasStyle;
        jp["styleSize"]      = p.styleSize;
        for (int c = 0; c < 4; c++) jp["styleColor"][c] = p.styleColor[c];
        jp["styleHAlign"]    = p.styleHAlign;
        jp["styleVAlign"]    = p.styleVAlign;
        for (int c = 0; c < 4; c++) jp["styleMargins"][c] = p.styleMargins[c];
        jp["styleAutoScale"] = p.styleAutoScale;
        jp["styleFontName"]  = p.styleFontName;
        jp["bgType"]    = p.bgType;
        jp["bgPath"]    = p.bgPath;
        for (int c = 0; c < 3; c++) jp["bgColor"][c] = p.bgColor[c];
    }

    for (size_t i = 0; i < m_Settings.transitions.presets.size(); i++) {
        const auto& tp = m_Settings.transitions.presets[i];
        auto& jt = j["transitions"]["presets"][i];
        jt["name"]              = tp.name;
        jt["type"]              = tp.type;
        jt["duration"]          = tp.duration;
        jt["affectsBackground"] = tp.affectsBackground;
        jt["affectsLyrics"]     = tp.affectsLyrics;
    }

    j["yggdrasil"]["targetIp"]   = m_Settings.yggdrasil.targetIp;
    j["yggdrasil"]["targetPort"] = m_Settings.yggdrasil.targetPort;
    j["yggdrasil"]["listenPort"] = m_Settings.yggdrasil.listenPort;
    j["yggdrasil"]["autoListen"] = m_Settings.yggdrasil.autoListen;
    for (size_t i = 0; i < m_Settings.yggdrasil.messages.size(); i++) {
        const auto& m = m_Settings.yggdrasil.messages[i];
        auto& jm = j["yggdrasil"]["messages"][i];
        jm["label"]    = m.label;
        jm["address"]  = m.address;
        jm["argsText"] = m.argsText;
        // lastSendOk/lastSentAt son de sesion, no se persisten a proposito.
    }
    for (size_t i = 0; i < m_Settings.yggdrasil.bindings.size(); i++) {
        const auto& b = m_Settings.yggdrasil.bindings[i];
        auto& jb = j["yggdrasil"]["bindings"][i];
        jb["paramName"]  = b.paramName;
        jb["oscAddress"] = b.oscAddress;
    }

    j["streaming"]["serverUrl"]        = m_Settings.streaming.serverUrl;
    j["streaming"]["streamKey"]        = m_Settings.streaming.streamKey;
    j["streaming"]["videoBitrateKbps"] = m_Settings.streaming.videoBitrateKbps;
    j["streaming"]["fps"]              = m_Settings.streaming.fps;
    j["streaming"]["width"]            = m_Settings.streaming.width;
    j["streaming"]["height"]           = m_Settings.streaming.height;

    j["sync"]["enabled"]    = m_Settings.sync.enabled;
    j["sync"]["port"]       = m_Settings.sync.port;
    j["sync"]["pairingPin"] = m_Settings.sync.pairingPin;

    j["ai"]["enabled"] = m_Settings.ai.enabled;
    j["ai"]["apiKey"]  = m_Settings.ai.apiKey;
    j["ai"]["model"]   = m_Settings.ai.model;

    std::string langStr = "es";
    if      (m_Settings.general.language == Language::English)    langStr = "en";
    else if (m_Settings.general.language == Language::Portuguese) langStr = "pt";

    j["general"]["language"]            = langStr;
    j["general"]["dismissedChangelog"]  = m_Settings.general.dismissedChangelog;
    j["general"]["startMinimized"]      = m_Settings.general.startMinimized;
    j["general"]["rememberLayout"]      = m_Settings.general.rememberLayout;
    j["general"]["confirmOnExit"]       = m_Settings.general.confirmOnExit;
    j["general"]["autoSave"]            = m_Settings.general.autoSave;
    j["general"]["autoSaveIntervalSec"] = m_Settings.general.autoSaveIntervalSec;
    j["general"]["defaultBiblesFolder"] = m_Settings.general.defaultBiblesFolder;
    j["general"]["defaultMediaFolder"]  = m_Settings.general.defaultMediaFolder;
    j["general"]["showRailLabels"]      = m_Settings.general.showRailLabels;
    j["general"]["showPerfPanel"]       = m_Settings.general.showPerfPanel;
    j["general"]["showViewQuickActions"]= m_Settings.general.showViewQuickActions;
    j["general"]["quickNotesText"]      = m_Settings.general.quickNotesText;

    j["audio"]["masterVolume"] = m_Settings.audio.masterVolume;
    j["audio"]["muted"]        = m_Settings.audio.muted;
    j["audio"]["muteOnBlank"]  = m_Settings.audio.muteOnBlank;
    j["audio"]["audioDevice"]  = m_Settings.audio.audioDevice;

    j["updates"]["checkOnStartup"] = m_Settings.updates.checkOnStartup;
    j["updates"]["autoDownload"]   = m_Settings.updates.autoDownload;
    j["updates"]["updateChannel"]  = m_Settings.updates.updateChannel;
    j["updates"]["lastChecked"]    = m_Settings.updates.lastChecked;

    j["workspace"]["layoutPreset"] = WorkspaceLayoutPresetToKey(m_Settings.workspace.layoutPreset);

    j["theme"]["preset"]         = ThemePresetToKey(t.preset);
    j["theme"]["windowRounding"] = t.windowRounding;
    j["theme"]["frameRounding"]  = t.frameRounding;
    j["theme"]["scrollbarSize"]  = t.scrollbarSize;
    j["theme"]["customFontPath"] = t.customFontPath;
    auto putCol = [&](const char* key, const float* v) {
        j["theme"][key] = { v[0], v[1], v[2], v[3] };
    };
    putCol("base", t.base); putCol("surface0", t.surface0);
    putCol("surface1", t.surface1); putCol("surface2", t.surface2);
    putCol("surface3", t.surface3); putCol("accent", t.accent);
    putCol("accentLight", t.accentLight); putCol("accentDim", t.accentDim);
    putCol("accentFaint", t.accentFaint); putCol("border", t.border);
    putCol("borderFaint", t.borderFaint); putCol("textPrimary", t.textPrimary);
    putCol("textDim", t.textDim); putCol("textFaint", t.textFaint);
    putCol("danger", t.danger); putCol("success", t.success);

    try {
        std::string path = GetSettingsPath();
        std::ofstream f(path);
        if (f.is_open()) { f << j.dump(4); }
        else std::cerr << "[Settings] No se pudo abrir para escritura: " << path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al guardar: " << e.what() << "\n";
    }
}

void SettingsManager::LoadSettings() {
    std::string path = GetSettingsPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
        return;
    }

    try {
        json j;
        f >> j;

        if (j.contains("projection")) {
            auto& p = m_Settings.projection;
            const auto& jp = j["projection"];
            p.targetMonitor = jp.value("targetMonitor", -1);
            p.extraMonitors = jp.value("extraMonitors", std::vector<int>{});
            p.textSize      = jp.value("textSize",      48.0f);
            p.textColorR    = jp.value("textColorR",    1.0f);
            p.textColorG    = jp.value("textColorG",    1.0f);
            p.textColorB    = jp.value("textColorB",    1.0f);
            p.textColorA    = jp.value("textColorA",    1.0f);
            p.textAlignment = jp.value("textAlignment", 1);
            p.vAlignment    = jp.value("vAlignment",    1);
            p.marginTop     = jp.value("marginTop",     50.0f);
            p.marginBottom  = jp.value("marginBottom",  50.0f);
            p.marginLeft    = jp.value("marginLeft",    50.0f);
            p.marginRight   = jp.value("marginRight",   50.0f);
            p.autoScale     = jp.value("autoScale",     true);
            p.selectedFont  = jp.value("selectedFont",  "default");
            p.textEffectsPacked = jp.value("textEffectsPacked", "");
            p.defaultBgR    = jp.value("defaultBgR",    0.0f);
            p.defaultBgG    = jp.value("defaultBgG",    0.0f);
            p.defaultBgB    = jp.value("defaultBgB",    0.0f);
            p.outputWidth        = jp.value("outputWidth",        0);
            p.outputHeight       = jp.value("outputHeight",       0);
            p.targetFPS          = jp.value("targetFPS",          60);
            p.outputQualityMode  = jp.value("outputQualityMode",  0);
            p.outputPresetIndex  = jp.value("outputPresetIndex",  3);
            p.loadingLogoPath    = jp.value("loadingLogoPath",    "");
            p.fsrEnabled            = jp.value("fsrEnabled",            true);
            p.fsrSharpness          = jp.value("fsrSharpness",          0.2f);
            p.nisEnabled            = jp.value("nisEnabled",            false);
            p.nisSharpness          = jp.value("nisSharpness",          0.5f);
            p.crtEnabled            = jp.value("crtEnabled",            false);
            p.crtScanlineIntensity  = jp.value("crtScanlineIntensity",  0.5f);
            p.grainEnabled          = jp.value("grainEnabled",          false);
            p.grainIntensity        = jp.value("grainIntensity",        0.15f);
            p.fxaaEnabled           = jp.value("fxaaEnabled",           false);
            p.saturationEnabled     = jp.value("saturationEnabled",     false);
            p.saturationAmount      = jp.value("saturationAmount",      1.3f);
            p.vignetteEnabled       = jp.value("vignetteEnabled",       false);
            p.vignetteIntensity     = jp.value("vignetteIntensity",     0.45f);
            p.blurEnabled           = jp.value("blurEnabled",           false);
            p.blurIntensity         = jp.value("blurIntensity",         0.35f);
            p.sharpenEnabled        = jp.value("sharpenEnabled",        false);
            p.sharpenIntensity      = jp.value("sharpenIntensity",      0.35f);
            p.bloomEnabled          = jp.value("bloomEnabled",          false);
            p.bloomIntensity        = jp.value("bloomIntensity",        0.35f);
            p.chromaticAberrationEnabled   = jp.value("chromaticAberrationEnabled",   false);
            p.chromaticAberrationIntensity = jp.value("chromaticAberrationIntensity", 0.35f);
            p.vhsEnabled            = jp.value("vhsEnabled",            false);
            p.vhsIntensity          = jp.value("vhsIntensity",          0.5f);
            p.cineEnabled           = jp.value("cineEnabled",           false);
            p.cineIntensity         = jp.value("cineIntensity",         0.5f);
            p.cineTint              = jp.value("cineTint",              0);
            p.contrastEnabled       = jp.value("contrastEnabled",       false);
            p.contrastAmount        = jp.value("contrastAmount",        1.3f);
            p.luminosityEnabled     = jp.value("luminosityEnabled",     false);
            p.luminosityAmount      = jp.value("luminosityAmount",      1.2f);
            p.taaEnabled            = jp.value("taaEnabled",            false);
            p.taaIntensity          = jp.value("taaIntensity",          0.5f);
            p.fillBlurEnabled       = jp.value("fillBlurEnabled",       false);
            p.fillBlurBrightness    = jp.value("fillBlurBrightness",    0.6f);
            p.videoRenderEngine     = jp.value("videoRenderEngine",     1);
        }

        if (j.contains("stageDisplay")) {
            auto& sd = m_Settings.stageDisplay;
            const auto& jsd = j["stageDisplay"];
            sd.layoutTemplateIndex = jsd.value("layoutTemplateIndex", 0);
            if (jsd.contains("cellWidget") && jsd["cellWidget"].is_array()) {
                const auto& arr = jsd["cellWidget"];
                for (int i = 0; i < kStageMaxCells && i < (int)arr.size(); i++)
                    sd.cellWidget[i] = arr[i].get<int>();
            }
            sd.extraMonitors = jsd.value("extraMonitors", std::vector<int>{});
        }

        if (j.contains("librarySidebar")) {
            auto& lsb = m_Settings.librarySidebar;
            const auto& jlsb = j["librarySidebar"];
            if (jlsb.contains("categoryColor") && jlsb["categoryColor"].is_array()) {
                const auto& arr = jlsb["categoryColor"];
                for (int i = 0; i < 10 && i < (int)arr.size(); i++)
                    for (int c = 0; c < 4 && c < (int)arr[i].size(); c++)
                        lsb.categoryColor[i][c] = arr[i][c].get<float>();
            }
        }

        if (j.contains("homeSidebar")) {
            auto& hsb = m_Settings.homeSidebar;
            const auto& jhsb = j["homeSidebar"];
            if (jhsb.contains("categoryColor") && jhsb["categoryColor"].is_array()) {
                const auto& arr = jhsb["categoryColor"];
                for (int i = 0; i < 6 && i < (int)arr.size(); i++)
                    for (int c = 0; c < 4 && c < (int)arr[i].size(); c++)
                        hsb.categoryColor[i][c] = arr[i][c].get<float>();
            }
        }

        if (j.contains("controlHub")) {
            auto& chs = m_Settings.controlHub;
            const auto& jchs = j["controlHub"];
            if (jchs.contains("categoryColor") && jchs["categoryColor"].is_array()) {
                const auto& arr = jchs["categoryColor"];
                for (int i = 0; i < 2 && i < (int)arr.size(); i++)
                    for (int c = 0; c < 4 && c < (int)arr[i].size(); c++)
                        chs.categoryColor[i][c] = arr[i][c].get<float>();
            }
        }

        if (j.contains("stylesHub")) {
            auto& shs = m_Settings.stylesHub;
            const auto& jshs = j["stylesHub"];
            if (jshs.contains("categoryColor") && jshs["categoryColor"].is_array()) {
                const auto& arr = jshs["categoryColor"];
                for (int i = 0; i < 6 && i < (int)arr.size(); i++)
                    for (int c = 0; c < 4 && c < (int)arr[i].size(); c++)
                        shs.categoryColor[i][c] = arr[i][c].get<float>();
            }
        }

        if (j.contains("viewTools")) {
            auto& vts = m_Settings.viewTools;
            const auto& jvts = j["viewTools"];
            if (jvts.contains("categoryColor") && jvts["categoryColor"].is_array()) {
                const auto& arr = jvts["categoryColor"];
                for (int i = 0; i < 4 && i < (int)arr.size(); i++)
                    for (int c = 0; c < 4 && c < (int)arr[i].size(); c++)
                        vts.categoryColor[i][c] = arr[i][c].get<float>();
            }
        }

        if (j.contains("capture") && j["capture"].contains("scenes") && j["capture"]["scenes"].is_array()) {
            const auto& arr = j["capture"]["scenes"];
            for (int i = 0; i < kCaptureSceneCount && i < (int)arr.size(); i++) {
                const auto& js = arr[i];
                auto& sc = m_Settings.capture.scenes[i];
                sc.assigned     = js.value("assigned",     false);
                sc.sourceType   = js.value("sourceType",   0);
                sc.sourceIndex  = js.value("sourceIndex", -1);
                sc.sourceHandle = js.value("sourceHandle", "");
                sc.sourceName   = js.value("sourceName",   "");
                sc.x0 = js.value("x0", 0.25f); sc.y0 = js.value("y0", 0.25f);
                sc.x1 = js.value("x1", 0.75f); sc.y1 = js.value("y1", 0.75f);
                sc.opacity = js.value("opacity", 1.0f);
            }
        }

        if (j.contains("streaming")) {
            const auto& js = j["streaming"];
            m_Settings.streaming.serverUrl        = js.value("serverUrl", "rtmp://");
            m_Settings.streaming.streamKey        = js.value("streamKey", "");
            m_Settings.streaming.videoBitrateKbps = js.value("videoBitrateKbps", 4500);
            m_Settings.streaming.fps              = js.value("fps", 30);
            m_Settings.streaming.width            = js.value("width", 1280);
            m_Settings.streaming.height           = js.value("height", 720);
        }

        if (j.contains("sync")) {
            const auto& jsy = j["sync"];
            m_Settings.sync.enabled    = jsy.value("enabled", false);
            m_Settings.sync.port       = jsy.value("port", 8090);
            m_Settings.sync.pairingPin = jsy.value("pairingPin", "");
        }

        if (j.contains("ai")) {
            const auto& jai = j["ai"];
            m_Settings.ai.enabled = jai.value("enabled", false);
            m_Settings.ai.apiKey  = jai.value("apiKey", "");
            m_Settings.ai.model   = jai.value("model", "claude-sonnet-5");
        }

        if (j.contains("yggdrasil")) {
            const auto& jy = j["yggdrasil"];
            m_Settings.yggdrasil.targetIp   = jy.value("targetIp",   "127.0.0.1");
            m_Settings.yggdrasil.targetPort = jy.value("targetPort", 9000);
            m_Settings.yggdrasil.listenPort = jy.value("listenPort", 9001);
            m_Settings.yggdrasil.autoListen = jy.value("autoListen", false);
            m_Settings.yggdrasil.messages.clear();
            if (jy.contains("messages") && jy["messages"].is_array()) {
                for (const auto& jm : jy["messages"]) {
                    OSCMessageDef m;
                    m.label    = jm.value("label",    "Luz 1");
                    m.address  = jm.value("address",  "/cue/1");
                    m.argsText = jm.value("argsText", "");
                    m_Settings.yggdrasil.messages.push_back(m);
                }
            }
            m_Settings.yggdrasil.bindings.clear();
            if (jy.contains("bindings") && jy["bindings"].is_array()) {
                for (const auto& jb : jy["bindings"]) {
                    OSCBinding b;
                    b.paramName  = jb.value("paramName",  "");
                    b.oscAddress = jb.value("oscAddress", "");
                    if (!b.paramName.empty() && !b.oscAddress.empty())
                        m_Settings.yggdrasil.bindings.push_back(b);
                }
            }
        }

        m_Settings.transitions.presets.clear();
        if (j.contains("transitions") && j["transitions"].contains("presets") &&
            j["transitions"]["presets"].is_array()) {
            for (const auto& jt : j["transitions"]["presets"]) {
                TransitionPresetSettings tp;
                tp.name              = jt.value("name", "");
                tp.type              = jt.value("type", 1);
                tp.duration          = jt.value("duration", 1.0f);
                tp.affectsBackground = jt.value("affectsBackground", false);
                tp.affectsLyrics     = jt.value("affectsLyrics", true);
                if (!tp.name.empty())
                    m_Settings.transitions.presets.push_back(tp);
            }
        }

        if (j.contains("pads") && j["pads"].contains("pads") && j["pads"]["pads"].is_array()) {
            const auto& arr = j["pads"]["pads"];
            for (int i = 0; i < kPadCount && i < (int)arr.size(); i++) {
                const auto& jp = arr[i];
                auto&       p  = m_Settings.pads.pads[i];
                p.assigned  = jp.value("assigned",  false);
                p.iconIndex = jp.value("iconIndex", 0);

                p.hasCapture = jp.value("hasCapture", false);
                if (jp.contains("capture")) {
                    const auto& jc = jp["capture"];
                    p.capture.assigned     = jc.value("assigned",     false);
                    p.capture.sourceType   = jc.value("sourceType",   0);
                    p.capture.sourceIndex  = jc.value("sourceIndex", -1);
                    p.capture.sourceHandle = jc.value("sourceHandle", "");
                    p.capture.sourceName   = jc.value("sourceName",   "");
                    p.capture.x0 = jc.value("x0", 0.25f); p.capture.y0 = jc.value("y0", 0.25f);
                    p.capture.x1 = jc.value("x1", 0.75f); p.capture.y1 = jc.value("y1", 0.75f);
                    p.capture.opacity = jc.value("opacity", 1.0f);
                }

                p.hasStyle  = jp.value("hasStyle",  false);
                p.styleSize = jp.value("styleSize", 60.0f);
                if (jp.contains("styleColor") && jp["styleColor"].is_array()) {
                    const auto& sc = jp["styleColor"];
                    for (int c = 0; c < 4 && c < (int)sc.size(); c++)
                        p.styleColor[c] = sc[c].get<float>();
                }
                p.styleHAlign = jp.value("styleHAlign", 1);
                p.styleVAlign = jp.value("styleVAlign", 1);
                if (jp.contains("styleMargins") && jp["styleMargins"].is_array()) {
                    const auto& sm = jp["styleMargins"];
                    for (int c = 0; c < 4 && c < (int)sm.size(); c++)
                        p.styleMargins[c] = sm[c].get<float>();
                }
                p.styleAutoScale = jp.value("styleAutoScale", true);
                p.styleFontName  = jp.value("styleFontName", "Predeterminada");
                p.bgType    = jp.value("bgType",    0);
                p.bgPath    = jp.value("bgPath",    "");
                if (jp.contains("bgColor") && jp["bgColor"].is_array()) {
                    const auto& bc = jp["bgColor"];
                    for (int c = 0; c < 3 && c < (int)bc.size(); c++)
                        p.bgColor[c] = bc[c].get<float>();
                }
            }
        }

        if (j.contains("general")) {
            const auto& jg = j["general"];
            std::string langStr = jg.value("language", "es");
            if      (langStr == "en") m_Settings.general.language = Language::English;
            else if (langStr == "pt") m_Settings.general.language = Language::Portuguese;
            else                      m_Settings.general.language = Language::Spanish;

            m_Settings.general.dismissedChangelog  = jg.value("dismissedChangelog",  "");
            m_Settings.general.startMinimized      = jg.value("startMinimized",      false);
            m_Settings.general.rememberLayout      = jg.value("rememberLayout",      true);
            m_Settings.general.confirmOnExit       = jg.value("confirmOnExit",       true);
            m_Settings.general.autoSave             = jg.value("autoSave",            true);
            m_Settings.general.autoSaveIntervalSec  = jg.value("autoSaveIntervalSec", 120);
            m_Settings.general.defaultBiblesFolder  = jg.value("defaultBiblesFolder", "");
            m_Settings.general.defaultMediaFolder   = jg.value("defaultMediaFolder",  "");
            m_Settings.general.showRailLabels       = jg.value("showRailLabels",      true);
            m_Settings.general.showPerfPanel        = jg.value("showPerfPanel",       false);
            m_Settings.general.showViewQuickActions = jg.value("showViewQuickActions", true);
            m_Settings.general.quickNotesText       = jg.value("quickNotesText",       "");
        }

        if (j.contains("audio")) {
            const auto& ja = j["audio"];
            m_Settings.audio.masterVolume = ja.value("masterVolume", 100);
            m_Settings.audio.muted        = ja.value("muted",        false);
            m_Settings.audio.muteOnBlank  = ja.value("muteOnBlank",  false);
            m_Settings.audio.audioDevice  = ja.value("audioDevice",  "");
        }

        if (j.contains("updates")) {
            const auto& ju = j["updates"];
            m_Settings.updates.checkOnStartup = ju.value("checkOnStartup", true);
            m_Settings.updates.autoDownload   = ju.value("autoDownload",   false);
            m_Settings.updates.updateChannel  = ju.value("updateChannel",  "stable");
            m_Settings.updates.lastChecked    = ju.value("lastChecked",    "");
        }

        if (j.contains("workspace")) {
            const auto& jw = j["workspace"];
            m_Settings.workspace.layoutPreset = WorkspaceLayoutPresetFromString(jw.value("layoutPreset", "classic"));
        }

        if (j.contains("theme")) {
            const auto& jt = j["theme"];
            ThemePreset preset = ThemePresetFromString(jt.value("preset", "dark"));

            // Los presets predefinidos siempre se regeneran desde código
            // (así si se ajusta un preset en una nueva versión, se actualiza).
            // "Custom" carga los colores guardados tal cual.
            if (preset == ThemePreset::Custom) {
                ThemeSettings t;
                t.preset = ThemePreset::Custom;
                auto getCol = [&](const char* key, float* out, const float* def) {
                    if (jt.contains(key) && jt[key].is_array() && jt[key].size() == 4) {
                        for (int i = 0; i < 4; i++) out[i] = jt[key][i].get<float>();
                    } else {
                        for (int i = 0; i < 4; i++) out[i] = def[i];
                    }
                };
                getCol("base", t.base, t.base); getCol("surface0", t.surface0, t.surface0);
                getCol("surface1", t.surface1, t.surface1); getCol("surface2", t.surface2, t.surface2);
                getCol("surface3", t.surface3, t.surface3); getCol("accent", t.accent, t.accent);
                getCol("accentLight", t.accentLight, t.accentLight); getCol("accentDim", t.accentDim, t.accentDim);
                getCol("accentFaint", t.accentFaint, t.accentFaint); getCol("border", t.border, t.border);
                getCol("borderFaint", t.borderFaint, t.borderFaint); getCol("textPrimary", t.textPrimary, t.textPrimary);
                getCol("textDim", t.textDim, t.textDim); getCol("textFaint", t.textFaint, t.textFaint);
                getCol("danger", t.danger, t.danger); getCol("success", t.success, t.success);
                t.windowRounding = jt.value("windowRounding", t.windowRounding);
                t.frameRounding  = jt.value("frameRounding",  t.frameRounding);
                t.scrollbarSize  = jt.value("scrollbarSize",  t.scrollbarSize);
                m_Settings.theme = t;
            } else {
                m_Settings.theme = MakeThemePreset(preset);
            }

            // Independiente del preset de colores (Custom o predefinido):
            // la fuente de la interfaz es una preferencia aparte.
            m_Settings.theme.customFontPath = jt.value("customFontPath", "");
        } else {
            m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
        }

    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al cargar: " << e.what() << "\n";
        m_Settings.theme = MakeThemePreset(ThemePreset::Dark);
    }
}

} // namespace ProyecThor::Settings