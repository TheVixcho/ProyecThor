#include "LayersStyleTab.h"
#include "LayersTheme.h"
#include "../../backend/core/PresentationCore.h"
#include "../../backend/core/FileDeletionManager.h"
#include "frontend/ui/UIManager.h"
#include "frontend/ui/IconRail.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
#else
    // En Linux/macOS seguimos la convencion XDG: usamos $XDG_DATA_HOME si
    // esta definida, o $HOME/.local/share en su defecto. Si tampoco existe
    // HOME, se consulta /etc/passwd como ultimo recurso.
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        base = xdg;
    } else if (const char* home = std::getenv("HOME"); home && *home) {
        base = fs::path(home) / ".local" / "share";
    } else if (struct passwd* pw = getpwuid(getuid())) {
        base = fs::path(pw->pw_dir) / ".local" / "share";
    } else {
        base = fs::current_path();
    }
    fs::path dir = base / "ProyecThor";
#endif
    std::error_code ec;
    fs::create_directories(dir / "themes", ec);
    return dir;
}
static fs::path ThemesDir() { return GetAppDataDir() / "themes"; }
static fs::path FontsDir()  { return GetAppDataDir() / "assets" / "fonts"; }

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────
LayersStyleTab::LayersStyleTab() {
    // m_CurrentStyle.lyrics/index ya arrancan con defaults razonables (ver
    // TextBoxStyle en PresentationCore.h).
    LoadThemeList();
    LoadFontsList();

    auto onFontImported = [this](const std::string& fontPath) {
        Core::PresentationCore::Get().LoadSingleFontIntoImGui(fontPath);
        LoadFontsList();
    };
    m_StyleEditor = std::make_unique<CanvaStyleEditor>(&m_AvailableFonts, onFontImported);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Abre el editor a pantalla completa -- pide a UIManager que oculte el
//  resto de los paneles (Biblioteca/Home/Diseño/etc), mismo mecanismo que
//  usa el editor de Overlays (ver OverlayLibraryTab::OpenEditorFullscreen).
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::OpenStyleEditorFullscreen(bool isNew, const std::string& name, const StyleData& data) {
    if (isNew) m_StyleEditor->OpenNew(data);
    else       m_StyleEditor->OpenEdit(name, data);

    if (!m_UIManager) return;

    m_UIManager->EnterFullscreenEditor([this]() {
        ImGuiViewport* vp    = ImGui::GetMainViewport();
        float          railH = IconRailThickness(false);

        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + railH));
        ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - railH));
        ImGui::SetNextWindowViewport(vp->ID);

        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoDecoration      |
            ImGuiWindowFlags_NoMove            |
            ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoDocking         |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, CanvaPalette::Surface0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##styleEditorFullscreen", nullptr, kFlags);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        bool saved = m_StyleEditor->Render([this](const std::string& n, const StyleData& d) {
            if (SaveTheme(n, d)) {
                LoadThemeList();
                m_CurrentStyle  = d;
                m_SelectedTheme = n;
                ApplyCurrentStyleToCore();
            }
        });

        ImGui::End();

        if (saved || !m_StyleEditor->IsOpen())
            m_UIManager->ExitFullscreenEditor();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Loaders
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::LoadThemeList() {
    m_AvailableThemes.clear();
    try {
        fs::path d = ThemesDir();
        if (fs::exists(d))
            for (const auto& e : fs::directory_iterator(d))
                if (e.path().extension()==".theme")
                    m_AvailableThemes.push_back(e.path().stem().string());
    } catch (...) {}
}

void LayersStyleTab::LoadFontsList() {
    m_AvailableFonts.clear();
    m_AvailableFonts.push_back("Predeterminada");
    try {
        fs::path d = FontsDir();
        fs::create_directories(d);
        if (fs::exists(d))
            for (const auto& e : fs::directory_iterator(d)) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(),ext.end(),ext.begin(),::tolower);
                if (ext==".ttf"||ext==".otf"||ext==".ttc")
                    m_AvailableFonts.push_back(e.path().stem().string());
            }
    } catch (...) {}
}
void LayersStyleTab::ReloadFonts() { LoadFontsList(); }

