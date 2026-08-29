#include "OverlayLibraryTab.h"
#include "OverlayRecipeIO.h"
#include "OverlaySvgImport.h"
#include "layers/LayersTheme.h"
#include "frontend/ui/UIManager.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/FileDeletionManager.h"
#include "backend/core/AppPaths.h"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <set>
#include <cstring>
#include <cstdio>
#include <GL/gl.h>
#include "stb_image.h"
#include "frontend/panels/stb_image_write.h"

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas — mismo patron que LayersStyleTab/LayersOverlayTab historico (cada
//  seccion resuelve su propia carpeta de datos dentro de assets/).
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    fs::path dir = fs::path(buf) / "ProyecThor";
#else
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
    fs::create_directories(dir, ec);
    return dir;
}
static fs::path OverlaysDir() {
    // Misma carpeta que resuelve ProyecThor::OverlaysPath() (AppPaths.h) --
    // usar ese helper compartido en vez de una segunda resolucion de ruta
    // independiente, para que SyncServer.cpp (control remoto del celular)
    // apunte exactamente al mismo lugar.
    fs::path dir = ProyecThor::OverlaysPath();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static fs::path FontsDir() { return GetAppDataDir() / "assets" / "fonts"; }
static fs::path OverlayImagesDir() {
    fs::path dir = OverlaysDir() / "images";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static fs::path BgImagesRootDir() {
    fs::path dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    dir = fs::path(buf) / "ProyecThor";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    fs::path base;
    if (xdgConfig && *xdgConfig) base = fs::path(xdgConfig);
    else { const char* home = std::getenv("HOME"); base = fs::path(home ? home : ".") / ".config"; }
    dir = base / "ProyecThor";
#endif
    return dir / "assets" / "backgrounds";
}

#ifndef _WIN32
static std::string OpenImageFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar imagen\" "
        "--file-filter=\"Imágenes | *.jpg *.jpeg *.png\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.jpg *.jpeg *.png|Imágenes\" 2>/dev/null"
    };
    for (const char* cmd : commands) {
        std::string result;
        char buffer[1024];
        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
        int status = pclose(pipe);
        if (status != 0) continue;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        if (!result.empty()) return result;
    }
    return {};
}

static std::string OpenSvgFileDialogUnix() {
    const char* commands[] = {
        "zenity --file-selection --title=\"Seleccionar SVG\" "
        "--file-filter=\"SVG | *.svg\" 2>/dev/null",
        "kdialog --getopenfilename . \"*.svg|SVG\" 2>/dev/null"
    };
    for (const char* cmd : commands) {
        std::string result;
        char buffer[1024];
        FILE* pipe = popen(cmd, "r");
        if (!pipe) continue;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) result += buffer;
        int status = pclose(pipe);
        if (status != 0) continue;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        if (!result.empty()) return result;
    }
    return {};
}
#endif

static ImTextureID LoadImageThumb(const char* path) {
    int w, h, n;
    unsigned char* d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d);
    return (ImTextureID)(intptr_t)tex;
}

