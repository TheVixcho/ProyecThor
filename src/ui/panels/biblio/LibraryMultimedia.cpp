#include "LibraryMultimedia.h"
#include "LibraryVideos.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/framework/DesignSystem.h"
#include "ui/panels/layers/LayersTheme.h"
#include "ui/views/audio/AudioHelpers.h"
#include "ui/views/audio/AudioAlbumArt.h"
#include "core/ThumbnailWorker.h"
#include "core/PresentationCore.h"
#include "core/FileDeletionManager.h"
#include "core/MediaImportManager.h"
#include "core/settings/SettingsManager.h"
#include "ui/panels/layers/LayersBgTab.h"
#include "ui/views/Audio.h"
#include "ui/framework/FilePicker.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>
#include <cmath>
#include <thread>
#include <chrono>
#include <GL/gl.h>
#include "stb_image.h"

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  Listas y carpetas
// =============================================================================

static std::vector<MMItem> s_Videos, s_Audios, s_Images;

const std::vector<MMItem>& GetVideoList() {
    static bool s_Init = false;
    if (!s_Init || (s_Videos.empty() && s_Images.empty() && s_Audios.empty())) {
        RefreshMultimediaLists();
        s_Init = true;
    }
    return s_Videos;
}

const std::vector<MMItem>& GetImageList() {
    static bool s_Init = false;
    if (!s_Init || (s_Videos.empty() && s_Images.empty() && s_Audios.empty())) {
        RefreshMultimediaLists();
        s_Init = true;
    }
    return s_Images;
}

const std::vector<MMItem>& GetAudioList() {
    static bool s_Init = false;
    if (!s_Init || (s_Videos.empty() && s_Images.empty() && s_Audios.empty())) {
        RefreshMultimediaLists();
        s_Init = true;
    }
    return s_Audios;
}

// ── Vista: grilla (miniaturas grandes) o lista (compacta) -- default grilla,
//    mismo criterio que la galeria de Overlays. s_ThumbZoom solo aplica en
//    grilla, igual que en LibraryVideos.
static bool  s_GridMode  = true;
static float s_ThumbZoom = 1.0f;

static std::string VideoFolder() { return GetVideoLibraryPath(); }
static std::string ImageFolder() { return GetAssetsPath() + "/images"; }
static std::string AudioFolder() { return ProyecThor::Audio::GetAudioPath(); }

static std::string ItemFullPath(const MMItem& it) {
    if (!it.customFullPath.empty()) return it.customFullPath;
    switch (it.type) {
        case Core::ItemType::Video: return VideoFolder() + "/" + it.filename;
        case Core::ItemType::Image: return ImageFolder() + "/" + it.filename;
        default:                    return AudioFolder() + "/" + it.filename;
    }
}

std::string GetMMItemFullPath(const MMItem& it) {
    return ItemFullPath(it);
}

std::string GetMMItemRootLabel(const MMItem& it) {
    return it.rootLabel.empty() ? "ProyecThor" : it.rootLabel;
}

std::vector<std::string> GetAvailableRootLabels(Core::ItemType /*type*/) {
    std::vector<std::string> roots;
    roots.push_back("ProyecThor");
    {
        const auto& watched = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.watchedFolders;
        for (const auto& wf : watched) {
            if (!wf.enabled || wf.path.empty()) continue;
            std::error_code ec;
            fs::path p = U8Path(wf.path);
            if (fs::exists(p, ec) && fs::is_directory(p, ec)) {
                std::string label = PathToUtf8(p.filename());
                if (label.empty()) label = wf.path;
                if (std::find(roots.begin(), roots.end(), label) == roots.end()) {
                    roots.push_back(label);
                }
            }
        }
    }
    return roots;
}

static void ScanFolder(const std::string& folder, const std::vector<std::string>& exts,
                        Core::ItemType type, std::vector<MMItem>& out,
                        const std::string& rootLabel = "ProyecThor", bool recursive = false)
{
    std::error_code ec;
    fs::path dir = U8Path(folder);
    if (!fs::exists(dir, ec)) return;

    auto processEntry = [&](const fs::directory_entry& entry) {
        if (!entry.is_regular_file()) return;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (std::find(exts.begin(), exts.end(), ext) == exts.end()) return;
        std::string full = PathToUtf8(entry.path());
        std::string fn   = PathToUtf8(entry.path().filename());
        out.push_back({ fn, type, full, rootLabel });
    };

    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) break;
            processEntry(entry);
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            processEntry(entry);
        }
    }
}

