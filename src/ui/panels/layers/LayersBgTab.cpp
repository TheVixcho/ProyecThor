#include "LayersBgTab.h"
#include "LayersTheme.h"
#include "core/PresentationCore.h"
#include "core/FileDeletionManager.h"
#include "core/MediaImportManager.h"
#include "core/AppPaths.h"
#include "SettingsManager.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/framework/FilePicker.h"
#include "ui/framework/bin/StyleGeneralApp.h"
#include <atomic>
#include <unordered_set>
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
#include <thread>
#include <chrono>
#include "stb_image.h"

namespace fs = std::filesystem;
namespace ProyecThor::UI {

static std::atomic<uint64_t> s_BgListRevision{1};

void BumpBackgroundsListRevision() {
    s_BgListRevision.fetch_add(1, std::memory_order_relaxed);
}

uint64_t GetBackgroundsListRevision() {
    return s_BgListRevision.load(std::memory_order_relaxed);
}

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
    return ext==".mp4"||ext==".mkv"||ext==".avi"||ext==".mov"||ext==".webm"||ext==".m4v"
          ||ext==".jpg"||ext==".jpeg"||ext==".png"||ext==".bmp"||ext==".webp";
}
static bool IsImage(const std::string& ext) {
    return ext==".jpg"||ext==".jpeg"||ext==".png"||ext==".bmp"||ext==".webp";
}

// Cuanto hay que mantener presionado antes de que cuente como "hold" (mostrar
// preview) en vez de un click normal. 0.45s evita que clicks humanos normales
// (150-350ms) sean tragados accidentalmente como "hold preview".
static constexpr float kHoldPreviewThreshold = 0.45f;

// Cache global de miniaturas en GPU compartido entre todas las instancias de
// LayersBgTab (StylesHubPanel y BackgroundsPanel), de modo que la precarga del
// SplashScreen quede disponible de inmediato en todos los paneles.
static std::unordered_map<std::string, ImTextureID>& SharedBgThumbnailCache() {
    static std::unordered_map<std::string, ImTextureID> s_Cache;
    return s_Cache;
}

static void EraseSharedBgThumbnail(const std::string& path) {
    auto& cache = SharedBgThumbnailCache();
    auto it = cache.find(path);
    if (it != cache.end()) {
        GLuint tex = static_cast<GLuint>(static_cast<uintptr_t>(it->second));
        if (tex != 0) glDeleteTextures(1, &tex);
        cache.erase(it);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Thumbnails
// ─────────────────────────────────────────────────────────────────────────────
static ImTextureID LoadImageThumb(const char* path);

// Miniaturas de video: se generan en 2do plano via ThumbnailWorker (ver
// core/ThumbnailWorker.h) — decodifica el primer frame real con
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
    auto sz = fs::file_size(Library::U8Path(absVideoPath), ec);
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return Library::PathToUtf8(ThumbCacheDir() / (std::to_string(h) + ".png"));
}

static ImTextureID LoadImageThumb(const char* path) {
    std::vector<uint8_t> pixels;
    unsigned w = 0, h = 0;
    if (!Core::ThumbnailWorker::LoadThumbnailPixelsFromDisk(path ? path : "", pixels, w, h, 512, 288))
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return (ImTextureID)(intptr_t)tex;
}

ImTextureID LayersBgTab::GetThumbnail(const std::string& path, bool isVideo) {
    auto& cache = SharedBgThumbnailCache();
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;

    std::error_code absEc;
    fs::path absFs = fs::absolute(Library::U8Path(path), absEc);
    std::string abs = absEc ? path : Library::PathToUtf8(absFs);

    if (!isVideo) {
        ImTextureID t = LoadImageThumb(abs.c_str());
        cache[path] = t;
        return t;
    }

    // Video: si ya se genero en una sesion anterior, esta en el cache de
    // disco — cargarlo de ahi es instantaneo (es una imagen mas, via
    // LoadImageThumb) y no necesita tocar el worker para nada.
    std::string cachePath = ThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(Library::U8Path(cachePath), ec)) {
        ImTextureID t = LoadImageThumb(cachePath.c_str());
        if (t) { cache[path] = t; return t; }
    }

    // Todavia no existe: se pide en 2do plano (ThumbnailWorker, sin abrir
    // ninguna ventana ni trabar la UI) y por ahora se deja SIN entrar en
    // el cache — asi el proximo frame vuelve a preguntar y, cuando
    // el worker termine, DrainThumbnailResults() ya habra puesto el
    // resultado real ahi. Mientras tanto la tarjeta cae en el icono
    // generico "VID" (texture id 0).
    m_ThumbWorker.Request(path, abs, cachePath);
    return 0;
}