// ─────────────────────────────────────────────────────────────────────────────
//  Serialización de temas
// ─────────────────────────────────────────────────────────────────────────────
// Mismo formato "key=value" y mismas claves ("lyrics*"/"index*") que el
// parser independiente de PresentationCore.cpp (GetSavedStyle/SaveStyle,
// ver comentario ahi) -- ambos leen/escriben los MISMOS archivos .theme.
static void WriteBoxKeys(std::ofstream& f, const char* prefix, const Core::TextBoxStyle& box) {
    f << prefix << "PosX="    << box.posX  << "\n";
    f << prefix << "PosY="    << box.posY  << "\n";
    f << prefix << "SizeW="   << box.sizeW << "\n";
    f << prefix << "SizeH="   << box.sizeH << "\n";
    f << prefix << "Font="    << box.fontName << "\n";
    f << prefix << "Color="   << box.color[0] << "," << box.color[1] << ","
                               << box.color[2] << "," << box.color[3] << "\n";
    f << prefix << "Size="    << box.textSize << "\n";
    f << prefix << "HAlign="  << box.hAlign << "\n";
    f << prefix << "VAlign="  << box.vAlign << "\n";
    f << prefix << "AutoScale=" << (box.autoScale ? 1 : 0) << "\n";
    f << prefix << "BgMediaEnabled=" << (box.bgMediaEnabled ? 1 : 0) << "\n";
    f << prefix << "BgMediaPath="    << box.bgMediaPath << "\n";
    f << prefix << "BgMediaOpacity=" << box.bgMediaOpacity << "\n";
    f << prefix << "Effects=" << Core::PackTextEffects(box.effects) << "\n";
}

static void BoxFromLegacyMargins(const float margins[4], Core::TextBoxStyle& box) {
    box.sizeW = std::max(0.02f, (1920.0f - margins[0] - margins[2]) / 1920.0f);
    box.sizeH = std::max(0.02f, (1080.0f - margins[1] - margins[3]) / 1080.0f);
    box.posX  = margins[0] / 1920.0f + box.sizeW * 0.5f;
    box.posY  = margins[1] / 1080.0f + box.sizeH * 0.5f;
}

bool LayersStyleTab::SaveTheme(const std::string& name, const StyleData& data) {
    if (name.empty()) return false;
    std::error_code ec;
    fs::path dir = ThemesDir();
    fs::create_directories(dir, ec);
    if (ec) return false;

    std::ofstream f(dir / (name + ".theme"));
    if (!f.is_open()) return false;

    // Claves legacy -- derivadas de la caja de Letras, solo para que un
    // .theme guardado con el editor nuevo siga siendo legible por codigo
    // viejo que solo conozca el formato plano (ver PresentationCore::SaveStyle).
    f << "textColor=" << data.lyrics.color[0] << "," << data.lyrics.color[1] << ","
                       << data.lyrics.color[2] << "," << data.lyrics.color[3] << "\n";
    f << "textSize="  << data.lyrics.textSize << "\n";
    f << "textAlign=" << data.lyrics.hAlign   << "\n";
    f << "vAlign="    << data.lyrics.vAlign   << "\n";
    float legacyMargins[4] = {
        (data.lyrics.posX - data.lyrics.sizeW * 0.5f) * 1920.0f,
        (data.lyrics.posY - data.lyrics.sizeH * 0.5f) * 1080.0f,
        (1.0f - (data.lyrics.posX + data.lyrics.sizeW * 0.5f)) * 1920.0f,
        (1.0f - (data.lyrics.posY + data.lyrics.sizeH * 0.5f)) * 1080.0f,
    };
    f << "margins="   << legacyMargins[0] << "," << legacyMargins[1] << ","
                       << legacyMargins[2] << "," << legacyMargins[3] << "\n";
    f << "autoScale=" << (data.lyrics.autoScale ? 1 : 0) << "\n";
    f << "font="      << data.lyrics.fontName << "\n";
    f << "textEffects=" << Core::PackTextEffects(data.lyrics.effects) << "\n";

    WriteBoxKeys(f, "lyrics", data.lyrics);
    WriteBoxKeys(f, "index",  data.index);
    f << "indexEnabled=" << (data.indexEnabled ? 1 : 0) << "\n";
    return true;
}