void RefreshMultimediaLists()
{
    s_Videos.clear();
    s_Images.clear();
    s_Audios.clear();

    const std::vector<std::string> vidExts = { ".mp4", ".mkv", ".avi", ".mov", ".webm", ".m4v" };
    const std::vector<std::string> imgExts = { ".jpg", ".jpeg", ".png", ".bmp", ".webp" };
    const std::vector<std::string> audExts = { ".mp3", ".flac", ".wav", ".ogg", ".aac", ".m4a", ".wma", ".opus", ".aiff" };

    // 1. Escaneo de carpetas internas de la aplicacion (Carpeta raíz "ProyecThor")
    ScanFolder(VideoFolder(), vidExts, Core::ItemType::Video, s_Videos, "ProyecThor", false);
    ScanFolder(ImageFolder(), imgExts, Core::ItemType::Image, s_Images, "ProyecThor", false);
    ScanFolder(AudioFolder(), audExts, Core::ItemType::Audio, s_Audios, "ProyecThor", false);

    // 2. Archivos externos vinculados explicitamente por el usuario
    const auto& storage = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage;
    for (const auto& rawPath : storage.externalMultimediaFiles) {
        if (rawPath.empty()) continue;
        std::error_code ec;
        fs::path p = U8Path(rawPath);
        if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) continue;

        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });

        bool isVid = (std::find(vidExts.begin(), vidExts.end(), ext) != vidExts.end());
        bool isImg = (std::find(imgExts.begin(), imgExts.end(), ext) != imgExts.end());
        bool isAud = (std::find(audExts.begin(), audExts.end(), ext) != audExts.end());
        if (!isVid && !isImg && !isAud) continue;

        std::string fname = PathToUtf8(p.filename());
        std::string fpath = PathToUtf8(p);

        std::string rootLabel = "ProyecThor";
        for (const auto& wf : storage.watchedFolders) {
            if (fpath.rfind(wf.path, 0) == 0) {
                fs::path wfp = U8Path(wf.path);
                rootLabel = PathToUtf8(wfp.filename());
                if (rootLabel.empty()) rootLabel = wf.path;
                break;
            }
        }

        if (isVid)      s_Videos.push_back({ fname, Core::ItemType::Video, fpath, rootLabel });
        else if (isImg) s_Images.push_back({ fname, Core::ItemType::Image, fpath, rootLabel });
        else            s_Audios.push_back({ fname, Core::ItemType::Audio, fpath, rootLabel });
    }

    // 3. Escaneo automático de carpetas externas vinculadas sin duplicar
    {
        for (const auto& wf : storage.watchedFolders) {
            if (!wf.enabled || wf.path.empty()) continue;
            std::error_code ec;
            fs::path p = U8Path(wf.path);
            if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) continue;

            std::string rootLabel = PathToUtf8(p.filename());
            if (rootLabel.empty()) rootLabel = wf.path;

            ScanFolder(wf.path, vidExts, Core::ItemType::Video, s_Videos, rootLabel, true);
            ScanFolder(wf.path, imgExts, Core::ItemType::Image, s_Images, rootLabel, true);
            ScanFolder(wf.path, audExts, Core::ItemType::Audio, s_Audios, rootLabel, true);
        }
    }

    // 4. Deduplicacion por ruta y ordenamiento
    auto SortAndDedupe = [](std::vector<MMItem>& list) {
        std::unordered_set<std::string> seenPaths;
        std::vector<MMItem> uniqueList;
        uniqueList.reserve(list.size());

        for (auto& item : list) {
            std::string pathKey = Core::MediaImportManager::NormalizePathKey(ItemFullPath(item));
            if (pathKey.empty() || seenPaths.count(pathKey) > 0) continue;

            seenPaths.insert(pathKey);
            if (item.rootLabel.empty()) item.rootLabel = "ProyecThor";
            uniqueList.push_back(std::move(item));
        }

        std::sort(uniqueList.begin(), uniqueList.end(), [](const MMItem& a, const MMItem& b) {
            if (a.rootLabel != b.rootLabel) {
                if (a.rootLabel == "ProyecThor") return true;
                if (b.rootLabel == "ProyecThor") return false;
                return a.rootLabel < b.rootLabel;
            }
            std::string ka = Core::MediaImportManager::NormalizeFilenameKey(a.filename);
            std::string kb = Core::MediaImportManager::NormalizeFilenameKey(b.filename);
            if (ka != kb) return ka < kb;
            return a.filename < b.filename;
        });

        list = std::move(uniqueList);
    };

    SortAndDedupe(s_Videos);
    SortAndDedupe(s_Images);
    SortAndDedupe(s_Audios);
}

void ImportMultimediaFile()
{
    // Abre el selector multiple y presenta el menu interactivo (Multimedia vs Background)
    // permitiendo abrir los archivos directamente desde su carpeta original (ej. Descargas)
    // sin duplicarlos en disco ni secuestrar defaultMediaFolder.
    Core::MediaImportManager::Get().OpenFilePickerForImport(Core::MediaImportDest::Multimedia);
}

// =============================================================================
//  Miniaturas — una cache por tipo, mismo criterio que el resto de Biblioteca
//  (cada seccion la suya, no compartida).
// =============================================================================