// Llamar una vez por frame desde Render(): sube a textura GL los frames que
// el worker haya terminado de decodificar desde el ultimo frame.
void LayersBgTab::DrainThumbnailResults() {
    auto& cache = SharedBgThumbnailCache();
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
        cache[r.key] = t;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / ReloadList
// ─────────────────────────────────────────────────────────────────────────────
LayersBgTab::LayersBgTab() {
    ReloadList();
}

void LayersBgTab::ReloadList() {
    m_SeenBgRevision = GetBackgroundsListRevision();
    m_AllBackgrounds.clear();
    m_BgFolders.clear();

    std::unordered_set<std::string> seenPaths;
    std::unordered_set<std::string> seenNames;

    auto TryPushEntry = [&](BgEntry&& bg) {
        std::string pathKey = Core::MediaImportManager::NormalizePathKey(bg.fullPath);
        std::string nameKey = Core::MediaImportManager::NormalizeFilenameKey(bg.name + bg.ext);
        if (nameKey.empty()) return;
        if (!pathKey.empty() && seenPaths.count(pathKey) > 0) return;
        if (seenNames.count(nameKey) > 0) return;

        if (!pathKey.empty()) seenPaths.insert(pathKey);
        seenNames.insert(nameKey);
        m_AllBackgrounds.push_back(std::move(bg));
    };

    fs::path root = BgRootDir();
    try {
        for (const auto& e : fs::directory_iterator(root)) {
            if (e.is_directory()) {
                std::string folderName = Library::PathToUtf8(e.path().filename());
                m_BgFolders.push_back(folderName);
                for (const auto& sub : fs::directory_iterator(e.path())) {
                    if (!sub.is_regular_file()) continue;
                    std::string ext = sub.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                                   [](unsigned char c) { return (char)std::tolower(c); });
                    if (!IsMedia(ext)) continue;
                    BgEntry bg;
                    bg.fullPath   = Library::PathToUtf8(sub.path());
                    bg.name       = Library::PathToUtf8(sub.path().stem());
                    bg.ext        = ext;
                    bg.folder     = folderName;
                    bg.isImage    = IsImage(ext);
                    bg.isExternal = false;
                    TryPushEntry(std::move(bg));
                }
            } else if (e.is_regular_file()) {
                std::string ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
                if (!IsMedia(ext)) continue;
                BgEntry bg;
                bg.fullPath   = Library::PathToUtf8(e.path());
                bg.name       = Library::PathToUtf8(e.path().stem());
                bg.ext        = ext;
                bg.folder     = "";
                bg.isImage    = IsImage(ext);
                bg.isExternal = false;
                TryPushEntry(std::move(bg));
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[LayersBgTab] " << ex.what() << "\n";
    }

    // Incluir fondos externos vinculados sin copiar (ej. desde Descargas)
    const auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
    for (const auto& rawEntry : extBgs) {
        std::string extFolder, extPath;
        Core::MediaImportManager::ParseExternalBgEntry(rawEntry, extFolder, extPath);
        if (extPath.empty()) continue;

        std::error_code ec;
        fs::path p = Library::U8Path(extPath);
        if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) continue;

        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (!IsMedia(ext)) continue;

        if (!extFolder.empty() &&
            std::find(m_BgFolders.begin(), m_BgFolders.end(), extFolder) == m_BgFolders.end()) {
            m_BgFolders.push_back(extFolder);
        }

        BgEntry bg;
        bg.fullPath   = Library::PathToUtf8(p);
        bg.name       = Library::PathToUtf8(p.stem());
        bg.ext        = ext;
        bg.folder     = extFolder;
        bg.isImage    = IsImage(ext);
        bg.isExternal = true;
        TryPushEntry(std::move(bg));
    }

    std::sort(m_BgFolders.begin(), m_BgFolders.end());
    m_BgFolders.erase(std::unique(m_BgFolders.begin(), m_BgFolders.end()), m_BgFolders.end());
}

void LayersBgTab::PreloadThumbnails(
    const std::function<void(int done, int total, const std::string& name)>& onProgress)
{
    const int total = static_cast<int>(m_AllBackgrounds.size());
    if (total == 0) {
        if (onProgress) onProgress(0, 0, "");
        return;
    }

    int done = 0;
    int pendingVideos = 0;
    std::string currentVideoLabel;

    auto& cache = SharedBgThumbnailCache();

    // 1. Videos primero: carga instantánea desde caché en disco o encolado
    //    temprano en ThumbnailWorker mientras en el paso 2 cargamos las imágenes.
    for (const auto& bg : m_AllBackgrounds) {
        if (bg.isImage) continue;
        if (cache.find(bg.fullPath) != cache.end()) {
            ++done;
            continue;
        }
        (void)GetThumbnail(bg.fullPath, true);
        if (cache.find(bg.fullPath) != cache.end()) {
            ++done;
            if (onProgress) onProgress(done, total, bg.name + bg.ext);
        } else {
            ++pendingVideos;
            if (currentVideoLabel.empty())
                currentVideoLabel = bg.name + bg.ext;
        }
    }

    // 2. Imágenes de fondo
    for (const auto& bg : m_AllBackgrounds) {
        if (!bg.isImage) continue;
        if (onProgress) onProgress(done, total, bg.name + bg.ext);
        (void)GetThumbnail(bg.fullPath, false);
        ++done;
        if (onProgress) onProgress(done, total, bg.name + bg.ext);
    }

    // 3. Esperar y subir los fotogramas de videos de fondo decodificados
    if (pendingVideos > 0 || m_ThumbWorker.PendingCount() > 0) {
        auto lastAdvance = std::chrono::steady_clock::now();
        size_t prevCacheSz = cache.size();

        while (true) {
            DrainThumbnailResults();
            size_t nowCacheSz = cache.size();
            if (nowCacheSz > prevCacheSz) {
                done = std::min(total, done + static_cast<int>(nowCacheSz - prevCacheSz));
                prevCacheSz = nowCacheSz;
                lastAdvance = std::chrono::steady_clock::now();
            }

            if (onProgress)
                onProgress(std::min(done, total), total, currentVideoLabel);

            if (m_ThumbWorker.PendingCount() == 0) {
                DrainThumbnailResults();
                break;
            }

            auto stalledMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - lastAdvance).count();
            if (stalledMs > 3000)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (onProgress)
        onProgress(total, total, "");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operaciones de disco
// ─────────────────────────────────────────────────────────────────────────────
bool LayersBgTab::ImportBackground() {
    Core::MediaImportManager::Get().OpenFilePickerForImport(
        Core::MediaImportDest::Background, m_CurrentBgFolder);
    return false;
}

bool LayersBgTab::CreateBgFolder(const std::string& name) {
    if (name.empty()) return false;
    std::error_code ec;
    fs::create_directories(BgRootDir() / Library::U8Path(name), ec);
    if (!ec) BumpBackgroundsListRevision();
    return !ec;
}
bool LayersBgTab::RenameBgFile(const std::string& oldPath, const std::string& newName) {
    if (newName.empty()) return false;
    if (Core::MediaImportManager::Get().IsExternalBackgroundPath(oldPath)) return false;
    fs::path src = Library::U8Path(oldPath);
    fs::path dst = src.parent_path() / Library::U8Path(newName + src.extension().string());
    std::error_code ec; fs::rename(src, dst, ec);
    if (!ec) BumpBackgroundsListRevision();
    return !ec;
}
bool LayersBgTab::RenameBgFolder(const std::string& oldName, const std::string& newName) {
    if (newName.empty() || oldName == newName) return false;
    std::error_code ec;
    fs::path srcDir = BgRootDir() / Library::U8Path(oldName);
    if (fs::exists(srcDir, ec)) {
        fs::rename(srcDir, BgRootDir() / Library::U8Path(newName), ec);
    }
    auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
    bool updatedExt = false;
    for (auto& raw : extBgs) {
        std::string f, p;
        Core::MediaImportManager::ParseExternalBgEntry(raw, f, p);
        if (f == oldName) {
            raw = Core::MediaImportManager::FormatExternalBgEntry(newName, p);
            updatedExt = true;
        }
    }
    if (updatedExt) ProyecThor::Settings::SettingsManager::Get().SaveSettings();
    BumpBackgroundsListRevision();
    return !ec || updatedExt;
}
bool LayersBgTab::DeleteBgFile(const std::string& fullPath) {
    EraseSharedBgThumbnail(fullPath);
    if (Core::MediaImportManager::Get().IsExternalBackgroundPath(fullPath)) {
        return Core::MediaImportManager::Get().UnlinkExternalBackground(fullPath);
    }
    bool ok = Core::FileDeletionManager::ForceDeleteFile(fullPath);
    if (ok) BumpBackgroundsListRevision();
    return ok;
}
bool LayersBgTab::DeleteBgFolder(const std::string& folderName) {
    auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
    size_t before = extBgs.size();
    extBgs.erase(std::remove_if(extBgs.begin(), extBgs.end(), [&](const std::string& raw) {
        std::string f, p;
        Core::MediaImportManager::ParseExternalBgEntry(raw, f, p);
        return f == folderName;
    }), extBgs.end());
    if (extBgs.size() != before) {
        ProyecThor::Settings::SettingsManager::Get().SaveSettings();
    }
    bool ok = Core::FileDeletionManager::ForceDeleteDirectory(Library::PathToUtf8(BgRootDir() / Library::U8Path(folderName)));
    BumpBackgroundsListRevision();
    return ok || (extBgs.size() != before);
}
bool LayersBgTab::MoveBgToFolder(const std::string& srcFull, const std::string& destFolder) {
    if (Core::MediaImportManager::Get().IsExternalBackgroundPath(srcFull)) {
        std::string normSrc = Core::MediaImportManager::NormalizePathKey(srcFull);
        auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
        bool moved = false;
        for (auto& raw : extBgs) {
            std::string f, p;
            Core::MediaImportManager::ParseExternalBgEntry(raw, f, p);
            if (Core::MediaImportManager::NormalizePathKey(p) == normSrc) {
                raw = Core::MediaImportManager::FormatExternalBgEntry(destFolder, p);
                moved = true;
            }
        }
        if (moved) {
            ProyecThor::Settings::SettingsManager::Get().SaveSettings();
            BumpBackgroundsListRevision();
        }
        return moved;
    }

    fs::path src  = Library::U8Path(srcFull);
    fs::path dest = BgRootDir();
    if (!destFolder.empty()) dest = dest / Library::U8Path(destFolder);
    std::error_code mkEc;
    fs::create_directories(dest, mkEc);
    dest = dest / src.filename();
    if (src == dest) return false;
    std::error_code ec; fs::rename(src, dest, ec);
    if (ec) { fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec); if (!ec) fs::remove(src, ec); }
    if (!ec) {
        EraseSharedBgThumbnail(srcFull);
        BumpBackgroundsListRevision();
    }
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

    const std::string fullFilename = entry.name + entry.ext;
    if (ImGui::Selectable("  Mover a Biblioteca (Media)")) {
        if (entry.isExternal) {
            Core::MediaImportManager::Get().MoveExternalToMultimedia(entry.fullPath);
            ReloadList();
        } else {
            std::error_code ec;
            fs::path src = Library::U8Path(entry.fullPath);
            if (!Core::MediaImportManager::Get().ExistsInMultimediaByName(fullFilename)) {
                std::string targetDir = entry.isImage ? (GetAssetsPath() + "/images") : Library::GetVideoLibraryPath();
                fs::create_directories(Library::U8Path(targetDir), ec);
                fs::path dst = Library::U8Path(targetDir) / src.filename();
                fs::rename(src, dst, ec);
            } else {
                Core::FileDeletionManager::ForceDeleteFile(entry.fullPath);
            }
            BumpBackgroundsListRevision();
            ReloadList();
            Library::RefreshMultimediaLists();
        }
    }
    if (ImGui::Selectable("  Copiar a Biblioteca (Media)")) {
        if (entry.isExternal) {
            Core::MediaImportManager::Get().LinkSingleExternalFileDirect(
                entry.fullPath, Core::MediaImportDest::Multimedia);
        } else if (!Core::MediaImportManager::Get().ExistsInMultimediaByName(fullFilename)) {
            std::error_code ec;
            fs::path src = Library::U8Path(entry.fullPath);
            std::string targetDir = entry.isImage ? (GetAssetsPath() + "/images") : Library::GetVideoLibraryPath();
            fs::create_directories(Library::U8Path(targetDir), ec);
            fs::path dst = Library::U8Path(targetDir) / src.filename();
            fs::copy_file(src, dst, fs::copy_options::skip_existing, ec);
            Library::RefreshMultimediaLists();
        }
    }
    ImGui::Separator();

    if (!entry.isExternal && ImGui::Selectable("  Renombrar")) {
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
    const char* delLabel = entry.isExternal ? "  Quitar vínculo" : "  Eliminar";
    if (ImGui::Selectable(delLabel))
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
                EraseSharedBgThumbnail(m_RenameOldPath);
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
    const float initCursorX = ImGui::GetCursorPosX();
    const float availW      = ImGui::GetContentRegionAvail().x;
    const float btnSz       = 28.0f;
    const float zoomW       = 78.0f;
    const float gap         = 6.0f;
    const float padX        = 14.0f; // Margen lateral izquierdo y derecho

    float collapseBtnW   = m_OnCollapseCallback ? 92.0f : 0.0f;
    float rightControlsW = zoomW + btnSz * 4.0f + gap * 5.0f +
                           (m_OnCollapseCallback ? (collapseBtnW + gap * 2.0f) : 0.0f);

    const float startX          = initCursorX + padX;
    const float effectiveAvailW = std::max(0.0f, availW - padX * 2.0f);

    // ── 1. Buscador estilo Librería (DS::BtnDefaultFill, DS::RadiusMedium) ──
    float searchW = std::clamp(effectiveAvailW - rightControlsW - 130.0f, 130.0f, 240.0f);

    ImGui::SetCursorPosX(startX);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  LP::AccentDim);
    ImGui::PushStyleColor(ImGuiCol_Border,         LP::Border);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(searchW);
    ImGui::InputTextWithHint("##bgSearch", "Buscar fondo...", m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    // Ícono de búsqueda superpuesto si hay ícono disponible
    if (StyleGeneralApp::Icons.count("search") > 0 && StyleGeneralApp::Icons["search"].textureID) {
        ImVec2 iMin = ImGui::GetItemRectMin();
        ImVec2 iMax = ImGui::GetItemRectMax();
        float bH = iMax.y - iMin.y;
        float pad = 5.0f;
        ImGui::GetWindowDrawList()->AddImage(
            StyleGeneralApp::Icons["search"].textureID,
            { iMax.x - bH + pad, iMin.y + pad },
            { iMax.x - pad,       iMax.y - pad },
            { 0, 0 }, { 1, 1 }, LPU32(LP::TextMuted));
    }

    // ── 2. Carpeta actual (sin repetir "Fondos", sólo el nombre de la carpeta) ──
    ImGui::SameLine(0, 8.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LP::Text);
    ImGui::Text("📁 %s", title.c_str());
    ImGui::PopStyleColor();

    // ── 3. Controles a la derecha (Zoom, Grid/List, +Carpeta, +Importar, Colapsar) ──
    const float targetRightX   = initCursorX + availW - padX - rightControlsW;
    const float rightControlsX = std::max(ImGui::GetCursorPosX(), targetRightX);
    ImGui::SameLine(rightControlsX);

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
        }, "Vista en cuadrícula", {btnSz,btnSz}, m_GridMode))
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
        ImportBackground();
    }

    if (m_OnCollapseCallback) {
        ImGui::SameLine(0, gap*2);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 5.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, LP::Surface2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LP::Surface3);
        ImGui::PushStyleColor(ImGuiCol_Border, LP::Border);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextSub);
        if (ImGui::Button("\xE2\x96\xBC Colapsar", ImVec2(collapseBtnW, btnSz))) {
            m_OnCollapseCallback();
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(3);
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

    // Fondo y barra de acento estilo Librería (DS::RowSelectedFill / DS::RowSelectedBar)
    ImU32 bgCol = selected ? DS::RowSelectedFill : (hovered ? DS::RowHoverFill : 0);
    if (bgCol) {
        dl->AddRectFilled(pos, {pos.x + w, pos.y + rowH}, bgCol, DS::RadiusSmall);
    }
    if (selected) {
        dl->AddRectFilled(pos, {pos.x + 3.0f, pos.y + rowH}, DS::RowSelectedBar, 1.5f);
    }

    // Ícono de carpeta
    ImU32 iconCol = selected ? DS::RowSelectedBar : (hovered ? LPU32(LP::Text) : LPU32(LP::TextMuted));
    LPDrawFolderGlyph(dl, {pos.x + 14.0f, pos.y + rowH * 0.5f}, 6.5f, iconCol);

    // Texto con elipse dinámica
    float rightReserved = 8.0f;
    if (count > 0) {
        std::string cs = std::to_string(count);
        rightReserved += ImGui::CalcTextSize(cs.c_str()).x + 12.0f;
    }
    float maxTextW = std::max(20.0f, w - 26.0f - rightReserved);

    std::string dn = label;
    if (ImGui::CalcTextSize(dn.c_str()).x > maxTextW) {
        while (dn.length() > 3 && ImGui::CalcTextSize((dn + "...").c_str()).x > maxTextW) {
            dn.pop_back();
        }
        dn += "...";
    }

    ImU32 textCol = selected ? DS::TextPrimary : (hovered ? LPU32(LP::Text) : LPU32(LP::TextSub));
    dl->AddText({pos.x + 26.0f, pos.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f}, textCol, dn.c_str());

    // Badge píldora con contador
    if (count > 0) {
        std::string cs = std::to_string(count);
        ImVec2 cSz = ImGui::CalcTextSize(cs.c_str());
        float badgeW = cSz.x + 10.0f;
        float badgeH = 17.0f;
        ImVec2 badgePos = { pos.x + w - badgeW - 6.0f, pos.y + (rowH - badgeH) * 0.5f };
        dl->AddRectFilled(badgePos, { badgePos.x + badgeW, badgePos.y + badgeH },
                          selected ? LPU32(LP::Surface2) : LPU32(LP::Surface1), 4.0f);
        dl->AddText({ badgePos.x + 5.0f, badgePos.y + (badgeH - cSz.y) * 0.5f },
                    selected ? LPU32(LP::Text) : LPU32(LP::TextMuted), cs.c_str());
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

    int totalCount = static_cast<int>(m_AllBackgrounds.size());
    chip("Todos", "", totalCount, m_CurrentBgFolder.empty());
    for (const auto& fn : m_BgFolders) {
        int cnt = 0;
        for (const auto& bg : m_AllBackgrounds) if (bg.folder == fn) cnt++;
        chip(fn, fn, cnt, m_CurrentBgFolder == fn);
    }

    ImGui::NewLine();
}

void LayersBgTab::RenderFolderSidebar(float w, float h) {
    (void)h;
    int totalCount = static_cast<int>(m_AllBackgrounds.size());

    RenderSidebarItem("Todos", "", totalCount, m_CurrentBgFolder.empty(), w);

    if (!m_BgFolders.empty()) {
        ImGui::Dummy({w, 2.0f});
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
void LayersBgTab::RenderBgCard(const BgEntry& e, float W, float H, int col, int cols, bool isActive) {
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

    float bt = isActive ? 1.0f : t;
    ImVec4 borderTarget = isActive ? LP::Green : LP::Accent;
    ImVec4 borderCol = ImVec4(
        LP::Border.x + (borderTarget.x-LP::Border.x)*bt,
        LP::Border.y + (borderTarget.y-LP::Border.y)*bt,
        LP::Border.z + (borderTarget.z-LP::Border.z)*bt,
        LP::Border.w + ((isActive ? 0.95f : 0.6f)-LP::Border.w)*bt);
    dl->AddRect(p0, p1, LPU32(borderCol), 10.0f, 0, isActive ? 2.2f : (1.0f + 0.8f*t));

    // Chip tipo
    {
        const char* lbl = e.isImage ? "IMG" : "VID";
        ImVec4 chipBg   = e.isImage ? LP::GreenDim : LP::AccentDim;
        ImVec4 chipFg   = e.isImage ? LP::Green    : LP::Accent;
        float bx=p0.x+7.0f, by=p0.y+7.0f;
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddRectFilled({bx,by},{bx+ts.x+8.0f,by+ts.y+4.0f},LPU32(chipBg),4.0f);
        dl->AddText({bx+4.0f,by+2.0f}, LPU32(chipFg), lbl);

        if (isActive) {
            const char* liveLbl = "ACTIVO";
            ImVec2 lts = ImGui::CalcTextSize(liveLbl);
            float lx = p1.x - lts.x - 15.0f;
            dl->AddRectFilled({lx, by}, {lx + lts.x + 8.0f, by + lts.y + 4.0f}, LPU32(LP::GreenDim), 4.0f);
            dl->AddText({lx + 4.0f, by + 2.0f}, LPU32(LP::Green), liveLbl);
        }
    }

    // Nombre
    std::string dn = e.name.length()>18 ? e.name.substr(0,15)+"..." : e.name;
    dl->AddRectFilled({p0.x,p1.y-26.0f},{p1.x,p1.y},
        LPU32({0,0,0,0.78f}), 10.0f, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x+(W-ns.x)*0.5f, p1.y-21.0f}, LPU32(LP::Text), dn.c_str());

    // Overlay de hover: play/imagen
    if (t > 0.02f) {
        ImVec2 center = { (p0.x + p1.x) * 0.5f, p0.y + (H - 26.0f) * 0.5f };
        dl->AddCircleFilled(center, 18.0f, IM_COL32(0, 0, 0, (int)(170.0f * t)), 24);
        dl->AddCircle(center, 18.0f, LPU32({ LP::Accent.x, LP::Accent.y, LP::Accent.z, 0.7f * t }), 24, 1.3f);
        if (e.isImage) {
            LPDrawImage(dl, center, 11.0f, LPU32(ImVec4(1, 1, 1, t)));
        } else {
            LPDrawPlay(dl, center, 12.0f, LPU32(ImVec4(1, 1, 1, t)));
        }
    }

    const bool clicked = ImGui::InvisibleButton(id.c_str(), {W, H});

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

    const ImGuiIO& io = ImGui::GetIO();
    const bool wasRealDrag = wasDragged || (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > 256.0f);

    // Mantener presionado (mas de kHoldPreviewThreshold sin soltar) = mostrar
    // el preview grande. Mientras se muestra el preview, soltar NO aplica el
    // fondo (el usuario solo estaba mirando) — aplicar sigue pasando nada
    // mas con un click corto (ver kHoldPreviewThreshold y MouseDownDurationPrev).
    if (!wasRealDrag && ImGui::IsItemActive()
        && io.MouseDownDuration[ImGuiMouseButton_Left] >= kHoldPreviewThreshold) {
        m_HeldPreviewPath    = e.fullPath;
        m_HeldPreviewIsVideo = !e.isImage;
    }

    if (clicked && !wasRealDrag && !m_JustEnteredFolder
        && io.MouseDownDurationPrev[ImGuiMouseButton_Left] < kHoldPreviewThreshold) {
        // allowAudio=false: esta pestaña es "Fondos" (loops decorativos),
        // nunca deben sonar. Activa proyeccion en vivo de inmediato para que
        // el primer fondo se vea en el primer click sin pasos adicionales.
        auto& core = Core::PresentationCore::Get();
        core.SetProjecting(true);
        core.SetBackgroundMedia(e.fullPath, !e.isImage, /*allowAudio=*/false);
    }

    if (ImGui::BeginPopupContextItem(("BgCtx_"+e.fullPath).c_str())) {
        BgContextMenu(e); ImGui::EndPopup();
    }

    if (col < cols-1) ImGui::SameLine();
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fila de background (lista)
// ─────────────────────────────────────────────────────────────────────────────
void LayersBgTab::RenderBgRow(const BgEntry& e, float W, float rowH, bool isActive) {
    const float thumbSz = 38.0f;
    ImGui::PushID(("bgr_"+e.fullPath).c_str());

    ImTextureID thumb = GetThumbnail(e.fullPath, !e.isImage);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x+W, pos.y+rowH});
    float t = LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float bt = isActive ? 1.0f : t;
    ImVec4 bgCol = ImVec4(
        LP::Surface1.x + (LP::Surface2.x-LP::Surface1.x)*bt,
        LP::Surface1.y + (LP::Surface2.y-LP::Surface1.y)*bt,
        LP::Surface1.z + (LP::Surface2.z-LP::Surface1.z)*bt, 1.0f);
    dl->AddRectFilled(pos, {pos.x+W,pos.y+rowH}, LPU32(bgCol), 8.0f);

    if (isActive) {
        dl->AddRectFilled(pos, {pos.x+3.5f, pos.y+rowH}, LPU32(LP::Green), 2.0f);
        dl->AddRect(pos, {pos.x+W, pos.y+rowH}, LPU32(ImVec4(LP::Green.x, LP::Green.y, LP::Green.z, 0.7f)), 8.0f, 0, 1.4f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(pos,{pos.x+3.0f,pos.y+rowH}, LPU32(ImVec4(LP::Accent.x,LP::Accent.y,LP::Accent.z,t)), 2.0f);
    }

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

    const bool clicked = ImGui::InvisibleButton(("##bgrow_"+e.fullPath).c_str(), {W, rowH});

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

    const ImGuiIO& io = ImGui::GetIO();
    const bool wasRealDrag = wasDragged || (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > 256.0f);

    if (!wasRealDrag && ImGui::IsItemActive()
        && io.MouseDownDuration[ImGuiMouseButton_Left] >= kHoldPreviewThreshold) {
        m_HeldPreviewPath    = e.fullPath;
        m_HeldPreviewIsVideo = !e.isImage;
    }

    if (clicked && !wasRealDrag && !m_JustEnteredFolder
        && io.MouseDownDurationPrev[ImGuiMouseButton_Left] < kHoldPreviewThreshold) {
        // allowAudio=false: esta pestaña es "Fondos" (loops decorativos),
        // nunca deben sonar. Activa proyeccion en vivo de inmediato para que
        // el primer fondo se vea en el primer click sin pasos adicionales.
        auto& core = Core::PresentationCore::Get();
        core.SetProjecting(true);
        core.SetBackgroundMedia(e.fullPath, !e.isImage, /*allowAudio=*/false);
    }

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

    const auto presState = Core::PresentationCore::Get().GetState();
    const bool hasActiveLiveBg = presState.isProjecting &&
        (presState.bgType == Core::PresentationState::BackgroundType::Video) &&
        !presState.bgPath.empty();

    std::string searchLower = m_SearchBuffer;
    for (char& c : searchLower) c = (char)::tolower((unsigned char)c);

    std::vector<const BgEntry*> files;
    for (const auto& bg : m_AllBackgrounds) {
        if (!m_CurrentBgFolder.empty() && bg.folder != m_CurrentBgFolder) continue;

        if (!searchLower.empty()) {
            std::string nameLower = bg.name;
            for (char& c : nameLower) c = (char)::tolower((unsigned char)c);
            if (nameLower.find(searchLower) == std::string::npos) continue;
        }
        files.push_back(&bg);
    }

    if (files.empty()) {
        ImGui::Dummy({0, 16});
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + w, p.y + 64}, LPU32(LP::Surface1), DS::RadiusMedium);
        dl->AddRect(p, {p.x + w, p.y + 64}, LPU32(LP::Border), DS::RadiusMedium);
        ImGui::Dummy({0, 20});
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        std::string msg;
        if (!searchLower.empty()) {
            msg = "No se encontraron fondos que coincidan con \"" + std::string(m_SearchBuffer) + "\".";
        } else if (m_CurrentBgFolder.empty()) {
            msg = "Sin fondos aun. Usa el botón '+' de arriba para importar.";
        } else {
            msg = "La carpeta \"" + m_CurrentBgFolder + "\" está vacía.";
        }
        float tw = ImGui::CalcTextSize(msg.c_str()).x;
        ImGui::SetCursorPosX(std::max(0.0f, (w - tw) * 0.5f));
        ImGui::Text("%s", msg.c_str());
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
            const bool isActive = hasActiveLiveBg && (presState.bgPath == files[i]->fullPath);
            RenderBgCard(*files[i], cW, cH, currentCol, cols, isActive);
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, minGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        for (const auto* bg : files) {
            const bool isActive = hasActiveLiveBg && (presState.bgPath == bg->fullPath);
            RenderBgRow(*bg, w, 44.0f, isActive);
        }
    }

    if (ImGui::BeginDragDropTarget()) {
        auto HandleDrop = [&](const ImGuiPayload* payload) {
            const char* droppedPath = (const char*)payload->Data;
            if (droppedPath && *droppedPath) {
                fs::path src = Library::U8Path(droppedPath);
                std::string fname = Library::PathToUtf8(src.filename());
                if (!Core::MediaImportManager::Get().ExistsInBackgroundByName(fname)) {
                    auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
                    extBgs.push_back(Core::MediaImportManager::FormatExternalBgEntry(m_CurrentBgFolder, droppedPath));
                    ProyecThor::Settings::SettingsManager::Get().SaveSettings();
                    BumpBackgroundsListRevision();
                    ReloadList();
                }
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
    if (!this) return;
    if (m_SeenBgRevision != GetBackgroundsListRevision()) {
        ReloadList();
    }

    // Subir a textura GL las miniaturas de video que el worker en 2do plano
    // haya terminado de decodificar desde el frame anterior — tiene que
    // pasar por aca (hilo con contexto GL), no por el worker.
    DrainThumbnailResults();

    // Limpiar flag de proteccion contra autoclick UNA vez al inicio del frame
    m_JustEnteredFolder = false;
    m_ContentFade = LPApproach(m_ContentFade, 1.0f, 9.0f);

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    RenderTopBar();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // Aviso chico y no-bloqueante: mientras el worker todavia tiene videos
    // por decodificar, se avisa sin frenar nada (las tarjetas ya visibles
    // siguen andando, esto es solo informativo).
    if (size_t pending = m_ThumbWorker.PendingCount(); pending > 0) {
        ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, LP::TextMuted);
        ImGui::Text("Generando miniaturas... (%zu restantes)", pending);
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    LPSeparatorLine();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

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
        const float sidebarW = 145.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, LP::Surface0);
        ImGui::PushStyleColor(ImGuiCol_Border,  LP::Border);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, DS::RadiusMedium);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 6.0f));

        ImGui::BeginChild("##bgSidebar", ImVec2(sidebarW, totalH), true,
                          ImGuiWindowFlags_NoScrollbar);
        RenderFolderSidebar(sidebarW - 8.0f, totalH);
        ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 6.0f);

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