bool LayersStyleTab::LoadThemeData(const std::string& name, StyleData& out) {
    std::ifstream f(ThemesDir() / (name + ".theme"));
    if (!f.is_open()) return false;

    out = StyleData{};

    // Legacy (fallback de migracion, ver abajo).
    float legacyMargins[4] = { 60.0f, 60.0f, 60.0f, 60.0f };
    Core::TextBoxStyle legacy;
    bool hasLyricsBoxKeys = false, hasIndexBoxKeys = false;

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k, v;
        if (!std::getline(ss, k, '=') || !std::getline(ss, v)) continue;
        v.erase(std::remove(v.begin(), v.end(), '\r'), v.end());
        v.erase(std::remove(v.begin(), v.end(), '\n'), v.end());

        if      (k == "textSize")  legacy.textSize  = std::stof(v);
        else if (k == "textAlign") legacy.hAlign     = std::stoi(v);
        else if (k == "vAlign")    legacy.vAlign     = std::stoi(v);
        else if (k == "autoScale") legacy.autoScale  = (std::stoi(v) != 0);
        else if (k == "font")      legacy.fontName   = v;
        else if (k == "textColor")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                &legacy.color[0], &legacy.color[1], &legacy.color[2], &legacy.color[3]);
        else if (k == "margins")
            sscanf(v.c_str(), "%f,%f,%f,%f",
                &legacyMargins[0], &legacyMargins[1], &legacyMargins[2], &legacyMargins[3]);

        else if (k == "lyricsPosX")    { out.lyrics.posX  = std::stof(v); hasLyricsBoxKeys = true; }
        else if (k == "lyricsPosY")    out.lyrics.posY    = std::stof(v);
        else if (k == "lyricsSizeW")   out.lyrics.sizeW   = std::stof(v);
        else if (k == "lyricsSizeH")   out.lyrics.sizeH   = std::stof(v);
        else if (k == "lyricsFont")    out.lyrics.fontName = v;
        else if (k == "lyricsColor")
            sscanf(v.c_str(), "%f,%f,%f,%f", &out.lyrics.color[0], &out.lyrics.color[1],
                   &out.lyrics.color[2], &out.lyrics.color[3]);
        else if (k == "lyricsSize")    out.lyrics.textSize = std::stof(v);
        else if (k == "lyricsHAlign")  out.lyrics.hAlign   = std::stoi(v);
        else if (k == "lyricsVAlign")  out.lyrics.vAlign   = std::stoi(v);
        else if (k == "lyricsAutoScale") out.lyrics.autoScale = (std::stoi(v) != 0);
        else if (k == "lyricsBgMediaEnabled") out.lyrics.bgMediaEnabled = (std::stoi(v) != 0);
        else if (k == "lyricsBgMediaPath")    out.lyrics.bgMediaPath = v;
        else if (k == "lyricsBgMediaOpacity") out.lyrics.bgMediaOpacity = std::stof(v);
        else if (k == "lyricsEffects") Core::UnpackTextEffects(v, out.lyrics.effects);

        else if (k == "indexPosX")     { out.index.posX  = std::stof(v); hasIndexBoxKeys = true; }
        else if (k == "indexPosY")     out.index.posY    = std::stof(v);
        else if (k == "indexSizeW")    out.index.sizeW   = std::stof(v);
        else if (k == "indexSizeH")    out.index.sizeH   = std::stof(v);
        else if (k == "indexFont")     out.index.fontName = v;
        else if (k == "indexColor")
            sscanf(v.c_str(), "%f,%f,%f,%f", &out.index.color[0], &out.index.color[1],
                   &out.index.color[2], &out.index.color[3]);
        else if (k == "indexSize")     out.index.textSize = std::stof(v);
        else if (k == "indexHAlign")   out.index.hAlign   = std::stoi(v);
        else if (k == "indexVAlign")   out.index.vAlign   = std::stoi(v);
        else if (k == "indexAutoScale") out.index.autoScale = (std::stoi(v) != 0);
        else if (k == "indexBgMediaEnabled") out.index.bgMediaEnabled = (std::stoi(v) != 0);
        else if (k == "indexBgMediaPath")    out.index.bgMediaPath = v;
        else if (k == "indexBgMediaOpacity") out.index.bgMediaOpacity = std::stof(v);
        else if (k == "indexEffects")  Core::UnpackTextEffects(v, out.index.effects);
        else if (k == "indexEnabled")  out.indexEnabled = (std::stoi(v) != 0);
    }

    // Fallback de migracion: un .theme guardado antes de la reforma a cajas
    // no tiene las claves "lyrics*"/"index*" -- se deriva una caja inicial
    // desde los campos legacy ya leidos arriba. El indice arranca
    // deshabilitado (los estilos viejos no tenian este concepto).
    if (!hasLyricsBoxKeys) {
        BoxFromLegacyMargins(legacyMargins, legacy);
        out.lyrics = legacy;
    }
    if (!hasIndexBoxKeys) out.index = out.lyrics;

    return true;
}