ImTextureID OverlayLibraryTab::GetThumbnail(const std::string& path) {
    auto it = m_ThumbnailCache.find(path);
    if (it != m_ThumbnailCache.end()) return it->second;
    std::string abs = fs::absolute(fs::path(path)).string();
    ImTextureID t = LoadImageThumb(abs.c_str());
    m_ThumbnailCache[path] = t;
    return t;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / listas
// ─────────────────────────────────────────────────────────────────────────────
OverlayLibraryTab::OverlayLibraryTab(UIManager* uiManager)
    : m_UIManager(uiManager)
{
    LoadFontsList();
    m_Editor = std::make_unique<OverlayCanvasEditor>(
        &m_AvailableFonts,
        [this](const std::string& name) { return ResolvePngPath(name); },
        [this]() { return ListBgImages(); },
        [this]() { return ImportOverlayImage(); },
        [this](int canvasW, int canvasH) { return ImportOverlaySvgAsLayers(canvasW, canvasH); },
        [this](int canvasW, int canvasH) { return ImportOverlaySvgSingle(canvasW, canvasH); });
    SeedDefaultOverlaysIfEmpty();
    ReloadList();
}

// ─────────────────────────────────────────────────────────────────────────────
//  SeedDefaultOverlaysIfEmpty — deja unos overlays de reloj ya listos para
//  probar, la primera vez que se ve cada uno. NO se evalua por "la carpeta
//  esta vacia" (el usuario puede tener overlays propios de antes) ni por "ya
//  existe el archivo" (si el usuario borra un predeterminado a mano, borrarlo
//  debe ser definitivo, no reaparecer solo porque el archivo ya no esta).
//  Se usa un marcador aparte (_defaults_seeded.txt, una linea por nombre ya
//  sembrado alguna vez) para distinguir "nunca lo cree" de "lo cree y el
//  usuario lo borro". Esto es lo que faltaba para que los predeterminados
//  convivan con contenido nuevo del usuario sin desaparecer solos.
//  Una capa Clock nunca se hornea al PNG (ver OverlayCanvasEditor::
//  RenderCanvas), asi que el PNG resultante es 100% transparente y se puede
//  escribir directo con stb_image_write, sin necesitar el editor ni un frame
//  de ImGui.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::SeedDefaultOverlaysIfEmpty() {
    struct Preset { const char* name; float posX, posY; float fontSize; bool bg; };
    static const Preset kPresets[] = {
        { "Reloj - Barra inferior",           0.5f,  0.92f, 88.0f,  true  },
        { "Reloj - Esquina inferior derecha", 0.88f, 0.90f, 60.0f,  true  },
        { "Reloj - Centrado",                 0.5f,  0.5f,  140.0f, false },
    };

    fs::path markerPath = OverlaysDir() / "_defaults_seeded.txt";
    std::set<std::string> alreadySeeded;
    {
        std::ifstream in(markerPath);
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (!line.empty()) alreadySeeded.insert(line);
        }
    }

    std::vector<const Preset*> toSeed;
    for (const auto& p : kPresets)
        if (!alreadySeeded.count(p.name)) toSeed.push_back(&p);
    if (toSeed.empty()) return;

    constexpr int kW = 1920, kH = 1080;
    std::vector<unsigned char> transparentPixels((size_t)kW * kH * 4, 0);

    std::ofstream markerOut(markerPath, std::ios::app);
    for (const Preset* pp : toSeed) {
        const Preset& p = *pp;
        OverlayDoc doc;
        doc.canvasW = kW;
        doc.canvasH = kH;

        OverlayLayer clock;
        clock.kind          = OverlayLayerKind::Clock;
        clock.text          = "00:00:00";
        clock.fontSize      = p.fontSize;
        clock.posX          = p.posX;
        clock.posY          = p.posY;
        clock.shadowEnabled = true;
        clock.bgEnabled     = p.bg;
        if (p.bg) {
            clock.bgColor[0] = 0.0f; clock.bgColor[1] = 0.0f; clock.bgColor[2] = 0.0f; clock.bgColor[3] = 0.55f;
            clock.bgPaddingX = 28.0f; clock.bgPaddingY = 14.0f; clock.bgRounding = 12.0f;
        }
        doc.layers.push_back(clock);

        ProyecThor::UI::SaveOverlayRecipe(OverlaysDir(), p.name, doc);
        stbi_write_png(ResolvePngPath(p.name).c_str(), kW, kH, 4, transparentPixels.data(), kW * 4);
        markerOut << p.name << "\n";
    }
}

void OverlayLibraryTab::LoadFontsList() {
    m_AvailableFonts.clear();
    m_AvailableFonts.push_back("Predeterminada");
    try {
        fs::path d = FontsDir();
        fs::create_directories(d);
        if (fs::exists(d))
            for (const auto& e : fs::directory_iterator(d)) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".ttf" || ext == ".otf" || ext == ".ttc")
                    m_AvailableFonts.push_back(e.path().stem().string());
            }
    } catch (...) {}
}