static ImTextureID LoadThumbFromDisk(const char* path) {
    const auto& perf = ProyecThor::Settings::SettingsManager::Get().GetSettings().performance;
    const unsigned maxW = perf.lowMemoryMode ? 256u : (perf.performanceMode ? 320u : 512u);
    const unsigned maxH = perf.lowMemoryMode ? 144u : (perf.performanceMode ? 180u : 288u);

    std::vector<uint8_t> pixels;
    unsigned w = 0, h = 0;
    if (!ProyecThor::Core::ThumbnailWorker::LoadThumbnailPixelsFromDisk(path ? path : "", pixels, w, h, maxW, maxH))
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

// ── Video: primer frame real via ThumbnailWorker, con cache en disco ────────
static fs::path VideoThumbCacheDir() {
    fs::path dir = fs::path(GetAssetsPath()) / "thumbnails";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static std::string VideoThumbCachePathFor(const std::string& absVideoPath) {
    std::error_code ec;
    auto sz = fs::file_size(U8Path(absVideoPath), ec);
    if (ec) {
        ec.clear();
        sz = fs::file_size(fs::path(absVideoPath), ec);
    }
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return (VideoThumbCacheDir() / (std::to_string(h) + ".png")).string();
}

static std::unordered_map<std::string, ImTextureID> s_VideoThumbCache;
static ProyecThor::Core::ThumbnailWorker            s_VideoThumbWorker;

static std::string ResolveAbsMediaPath(const std::string& path) {
    std::error_code ec;
    fs::path p = U8Path(path);
    if (fs::exists(p, ec))
        return PathToUtf8(fs::absolute(p, ec));
    ec.clear();
    return fs::absolute(fs::path(path), ec).string();
}

static ImTextureID GetVideoThumbnail(const std::string& path) {
    auto it = s_VideoThumbCache.find(path);
    if (it != s_VideoThumbCache.end()) return it->second;

    std::string abs = ResolveAbsMediaPath(path);
    std::string cachePath = VideoThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(U8Path(cachePath), ec) || fs::exists(fs::path(cachePath), ec)) {
        ImTextureID t = LoadThumbFromDisk(cachePath.c_str());
        if (t) { s_VideoThumbCache[path] = t; return t; }
    }
    // Compatibilidad con hashes generados anteriormente usando fs::path directo:
    std::string legacyAbs = fs::absolute(fs::path(path), ec).string();
    if (legacyAbs != abs) {
        std::string legacyCache = VideoThumbCachePathFor(legacyAbs);
        if (fs::exists(fs::path(legacyCache), ec)) {
            ImTextureID t = LoadThumbFromDisk(legacyCache.c_str());
            if (t) { s_VideoThumbCache[path] = t; return t; }
        }
    }

    s_VideoThumbWorker.Request(path, abs, cachePath);
    return 0;
}

static int DrainVideoThumbnails(std::string* outLastKey = nullptr) {
    std::vector<ProyecThor::Core::ThumbnailWorker::Result> results;
    s_VideoThumbWorker.DrainResults(results);
    int count = 0;
    for (auto& r : results) {
        ImTextureID t = 0;
        if (!r.pixels.empty() && r.width > 0 && r.height > 0) {
            GLuint tex; glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)r.width, (GLsizei)r.height,
                        0, GL_RGBA, GL_UNSIGNED_BYTE, r.pixels.data());
            t = (ImTextureID)(intptr_t)tex;
        }
        s_VideoThumbCache[r.key] = t;
        if (outLastKey) *outLastKey = r.key;
        ++count;
    }
    return count;
}

ImTextureID QuerySharedVideoThumbnail(const std::string& path) {
    auto it = s_VideoThumbCache.find(path);
    if (it != s_VideoThumbCache.end())
        return it->second;
    return 0;
}

// ── Imagen: la imagen misma, escalada por ImGui al dibujar ──────────────────
static std::unordered_map<std::string, ImTextureID> s_ImageThumbCache;

static ImTextureID GetImageThumbnail(const std::string& path) {
    auto it = s_ImageThumbCache.find(path);
    if (it != s_ImageThumbCache.end()) return it->second;
    ImTextureID t = LoadThumbFromDisk(path.c_str());
    s_ImageThumbCache[path] = t;
    return t;
}

// ── Audio: portada embebida (ID3/FLAC/M4A/OGG), generica si no hay ──────────
static std::unordered_map<std::string, ImTextureID> s_AudioArtCache;

static ImTextureID GetAudioThumbnail(const std::string& path) {
    auto it = s_AudioArtCache.find(path);
    if (it != s_AudioArtCache.end()) return it->second;

    ImTextureID t = 0;
    Audio::AlbumArt art = Audio::ExtractAlbumArt(path);
    if (art.HasData()) {
        Audio::UploadAlbumArtToGL(art);
        if (art.HasTexture()) t = (ImTextureID)(intptr_t)art.texID;
    }
    s_AudioArtCache[path] = t;
    return t;
}

ImTextureID GetMMVideoThumbnail(const std::string& path) {
    DrainVideoThumbnails();
    return GetVideoThumbnail(path);
}

ImTextureID GetMMImageThumbnail(const std::string& path) {
    return GetImageThumbnail(path);
}

ImTextureID GetMMAudioThumbnail(const std::string& path) {
    return GetAudioThumbnail(path);
}

void ClearMultimediaThumbnailCaches() {
    ClearLocalVideoThumbnailCache();

    std::unordered_set<GLuint> uniqueTex;
    auto collectFrom = [&](const std::unordered_map<std::string, ImTextureID>& cache) {
        for (const auto& kv : cache) {
            GLuint tex = static_cast<GLuint>(static_cast<uintptr_t>(kv.second));
            if (tex != 0) uniqueTex.insert(tex);
        }
    };
    collectFrom(s_VideoThumbCache);
    collectFrom(s_ImageThumbCache);
    collectFrom(s_AudioArtCache);

    for (GLuint tex : uniqueTex) {
        glDeleteTextures(1, &tex);
    }

    s_VideoThumbCache.clear();
    s_ImageThumbCache.clear();
    s_AudioArtCache.clear();
    s_VideoThumbWorker.ResetSeen();
}

