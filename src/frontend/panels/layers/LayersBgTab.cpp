#include "LayersBgTab.h"
#include "LayersTheme.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/AppPaths.h"
#include "frontend/panels/biblio/LibraryMultimedia.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#endif
#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <GL/gl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <functional>
#include "stb_image.h"

namespace fs = std::filesystem;
namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Rutas
//  Multiplataforma: en Windows usa la carpeta AppData del usuario, en Linux
//  sigue la convencion XDG ($XDG_CONFIG_HOME o $HOME/.config).
// ─────────────────────────────────────────────────────────────────────────────
static fs::path GetAppDataDir() {
    fs::path dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    dir = fs::path(buf) / "ProyecThor";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    fs::path base;
    if (xdgConfig && *xdgConfig)
    {
        base = fs::path(xdgConfig);
    }
    else
    {
        const char* home = std::getenv("HOME");
        base = fs::path(home ? home : ".") / ".config";
    }
    dir = base / "ProyecThor";
#endif
    std::error_code ec;
    fs::create_directories(dir / "themes", ec);
    return dir;
}
static fs::path BgRootDir() {
    fs::path dir = GetAppDataDir() / "assets" / "backgrounds";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers locales
// ─────────────────────────────────────────────────────────────────────────────
static bool IsMedia(const std::string& ext) {
    return ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mov"
          ||ext==".jpg"||ext==".jpeg"||ext==".png";
}
static bool IsImage(const std::string& ext) {
    return ext==".jpg"||ext==".jpeg"||ext==".png";
}

// Cuanto hay que mantener presionado antes de que cuente como "hold" (mostrar
// preview) en vez de un click normal. Ver RenderBgCard/RenderBgRow/RenderHoldPreview.
static constexpr float kHoldPreviewThreshold = 0.18f;

// ─────────────────────────────────────────────────────────────────────────────
//  Thumbnails
// ─────────────────────────────────────────────────────────────────────────────
static ImTextureID LoadImageThumb(const char* path);

// Miniaturas de video: se generan en 2do plano via ThumbnailWorker (ver
// backend/core/ThumbnailWorker.h) — decodifica el primer frame real con
// libVLC en modo callback puro, asi que nunca abre una ventana propia (a
// diferencia de pedirle a libVLC un snapshot con salida de video por
// defecto, que si la abre). El resultado se cachea en disco en
// ThumbCacheDir(), asi que solo se paga el costo de decodificar una vez por
// video en la vida de la instalacion, no en cada sesion de la app.
static fs::path ThumbCacheDir() {
    fs::path dir = GetAppDataDir() / "thumbnails";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

// Hash de path+tamaño (no solo path): si el archivo se reemplaza por otro
// con el mismo nombre pero distinto contenido/tamaño, se regenera en vez de
// mostrar para siempre la miniatura vieja.
static std::string ThumbCachePathFor(const std::string& absVideoPath) {
    std::error_code ec;
    auto sz = fs::file_size(absVideoPath, ec);
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return (ThumbCacheDir() / (std::to_string(h) + ".png")).string();
}

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

ImTextureID LayersBgTab::GetThumbnail(const std::string& path, bool isVideo) {
    auto it = m_ThumbnailCache.find(path);
    if (it != m_ThumbnailCache.end()) return it->second;

    std::string abs = fs::absolute(fs::path(path)).string();

    if (!isVideo) {
        ImTextureID t = LoadImageThumb(abs.c_str());
        m_ThumbnailCache[path] = t;
        return t;
    }

    // Video: si ya se genero en una sesion anterior, esta en el cache de
    // disco — cargarlo de ahi es instantaneo (es una imagen mas, via
    // LoadImageThumb) y no necesita tocar el worker para nada.
    std::string cachePath = ThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(cachePath, ec)) {
        ImTextureID t = LoadImageThumb(cachePath.c_str());
        if (t) { m_ThumbnailCache[path] = t; return t; }
    }

    // Todavia no existe: se pide en 2do plano (ThumbnailWorker, sin abrir
    // ninguna ventana ni trabar la UI) y por ahora se deja SIN entrar en
    // m_ThumbnailCache — asi el proximo frame vuelve a preguntar y, cuando
    // el worker termine, DrainThumbnailResults() ya habra puesto el
    // resultado real ahi. Mientras tanto la tarjeta cae en el icono
    // generico "VID" (texture id 0).
    m_ThumbWorker.Request(path, abs, cachePath);
    return 0;
}

// Llamar una vez por frame desde Render(): sube a textura GL los frames que
// el worker haya terminado de decodificar desde el ultimo frame.
void LayersBgTab::DrainThumbnailResults() {
    std::vector<Core::ThumbnailWorker::Result> results;
    m_ThumbWorker.DrainResults(results);
    for (auto& r : results) {
        ImTextureID t = 0;
        if (!r.pixels.empty() && r.width > 0 && r.height > 0) {
            GLuint tex; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(r.width),
                         static_cast<GLsizei>(r.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         r.pixels.data());
            t = (ImTextureID)(intptr_t)tex;
        }
        // Si fallo (t == 0) igual lo dejamos en el cache: evita reintentar
        // sin fin un archivo que no se puede decodificar, se queda con el
        // icono generico "VID".
        m_ThumbnailCache[r.key] = t;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / ReloadList
// ─────────────────────────────────────────────────────────────────────────────
LayersBgTab::LayersBgTab() {
    ReloadList();
}

void LayersBgTab::ReloadList() {
    m_AllBackgrounds.clear();
    m_BgFolders.clear();
    fs::path root = BgRootDir();
    try {
        for (const auto& e : fs::directory_iterator(root)) {
            if (e.is_directory()) {
                m_BgFolders.push_back(e.path().filename().string());
                for (const auto& sub : fs::directory_iterator(e.path())) {
                    if (!sub.is_regular_file()) continue;
                    std::string ext = sub.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (!IsMedia(ext)) continue;
                    BgEntry bg;
                    bg.fullPath = sub.path().string();
                    bg.name     = sub.path().stem().string();
                    bg.ext      = ext;
                    bg.folder   = e.path().filename().string();
                    bg.isImage  = IsImage(ext);
                    m_AllBackgrounds.push_back(std::move(bg));
                }
            } else if (e.is_regular_file()) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (!IsMedia(ext)) continue;
                BgEntry bg;
                bg.fullPath = e.path().string();
                bg.name     = e.path().stem().string();
                bg.ext      = ext;
                bg.folder   = "";
                bg.isImage  = IsImage(ext);
                m_AllBackgrounds.push_back(std::move(bg));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[LayersBgTab] " << ex.what() << "\n";
    }
    std::sort(m_BgFolders.begin(), m_BgFolders.end());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operaciones de disco
// ─────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32
bool LayersBgTab::ImportBackground() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg))))
        return false;
    COMDLG_FILTERSPEC fs[] = {
        {L"Video e Imagen", L"*.mp4;*.mkv;*.avi;*.mov;*.jpg;*.jpeg;*.png"},
        {L"Videos",         L"*.mp4;*.mkv;*.avi;*.mov"},
        {L"Imágenes",       L"*.jpg;*.jpeg;*.png"}
    };
    dlg->SetFileTypes(3, fs); dlg->SetFileTypeIndex(1); dlg->SetTitle(L"Importar Fondo");
    FILEOPENDIALOGOPTIONS o = 0; dlg->GetOptions(&o);
    dlg->SetOptions(o | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);

    bool imported = false;
    if (SUCCEEDED(dlg->Show(nullptr))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dlg->GetResults(&items))) {
            DWORD count = 0; items->GetCount(&count);
            ::fs::path dest = BgRootDir();
            if (!m_CurrentBgFolder.empty()) dest = dest / m_CurrentBgFolder;
            std::error_code ec; ::fs::create_directories(dest, ec);
            for (DWORD i = 0; i < count; i++) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &item))) {
                    PWSTR pp = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pp))) {
                        ::fs::path src = pp;
                        ::fs::path dst = dest / src.filename();
                        ::fs::copy_file(src, dst, ::fs::copy_options::overwrite_existing, ec);
                        if (!ec) imported = true;
                        CoTaskMemFree(pp);
                    }
                    item->Release();
                }
            }
            items->Release();
        }
    }
    dlg->Release();
    return imported;
}
#else
bool LayersBgTab::ImportBackground() {
    // En Linux se usa "zenity --file-selection" con selección multiple como
    // reemplazo del dialogo IFileOpenDialog de Windows. Requiere que zenity
    // este instalado en el sistema (paquete "zenity" en la mayoria de las
    // distribuciones).
    std::string command =
        "zenity --file-selection --multiple --separator=\"\\n\" "
        "--file-filter=\"Video e Imagen | *.mp4 *.mkv *.avi *.mov *.jpg *.jpeg *.png\" "
        "--title=\"Importar Fondo\" 2>/dev/null";

    std::string result;
    char buffer[1024];
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;
    int status = pclose(pipe);
    if (status != 0 || result.empty()) return false;

    fs::path dest = BgRootDir();
    if (!m_CurrentBgFolder.empty()) dest = dest / m_CurrentBgFolder;
    std::error_code ec;
    fs::create_directories(dest, ec);

    bool imported = false;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty()) continue;
        fs::path src(line);
        fs::path dst = dest / src.filename();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (!ec) imported = true;
    }
    return imported;
}
#endif

