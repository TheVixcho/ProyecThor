#pragma once
#include <string>
#include <filesystem>
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "MonitorTheme.h"
#include "frontend/ui/bin/StyleGeneralApp.h"

// =============================================================================
//  MonitorQueueHelpers.h
//  Funciones utilitarias para la cola de reproduccion:
//    - prefijos de entrada (Local / URL)
//    - ruta de persistencia
//    - nombres de display truncados
//    - boton con color personalizado
//    - barras animadas de "reproduciendo"
// =============================================================================

namespace ProyecThor::UI::QueueHelpers {

namespace fs = std::filesystem;
using namespace MonitorTheme;

// ── Prefijos de entrada ───────────────────────────────────────────────────────
inline constexpr char k_PfxLocal = 'L';
inline constexpr char k_PfxURL   = 'U';

inline std::string QueueEntry(char pfx, const std::string& s)
{
    return pfx + std::string("|") + s;
}

inline char QueuePfx(const std::string& e)
{
    return e.empty() ? '?' : e[0];
}

inline std::string QueuePath(const std::string& e)
{
    return (e.size() > 2) ? e.substr(2) : "";
}

// ── Truncado de nombres para display ─────────────────────────────────────────
inline std::string TruncPath(const std::string& p, size_t maxLen = 46)
{
    std::string name = fs::path(p).stem().string();
    if (name.size() > maxLen)
        name = name.substr(0, maxLen - 3) + "...";
    return name;
}

inline std::string QueueDisplayName(const std::string& entry)
{
    char pfx        = QueuePfx(entry);
    std::string path = QueuePath(entry);

    if (pfx == k_PfxLocal)
        return TruncPath(path, 46);

    // URL: quitar schema y truncar
    if (path.rfind("https://", 0) == 0)      path = path.substr(8);
    else if (path.rfind("http://", 0) == 0)  path = path.substr(7);
    if (path.size() > 46)                    path = path.substr(0, 43) + "...";
    return path;
}

// ── Ruta de persistencia de la cola ──────────────────────────────────────────
// Multiplataforma:
//   - Windows: %APPDATA%\ProyecThor\assets\play_queue.txt
//   - Linux:   $XDG_DATA_HOME/ProyecThor/assets/play_queue.txt
//              (o $HOME/.local/share/ProyecThor/assets/play_queue.txt si
//               XDG_DATA_HOME no esta definida)
inline const std::string& GetQueueFilePath()
{
    static std::string s_Path;
    if (!s_Path.empty()) return s_Path;

#ifdef _WIN32
    char buf[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, buf)))
        s_Path = (fs::path(buf) / "ProyecThor" / "assets" / "play_queue.txt").string();
    else
        s_Path = "play_queue.txt";
#else
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    fs::path base;
    if (xdgData && *xdgData)
    {
        base = fs::path(xdgData);
    }
    else
    {
        const char* home = std::getenv("HOME");
        base = fs::path(home ? home : ".") / ".local" / "share";
    }
    s_Path = (base / "ProyecThor" / "assets" / "play_queue.txt").string();
#endif

    std::error_code ec;
    fs::create_directories(fs::path(s_Path).parent_path(), ec);

    return s_Path;
}

// ── Barras animadas de "reproduciendo" ───────────────────────────────────────
// Dibuja 3 barras verticales animadas a la izquierda del item activo.
inline void DrawPlayingBars(ImDrawList* dl, ImVec2 rowMin, float rowH, float baseX)
{
    float t        = static_cast<float>(ImGui::GetTime());
    float barBaseY = rowMin.y + rowH * 0.5f;
    for (int b = 0; b < 3; b++) {
        float phase = t * 3.0f + b * 1.2f;
        float barH  = 4.0f + std::abs(std::sin(phase)) * 7.0f;
        float bx    = baseX + static_cast<float>(b) * 5.0f;
        dl->AddRectFilled(
            { bx,        barBaseY - barH * 0.5f },
            { bx + 3.0f, barBaseY + barH * 0.5f },
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.35f, 0.78f, 1.0f, 0.92f)),
            1.5f);
    }
}

