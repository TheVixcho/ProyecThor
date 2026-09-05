#include "LibraryMultimedia.h"
#include "LibraryIcons.h"
#include "LibraryStyles.h"
#include "LibraryHelpers.h"
#include "ui/DesignSystem.h"
#include "frontend/panels/layers/LayersTheme.h"
#include "frontend/views/audio/AudioHelpers.h"
#include "frontend/views/audio/AudioAlbumArt.h"
#include "backend/core/ThumbnailWorker.h"
#include "backend/core/PresentationCore.h"
#include "backend/core/FileDeletionManager.h"
#include "backend/settings/SettingsManager.h"
#include "frontend/views/Audio.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>
#include <cmath>
#include <GL/gl.h>
#include "stb_image.h"

namespace DS = ProyecThor::UI::DS;

namespace ProyecThor::Library {

// =============================================================================
//  Listas y carpetas
// =============================================================================

struct MMItem {
    std::string    filename;
    Core::ItemType type;
    std::string    customFullPath = "";
};

static std::vector<MMItem> s_Videos, s_Audios, s_Images;

// ── Vista: grilla (miniaturas grandes) o lista (compacta) -- default grilla,
//    mismo criterio que la galeria de Overlays. s_ThumbZoom solo aplica en
//    grilla, igual que en LibraryVideos.
static bool  s_GridMode  = true;
static float s_ThumbZoom = 1.0f;

static std::string VideoFolder() { return GetAssetsPath() + "/videos"; }
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

static void ScanFolder(const std::string& folder, const std::vector<std::string>& exts,
                        Core::ItemType type, std::vector<MMItem>& out)
{
    std::error_code ec;
    fs::path dir = U8Path(folder);
    if (!fs::exists(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;
        out.push_back({ PathToUtf8(entry.path().filename()), type, "" });
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

    // 1. Escaneo de carpetas internas de la aplicacion
    ScanFolder(VideoFolder(), vidExts, Core::ItemType::Video, s_Videos);
    ScanFolder(ImageFolder(), imgExts, Core::ItemType::Image, s_Images);
    ScanFolder(AudioFolder(), audExts, Core::ItemType::Audio, s_Audios);

    // 2. Escaneo de carpetas vinculadas (Watched Folders de Ajustes > Datos)
    const auto& watched = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.watchedFolders;
    for (const auto& wf : watched) {
        if (!wf.enabled || wf.path.empty()) continue;
        std::error_code ec;
        fs::path dir = U8Path(wf.path);
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;

        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

            bool isVid = (std::find(vidExts.begin(), vidExts.end(), ext) != vidExts.end());
            bool isImg = (std::find(imgExts.begin(), imgExts.end(), ext) != imgExts.end());
            bool isAud = (std::find(audExts.begin(), audExts.end(), ext) != audExts.end());

            if (!isVid && !isImg && !isAud) continue;

            std::string fname = PathToUtf8(entry.path().filename());

            if (wf.copyToDataDir) {
                // Modo copiar: copiar a la carpeta de la app si no existe
                std::string targetDir = isVid ? VideoFolder() : (isImg ? ImageFolder() : AudioFolder());
                fs::path targetPath = fs::path(targetDir) / entry.path().filename();
                if (!fs::exists(targetPath, ec)) {
                    fs::copy_file(entry.path(), targetPath, fs::copy_options::overwrite_existing, ec);
                    if (isVid) s_Videos.push_back({ fname, Core::ItemType::Video, "" });
                    else if (isImg) s_Images.push_back({ fname, Core::ItemType::Image, "" });
                    else s_Audios.push_back({ fname, Core::ItemType::Audio, "" });
                }
            } else {
                // Modo cargar sin copiar: indexar directamente la ruta original
                std::string fpath = PathToUtf8(entry.path());
                if (isVid) s_Videos.push_back({ fname, Core::ItemType::Video, fpath });
                else if (isImg) s_Images.push_back({ fname, Core::ItemType::Image, fpath });
                else s_Audios.push_back({ fname, Core::ItemType::Audio, fpath });
            }
        }
    }

    auto SortAndDedupe = [](std::vector<MMItem>& list) {
        std::sort(list.begin(), list.end(), [](const MMItem& a, const MMItem& b) {
            return a.filename < b.filename;
        });
        list.erase(std::unique(list.begin(), list.end(), [](const MMItem& a, const MMItem& b) {
            return a.filename == b.filename && a.customFullPath == b.customFullPath;
        }), list.end());
    };

    SortAndDedupe(s_Videos);
    SortAndDedupe(s_Images);
    SortAndDedupe(s_Audios);
}

// =============================================================================
//  Miniaturas — una cache por tipo, mismo criterio que el resto de Biblioteca
//  (cada seccion la suya, no compartida).
// =============================================================================

static ImTextureID LoadThumbFromDisk(const char* path) {
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

// ── Video: primer frame real via ThumbnailWorker, con cache en disco ────────
static fs::path VideoThumbCacheDir() {
    fs::path dir = fs::path(GetAssetsPath()) / "thumbnails";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}
static std::string VideoThumbCachePathFor(const std::string& absVideoPath) {
    std::error_code ec;
    auto sz = fs::file_size(absVideoPath, ec);
    size_t h = std::hash<std::string>{}(absVideoPath + "|" + std::to_string(ec ? 0 : sz));
    return (VideoThumbCacheDir() / (std::to_string(h) + ".png")).string();
}

static std::unordered_map<std::string, ImTextureID> s_VideoThumbCache;
static ProyecThor::Core::ThumbnailWorker            s_VideoThumbWorker;

static ImTextureID GetVideoThumbnail(const std::string& path) {
    auto it = s_VideoThumbCache.find(path);
    if (it != s_VideoThumbCache.end()) return it->second;

    std::string abs = fs::absolute(fs::path(path)).string();
    std::string cachePath = VideoThumbCachePathFor(abs);
    std::error_code ec;
    if (fs::exists(cachePath, ec)) {
        ImTextureID t = LoadThumbFromDisk(cachePath.c_str());
        if (t) { s_VideoThumbCache[path] = t; return t; }
    }
    s_VideoThumbWorker.Request(path, abs, cachePath);
    return 0;
}

static void DrainVideoThumbnails() {
    std::vector<ProyecThor::Core::ThumbnailWorker::Result> results;
    s_VideoThumbWorker.DrainResults(results);
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
    }
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
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    if (ImGui::BeginPopupModal("Renombrar##mm", &s_ShowRenameModal, ImGuiWindowFlags_NoResize)) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##mmRenameBuf", s_RenameBuffer, sizeof(s_RenameBuffer));

        if (ImGui::Button("Renombrar", ImVec2(160, 0))) {
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
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    if (ImGui::BeginPopupModal("Eliminar##mm", &s_ShowDeleteModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Eliminar \"%s\"? Esta acción no se puede deshacer.",
                           s_DeleteItem.filename.c_str());
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        DS::DangerColorDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  DS::DangerColor);
        if (ImGui::Button("Eliminar", ImVec2(160, 0))) {
            std::string fullPath = ItemFullPath(s_DeleteItem);
            if (s_SelectedFile == s_DeleteItem.filename) {
                s_SelectedFile.clear();
            }
            s_VideoThumbCache.erase(fullPath);
            Core::FileDeletionManager::ForceDeleteFile(fullPath);
            RefreshMultimediaLists();
            s_ShowDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
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
    if (item.type == Core::ItemType::Video) {
        if (ImGui::MenuItem("Enviar al monitor")) {
            Core::PresentationCore::Get().SetBackgroundMedia(ItemFullPath(item), true, /*allowAudio=*/true);
            Core::PresentationCore::Get().SetProjecting(true);
        }
        ImGui::Separator();
    }
    if (item.type == Core::ItemType::Video || item.type == Core::ItemType::Image) {
        if (ImGui::MenuItem("Mover a Fondos (Backgrounds)")) {
            std::error_code ec;
            std::string srcPath = ItemFullPath(item);
            fs::path src(srcPath);
            std::string targetDir = GetAssetsPath() + "/backgrounds";
            fs::create_directories(targetDir, ec);
            fs::path dst = fs::path(targetDir) / src.filename();
            fs::rename(src, dst, ec);
            RefreshMultimediaLists();
        }
        if (ImGui::MenuItem("Copiar a Fondos (Backgrounds)")) {
            std::error_code ec;
            std::string srcPath = ItemFullPath(item);
            fs::path src(srcPath);
            std::string targetDir = GetAssetsPath() + "/backgrounds";
            fs::create_directories(targetDir, ec);
            fs::path dst = fs::path(targetDir) / src.filename();
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        }
        ImGui::Separator();
    }
    if (ImGui::MenuItem("Renombrar")) RequestRename(item);
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, DS::DangerColor);
    if (ImGui::MenuItem("Eliminar")) RequestDelete(item);
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
                ap->PlayFileLive(item.filename);
            }
        } else {
            Core::PresentationCore::Get().SetBackgroundMedia(ItemFullPath(item), item.type == Core::ItemType::Video, /*allowAudio=*/true);
            Core::PresentationCore::Get().SetProjecting(true);
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
                ap->PlayFileLive(item.filename);
            }
        } else {
            Core::PresentationCore::Get().SetBackgroundMedia(ItemFullPath(item), item.type == Core::ItemType::Video, /*allowAudio=*/true);
            Core::PresentationCore::Get().SetProjecting(true);
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

void RenderMultimediaSection(LibraryContext& ctx, MultimediaFilter& filter)
{
    static bool s_Loaded = false;
    if (!s_Loaded) { RefreshMultimediaLists(); s_Loaded = true; }

    DrainVideoThumbnails();

    // ── Barra de busqueda ────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImGui::ColorConvertU32ToFloat4(DS::BtnDefaultFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::ColorConvertU32ToFloat4(DS::BtnHoverFill));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImGui::ColorConvertU32ToFloat4(DS::AccentColorDim));
    ImGui::PushStyleColor(ImGuiCol_Border,         ImVec4(1.00f, 1.00f, 1.00f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 7.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##mmsearch", "Buscar...", ctx.searchBuffer, ctx.searchBufferSize);
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
        if (UI::LPCornerIconBtn("##mmimport", UI::LPDrawPlus, "Importar archivo", {btnSz, btnSz}))
            ctx.importFile();
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

    std::string searchLower(ctx.searchBuffer);
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