void PreloadMultimediaThumbnails(const MultimediaPreloadProgressFn& onProgress)
{
    if (s_Videos.empty() && s_Images.empty() && s_Audios.empty())
        RefreshMultimediaLists();

    const int total = static_cast<int>(s_Images.size() + s_Audios.size() + s_Videos.size());
    if (total == 0) {
        if (onProgress) onProgress(0, 0, "");
        return;
    }

    int done = 0;

    // 1. Primero cargamos miniaturas de video ya cacheadas en disco y encolamos
    //    las no cacheadas en el worker en segundo plano para que se decodifiquen
    //    en paralelo mientras el hilo principal carga imágenes y portadas de audio.
    int pendingWorkerVideos = 0;
    std::string firstUncachedVideoName;
    for (const auto& vid : s_Videos) {
        const std::string fullPath = ItemFullPath(vid);
        if (s_VideoThumbCache.find(fullPath) != s_VideoThumbCache.end()) {
            ++done;
            continue;
        }
        (void)GetVideoThumbnail(fullPath);
        if (s_VideoThumbCache.find(fullPath) != s_VideoThumbCache.end()) {
            ++done;
            if (onProgress) onProgress(done, total, vid.filename);
        } else {
            ++pendingWorkerVideos;
            if (firstUncachedVideoName.empty())
                firstUncachedVideoName = vid.filename;
        }
    }

    // 2. Imágenes de proyección
    for (const auto& img : s_Images) {
        if (onProgress) onProgress(done, total, img.filename);
        (void)GetImageThumbnail(ItemFullPath(img));
        ++done;
        if (onProgress) onProgress(done, total, img.filename);
    }

    // 3. Portadas embebidas de pistas de audio
    for (const auto& aud : s_Audios) {
        if (onProgress) onProgress(done, total, aud.filename);
        (void)GetAudioThumbnail(ItemFullPath(aud));
        ++done;
        if (onProgress) onProgress(done, total, aud.filename);
    }

    // 4. Drenar fotogramas de video generados por ThumbnailWorker en tiempo real
    if (pendingWorkerVideos > 0 || s_VideoThumbWorker.PendingCount() > 0) {
        std::string currentLabel = firstUncachedVideoName;
        auto lastAdvance = std::chrono::steady_clock::now();

        while (true) {
            std::string finishedKey;
            int newlyDone = DrainVideoThumbnails(&finishedKey);
            if (newlyDone > 0) {
                done = std::min(total, done + newlyDone);
                lastAdvance = std::chrono::steady_clock::now();
                if (!finishedKey.empty()) {
                    currentLabel = PathToUtf8(U8Path(finishedKey).filename());
                }
            }

            if (onProgress)
                onProgress(std::min(done, total), total, currentLabel);

            if (s_VideoThumbWorker.PendingCount() == 0) {
                std::string extraKey;
                int extra = DrainVideoThumbnails(&extraKey);
                if (extra > 0) done = std::min(total, done + extra);
                break;
            }

            // Salvaguarda por si un archivo corrupto bloquea el decodificador mas alla del timeout
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

// ── Miniatura + icono/acento por tipo ────────────────────────────────────
// El icono de respaldo (mientras no hay miniatura real, o el tipo no tiene)
// reutiliza los MISMOS glifos que ya dibujan los botones de filtro de arriba
// (Video/Audio/Imagen) -- reconocibles de un vistazo en vez de una letra
// suelta. El acento de color sale de las categorias de color YA definidas en
// Ajustes > Apariencia para Video/Imagen/Audio (categoryColor[1]/[2]/[5]) --
// hoy sin uso real en el sidebar (ver LibrarySidebar.cpp, que indexa ese
// array por posicion de boton, no por tipo), asi que esta es su primera
// aplicacion util: cada tipo se distingue por color tanto en lista como en
// grilla.
static ImTextureID GetThumbAndAccent(const MMItem& item, UI::LPDrawIconFn& outIcon, ImU32& outAccent)
{
    const auto& cc = ProyecThor::Settings::SettingsManager::Get().GetSettings().librarySidebar.categoryColor;
    switch (item.type) {
        case Core::ItemType::Video:
            outIcon   = DrawIcon_Play;
            outAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[1][0], cc[1][1], cc[1][2], 1.0f));
            return GetVideoThumbnail(ItemFullPath(item));
        case Core::ItemType::Image:
            outIcon   = DrawIcon_Image;
            outAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[2][0], cc[2][1], cc[2][2], 1.0f));
            return GetImageThumbnail(ItemFullPath(item));
        default:
            outIcon   = DrawIcon_Audio;
            outAccent = ImGui::ColorConvertFloat4ToU32(ImVec4(cc[5][0], cc[5][1], cc[5][2], 1.0f));
            return GetAudioThumbnail(ItemFullPath(item));
    }
}

// =============================================================================
//  Renombrar / Eliminar — estado propio (no reusa ctx.showRenameModal: ese
//  modal resuelve la carpeta por m_CurrentCategory, que aca es "Multimedia"
//  y no alcanza para saber a que carpeta pertenece el item).
// =============================================================================

static std::string s_SelectedFile;

static bool        s_ShowRenameModal = false;
static MMItem       s_RenameItem;
static char         s_RenameBuffer[256]{};

static bool        s_ShowDeleteModal = false;
static MMItem       s_DeleteItem;

static void RequestRename(const MMItem& item) {
    s_RenameItem = item;
    std::string ext;
    std::string stem = SplitExtension(item.filename, ext);
    std::memset(s_RenameBuffer, 0, sizeof(s_RenameBuffer));
    std::strncpy(s_RenameBuffer, stem.c_str(), sizeof(s_RenameBuffer) - 1);
    s_ShowRenameModal = true;
}

static void RequestDelete(const MMItem& item) {
    s_DeleteItem      = item;
    s_ShowDeleteModal = true;
}