// ── Animacion de hover/press reutilizable ────────────────────────────────────
// ImGuiStorage + lerp con DeltaTime -- mismo patron que BroadcastAnimT
// (BroadcastPanel.cpp) / AnimT (CategoryTheme.cpp), reescrito liviano aca
// para no crear una dependencia cruzada entre paneles por un helper tan chico
// (mismo criterio ya establecido en el resto de la app).
inline float QueueAnimT(ImGuiID id, ImU32 salt, bool target, float speed = 14.0f)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float*        t       = storage->GetFloatRef(id ^ salt, target ? 1.0f : 0.0f);
    *t += ((target ? 1.0f : 0.0f) - *t) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
    return *t;
}

// ── Boton animado para la cola ────────────────────────────────────────────────
// Reemplaza el viejo QueueColorBtn (ImGui::Button + PushStyleColor plano, sin
// animacion, con el icono superpuesto aparte via DrawBtnIcon y el label
// relleno de espacios al principio para dejarle lugar) -- pedido explicito
// ("los botones son feos, la calidad de UI es horrible, pone animaciones y
// mejora"). InvisibleButton + hover lerp, fondo con highlight superior sutil
// (mismo truco que PillButton en LibraryIcons.h) e icono+texto centrados
// juntos como un solo bloque en vez de dos elementos posicionados a mano por
// separado.
inline bool QueueActionButton(
    const char* strId,
    const char* label,
    const char* iconName,
    ImVec2      size,
    ImVec4      base,
    ImVec4      hov,
    ImVec4      textCol,
    float       rounding = 8.0f)
{
    ImGui::PushID(strId);
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = { p0.x + size.x, p0.y + size.y };

    ImGui::InvisibleButton("##btn", size);
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    bool pressed = ImGui::IsItemClicked();

    float t = QueueAnimT(ImGui::GetID("##btn"), 0xA1u, hovered, 14.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 fillC = {
        base.x + (hov.x - base.x) * t, base.y + (hov.y - base.y) * t,
        base.z + (hov.z - base.z) * t, base.w + (hov.w - base.w) * t
    };
    if (held) fillC.w *= 0.80f;

    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(fillC), rounding);

    // Highlight superior sutil, crece un poco en hover -- le da algo de
    // "cuerpo" al boton en vez de un rectangulo de color plano.
    float hlAlpha = 0.08f + t * 0.10f;
    dl->AddRectFilled(p0, { p1.x, p0.y + size.y * 0.42f },
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, hlAlpha)),
        rounding, ImDrawFlags_RoundCornersTop);
    dl->AddRect(p0, p1,
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.07f + t * 0.10f)),
        rounding);

    // Icono + texto como un solo bloque centrado (icono a la izquierda del
    // texto, con un gap fijo) -- reemplaza el truco de espacios al principio
    // del label + DrawBtnIcon posicionado aparte que usaba el boton viejo.
    bool hasIcon = iconName && StyleGeneralApp::Icons.count(iconName) &&
                   StyleGeneralApp::Icons[iconName].textureID;
    float  iconSz = std::min(16.0f, size.y - 12.0f);
    ImVec2 textSz = ImGui::CalcTextSize(label);
    float  gap    = hasIcon ? 8.0f : 0.0f;
    float  blockW = (hasIcon ? iconSz : 0.0f) + gap + textSz.x;
    float  blockX = p0.x + (size.x - blockW) * 0.5f;
    float  cy     = p0.y + size.y * 0.5f;

    if (hasIcon)
    {
        void* icon = StyleGeneralApp::Icons[iconName].textureID;
        dl->AddImage(icon, { blockX, cy - iconSz * 0.5f }, { blockX + iconSz, cy + iconSz * 0.5f },
            ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(textCol));
        blockX += iconSz + gap;
    }
    dl->AddText({ blockX, cy - textSz.y * 0.5f }, ImGui::ColorConvertFloat4ToU32(textCol), label);

    ImGui::PopID();
    return pressed;
}

} // namespace ProyecThor::UI::QueueHelpers