void LayersStyleTab::ApplyTheme(const std::string& name) {
    StyleData d;
    if (!LoadThemeData(name, d)) return;
    m_CurrentStyle  = d;
    m_SelectedTheme = name;
    ApplyCurrentStyleToCore();
}

void LayersStyleTab::DeleteTheme(const std::string& name) {
    Core::FileDeletionManager::ForceDeleteFile((ThemesDir() / (name + ".theme")).string());
    LoadThemeList();
    if (m_SelectedTheme == name) m_SelectedTheme.clear();
}

// FIXED: also call SetProjecting(true) so the projector re-renders with the
// updated style immediately (previously style changes were applied to the
// core state but the projector was left in its old state).
void LayersStyleTab::ApplyCurrentStyleToCore() {
    auto& core = Core::PresentationCore::Get();

    core.UpdateLyricsBoxStyle(m_CurrentStyle.lyrics);
    core.UpdateIndexBoxStyle(m_CurrentStyle.index, m_CurrentStyle.indexEnabled);

    // FIXED: Notify projector that something changed so it re-draws.
    // Only do this if we're already projecting — don't start projection
    // just because the user tweaked a style setting.
    auto state = core.GetState();
    if (state.isProjecting) {
        core.SetProjecting(true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Riel izquierdo — compacto, solo iconos, apilado vertical (antes era una
//  barra horizontal arriba de la galeria; asi el alto disponible es todo
//  para las tarjetas de tema, ver Render()).
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderLeftRail() {
    const float btnSz = 24.0f;
    const float gap   = 4.0f;

    // 1. Nuevo estilo (Acción principal arriba)
    if (LPCornerIconBtn("##newstyle", LPDrawPlus, "Nuevo estilo", { btnSz, btnSz }, true))
        OpenStyleEditorFullscreen(true, "", m_CurrentStyle);
    ImGui::Dummy(ImVec2(0.0f, gap));

    // 2. Ajustes rápidos (Más accesible, directamente abajo de nuevo estilo)
    if (LPCornerIconBtn("##quickadjust", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            float th = std::max(1.2f, r * 0.16f);
            const float xs[3]    = { -0.5f, 0.0f, 0.5f };
            const float knobY[3] = { 0.18f, -0.28f, 0.05f };
            for (int i = 0; i < 3; i++) {
                float x = c.x + xs[i] * r;
                dl->AddLine({x, c.y - r*0.75f}, {x, c.y + r*0.75f}, col, th);
                dl->AddCircleFilled({x, c.y + knobY[i]*r}, r*0.16f, col, 12);
            }
        }, "Ajustes rápidos", { btnSz, btnSz }))
        ImGui::OpenPopup("##QuickAdjustPopup");

    ImGui::Dummy(ImVec2(0.0f, gap + 2.0f));

    // 3. Modos de vista (Cuadrícula / Lista)
    ImGui::PushID("styleview");
    if (LPCornerIconBtn("##sgridm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            float cs = r*0.42f, g = r*0.18f;
            for (int rI=0; rI<2; rI++) for (int cI=0; cI<2; cI++) {
                ImVec2 o = { c.x - cs - g*0.5f + cI*(cs+g), c.y - cs - g*0.5f + rI*(cs+g) };
                dl->AddRectFilled(o, {o.x+cs, o.y+cs}, col, 1.5f);
            }
        }, "Vista en cuadrícula", { btnSz, btnSz }, m_GridMode))
        m_GridMode = true;
    ImGui::Dummy(ImVec2(0.0f, gap));
    if (LPCornerIconBtn("##slistm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            for (int i=0;i<3;i++) {
                float y = c.y - r*0.5f + i*r*0.5f;
                dl->AddRectFilled({c.x-r*0.7f, y}, {c.x+r*0.7f, y+r*0.22f}, col, 1.0f);
            }
        }, "Vista en lista", { btnSz, btnSz }, !m_GridMode))
        m_GridMode = false;
    ImGui::PopID();

    ImGui::Dummy(ImVec2(0.0f, gap + 2.0f));

    // 4. Recargar fuentes
    if (LPCornerIconBtn("##reloadfonts", LPDrawRefresh, "Recargar fuentes", { btnSz, btnSz }))
        LoadFontsList();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta de tema (grid)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeCard(const std::string& name, float W, float H,
                                     int idx, int col, int cols) {
    ImGui::PushID(idx);
    bool isSel = (m_SelectedTheme == name);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, { pos.x + W, pos.y + H });
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float inset = 2.0f * t;
    ImVec2 p0 = { pos.x - inset, pos.y - inset };
    ImVec2 p1 = { pos.x + W + inset, pos.y + H + inset };

    // Card background: subtle dark glass container
    ImU32 bg = isSel ? LPU32({ 0.16f, 0.18f, 0.28f, 1.0f })
                     : LPU32(ImVec4(LP::Surface1.x + (LP::Surface2.x - LP::Surface1.x) * t,
                                    LP::Surface1.y + (LP::Surface2.y - LP::Surface1.y) * t,
                                    LP::Surface1.z + (LP::Surface2.z - LP::Surface1.z) * t, 1.0f));
    dl->AddRectFilled(p0, p1, bg, 10.0f);

    // Glowing border
    ImVec4 borderC = isSel ? LP::Accent
                     : ImVec4(LP::Border.x + (LP::Accent.x - LP::Border.x) * 0.45f * t,
                              LP::Border.y + (LP::Accent.y - LP::Border.y) * 0.45f * t,
                              LP::Border.z + (LP::Accent.z - LP::Border.z) * 0.45f * t,
                              LP::Border.w + (0.5f - LP::Border.w) * t);
    dl->AddRect(p0, p1, LPU32(borderC), 10.0f, 0, isSel ? 2.0f : (1.0f + 0.5f * t));

    // Try loading theme data for real font & color preview
    StyleData thData;
    bool hasData = LoadThemeData(name, thData);

    ImVec4 textCol = hasData ? ImVec4(thData.lyrics.color[0], thData.lyrics.color[1], thData.lyrics.color[2], thData.lyrics.color[3])
                             : ImVec4(1.0f, 1.0f, 1.0f, 0.95f);
    std::string fontName = (hasData && !thData.lyrics.fontName.empty()) ? thData.lyrics.fontName : "Fuente Predeterminada";

    // Obtain the real assigned font
    ImFont* customFont = hasData ? Core::PresentationCore::Get().GetImGuiFont(thData.lyrics.fontName, 22.0f) : nullptr;
    if (!customFont) customFont = ImGui::GetFont();
    float previewFontSize = 20.0f;

    // Text preview area
    ImVec2 pp = { p0.x + 14.0f, p0.y + 12.0f };
    // Shadow
    dl->AddText(customFont, previewFontSize, { pp.x + 1.0f, pp.y + 1.0f }, IM_COL32(0, 0, 0, 170), "Aa Bb Gg");
    // Main sample text in real style font & color
    dl->AddText(customFont, previewFontSize, pp, LPU32(textCol), "Aa Bb Gg");

    // Font name badge / subtitle
    std::string fontShort = fontName.length() > 16 ? fontName.substr(0, 14) + "..." : fontName;
    dl->AddText(ImGui::GetFont(), 11.0f, { pp.x, pp.y + 26.0f }, LPU32(LP::TextMuted), fontShort.c_str());

    // Active badge
    if (isSel) {
        LPBadge(dl, { p1.x - 56.0f, p0.y + 8.0f }, "ACTIVO", LP::Accent, { 1, 1, 1, 1 }, 6.0f, 2.5f);
    }

    // Bottom banner with theme name
    std::string dn = name.length() > 18 ? name.substr(0, 15) + "..." : name;
    dl->AddRectFilled({ p0.x, p1.y - 26.0f }, { p1.x, p1.y },
                      LPU32({ 0, 0, 0, 0.65f }), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({ p0.x + (W - ns.x) * 0.5f, p1.y - 21.0f },
                isSel ? LPU32(LP::Accent) : LPU32(LP::Text), dn.c_str());

    ImGui::InvisibleButton("##tcard", { W, H });
    if (ImGui::IsItemClicked()) ApplyTheme(name);

    if (ImGui::BeginPopupContextItem("ThCtx")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", name.c_str()); ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            StyleData ed; if (LoadThemeData(name, ed)) OpenStyleEditorFullscreen(false, name, ed);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) DeleteTheme(name);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (col < cols - 1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de tema (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeRow(const std::string& name, float W, float rowH, int idx) {
    ImGui::PushID(idx);
    bool isSel = (m_SelectedTheme == name);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, { pos.x + W, pos.y + rowH });
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = LPU32(ImVec4(LP::Surface1.x + (LP::Surface2.x - LP::Surface1.x) * t,
                             LP::Surface1.y + (LP::Surface2.y - LP::Surface1.y) * t,
                             LP::Surface1.z + (LP::Surface2.z - LP::Surface1.z) * t, 1.0f));
    if (isSel) bg = LPU32(ImVec4(0.16f, 0.18f, 0.26f, 1.0f));
    dl->AddRectFilled(pos, { pos.x + W, pos.y + rowH }, bg, 6.0f);

    if (isSel) {
        dl->AddRectFilled(pos, { pos.x + 3.5f, pos.y + rowH }, LPU32(LP::Accent), 6.0f, ImDrawFlags_RoundCornersLeft);
        dl->AddRect(pos, { pos.x + W, pos.y + rowH }, LPU32(LP::Accent), 6.0f, 0, 1.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(pos, { pos.x + 3.5f, pos.y + rowH }, LPU32({ LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.4f * t }), 6.0f, ImDrawFlags_RoundCornersLeft);
    }

    StyleData thData;
    bool hasData = LoadThemeData(name, thData);
    ImVec4 textCol = hasData ? ImVec4(thData.lyrics.color[0], thData.lyrics.color[1], thData.lyrics.color[2], thData.lyrics.color[3])
                             : ImVec4(1.0f, 1.0f, 1.0f, 0.95f);

    ImFont* customFont = hasData ? Core::PresentationCore::Get().GetImGuiFont(thData.lyrics.fontName, 16.0f) : nullptr;
    if (!customFont) customFont = ImGui::GetFont();

    float px = pos.x + 14.0f, py = pos.y + (rowH - 26.0f) * 0.5f;
    // Mini chip preview with real font
    dl->AddRectFilled({ px, py }, { px + 36.0f, py + 26.0f }, IM_COL32(0, 0, 0, 140), 4.0f);
    dl->AddText(customFont, 16.0f, { px + 6.0f, py + 3.0f }, LPU32(textCol), "Aa");

    std::string dn = name.length() > 28 ? name.substr(0, 25) + "..." : name;
    float txtX = px + 46.0f, txtY = pos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f;
    dl->AddText({ txtX, txtY }, isSel ? LPU32(LP::Accent) : LPU32(LP::Text), dn.c_str());

    if (hasData && !thData.lyrics.fontName.empty()) {
        std::string fn = thData.lyrics.fontName;
        if (fn.length() > 18) fn = fn.substr(0, 16) + "...";
        ImVec2 fSz = ImGui::CalcTextSize(fn.c_str());
        dl->AddText({ pos.x + W - fSz.x - (isSel ? 72.0f : 16.0f), txtY }, LPU32(LP::TextMuted), fn.c_str());
    }

    if (isSel) {
        LPBadge(dl, { pos.x + W - 58.0f, pos.y + (rowH - 18.0f) * 0.5f }, "ACTIVO", LP::Accent, { 1, 1, 1, 1 }, 5.0f, 2.0f);
    }

    ImGui::InvisibleButton("##trow", { W, rowH });
    if (ImGui::IsItemClicked()) ApplyTheme(name);

    if (ImGui::BeginPopupContextItem("ThCtxL")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", name.c_str()); ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            StyleData ed; if (LoadThemeData(name, ed)) OpenStyleEditorFullscreen(false, name, ed);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) DeleteTheme(name);
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::Dummy({ 0, 4.0f });
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RenderThemeGrid
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderThemeGrid() {
    if (m_AvailableThemes.empty()) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x + w, p.y + 60}, LPU32(LP::Surface1), 12.0f);
        ImGui::Dummy({0, 18});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* h = "Crea tu primer estilo con el botón + de arriba";
        float tw = ImGui::CalcTextSize(h).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w - tw) * 0.5f));
        ImGui::Text("%s", h);
        ImGui::PopStyleColor();
        ImGui::Dummy({0, 8});
        return;
    }

    // Header bar with count and Zoom Slider
    {
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::Text("Estilos (%d)", (int)m_AvailableThemes.size());
        ImGui::PopStyleColor();

        if (m_GridMode) {
            const float zoomW = 85.0f;
            float avail = ImGui::GetWindowContentRegionMax().x;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - zoomW - 10.0f));
            LPZoomSlider("##styleZoom", &m_ThumbZoom, 0.65f, 1.8f, zoomW);
        }
        ImGui::Spacing();
    }

    if (m_GridMode) {
        const float cW = 145.0f * m_ThumbZoom, cH = 90.0f * m_ThumbZoom, gap2 = 10.0f;
        float pW = ImGui::GetContentRegionAvail().x;
        
        int cols = std::max(1, (int)((pW + gap2) / (cW + gap2)));
        
        // Centrado dinámico de la cuadrícula (Grid)
        float totalGridWidth = (cols * cW) + ((cols - 1) * gap2);
        float offsetX = std::max(0.0f, (pW - totalGridWidth) * 0.5f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {gap2, gap2});
        for (int i = 0; i < (int)m_AvailableThemes.size(); i++) {
            if (i % cols == 0) {
                ImGui::SetCursorPosX(offsetX); // Empuja el inicio de cada fila al centro
            }
            RenderThemeCard(m_AvailableThemes[i], cW, cH, i, i % cols, cols);
        }
        ImGui::PopStyleVar();
    } else {
        float pW = ImGui::GetContentRegionAvail().x;
        for (int i = 0; i < (int)m_AvailableThemes.size(); i++)
            RenderThemeRow(m_AvailableThemes[i], pW, 48.0f, i);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ajustes Rapidos (Adaptado para panel lateral, sin CollapsingHeader)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderQuickAdjust() {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    
    // Titulo limpio en lugar de un header colapsable que roba espacio
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::Text(" AJUSTES RÁPIDOS");
    ImGui::PopStyleColor();
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    bool changed = false;

    if (ImGui::BeginTable("##QuickAdjustTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, LP::Surface1);

        // --- FUENTE ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Fuente");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##qf", m_CurrentStyle.lyrics.fontName.c_str())) {
            for (const auto& f : m_AvailableFonts) {
                bool sel = (m_CurrentStyle.lyrics.fontName == f);
                if (ImGui::Selectable(f.c_str(), sel)) {
                    m_CurrentStyle.lyrics.fontName = f;
                    changed = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --- COLOR ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Color Base");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::ColorEdit4("##qc", m_CurrentStyle.lyrics.color,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaPreviewHalf);

        // --- TAMAÑO ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Tamaño");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragFloat("##qs", &m_CurrentStyle.lyrics.textSize, 1.0f, 10.0f, 500.0f, "%.1f px");

        // --- ALINEACIÓN HORIZONTAL ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Alineación H");
        ImGui::TableNextColumn();

        const char* hA[] = {"Izq", "Cen", "Der"};
        float btnW = ImGui::GetContentRegionAvail().x / 3.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        for (int a = 0; a < 3; a++) {
            if (a > 0) ImGui::SameLine();
            bool act = (m_CurrentStyle.lyrics.hAlign == a);
            ImGui::PushStyleColor(ImGuiCol_Button,        act ? ImVec4(0.3f,0.3f,0.3f,1.0f) : LP::Surface0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ImVec4(0.35f,0.35f,0.35f,1.0f) : LP::Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text,          act ? ImVec4(1,1,1,1) : LP::TextSub);
            float rounding = (a == 0 || a == 2) ? 3.0f : 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            std::string btnId = std::string(hA[a]) + "##qa" + std::to_string(a);
            if (ImGui::Button(btnId.c_str(), ImVec2(btnW, 26))) {
                m_CurrentStyle.lyrics.hAlign = a;
                changed = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::PopStyleVar();

        // --- ALINEACIÓN VERTICAL ---
        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(LP::TextMuted, "Alineación V");
        ImGui::TableNextColumn();

        const char* vA[] = {"Arr", "Cen", "Aba"};
        btnW = ImGui::GetContentRegionAvail().x / 3.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        for (int a = 0; a < 3; a++) {
            if (a > 0) ImGui::SameLine();
            bool act = (m_CurrentStyle.lyrics.vAlign == a);
            ImGui::PushStyleColor(ImGuiCol_Button,        act ? ImVec4(0.3f,0.3f,0.3f,1.0f) : LP::Surface0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, act ? ImVec4(0.35f,0.35f,0.35f,1.0f) : LP::Surface2);
            ImGui::PushStyleColor(ImGuiCol_Text,          act ? ImVec4(1,1,1,1) : LP::TextSub);
            float rounding = (a == 0 || a == 2) ? 3.0f : 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
            std::string btnId = std::string(vA[a]) + "##qv" + std::to_string(a);
            if (ImGui::Button(btnId.c_str(), ImVec2(btnW, 26))) {
                m_CurrentStyle.lyrics.vAlign = a;
                changed = true;
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }
        ImGui::PopStyleVar();

        ImGui::PopStyleColor(); // FrameBg
        ImGui::PopStyleVar();   // FrameRounding
        ImGui::EndTable();
    }

    if (changed) ApplyCurrentStyleToCore();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Popup de Ajustes Rapidos — se abre desde el icono de sliders en la
//  toolbar (ver RenderTopBar). Antes vivia fijo debajo de la galeria de
//  temas, robandole ~45% del alto; ahora la galeria usa todo el espacio y
//  esto aparece solo cuando el usuario lo pide.
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::RenderQuickAdjustPopup() {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    if (ImGui::BeginPopup("##QuickAdjustPopup")) {
        ImGui::BeginChild("##quickAdjustPopupContent", ImVec2(300.0f, 0.0f), false);
        RenderQuickAdjust();
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal del tab (AHORA CON LAYOUT DE 2 COLUMNAS)
// ─────────────────────────────────────────────────────────────────────────────
void LayersStyleTab::Render() {
    // ── Riel angosto a la izquierda (grid/lista, zoom, recargar fuentes,
    //    nuevo estilo, ajustes rapidos) + galeria de temas a la derecha,
    //    usando todo el alto disponible -- antes el mismo riel era una
    //    barra horizontal arriba de la galeria, robandole alto util a las
    //    tarjetas de tema (ver RenderLeftRail).
    constexpr float kRailW = 28.0f;
    const float     availH = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##stylesLeftRail", ImVec2(kRailW, availH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::Dummy(ImVec2(0.0f, 1.0f));
    RenderLeftRail();
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 8.0f);

    // Divisor vertical -- mismo criterio que el divisor horizontal de
    // StylesHubPanel entre su rail de arriba y el contenido.
    {
        ImVec2      p0 = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(p0, { p0.x, p0.y + availH },
            LPU32({ LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.30f }), 1.0f);
        ImGui::Dummy(ImVec2(1.0f, availH));
    }
    ImGui::SameLine(0.0f, 10.0f);

    ImGui::BeginChild("##ThemesListChild", ImGui::GetContentRegionAvail(), false);
    RenderThemeGrid();
    ImGui::EndChild();

    RenderQuickAdjustPopup();

    // El editor de estilos ya no se dibuja aca: se abre a pantalla completa
    // via UIManager::EnterFullscreenEditor (ver OpenStyleEditorFullscreen),
    // disparado desde "Nuevo estilo" / "Editar" arriba.
}

} // namespace ProyecThor::UI