static void RenderRenameModal() {
    if (!s_ShowRenameModal) return;
    ImGui::OpenPopup("Renombrar##mm");
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    if (ImGui::BeginPopupModal("Renombrar##mm", &s_ShowRenameModal,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enterPressed = ImGui::InputText("##mmRenameBuf", s_RenameBuffer, sizeof(s_RenameBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue);

        if (ImGui::Button("Renombrar", ImVec2(160, 0)) || enterPressed) {
            std::string ext;
            SplitExtension(s_RenameItem.filename, ext);
            std::string newName = std::string(s_RenameBuffer) + ext;
            std::string folder = (s_RenameItem.type == Core::ItemType::Video) ? VideoFolder()
                                : (s_RenameItem.type == Core::ItemType::Image) ? ImageFolder()
                                                                                : AudioFolder();
            std::error_code ec;
            fs::rename(U8Path(folder + "/" + s_RenameItem.filename),
                      U8Path(folder + "/" + newName), ec);
            RefreshMultimediaLists();
            s_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            s_ShowRenameModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void RenderDeleteModal() {
    if (!s_ShowDeleteModal) return;
    ImGui::OpenPopup("Eliminar##mm");
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowViewport(vp->ID);
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }
    const bool isExternal = !s_DeleteItem.customFullPath.empty();
    ImGui::SetNextWindowSize(ImVec2(isExternal ? 440.0f : 360.0f, 0));
    if (ImGui::BeginPopupModal("Eliminar##mm", &s_ShowDeleteModal,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        if (isExternal) {
            ImGui::TextWrapped("¿Quitar \"%s\" de ProyecThor?\nEl archivo original permanecerá intacto en su carpeta.",
                               s_DeleteItem.filename.c_str());
        } else {
            ImGui::TextWrapped("Eliminar \"%s\"? Esta acción no se puede deshacer.",
                               s_DeleteItem.filename.c_str());
        }
        ImGui::Spacing();

        if (isExternal) {
            if (ImGui::Button("Quitar vínculo", ImVec2(150, 0))) {
                std::string fullPath = ItemFullPath(s_DeleteItem);
                if (s_SelectedFile == s_DeleteItem.filename) s_SelectedFile.clear();
                s_VideoThumbCache.erase(fullPath);
                Core::MediaImportManager::Get().UnlinkExternalMultimedia(fullPath);
                RefreshMultimediaLists();
                s_ShowDeleteModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,        DS::DangerColorDim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::DangerColor);
            if (ImGui::Button("Borrar del disco", ImVec2(140, 0))) {
                std::string fullPath = ItemFullPath(s_DeleteItem);
                if (s_SelectedFile == s_DeleteItem.filename) s_SelectedFile.clear();
                s_VideoThumbCache.erase(fullPath);
                Core::MediaImportManager::Get().UnlinkExternalMultimedia(fullPath);
                Core::FileDeletionManager::ForceDeleteFile(fullPath);
                RefreshMultimediaLists();
                s_ShowDeleteModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        DS::DangerColorDim);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DS::DangerColor);
            if (ImGui::Button("Eliminar", ImVec2(160, 0))) {
                std::string fullPath = ItemFullPath(s_DeleteItem);
                if (s_SelectedFile == s_DeleteItem.filename) s_SelectedFile.clear();
                s_VideoThumbCache.erase(fullPath);
                Core::FileDeletionManager::ForceDeleteFile(fullPath);
                RefreshMultimediaLists();
                s_ShowDeleteModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
        }

        if (ImGui::Button("Cancelar", ImVec2(110, 0))) {
            s_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// =============================================================================
//  Interaccion compartida entre fila (lista) y tarjeta (grilla): seleccion,
//  drag-drop de video y menu contextual son EXACTAMENTE los mismos en los
//  dos modos de vista, asi que viven en un solo lugar en vez de duplicarse.
// =============================================================================

static void SelectMMItem(const MMItem& item) {
    s_SelectedFile = item.filename;
    Core::LibrarySelection s;
    s.title = ItemFullPath(item);
    s.type  = item.type;
    Core::PresentationCore::Get().SetSelection(s);
}

static void RenderMMDragSource(const MMItem& item, const std::string& disp) {
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
    std::string fullPath = ItemFullPath(item);
    if (item.type == Core::ItemType::Video) {
        ImGui::SetDragDropPayload("VIDEO_TO_QUEUE", fullPath.c_str(), fullPath.size() + 1);
    }
    ImGui::SetDragDropPayload("MEDIA_ITEM_PATH", fullPath.c_str(), fullPath.size() + 1);
    ImGui::PushStyleColor(ImGuiCol_Text, DS::SuccessColor);
    ImGui::TextUnformatted(disp.c_str());
    ImGui::PopStyleColor();
    ImGui::EndDragDropSource();
}

static void RenderMMContextMenu(const MMItem& item, const char* popupId) {
    if (!ImGui::BeginPopupContextItem(popupId)) return;
    if (item.type == Core::ItemType::Video || item.type == Core::ItemType::Image) {
        if (ImGui::MenuItem("Enviar al monitor")) {
            auto& core = Core::PresentationCore::Get();
            core.SetProjecting(true);
            core.SetBackgroundMedia(ItemFullPath(item), item.type == Core::ItemType::Video, /*allowAudio=*/item.type == Core::ItemType::Video);
        }
        ImGui::Separator();
    } else if (item.type == Core::ItemType::Audio) {
        if (ImGui::MenuItem("Enviar en vivo al proyector")) {
            if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                ap->PlayFileLive(ItemFullPath(item));
            }
        }
        ImGui::Separator();
    }
    if (item.type == Core::ItemType::Video || item.type == Core::ItemType::Image) {
        if (ImGui::MenuItem("Mover a Fondos (Backgrounds)")) {
            if (!item.customFullPath.empty()) {
                Core::MediaImportManager::Get().MoveExternalToBackground(item.customFullPath);
            } else {
                std::error_code ec;
                std::string srcPath = ItemFullPath(item);
                fs::path src = U8Path(srcPath);
                if (!Core::MediaImportManager::Get().ExistsInBackgroundByName(item.filename)) {
                    std::string targetDir = GetAssetsPath() + "/backgrounds";
                    fs::create_directories(U8Path(targetDir), ec);
                    fs::path dst = U8Path(targetDir) / src.filename();
                    fs::rename(src, dst, ec);
                } else {
                    Core::FileDeletionManager::ForceDeleteFile(srcPath);
                }
                UI::BumpBackgroundsListRevision();
                RefreshMultimediaLists();
            }
        }
        if (ImGui::MenuItem("Copiar a Fondos (Backgrounds)")) {
            if (!Core::MediaImportManager::Get().ExistsInBackgroundByName(item.filename)) {
                if (!item.customFullPath.empty()) {
                    Core::MediaImportManager::Get().LinkSingleExternalFileDirect(
                        item.customFullPath, Core::MediaImportDest::Background);
                } else {
                    std::error_code ec;
                    std::string srcPath = ItemFullPath(item);
                    fs::path src = U8Path(srcPath);
                    std::string targetDir = GetAssetsPath() + "/backgrounds";
                    fs::create_directories(U8Path(targetDir), ec);
                    fs::path dst = U8Path(targetDir) / src.filename();
                    fs::copy_file(src, dst, fs::copy_options::skip_existing, ec);
                    UI::BumpBackgroundsListRevision();
                }
            }
        }
        ImGui::Separator();
    }
    if (item.customFullPath.empty() && ImGui::MenuItem("Renombrar")) RequestRename(item);
    if (item.customFullPath.empty()) ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
    if (ImGui::MenuItem(item.customFullPath.empty() ? "Eliminar" : "Quitar de ProyecThor")) RequestDelete(item);
    ImGui::PopStyleColor();
    ImGui::EndPopup();
}

// =============================================================================
//  Fila de item (vista lista) — mismo lenguaje visual que RenderVideoRow
//  (LibraryVideos.cpp), con el icono de respaldo real en vez de una letra.
// =============================================================================

static void RenderMMRow(const MMItem& item, int rowIdx) {
    ImGui::PushID(rowIdx);

    UI::LPDrawIconFn icon   = nullptr;
    ImU32            accent = DS::TextSecondary;
    ImTextureID      thumb  = GetThumbAndAccent(item, icon, accent);

    const float thumbSz = 22.0f;
    const float indent  = thumbSz + 16.0f;

    ImVec2      rowPos = ImGui::GetCursorScreenPos();
    bool        sel    = (s_SelectedFile == item.filename);
    std::string disp   = StripExtension(item.filename);

    bool clicked = DS::GlassListRow(disp.c_str(), sel, indent);
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       thumbY = rowPos.y + (DS::RowHeight - thumbSz) * 0.5f;
    if (thumb) {
        dl->AddImageRounded(thumb, {rowPos.x + 8.0f, thumbY},
                            {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                            {0,0}, {1,1}, IM_COL32_WHITE, DS::RadiusSmall);
    } else {
        ImVec4 af = ImGui::ColorConvertU32ToFloat4(accent);
        dl->AddRectFilled({rowPos.x + 8.0f, thumbY},
                          {rowPos.x + 8.0f + thumbSz, thumbY + thumbSz},
                          ImGui::ColorConvertFloat4ToU32(ImVec4(af.x, af.y, af.z, 0.18f)), DS::RadiusSmall);
        float pad = thumbSz * 0.18f;
        if (icon) icon(dl, {rowPos.x + 8.0f + pad, thumbY + pad}, thumbSz - pad * 2.0f, accent);
    }

    if (clicked) SelectMMItem(item);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        SelectMMItem(item);
        if (item.type == Core::ItemType::Audio) {
            if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                ap->PlayFile(ItemFullPath(item), /*live=*/false);
            }
        } else if (item.type == Core::ItemType::Video) {
            auto& core = Core::PresentationCore::Get();
            core.RequestPreviewLoad(ItemFullPath(item), /*loop=*/false, /*startMuted=*/false);
        }
    }

    RenderMMDragSource(item, disp);
    RenderMMContextMenu(item, ("##ctx_mm" + std::to_string(rowIdx)).c_str());

    ImGui::PopID();
}

// =============================================================================
//  Tarjeta de item (vista grilla) — mismo lenguaje visual que
//  LibraryVideos::RenderVideoCard (miniatura grande + chip de tipo + franja
//  de nombre), generalizado a los 3 tipos con el acento de color de cada uno.
// =============================================================================

static void RenderMMCard(const MMItem& item, int cardIdx, float W, float H, int col, int cols) {
    ImGui::PushID(cardIdx);

    UI::LPDrawIconFn icon   = nullptr;
    ImU32            accent = DS::AccentColor;
    ImTextureID      thumb  = GetThumbAndAccent(item, icon, accent);

    ImVec2      pos    = ImGui::GetCursorScreenPos();
    bool        hovRaw = ImGui::IsMouseHoveringRect(pos, {pos.x + W, pos.y + H});
    float       t      = UI::LPHoverLerp(ImGui::GetID("##hov"), hovRaw);
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    bool        sel     = (s_SelectedFile == item.filename);

    float  inset = 2.0f * t;
    ImVec2 p0    = { pos.x - inset, pos.y - inset };
    ImVec2 p1    = { pos.x + W + inset, pos.y + H + inset };

    dl->AddRectFilled(p0, p1, DS::BtnDefaultFill, DS::RadiusMedium);
    if (thumb) {
        dl->AddImageRounded(thumb, p0, p1, {0,0}, {1,1}, IM_COL32_WHITE, DS::RadiusMedium);
    } else {
        ImVec4 af = ImGui::ColorConvertU32ToFloat4(accent);
        dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(ImVec4(af.x, af.y, af.z, 0.16f)), DS::RadiusMedium);
        float iconSz = std::min(W, H) * 0.32f;
        if (icon) icon(dl, { p0.x + (W - iconSz) * 0.5f, p0.y + (H - iconSz) * 0.5f - 6.0f }, iconSz, accent);
    }

    ImVec4 accentF = ImGui::ColorConvertU32ToFloat4(accent);
    ImVec4 borderA = ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultBord);
    float  bt      = sel ? 1.0f : t;
    ImVec4 borderCol(
        borderA.x + (accentF.x - borderA.x) * bt,
        borderA.y + (accentF.y - borderA.y) * bt,
        borderA.z + (accentF.z - borderA.z) * bt,
        borderA.w + (1.0f - borderA.w) * bt);
    dl->AddRect(p0, p1, ImGui::ColorConvertFloat4ToU32(borderCol), DS::RadiusMedium, 0, 1.0f + 0.8f * bt);

    // Chip de tipo (VID/AUD/IMG) -- mismo lenguaje que RenderVideoCard, ahora
    // con un color distinto por tipo en vez de un solo acento fijo.
    {
        const char* tag = item.type == Core::ItemType::Video ? "VID"
                         : item.type == Core::ItemType::Audio ? "AUD" : "IMG";
        ImVec2 ts = ImGui::CalcTextSize(tag);
        float  bx = p0.x + 7.0f, by = p0.y + 7.0f;
        ImU32  chipBg = ImGui::ColorConvertFloat4ToU32(ImVec4(accentF.x, accentF.y, accentF.z, 0.35f));
        dl->AddRectFilled({bx, by}, {bx + ts.x + 8.0f, by + ts.y + 4.0f}, chipBg, DS::RadiusSmall);
        dl->AddText({bx + 4.0f, by + 2.0f}, accent, tag);
    }

    std::string disp = StripExtension(item.filename);
    std::string dn   = disp.length() > 18 ? disp.substr(0, 15) + "..." : disp;
    dl->AddRectFilled({p0.x, p1.y - 26.0f}, {p1.x, p1.y}, IM_COL32(0, 0, 0, 200), DS::RadiusMedium, ImDrawFlags_RoundCornersBottom);
    ImVec2 ns = ImGui::CalcTextSize(dn.c_str());
    dl->AddText({p0.x + (W - ns.x) * 0.5f, p1.y - 21.0f}, DS::TextPrimary, dn.c_str());

    bool cardClicked = ImGui::InvisibleButton(("##mmcard" + std::to_string(cardIdx)).c_str(), {W, H});
    bool cardHovered = ImGui::IsItemHovered();
    if (cardClicked)
        SelectMMItem(item);
    if (cardHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        SelectMMItem(item);
        if (item.type == Core::ItemType::Audio) {
            if (auto* ap = UI::AudioPanel::GetActiveInstance()) {
                ap->PlayFile(ItemFullPath(item), /*live=*/false);
            }
        } else if (item.type == Core::ItemType::Video) {
            auto& core = Core::PresentationCore::Get();
            core.RequestPreviewLoad(ItemFullPath(item), /*loop=*/false, /*startMuted=*/false);
        }
    }

    RenderMMDragSource(item, disp);
    RenderMMContextMenu(item, ("##ctx_mmc" + std::to_string(cardIdx)).c_str());

    if (col < cols - 1) ImGui::SameLine();
    ImGui::PopID();
}

// =============================================================================
//  Seccion (encabezado + filas/tarjetas) — todas o solo el tipo filtrado
// =============================================================================

static void RenderMMSection(const char* label, const std::vector<MMItem>& items,
                            const std::string& searchLower, int& rowCounter, bool gridMode)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, DS::TextSecondary);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    std::vector<const MMItem*> filtered;
    filtered.reserve(items.size());
    for (const auto& item : items) {
        if (!searchLower.empty()) {
            std::string lo = item.filename;
            std::transform(lo.begin(), lo.end(), lo.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (lo.find(searchLower) == std::string::npos) continue;
        }
        filtered.push_back(&item);
    }

    if (filtered.empty()) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, DS::TextHint);
        ImGui::TextUnformatted("(vacio)");
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        return;
    }

    if (gridMode) {
        const float cW     = 150.0f * s_ThumbZoom;
        const float cH     = 100.0f * s_ThumbZoom;
        const float minGap = 10.0f;

        float availWidth = ImGui::GetContentRegionAvail().x;
        int   cols        = std::max(1, (int)((availWidth + minGap) / (cW + minGap)));
        int   currentCol  = 0;

        for (const MMItem* item : filtered) {
            RenderMMCard(*item, rowCounter++, cW, cH, currentCol, cols);
            currentCol++;
            if (currentCol < cols) {
                ImGui::SameLine(0.0f, minGap);
            } else {
                currentCol = 0;
                ImGui::Dummy({0.0f, minGap});
            }
        }
    } else {
        for (const MMItem* item : filtered)
            RenderMMRow(*item, rowCounter++);
    }

    ImGui::Dummy(ImVec2(0.0f, 16.0f));
}

// =============================================================================
//  RenderMultimediaSection
// =============================================================================

void RenderMultimediaSection(char* searchBuffer, int searchBufferSize, MultimediaFilter& filter, std::function<void()> importFile)
{
    static bool s_Loaded = false;
    if (!s_Loaded) { RefreshMultimediaLists(); s_Loaded = true; }

    DrainVideoThumbnails();

    // ── Barra de busqueda ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    if (searchBuffer && searchBufferSize > 0)
        ImGui::InputTextWithHint("##mmsearch", "Buscar...", searchBuffer, searchBufferSize);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // ── Filtros: Todos | Video | Audio | Imagen ─────────────────────────
    {
        const float btnSz = 26.0f;
        const float gap   = 4.0f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);
        ImGui::PushID("mmfilters");
        if (UI::LPCornerIconBtn("##mmall", UI::LPDrawAll, "Todos los medios", {btnSz, btnSz}, filter == MultimediaFilter::All))
            filter = MultimediaFilter::All;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmvid", UI::LPDrawPlay, "Solo videos", {btnSz, btnSz}, filter == MultimediaFilter::Video))
            filter = MultimediaFilter::Video;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmaud", UI::LPDrawAudio, "Solo audios", {btnSz, btnSz}, filter == MultimediaFilter::Audio))
            filter = MultimediaFilter::Audio;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmimg", UI::LPDrawImage, "Solo imágenes", {btnSz, btnSz}, filter == MultimediaFilter::Image))
            filter = MultimediaFilter::Image;
        ImGui::PopID();

        // Separador chico entre los filtros y las utilidades (Actualizar/Importar)
        ImGui::SameLine(0, gap * 2.0f);
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                { p.x, p.y + 4.0f }, { p.x, p.y + btnSz - 4.0f },
                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.12f)), 1.0f);
            ImGui::Dummy(ImVec2(1.0f, btnSz));
        }
        ImGui::SameLine(0, gap * 2.0f);

        if (UI::LPCornerIconBtn("##mmrefresh", UI::LPDrawRefresh, "Actualizar lista", {btnSz, btnSz}))
            RefreshMultimediaLists();

        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmimport", UI::LPDrawPlus, "Importar archivo", {btnSz, btnSz})) {
            if (importFile) importFile();
            else ImportMultimediaFile();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // ── Fila 2: Modos de Vista (Grilla / Lista) + Slider de Zoom a la derecha ──
    {
        const float btnSz = 26.0f;
        const float gap   = 4.0f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);
        ImGui::PushID("mmview");
        if (UI::LPCornerIconBtn("##mmgridm", UI::LPDrawGrid, "Vista en cuadrícula", {btnSz, btnSz}, s_GridMode))
            s_GridMode = true;
        ImGui::SameLine(0, gap);
        if (UI::LPCornerIconBtn("##mmlistm", UI::LPDrawList, "Vista en lista", {btnSz, btnSz}, !s_GridMode))
            s_GridMode = false;
        ImGui::PopID();

        if (s_GridMode) {
            const float zoomW = 76.0f;
            const float avail = ImGui::GetWindowContentRegionMax().x;
            const float sliderX = std::max(ImGui::GetCursorPosX() + gap, avail - zoomW - 2.0f);
            ImGui::SameLine(sliderX);
            UI::LPZoomSlider("##mmzoom", &s_ThumbZoom, 0.65f, 1.8f, zoomW);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    std::string searchLower(searchBuffer ? searchBuffer : "");
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_Border,  ImVec4(1.f, 1.f, 1.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,   DS::RadiusMedium);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(4.0f, 10.0f));

    if (ImGui::BeginChild("##mm_list", { 0.f, 0.f }, true, ImGuiChildFlags_AlwaysUseWindowPadding)) {
        int rowCounter = 0;
        if (filter == MultimediaFilter::All) {
            RenderMMSection("VIDEOS",   s_Videos, searchLower, rowCounter, s_GridMode);
            RenderMMSection("AUDIO",    s_Audios, searchLower, rowCounter, s_GridMode);
            RenderMMSection("IMAGENES", s_Images, searchLower, rowCounter, s_GridMode);
        } else if (filter == MultimediaFilter::Video) {
            RenderMMSection("VIDEOS", s_Videos, searchLower, rowCounter, s_GridMode);
        } else if (filter == MultimediaFilter::Audio) {
            RenderMMSection("AUDIO", s_Audios, searchLower, rowCounter, s_GridMode);
        } else {
            RenderMMSection("IMAGENES", s_Images, searchLower, rowCounter, s_GridMode);
        }

        if (ImGui::BeginDragDropTarget()) {
            auto HandleDrop = [](const ImGuiPayload* payload) {
                const char* droppedPath = (const char*)payload->Data;
                if (droppedPath && *droppedPath) {
                    std::error_code ec;
                    fs::path src(droppedPath);
                    std::string ext = src.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                    bool isImg = (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp");
                    std::string targetDir = isImg ? ImageFolder() : VideoFolder();
                    fs::create_directories(targetDir, ec);
                    fs::path dst = fs::path(targetDir) / src.filename();
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                    RefreshMultimediaLists();
                }
            };
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BG_FILE")) {
                HandleDrop(payload);
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BG_ITEM_PATH")) {
                HandleDrop(payload);
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    RenderRenameModal();
    RenderDeleteModal();
}

} // namespace ProyecThor::Library