void OverlayLibraryTab::ReloadList() {
    m_Overlays.clear();
    try {
        for (const auto& e : fs::directory_iterator(OverlaysDir())) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() != ".overlay") continue;
            std::string name = e.path().stem().string();
            fs::path png = OverlaysDir() / (name + ".png");
            if (!fs::exists(png)) continue;

            OverlayEntry entry;
            entry.name    = name;
            entry.pngPath = png.string();

            OverlayDoc doc;
            if (ProyecThor::UI::LoadOverlayRecipe(OverlaysDir(), name, doc)) {
                entry.canvasW = doc.canvasW;
                entry.canvasH = doc.canvasH;
                if (const OverlayLayer* cl = FindClockLayer(doc)) {
                    entry.hasClock   = true;
                    entry.clockLayer = *cl;
                }
            }
            m_Overlays.push_back(std::move(entry));
        }
    } catch (...) {}
    std::sort(m_Overlays.begin(), m_Overlays.end(),
        [](const OverlayEntry& a, const OverlayEntry& b) { return a.name < b.name; });
}

std::string OverlayLibraryTab::ResolvePngPath(const std::string& name) {
    return (OverlaysDir() / (name + ".png")).string();
}

std::vector<std::string> OverlayLibraryTab::ListBgImages() {
    std::vector<std::string> out;
    std::error_code ec;
    fs::path root = BgImagesRootDir();
    if (!fs::exists(root, ec)) return out;
    for (const auto& e : fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png")
            out.push_back(e.path().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string OverlayLibraryTab::ImportOverlayImage() {
    std::string selectedPath;
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = "Imágenes\0*.jpg;*.jpeg;*.png\0Todos los archivos\0*.*\0";
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return {};
    selectedPath = filename;
#else
    selectedPath = OpenImageFileDialogUnix();
    if (selectedPath.empty()) return {};
#endif

    std::error_code ec;
    fs::path dstDir = OverlayImagesDir();
    fs::path src(selectedPath);
    fs::path dst = dstDir / src.filename();
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) return {};
    return dst.string();
}

std::string OverlayLibraryTab::PickSvgFile() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = NULL;
    ofn.lpstrFilter = "Archivos SVG\0*.svg\0Todos los archivos\0*.*\0";
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return {};
    return filename;
#else
    return OpenSvgFileDialogUnix();
#endif
}

std::vector<OverlayLayer> OverlayLibraryTab::ImportOverlaySvgAsLayers(int canvasW, int canvasH) {
    std::string selectedPath = PickSvgFile();
    if (selectedPath.empty()) return {};
    return ImportSvgAsLayers(selectedPath, canvasW, canvasH, OverlayImagesDir().string());
}

OverlayLayer OverlayLibraryTab::ImportOverlaySvgSingle(int canvasW, int canvasH) {
    std::string selectedPath = PickSvgFile();
    if (selectedPath.empty()) return {};
    return ImportSvgAsSingleImage(selectedPath, canvasW, canvasH, OverlayImagesDir().string());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistencia de la receta editable (.overlay) -- ver OverlayRecipeIO.h/.cpp
//  (extraido para que SyncServer.cpp comparta el mismo parser al servir el
//  control remoto de Overlays desde el celular).
// ─────────────────────────────────────────────────────────────────────────────
bool OverlayLibraryTab::DeleteOverlay(const std::string& name) {
    Core::FileDeletionManager::ForceDeleteFile((OverlaysDir() / (name + ".overlay")).string());
    Core::FileDeletionManager::ForceDeleteFile((OverlaysDir() / (name + ".png")).string());
    m_ThumbnailCache.erase(ResolvePngPath(name));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Apertura del editor a pantalla completa (ver UIManager::EnterFullscreenEditor)
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::OpenEditorFullscreen(bool isNew, const std::string& name, const OverlayDoc& doc) {
    LoadFontsList();
    if (isNew) m_Editor->OpenNew();
    else       m_Editor->OpenEdit(name, doc);

    if (m_UIManager) {
        m_UIManager->EnterFullscreenEditor([this]() {
            m_Editor->Render(
                [this](const std::string& n, const OverlayDoc& d) {
                    if (ProyecThor::UI::SaveOverlayRecipe(OverlaysDir(), n, d)) {
                        std::string pngPath = ResolvePngPath(n);
                        m_ThumbnailCache.erase(pngPath);
                        ReloadList();

                        // Si el overlay editado es el que ya esta en vivo,
                        // se refresca la textura/cuadro de reloj proyectados
                        // con la version recien guardada -- la transmision al
                        // publico NUNCA se toca mientras se edita (abrir/
                        // cerrar el editor no llama nada de PresentationCore),
                        // solo se actualiza aca, una vez que el guardado ya
                        // se confirmo.
                        auto& core = Core::PresentationCore::Get();
                        if (core.GetOverlayPath() == pngPath) {
                            core.SetOverlayMedia(pngPath);
                            if (const OverlayLayer* cl = FindClockLayer(d))
                                core.SetOverlayClockLayer(true, *cl, d.canvasW, d.canvasH);
                            else
                                core.SetOverlayClockLayer(false, OverlayLayer{}, d.canvasW, d.canvasH);
                        }
                    }
                },
                [this]() {
                    if (m_UIManager) m_UIManager->ExitFullscreenEditor();
                });
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar superior — compacta, solo iconos
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::RenderTopBar() {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Overlays");
    ImGui::PopStyleColor();

    const float btnSz = 26.0f;
    const float zoomW = 76.0f;
    const float gap   = 4.0f;
    const float rowW  = zoomW + gap + btnSz * 3 + gap * 3;
    const float avail = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));

    if (m_GridMode) {
        LPZoomSlider("##ovlzoom", &m_ThumbZoom, 0.65f, 1.8f, zoomW);
        ImGui::SameLine(0, gap);
    } else {
        ImGui::Dummy(ImVec2(zoomW, btnSz));
        ImGui::SameLine(0, gap);
    }

    ImGui::PushID("ovlview");
    if (LPCornerIconBtn("##ovlgridm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            float cs = r*0.42f, g = r*0.18f;
            for (int rI=0; rI<2; rI++) for (int cI=0; cI<2; cI++) {
                ImVec2 o = { c.x - cs - g*0.5f + cI*(cs+g), c.y - cs - g*0.5f + rI*(cs+g) };
                dl->AddRectFilled(o, {o.x+cs, o.y+cs}, col, 1.5f);
            }
        }, "Vista en cuadricula", {btnSz,btnSz}, m_GridMode))
        m_GridMode = true;
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##ovllistm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            for (int i=0;i<3;i++) {
                float y = c.y - r*0.5f + i*r*0.5f;
                dl->AddRectFilled({c.x-r*0.7f, y}, {c.x+r*0.7f, y+r*0.22f}, col, 1.0f);
            }
        }, "Vista en lista", {btnSz,btnSz}, !m_GridMode))
        m_GridMode = false;
    ImGui::PopID();

    ImGui::SameLine(0, gap*2);
    if (LPCornerIconBtn("##ovlnew", LPDrawPlus, "Nuevo overlay", {btnSz,btnSz}, true))
        OpenEditorFullscreen(true, "", OverlayDoc{});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta / fila
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::RenderCard(const OverlayEntry& e, float W, float H, int col, int cols) {
    ImGui::PushID(e.name.c_str());

    ImTextureID thumb = GetThumbnail(e.pngPath);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float inset = 2.0f * t;
    ImVec2 p0 = { pos.x - inset, pos.y - inset };
    ImVec2 p1 = { pos.x + W + inset, pos.y + H + inset };

    dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), 10.0f);
    if (thumb) dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32_WHITE, 10.0f);

    ImVec4 borderCol(
        LP::Border.x + (LP::Accent.x-LP::Border.x)*t,
        LP::Border.y + (LP::Accent.y-LP::Border.y)*t,
        LP::Border.z + (LP::Accent.z-LP::Border.z)*t,
        LP::Border.w + (0.6f-LP::Border.w)*t);
    dl->AddRect(p0, p1, LPU32(borderCol), 10.0f, 0, 1.0f + 0.8f*t);

    std::string dn = e.name.length() > 18 ? e.name.substr(0,15) + "..." : e.name;
    dl->AddRectFilled({p0.x, p1.y-26.0f}, {p1.x, p1.y}, LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, LPU32(LP::Text), dn.c_str());

    ImGui::InvisibleButton(("##ovc_"+e.name).c_str(), {W, H});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Core::PresentationCore::Get().SetOverlayMedia(e.pngPath);
        Core::PresentationCore::Get().SetOverlayClockLayer(e.hasClock, e.clockLayer, e.canvasW, e.canvasH);
    }

    if (ImGui::BeginPopupContextItem(("OvCtx_"+e.name).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", e.name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            OverlayDoc doc;
            if (ProyecThor::UI::LoadOverlayRecipe(OverlaysDir(), e.name, doc))
                OpenEditorFullscreen(false, e.name, doc);
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) {
            if (DeleteOverlay(e.name)) ReloadList();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

void OverlayLibraryTab::RenderRow(const OverlayEntry& e, float W, float rowH) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("ovr_"+e.name).c_str());

    ImTextureID thumb = GetThumbnail(e.pngPath);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 bgCol(LP::Surface1.x+(LP::Surface2.x-LP::Surface1.x)*t,
                 LP::Surface1.y+(LP::Surface2.y-LP::Surface1.y)*t,
                 LP::Surface1.z+(LP::Surface2.z-LP::Surface1.z)*t, 1.0f);
    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH}, LPU32(bgCol), 8.0f);
    if (t > 0.01f)
        dl->AddRectFilled(pos, {pos.x+3.0f,pos.y+rowH}, LPU32(ImVec4(LP::Accent.x,LP::Accent.y,LP::Accent.z,t)), 2.0f);

    float tx = pos.x+8.0f, ty = pos.y+(rowH-thumbSz)*0.5f;
    if (thumb) dl->AddImageRounded(thumb, {tx,ty}, {tx+thumbSz,ty+thumbSz}, {0,0},{1,1}, IM_COL32_WHITE, 5.0f);
    else       dl->AddRectFilled({tx,ty}, {tx+thumbSz,ty+thumbSz}, LPU32(LP::Surface0), 5.0f);

    std::string dn = e.name.length() > 32 ? e.name.substr(0,29) + "..." : e.name;
    dl->AddText({tx+thumbSz+10.0f, pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f}, LPU32(LP::Text), dn.c_str());

    ImGui::InvisibleButton(("##ovrow_"+e.name).c_str(), {W, rowH});
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        Core::PresentationCore::Get().SetOverlayMedia(e.pngPath);
        Core::PresentationCore::Get().SetOverlayClockLayer(e.hasClock, e.clockLayer, e.canvasW, e.canvasH);
    }

    if (ImGui::BeginPopupContextItem(("OvRowCtx_"+e.name).c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("%s", e.name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::Selectable("  Editar")) {
            OverlayDoc doc;
            if (ProyecThor::UI::LoadOverlayRecipe(OverlaysDir(), e.name, doc))
                OpenEditorFullscreen(false, e.name, doc);
        }
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
        if (ImGui::Selectable("  Eliminar")) {
            if (DeleteOverlay(e.name)) ReloadList();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::Dummy({0, 5.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Galeria
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::RenderGallery() {
    if (m_Overlays.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w  = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x+w,p.y+64}, LPU32(LP::Surface1), 10.0f);
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* msg = "Sin overlays aun. Usa el botón + de arriba para crear uno.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w-tw)*0.5f));
        ImGui::Text("%s", msg);
        ImGui::PopStyleColor();
        return;
    }

    if (m_GridMode) {
        const float cW = 150.0f * m_ThumbZoom;
        const float cH = 92.0f  * m_ThumbZoom;
        const float minGap = 10.0f;

        float availWidth = ImGui::GetContentRegionAvail().x;
        int cols = std::max(1, static_cast<int>((availWidth + minGap) / (cW + minGap)));

        float totalGaps = static_cast<float>(cols - 1);
        float dynamicGap = minGap;
        if (totalGaps > 0) {
            float extraSpace = availWidth - (cols * cW);
            dynamicGap = std::max(minGap, extraSpace / totalGaps);
        }

        int currentCol = 0;
        for (size_t i = 0; i < m_Overlays.size(); ++i) {
            RenderCard(m_Overlays[i], cW, cH, currentCol, cols);
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, dynamicGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        float w = ImGui::GetContentRegionAvail().x;
        for (const auto& e : m_Overlays)
            RenderRow(e, w, 44.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal — SOLO la galeria: el editor se muestra a pantalla
//  completa a traves de UIManager::EnterFullscreenEditor (ver
//  OpenEditorFullscreen), no dentro de este panel.
// ─────────────────────────────────────────────────────────────────────────────
void OverlayLibraryTab::Render() {
    RenderTopBar();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    RenderGallery();
}

} // namespace ProyecThor::UI