bool LayersBgTab::CreateBgFolder(const std::string& name) {
    if (name.empty()) return false;
    std::error_code ec;
    fs::create_directories(BgRootDir() / name, ec);
    return !ec;
}
bool LayersBgTab::RenameBgFile(const std::string& oldPath, const std::string& newName) {
    if (newName.empty()) return false;
    fs::path src = oldPath;
    fs::path dst = src.parent_path() / (newName + src.extension().string());
    std::error_code ec; fs::rename(src, dst, ec); return !ec;
}
bool LayersBgTab::RenameBgFolder(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || oldName == newName) return false;
    std::error_code ec; fs::rename(BgRootDir()/oldName, BgRootDir()/newName, ec); return !ec;
}
bool LayersBgTab::DeleteBgFile(const std::string& fullPath) {
    std::error_code ec; fs::remove(fs::path(fullPath), ec); return !ec;
}
bool LayersBgTab::DeleteBgFolder(const std::string& folderName) {
    std::error_code ec; fs::remove_all(BgRootDir()/folderName, ec); return !ec;
}
bool LayersBgTab::MoveBgToFolder(const std::string& srcFull, const std::string& destFolder) {
    fs::path src  = srcFull;
    fs::path dest = BgRootDir();
    if (!destFolder.empty()) dest = dest / destFolder;
    dest = dest / src.filename();
    if (src == dest) return false;
    std::error_code ec; fs::rename(src, dest, ec);
    if (ec) { fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec); if (!ec) fs::remove(src, ec); }
    return !ec;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Menus contextuales
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::BgContextMenu(const BgEntry& entry) {
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
    std::string disp = entry.name.length() > 22 ? entry.name.substr(0,19)+"..." : entry.name;
    ImGui::Text("%s", disp.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (ImGui::Selectable("  Mover a Biblioteca (Media)")) {
        std::error_code ec;
        fs::path src(entry.fullPath);
        std::string targetDir = entry.isImage ? (GetAssetsPath() + "/images") : (GetAssetsPath() + "/videos");
        fs::create_directories(targetDir, ec);
        fs::path dst = fs::path(targetDir) / src.filename();
        fs::rename(src, dst, ec);
        ReloadList();
        Library::RefreshMultimediaLists();
    }
    if (ImGui::Selectable("  Copiar a Biblioteca (Media)")) {
        std::error_code ec;
        fs::path src(entry.fullPath);
        std::string targetDir = entry.isImage ? (GetAssetsPath() + "/images") : (GetAssetsPath() + "/videos");
        fs::create_directories(targetDir, ec);
        fs::path dst = fs::path(targetDir) / src.filename();
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        Library::RefreshMultimediaLists();
    }
    ImGui::Separator();

    if (ImGui::Selectable("  Renombrar")) {
        m_RenamingBg    = true;
        m_RenameOldPath = entry.fullPath;
        size_t len = std::min(entry.name.size(), sizeof(m_RenameBuf)-1);
        memcpy(m_RenameBuf, entry.name.c_str(), len); m_RenameBuf[len] = '\0';
    }
    if (!m_BgFolders.empty() && ImGui::BeginMenu("  Mover a carpeta")) {
        if (!entry.folder.empty() && ImGui::MenuItem("  Raiz")) {
            if (MoveBgToFolder(entry.fullPath, "")) ReloadList();
        }
        for (const auto& fn : m_BgFolders) {
            if (fn == entry.folder) continue;
            if (ImGui::MenuItem(fn.c_str()))
                if (MoveBgToFolder(entry.fullPath, fn)) ReloadList();
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
    if (ImGui::Selectable("  Eliminar"))
        if (DeleteBgFile(entry.fullPath)) ReloadList();
    ImGui::PopStyleColor();
}

void LayersBgTab::FolderContextMenu(const std::string& folderName) {
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
    std::string disp = folderName.length()>22 ? folderName.substr(0,19)+"..." : folderName;
    ImGui::Text("%s", disp.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::Selectable("  Renombrar")) {
        m_RenamingFolder  = true;
        m_RenameFolderOld = folderName;
        size_t len = std::min(folderName.size(), sizeof(m_RenameFolderBuf)-1);
        memcpy(m_RenameFolderBuf, folderName.c_str(), len); m_RenameFolderBuf[len]='\0';
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Red);
    if (ImGui::Selectable("  Eliminar (con contenido)")) {
        if (DeleteBgFolder(folderName)) {
            if (m_CurrentBgFolder == folderName) m_CurrentBgFolder.clear();
            ReloadList();
        }
    }
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Modales
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderCreateFolderModal() {
    if (m_CreatingFolder) {
        ImGui::OpenPopup("##NewFolderPop");
        m_CreatingFolder = false;
        memset(m_NewFolderBuf, 0, sizeof(m_NewFolderBuf));
    }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    if (ImGui::BeginPopup("##NewFolderPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
        ImGui::Text("Nueva carpeta");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(200.0f);
        bool confirm = ImGui::InputText("##nf", m_NewFolderBuf, sizeof(m_NewFolderBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus();
        ImGui::Spacing();
        if (LPPrimaryBtn("Crear") || confirm) {
            if (strlen(m_NewFolderBuf)>0 && CreateBgFolder(m_NewFolderBuf)) ReloadList();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

void LayersBgTab::RenderRenameBgModal() {
    if (m_RenamingBg) { ImGui::OpenPopup("##RenameBgPop"); m_RenamingBg = false; }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16,14));
    if (ImGui::BeginPopup("##RenameBgPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Accent);
        ImGui::Text("Renombrar archivo");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(240.0f);
        bool confirm = ImGui::InputText("##rb", m_RenameBuf, sizeof(m_RenameBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus(); ImGui::Spacing();
        if (LPPrimaryBtn("Renombrar") || confirm) {
            if (strlen(m_RenameBuf)>0 && RenameBgFile(m_RenameOldPath, m_RenameBuf)) {
                m_ThumbnailCache.erase(m_RenameOldPath);
                ReloadList();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

void LayersBgTab::RenderRenameFolderModal() {
    if (m_RenamingFolder) { ImGui::OpenPopup("##RenameFolderPop"); m_RenamingFolder = false; }
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f,0.11f,0.14f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16,14));
    if (ImGui::BeginPopup("##RenameFolderPop")) {
        ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
        ImGui::Text("Renombrar carpeta");
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, LP::Surface3);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::SetNextItemWidth(200.0f);
        bool confirm = ImGui::InputText("##rfn", m_RenameFolderBuf, sizeof(m_RenameFolderBuf),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus(); ImGui::Spacing();
        if (LPPrimaryBtn("Renombrar") || confirm) {
            if (strlen(m_RenameFolderBuf)>0) {
                std::string nn(m_RenameFolderBuf);
                if (RenameBgFolder(m_RenameFolderOld, nn)) {
                    if (m_CurrentBgFolder == m_RenameFolderOld) m_CurrentBgFolder = nn;
                    ReloadList();
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0,6);
        if (LPGhostBtn("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Seleccion de carpeta (sidebar)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::SelectFolder(const std::string& folderKey) {
    if (folderKey == m_CurrentBgFolder) return;
    m_CurrentBgFolder   = folderKey;
    m_JustEnteredFolder = true;
    m_ContentFade       = 0.0f; // dispara el fade-in del contenido
}

// ─────────────────────────────────────────────────────────────────────────────
//  Toolbar superior — compacta, solo iconos (estilo ProPresenter/Holyrics)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderTopBar() {
    const std::string title = m_CurrentBgFolder.empty() ? "Todos" : m_CurrentBgFolder;

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
    ImGui::TextUnformatted("Fondos");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
    ImGui::TextUnformatted("\xE2\x80\xBA"); // ›
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Gold);
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopStyleColor();

    // ── Botones a la derecha: zoom, grid/lista, + carpeta, + importar ───────
    const float btnSz = 26.0f;
    const float zoomW = 76.0f;
    const float gap   = 4.0f;
    const float rowW  = zoomW + gap + btnSz*4 + gap*4;
    const float avail = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), avail - rowW));

    if (m_GridMode) {
        LPZoomSlider("##bgzoom", &m_ThumbZoom, 0.65f, 1.8f, zoomW);
        ImGui::SameLine(0, gap);
    } else {
        ImGui::Dummy(ImVec2(zoomW, btnSz));
        ImGui::SameLine(0, gap);
    }

    ImGui::PushID("bgview");
    if (LPCornerIconBtn("##gridm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            float cs = r*0.42f, g = r*0.18f;
            for (int rI=0; rI<2; rI++) for (int cI=0; cI<2; cI++) {
                ImVec2 o = { c.x - cs - g*0.5f + cI*(cs+g), c.y - cs - g*0.5f + rI*(cs+g) };
                dl->AddRectFilled(o, {o.x+cs, o.y+cs}, col, 1.5f);
            }
        }, "Vista en cuadricula", {btnSz,btnSz}, m_GridMode))
        m_GridMode = true;
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##listm", +[](ImDrawList* dl, ImVec2 c, float r, ImU32 col){
            for (int i=0;i<3;i++) {
                float y = c.y - r*0.5f + i*r*0.5f;
                dl->AddRectFilled({c.x-r*0.7f, y}, {c.x+r*0.7f, y+r*0.22f}, col, 1.0f);
            }
        }, "Vista en lista", {btnSz,btnSz}, !m_GridMode))
        m_GridMode = false;
    ImGui::PopID();

    ImGui::SameLine(0, gap*2);
    if (LPCornerIconBtn("##newfolder", LPDrawFolderPlus, "Nueva carpeta", {btnSz,btnSz}))
        m_CreatingFolder = true;
    ImGui::SameLine(0, gap);
    if (LPCornerIconBtn("##import", LPDrawPlus, "Importar fondo", {btnSz,btnSz}, true)) {
        if (ImportBackground()) ReloadList();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sidebar de carpetas (columna izquierda)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderSidebarItem(const std::string& label, const std::string& folderKey,
                                    int count, bool selected, float w) {
    ImGui::PushID(("sfi_"+folderKey).c_str());

    const float rowH = 30.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool hovered = ImGui::IsMouseHoveringRect(pos, {pos.x+w, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovered);

    if (selected) {
        ImVec4 bg = ImVec4(LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.16f);
        dl->AddRectFilled(pos, {pos.x+w, pos.y+rowH}, LPU32(bg), 5.0f);
        dl->AddRectFilled(pos, {pos.x+2.5f, pos.y+rowH}, LPU32(LP::Accent), 2.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(pos, {pos.x+w, pos.y+rowH}, LPU32(ImVec4(1,1,1,0.05f*t)), 5.0f);
    }

    ImU32 iconCol = selected ? LPU32(LP::Accent) : LPU32(ImVec4(LP::TextMuted.x, LP::TextMuted.y, LP::TextMuted.z, 0.55f+0.45f*t));
    LPDrawFolderGlyph(dl, {pos.x + 15.0f, pos.y + rowH*0.5f}, 7.0f, iconCol);

    std::string dn = label.length() > 14 ? label.substr(0,12)+"..." : label;
    ImU32 textCol = selected ? LPU32(LP::Text) : LPU32(ImVec4(LP::TextSub.x, LP::TextSub.y, LP::TextSub.z, 0.75f+0.25f*t));
    dl->AddText({pos.x + 28.0f, pos.y + (rowH - ImGui::GetTextLineHeight())*0.5f}, textCol, dn.c_str());

    if (count > 0) {
        std::string cs = std::to_string(count);
        ImVec2 cSz = ImGui::CalcTextSize(cs.c_str());
        dl->AddText({pos.x + w - cSz.x - 8.0f, pos.y + (rowH - cSz.y)*0.5f}, LPU32(LP::TextMuted), cs.c_str());
    }

    ImGui::InvisibleButton("##sfibtn", {w, rowH});

    if (!folderKey.empty()) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("BG_FILE")) {
                std::string src(static_cast<const char*>(p->Data), p->DataSize-1);
                if (MoveBgToFolder(src, folderKey)) ReloadList();
            }
            ImGui::EndDragDropTarget();
            dl->AddRect(pos, {pos.x+w,pos.y+rowH}, LPU32(LP::Gold), 5.0f, 0, 2.0f);
        }
    } else if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("BG_FILE")) {
            std::string src(static_cast<const char*>(p->Data), p->DataSize-1);
            if (MoveBgToFolder(src, "")) ReloadList();
        }
        ImGui::EndDragDropTarget();
        dl->AddRect(pos, {pos.x+w,pos.y+rowH}, LPU32(LP::Gold), 5.0f, 0, 2.0f);
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SelectFolder(folderKey);

    if (!folderKey.empty() && ImGui::BeginPopupContextItem(("SbCtx_"+folderKey).c_str())) {
        FolderContextMenu(folderKey); ImGui::EndPopup();
    }

    ImGui::Dummy({0, 2.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Selector de carpetas compacto (fila de chips que envuelve) -- usado en
//  vez de RenderFolderSidebar cuando el panel queda angosto (ver Render).
//  Mismos datos y acciones (SelectFolder, mover por drag&drop, menu
//  contextual de carpeta) que la sidebar de 116px, solo que en fila en vez
//  de columna para no robarle ancho al contenido.
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderFolderChips() {
    int rootCount = 0;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder.empty()) rootCount++;

    bool first = true;
    auto chip = [&](const std::string& label, const std::string& folderKey, int count, bool selected) {
        std::string text = count > 0 ? (label + " (" + std::to_string(count) + ")") : label;
        float w = ImGui::CalcTextSize(text.c_str()).x + 20.0f;

        if (!first) {
            if (ImGui::GetContentRegionAvail().x < w) ImGui::NewLine();
            else                                       ImGui::SameLine(0.0f, 6.0f);
        }
        first = false;

        bool sel = selected;
        ImGui::PushStyleColor(ImGuiCol_Button,
            sel ? ImVec4(LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.28f) : ImVec4(1,1,1,0.05f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.20f));
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? LP::Text : LP::TextSub);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        std::string btnId = text + "##fc_" + (folderKey.empty() ? "root" : folderKey);
        bool clicked = ImGui::Button(btnId.c_str(), ImVec2(w, 26.0f));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (!folderKey.empty() && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("BG_FILE")) {
                std::string src(static_cast<const char*>(p->Data), p->DataSize - 1);
                if (MoveBgToFolder(src, folderKey)) ReloadList();
            }
            ImGui::EndDragDropTarget();
        }

        if (clicked) SelectFolder(folderKey);

        if (!folderKey.empty() && ImGui::BeginPopupContextItem(("SbCtxChip_" + folderKey).c_str())) {
            FolderContextMenu(folderKey);
            ImGui::EndPopup();
        }
    };

    chip("Todos", "", rootCount, m_CurrentBgFolder.empty());
    for (const auto& fn : m_BgFolders) {
        int cnt = 0;
        for (const auto& bg : m_AllBackgrounds) if (bg.folder == fn) cnt++;
        chip(fn, fn, cnt, m_CurrentBgFolder == fn);
    }

    ImGui::NewLine();
}

void LayersBgTab::RenderFolderSidebar(float w, float h) {
    (void)h;
    int rootCount = 0;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder.empty()) rootCount++;

    ImGui::Dummy({w, 4.0f});
    RenderSidebarItem("Todos", "", rootCount, m_CurrentBgFolder.empty(), w);

    if (!m_BgFolders.empty()) {
        ImGui::Dummy({w, 4.0f});
        for (const auto& fn : m_BgFolders) {
            int cnt = 0;
            for (const auto& bg : m_AllBackgrounds) if (bg.folder == fn) cnt++;
            RenderSidebarItem(fn, fn, cnt, m_CurrentBgFolder == fn, w);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tarjeta de background (grid)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBgCard(const BgEntry& e, float W, float H, int col, int cols) {
    std::string id = "##bgc_"+e.fullPath;
    ImGui::PushID(id.c_str());

    ImTextureID thumb = GetThumbnail(e.fullPath, !e.isImage);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+H});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Efecto "pop": la tarjeta crece muy sutilmente hacia el cursor al pasar el mouse.
    float inset = 2.0f * t;
    ImVec2 p0 = {pos.x - inset, pos.y - inset};
    ImVec2 p1 = {pos.x + W + inset, pos.y + H + inset};

    dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), 10.0f);
    if (thumb)
        dl->AddImageRounded(thumb, p0, p1, {0,0},{1,1}, IM_COL32_WHITE, 10.0f);

    ImVec4 borderCol = ImVec4(
        LP::Border.x + (LP::Accent.x-LP::Border.x)*t,
        LP::Border.y + (LP::Accent.y-LP::Border.y)*t,
        LP::Border.z + (LP::Accent.z-LP::Border.z)*t,
        LP::Border.w + (0.6f-LP::Border.w)*t);
    dl->AddRect(p0, p1, LPU32(borderCol), 10.0f, 0, 1.0f + 0.8f*t);

    // Chip tipo
    {
        const char* lbl = e.isImage ? "IMG" : "VID";
        ImVec4 chipBg   = e.isImage ? LP::GreenDim : LP::AccentDim;
        ImVec4 chipFg   = e.isImage ? LP::Green    : LP::Accent;
        float bx=p0.x+7.0f, by=p0.y+7.0f;
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddRectFilled({bx,by},{bx+ts.x+8.0f,by+ts.y+4.0f},LPU32(chipBg),4.0f);
        dl->AddText({bx+4.0f,by+2.0f}, LPU32(chipFg), lbl);
    }

    // Nombre
    std::string dn = e.name.length()>18 ? e.name.substr(0,15)+"..." : e.name;
    dl->AddRectFilled({p0.x,p1.y-26.0f},{p1.x,p1.y},
        LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, LPU32(LP::Text), dn.c_str());

    // Overlay de hover: play/imagen
    if (t > 0.02f) {
        const char* icon = e.isImage ? "[ IMG ]" : "[ PLAY ]";
        ImVec2 is = ImGui::CalcTextSize(icon);
        dl->AddRectFilled({p0.x,p0.y},{p1.x,p1.y-26.0f},
            LPU32({0,0,0,0.35f*t}), 10.0f, ImDrawFlags_RoundCornersTop);
        dl->AddText({p0.x+(W-is.x)*0.5f, p0.y+(H-26.0f-is.y)*0.5f},
            LPU32(ImVec4(1,1,1,t)), icon);
    }

    ImGui::InvisibleButton(id.c_str(), {W, H});

    // Drag source
    bool wasDragged = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        wasDragged = true;
        ImGui::SetDragDropPayload("BG_FILE", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::SetDragDropPayload("BG_ITEM_PATH", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::Text("Mover: %s", dn.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    // Mantener presionado (mas de kHoldPreviewThreshold sin soltar) = mostrar
    // el preview grande. Mientras se muestra el preview, soltar NO aplica el
    // fondo (el usuario solo estaba mirando) — aplicar sigue pasando nada
    // mas con un click corto (ver kHoldPreviewThreshold y MouseDownDurationPrev).
    if (!wasDragged && ImGui::IsItemActive()
        && ImGui::GetIO().MouseDownDuration[ImGuiMouseButton_Left] >= kHoldPreviewThreshold) {
        m_HeldPreviewPath    = e.fullPath;
        m_HeldPreviewIsVideo = !e.isImage;
    }

    if (!wasDragged && !m_JustEnteredFolder
        && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        && ImGui::GetIO().MouseDownDurationPrev[ImGuiMouseButton_Left] < kHoldPreviewThreshold)
        // allowAudio=false: esta pestaña es "Fondos" (loops decorativos),
        // nunca deben sonar. Solo "Enviar al monitor" (LibraryVideos) y la
        // cola del Monitor pasan allowAudio=true — este tab no es eso.
        Core::PresentationCore::Get().SetBackgroundMedia(e.fullPath, !e.isImage, /*allowAudio=*/false);

    if (ImGui::BeginPopupContextItem(("BgCtx_"+e.fullPath).c_str())) {
        BgContextMenu(e); ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de background (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBgRow(const BgEntry& e, float W, float rowH) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("bgr_"+e.fullPath).c_str());

    ImTextureID thumb = GetThumbnail(e.fullPath, !e.isImage);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec4 bgCol = ImVec4(
        LP::Surface1.x + (LP::Surface2.x-LP::Surface1.x)*t,
        LP::Surface1.y + (LP::Surface2.y-LP::Surface1.y)*t,
        LP::Surface1.z + (LP::Surface2.z-LP::Surface1.z)*t, 1.0f);
    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH}, LPU32(bgCol), 8.0f);

    if (t > 0.01f)
        dl->AddRectFilled(pos,{pos.x+3.0f,pos.y+rowH}, LPU32(ImVec4(LP::Accent.x,LP::Accent.y,LP::Accent.z,t)), 2.0f);

    float tx=pos.x+8.0f, ty=pos.y+(rowH-thumbSz)*0.5f;
    if (thumb)
        dl->AddImageRounded(thumb,{tx,ty},{tx+thumbSz,ty+thumbSz},{0,0},{1,1},IM_COL32_WHITE,5.0f);
    else {
        dl->AddRectFilled({tx,ty},{tx+thumbSz,ty+thumbSz},LPU32(LP::Surface0),5.0f);
        const char* ic = e.isImage?"IMG":"VID";
        ImVec4 ic4 = e.isImage ? LP::Green : LP::Accent;
        ImVec2 is = ImGui::CalcTextSize(ic);
        dl->AddText({tx+(thumbSz-is.x)*0.5f,ty+(thumbSz-is.y)*0.5f},LPU32(ic4),ic);
    }

    std::string dn = e.name.length()>32 ? e.name.substr(0,29)+"..." : e.name;
    float txtX=tx+thumbSz+10.0f, txtY=pos.y+(rowH-ImGui::GetTextLineHeight())*0.5f;
    dl->AddText({txtX,txtY}, LPU32(LP::Text), dn.c_str());

    // Tag derecha
    const char* tag = e.isImage?"IMG":"VID";
    ImVec4 tagBg  = e.isImage ? LP::GreenDim : LP::AccentDim;
    ImVec4 tagFg  = e.isImage ? LP::Green    : LP::Accent;
    ImVec2 tagSz  = ImGui::CalcTextSize(tag);
    float  tagX   = pos.x+W-tagSz.x-16.0f;
    float  tagY   = pos.y+(rowH-tagSz.y)*0.5f;
    LPBadge(dl, {tagX,tagY}, tag, tagBg, tagFg);

    ImGui::InvisibleButton(("##bgrow_"+e.fullPath).c_str(), {W, rowH});

    bool wasDragged = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        wasDragged = true;
        ImGui::SetDragDropPayload("BG_FILE", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::SetDragDropPayload("BG_ITEM_PATH", e.fullPath.c_str(), e.fullPath.size()+1);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        ImGui::Text("Mover: %s", dn.c_str());
        ImGui::PopStyleColor();
        ImGui::EndDragDropSource();
    }

    if (!wasDragged && ImGui::IsItemActive()
        && ImGui::GetIO().MouseDownDuration[ImGuiMouseButton_Left] >= kHoldPreviewThreshold) {
        m_HeldPreviewPath    = e.fullPath;
        m_HeldPreviewIsVideo = !e.isImage;
    }

    if (!wasDragged && !m_JustEnteredFolder
        && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        && ImGui::GetIO().MouseDownDurationPrev[ImGuiMouseButton_Left] < kHoldPreviewThreshold)
        // allowAudio=false: esta pestaña es "Fondos" (loops decorativos),
        // nunca deben sonar. Solo "Enviar al monitor" (LibraryVideos) y la
        // cola del Monitor pasan allowAudio=true — este tab no es eso.
        Core::PresentationCore::Get().SetBackgroundMedia(e.fullPath, !e.isImage, /*allowAudio=*/false);

    if (ImGui::BeginPopupContextItem(("BgRowCtx_"+e.fullPath).c_str())) {
        BgContextMenu(e); ImGui::EndPopup();
    }

    ImGui::Dummy({0, 5.0f});
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Area de contenido (columna central) — archivos de la carpeta seleccionada
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderContentArea(float w, float h) {
    (void)h;
    // Se limpia al empezar el frame; si alguna tarjeta/fila sigue con el
    // mouse presionado, se vuelve a fijar mas abajo (ver RenderBgCard/Row).
    m_HeldPreviewPath.clear();

    std::vector<const BgEntry*> files;
    for (const auto& bg : m_AllBackgrounds) if (bg.folder == m_CurrentBgFolder) files.push_back(&bg);

    if (files.empty()) {
        ImGui::Dummy({0,16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+w,p.y+64},LPU32(LP::Surface1),10.0f);
        ImGui::Dummy({0,12});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        const char* msg = m_CurrentBgFolder.empty()
            ? "Sin fondos aun. Usa el botón + de arriba para importar."
            : "Esta carpeta esta vacia.";
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

        float availWidth = w;
        int cols = std::max(1, static_cast<int>((availWidth + minGap) / (cW + minGap)));

        // Gap fijo entre tarjetas: antes se estiraba para repartir TODO el
        // ancho sobrante de la fila entre los huecos ya dibujados, asi que
        // con pocos archivos (fila incompleta) el espacio entre tarjetas se
        // inflaba muchisimo. El sobrante ahora queda como margen libre a la
        // derecha/abajo, que es lo esperable en una grilla con pocos items.
        int currentCol = 0;
        for (size_t i = 0; i < files.size(); ++i) {
            RenderBgCard(*files[i], cW, cH, currentCol, cols);
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, minGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        for (const auto* bg : files)
            RenderBgRow(*bg, w, 44.0f);
    }

    if (ImGui::BeginDragDropTarget()) {
        auto HandleDrop = [&](const ImGuiPayload* payload) {
            const char* droppedPath = (const char*)payload->Data;
            if (droppedPath && *droppedPath) {
                std::error_code ec;
                fs::path src(droppedPath);
                fs::path dstFolder = m_CurrentBgFolder.empty() ? BgRootDir() : (BgRootDir() / m_CurrentBgFolder);
                fs::create_directories(dstFolder, ec);
                fs::path dst = dstFolder / src.filename();
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                ReloadList();
            }
        };
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MEDIA_ITEM_PATH")) {
            HandleDrop(payload);
        }
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VIDEO_TO_QUEUE")) {
            HandleDrop(payload);
        }
        ImGui::EndDragDropTarget();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Preview grande al mantener presionado — se dibuja centrado sobre la
//  ventana actual del tab con el foreground draw list (no abre una ventana
//  ImGui nueva: bajo ImGuiConfigFlags_ViewportsEnable, ese Begin/End termina
//  creando su propia ventana de plataforma y no se ve de forma confiable).
//  Solo existe mientras m_HeldPreviewPath no este vacio (ver reset en
//  RenderContentArea y set en RenderBgCard/RenderBgRow). No aplica el fondo:
//  eso sigue pasando solo con un click corto, ver kHoldPreviewThreshold.
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderHoldPreview() {
    if (m_HeldPreviewPath.empty()) return;

    ImTextureID thumb = GetThumbnail(m_HeldPreviewPath, m_HeldPreviewIsVideo);

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSz  = ImGui::GetWindowSize();
    ImVec2 center(winPos.x + winSz.x * 0.5f, winPos.y + winSz.y * 0.5f);

    ImVec2 sz(std::min(480.0f, winSz.x * 0.7f), 0.0f);
    sz.y = std::min(sz.x * 9.0f / 16.0f, winSz.y * 0.7f);
    ImVec2 p0 = { center.x - sz.x * 0.5f, center.y - sz.y * 0.5f };
    ImVec2 p1 = { center.x + sz.x * 0.5f, center.y + sz.y * 0.5f };

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled({p0.x-4.0f, p0.y-4.0f}, {p1.x+4.0f, p1.y+4.0f}, IM_COL32(6,6,9,235), 14.0f);
    if (thumb) {
        dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32_WHITE, 10.0f);
    } else {
        // Sin miniatura todavia (se esta generando en 2do plano, o no se
        // pudo decodificar el video): mismo fallback que la tarjeta chica.
        dl->AddRectFilled(p0, p1, LPU32(LP::Surface1), 10.0f);
        const char* lbl = m_HeldPreviewIsVideo ? "VID" : "IMG";
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddText({p0.x + (sz.x-ts.x)*0.5f, p0.y + (sz.y-ts.y)*0.5f}, LPU32(LP::TextMuted), lbl);
    }
    dl->AddRect(p0, p1, LPU32(LP::BorderHov), 10.0f, 0, 1.5f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Render principal del tab — layout horizontal tipo ProPresenter:
//  sidebar de carpetas a la izquierda + contenido al centro.
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::Render() {
    // Subir a textura GL las miniaturas de video que el worker en 2do plano
    // haya terminado de decodificar desde el frame anterior — tiene que
    // pasar por aca (hilo con contexto GL), no por el worker.
    DrainThumbnailResults();

    // Limpiar flag de proteccion contra autoclick UNA vez al inicio del frame
    m_JustEnteredFolder = false;
    m_ContentFade = LPApproach(m_ContentFade, 1.0f, 9.0f);

    RenderTopBar();

    // Aviso chico y no-bloqueante: mientras el worker todavia tiene videos
    // por decodificar, se avisa sin frenar nada (las tarjetas ya visibles
    // siguen andando, esto es solo informativo).
    if (size_t pending = m_ThumbWorker.PendingCount(); pending > 0) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::Text("Generando miniaturas... (%zu restantes)", pending);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    LPSeparatorLine();

    const float totalW = ImGui::GetContentRegionAvail().x;
    const float totalH = ImGui::GetContentRegionAvail().y;

    // Columna angosta (ej. "Diseño" en Ajustes > Apariencia > Entorno de
    // trabajo > Simple): la sidebar fija de 116px le restaba demasiado ancho
    // al contenido -- se reemplaza por una fila de chips que envuelve arriba
    // del contenido (ver RenderFolderChips), que ocupa solo lo que necesita.
    const bool narrow = totalW < 300.0f;

    if (narrow)
    {
        RenderFolderChips();
        ImGui::Spacing();

        ImGui::BeginChild("##bgContent", ImVec2(0, 0), false);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * m_ContentFade);
        RenderContentArea(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    else
    {
        const float sidebarW = 116.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
        ImGui::BeginChild("##bgSidebar", ImVec2(sidebarW, totalH), false,
                          ImGuiWindowFlags_NoScrollbar);
        RenderFolderSidebar(sidebarW, totalH);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine();
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine({p.x, p.y}, {p.x, p.y+totalH}, LPU32(LP::Border), 1.0f);
            ImGui::Dummy(ImVec2(1.0f, totalH));
        }
        ImGui::SameLine();

        ImGui::BeginChild("##bgContent", ImVec2(0, totalH), false);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * m_ContentFade);
        RenderContentArea(ImGui::GetContentRegionAvail().x, totalH);
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    RenderHoldPreview();

    // Modales (abrir popups debe ir fuera de los InvisibleButtons)
    RenderCreateFolderModal();
    RenderRenameBgModal();
    RenderRenameFolderModal();
}

} // namespace ProyecThor::UI
