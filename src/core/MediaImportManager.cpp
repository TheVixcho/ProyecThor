#include "MediaImportManager.h"

#include "AppPaths.h"
#include "settings/SettingsManager.h"
#include "ui/framework/FilePicker.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include "ui/panels/biblio/LibraryMultimedia.h"
#include "ui/panels/layers/LayersBgTab.h"
#include <GL/glew.h>
#include "ui/views/audio/AudioHelpers.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace ProyecThor::Core {

namespace {

constexpr const char* kFolderPrefix = "@folder:";

fs::path Utf8ToFsPath(const std::string& u8) {
    if (u8.empty()) return {};
#ifdef _WIN32
    int len = MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), nullptr, 0);
    if (len <= 0) return fs::path(u8);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), w.data(), len);
    return fs::path(w);
#else
    return fs::path(u8);
#endif
}

std::string FsPathToUtf8(const fs::path& p) {
#ifdef _WIN32
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

std::string ToLowerAsciiOrFold(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool IsTemporaryDownloadExtension(const std::string& lowerExt, const std::string& lowerName) {
    if (lowerExt == ".crdownload" || lowerExt == ".part" || lowerExt == ".tmp" ||
        lowerExt == ".opdownload" || lowerExt == ".download" || lowerExt == ".partial") {
        return true;
    }
    if (!lowerName.empty() && (lowerName[0] == '.' || lowerName[0] == '~'))
        return true;
    return false;
}

std::string FriendlyParentLabel(const fs::path& filePath, const std::string& downloadsNorm) {
    fs::path parent = filePath.parent_path();
    std::string parentU8 = FsPathToUtf8(parent);
    if (!downloadsNorm.empty() && MediaImportManager::NormalizePathKey(parentU8) == downloadsNorm)
        return "Descargas";
    std::string leaf = FsPathToUtf8(parent.filename());
    return leaf.empty() ? parentU8 : leaf;
}

// Cola interna para aplicar en el hilo principal los archivos base de Descargas
// o duplicados ya existentes sin mutar SettingsManager desde hilos secundarios.
std::mutex               s_KnownQueueMutex;
std::vector<std::string> s_KnownPathsToPersist;
bool                     s_MarkDownloadsBaselineDone = false;

void AppendUniqueKnown(std::vector<std::string>& vec, const std::unordered_set<std::string>& existingNorm, const std::string& rawPath) {
    std::string norm = MediaImportManager::NormalizePathKey(rawPath);
    if (norm.empty() || existingNorm.count(norm)) return;
    vec.push_back(norm);
}

} // namespace

MediaImportManager& MediaImportManager::Get() {
    static MediaImportManager s_Instance;
    return s_Instance;
}

std::string MediaImportManager::NormalizePathKey(const std::string& utf8Path) {
    if (utf8Path.empty()) return {};
    std::error_code ec;
    fs::path p = Utf8ToFsPath(utf8Path);
    fs::path absP = fs::weakly_canonical(p, ec);
    if (ec || absP.empty()) {
        ec.clear();
        absP = fs::absolute(p, ec);
        if (ec || absP.empty()) absP = p;
    }
    std::string s = FsPathToUtf8(absP);
    std::replace(s.begin(), s.end(), '\\', '/');
    while (s.size() > 1 && s.back() == '/') s.pop_back();
#ifdef _WIN32
    s = ToLowerAsciiOrFold(s);
#endif
    return s;
}

std::string MediaImportManager::NormalizeFilenameKey(const std::string& utf8Filename) {
    return ToLowerAsciiOrFold(utf8Filename);
}

void MediaImportManager::ParseExternalBgEntry(const std::string& rawEntry,
                                              std::string& outFolder,
                                              std::string& outFullPath) {
    outFolder.clear();
    outFullPath = rawEntry;
    if (rawEntry.rfind(kFolderPrefix, 0) == 0) {
        size_t sep = rawEntry.find('|', 8);
        if (sep != std::string::npos) {
            outFolder   = rawEntry.substr(8, sep - 8);
            outFullPath = rawEntry.substr(sep + 1);
        }
    }
}

std::string MediaImportManager::FormatExternalBgEntry(const std::string& folder,
                                                      const std::string& fullPath) {
    if (folder.empty()) return fullPath;
    return std::string(kFolderPrefix) + folder + "|" + fullPath;
}

std::string MediaImportManager::GetSystemDownloadsFolderUtf8() {
#ifdef _WIN32
    PWSTR widePath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &widePath)) && widePath) {
        std::wstring ws(widePath);
        CoTaskMemFree(widePath);
        return FsPathToUtf8(fs::path(ws));
    }
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) {
        return FsPathToUtf8(fs::path(userProfile) / "Downloads");
    }
    return {};
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        fs::path p1 = fs::path(home) / "Downloads";
        std::error_code ec;
        if (fs::exists(p1, ec)) return p1.string();
        fs::path p2 = fs::path(home) / "Descargas";
        if (fs::exists(p2, ec)) return p2.string();
        return p1.string();
    }
    return {};
#endif
}

bool MediaImportManager::ClassifyExtension(const std::string& lowerExt, CandidateMediaKind& outKind) {
    if (lowerExt == ".mp4" || lowerExt == ".mkv" || lowerExt == ".avi" ||
        lowerExt == ".mov" || lowerExt == ".webm" || lowerExt == ".m4v") {
        outKind = CandidateMediaKind::Video;
        return true;
    }
    if (lowerExt == ".jpg" || lowerExt == ".jpeg" || lowerExt == ".png" ||
        lowerExt == ".bmp" || lowerExt == ".webp") {
        outKind = CandidateMediaKind::Image;
        return true;
    }
    if (lowerExt == ".mp3" || lowerExt == ".flac" || lowerExt == ".wav" ||
        lowerExt == ".ogg" || lowerExt == ".aac"  || lowerExt == ".m4a" ||
        lowerExt == ".wma" || lowerExt == ".opus" || lowerExt == ".aiff") {
        outKind = CandidateMediaKind::Audio;
        return true;
    }
    return false;
}

MediaImportDest MediaImportManager::GuessSmartDestination(const std::string& filename,
                                                          CandidateMediaKind kind,
                                                          MediaImportDest fallback) {
    if (kind == CandidateMediaKind::Audio)
        return MediaImportDest::Multimedia;

    std::string low = ToLowerAsciiOrFold(filename);
    if (low.find("loop") != std::string::npos ||
        low.find("background") != std::string::npos ||
        low.find("fondo") != std::string::npos ||
        low.find("motion") != std::string::npos ||
        low.find("bg_") != std::string::npos ||
        low.find("-bg") != std::string::npos ||
        low.find("_bg") != std::string::npos) {
        return MediaImportDest::Background;
    }
    return fallback;
}

void MediaImportManager::CollectExistingSets(std::unordered_set<std::string>& mmPaths,
                                             std::unordered_set<std::string>& mmNames,
                                             std::unordered_set<std::string>& bgPaths,
                                             std::unordered_set<std::string>& bgNames) const {
    mmPaths.clear();
    mmNames.clear();
    bgPaths.clear();
    bgNames.clear();

    auto scanFlatDir = [](const std::string& dirUtf8,
                          std::unordered_set<std::string>& pathsOut,
                          std::unordered_set<std::string>& namesOut) {
        if (dirUtf8.empty()) return;
        std::error_code ec;
        fs::path dir = Utf8ToFsPath(dirUtf8);
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            std::string ext = ToLowerAsciiOrFold(FsPathToUtf8(e.path().extension()));
            CandidateMediaKind k;
            if (!ClassifyExtension(ext, k)) continue;
            pathsOut.insert(NormalizePathKey(FsPathToUtf8(e.path())));
            namesOut.insert(NormalizeFilenameKey(FsPathToUtf8(e.path().filename())));
        }
    };

    const std::string assetsBase = ProyecThor::GetAssetsPath();
    scanFlatDir(ProyecThor::Library::GetVideoLibraryPath(), mmPaths, mmNames);
    scanFlatDir(assetsBase + "/images", mmPaths, mmNames);
    scanFlatDir(ProyecThor::Audio::GetAudioPath(), mmPaths, mmNames);

    // Fondos internos (raiz + subcarpetas)
    {
        std::error_code ec;
        fs::path bgRoot = Utf8ToFsPath(assetsBase + "/backgrounds");
        if (fs::exists(bgRoot, ec) && fs::is_directory(bgRoot, ec)) {
            for (const auto& e : fs::directory_iterator(bgRoot, ec)) {
                if (ec) break;
                if (e.is_directory()) {
                    std::error_code subEc;
                    for (const auto& sub : fs::directory_iterator(e.path(), subEc)) {
                        if (subEc) break;
                        if (!sub.is_regular_file()) continue;
                        std::string ext = ToLowerAsciiOrFold(FsPathToUtf8(sub.path().extension()));
                        CandidateMediaKind k;
                        if (!ClassifyExtension(ext, k) || k == CandidateMediaKind::Audio) continue;
                        bgPaths.insert(NormalizePathKey(FsPathToUtf8(sub.path())));
                        bgNames.insert(NormalizeFilenameKey(FsPathToUtf8(sub.path().filename())));
                    }
                } else if (e.is_regular_file()) {
                    std::string ext = ToLowerAsciiOrFold(FsPathToUtf8(e.path().extension()));
                    CandidateMediaKind k;
                    if (!ClassifyExtension(ext, k) || k == CandidateMediaKind::Audio) continue;
                    bgPaths.insert(NormalizePathKey(FsPathToUtf8(e.path())));
                    bgNames.insert(NormalizeFilenameKey(FsPathToUtf8(e.path().filename())));
                }
            }
        }
    }

    const auto& storage = Settings::SettingsManager::Get().GetSettings().storage;
    for (const auto& extMm : storage.externalMultimediaFiles) {
        if (extMm.empty()) continue;
        fs::path p = Utf8ToFsPath(extMm);
        mmPaths.insert(NormalizePathKey(extMm));
        mmNames.insert(NormalizeFilenameKey(FsPathToUtf8(p.filename())));
    }
    for (const auto& rawBg : storage.externalBackgroundFiles) {
        if (rawBg.empty()) continue;
        std::string folder, fpath;
        ParseExternalBgEntry(rawBg, folder, fpath);
        if (fpath.empty()) continue;
        fs::path p = Utf8ToFsPath(fpath);
        bgPaths.insert(NormalizePathKey(fpath));
        bgNames.insert(NormalizeFilenameKey(FsPathToUtf8(p.filename())));
    }
}

bool MediaImportManager::IsExternalBackgroundPath(const std::string& utf8Path) const {
    if (utf8Path.empty()) return false;
    const std::string key = NormalizePathKey(utf8Path);
    const auto& storage = Settings::SettingsManager::Get().GetSettings().storage;
    for (const auto& raw : storage.externalBackgroundFiles) {
        std::string folder, fpath;
        ParseExternalBgEntry(raw, folder, fpath);
        if (NormalizePathKey(fpath) == key) return true;
    }
    return false;
}

bool MediaImportManager::IsExternalMultimediaPath(const std::string& utf8Path) const {
    if (utf8Path.empty()) return false;
    const std::string key = NormalizePathKey(utf8Path);
    const auto& storage = Settings::SettingsManager::Get().GetSettings().storage;
    for (const auto& fpath : storage.externalMultimediaFiles) {
        if (NormalizePathKey(fpath) == key) return true;
    }
    return false;
}

bool MediaImportManager::ExistsInMultimediaByName(const std::string& filename) const {
    std::unordered_set<std::string> mmP, mmN, bgP, bgN;
    CollectExistingSets(mmP, mmN, bgP, bgN);
    return mmN.count(NormalizeFilenameKey(filename)) > 0;
}

bool MediaImportManager::ExistsInBackgroundByName(const std::string& filename) const {
    std::unordered_set<std::string> mmP, mmN, bgP, bgN;
    CollectExistingSets(mmP, mmN, bgP, bgN);
    return bgN.count(NormalizeFilenameKey(filename)) > 0;
}

void MediaImportManager::RefreshCandidateDuplicateFlags() {
    std::unordered_set<std::string> mmPaths, mmNames, bgPaths, bgNames;
    CollectExistingSets(mmPaths, mmNames, bgPaths, bgNames);

    for (auto& c : m_Candidates) {
        const std::string pKey = NormalizePathKey(c.fullPath);
        const std::string nKey = NormalizeFilenameKey(c.filename);
        c.alreadyInMultimedia = (mmPaths.count(pKey) > 0) || (mmNames.count(nKey) > 0);
        c.alreadyInBackground = (bgPaths.count(pKey) > 0) || (bgNames.count(nKey) > 0);

        if (c.kind == CandidateMediaKind::Audio)
            c.destination = MediaImportDest::Multimedia;
    }
}

void MediaImportManager::TriggerImmediateScan() {
    m_ForceImmediateScan = true;
}

void MediaImportManager::Update() {
    // 1. Persistir en el hilo principal cualquier actualizacion de archivos vistos
    {
        std::vector<std::string> toPersist;
        bool markBaseline = false;
        {
            std::lock_guard<std::mutex> lk(s_KnownQueueMutex);
            if (!s_KnownPathsToPersist.empty() || s_MarkDownloadsBaselineDone) {
                toPersist.swap(s_KnownPathsToPersist);
                markBaseline = s_MarkDownloadsBaselineDone;
                s_MarkDownloadsBaselineDone = false;
            }
        }
        if (!toPersist.empty() || markBaseline) {
            auto& mgr     = Settings::SettingsManager::Get();
            auto& storage = mgr.GetSettings().storage;
            std::unordered_set<std::string> knownSet(storage.knownWatchedFiles.begin(),
                                                     storage.knownWatchedFiles.end());
            bool changed = false;
            for (const auto& p : toPersist) {
                if (knownSet.insert(p).second) {
                    storage.knownWatchedFiles.push_back(p);
                    changed = true;
                }
            }
            if (markBaseline && !storage.downloadsBaselineInitialized) {
                storage.downloadsBaselineInitialized = true;
                changed = true;
            }
            if (changed)
                mgr.SaveSettings();
        }
    }

    // 2. Si el modal no esta abierto (o ya esta abierto y llega un lote nuevo), fusionar lotes pendientes
    {
        std::vector<PendingBatch> incoming;
        {
            std::lock_guard<std::mutex> lk(m_PendingMutex);
            if (!m_PendingBatches.empty())
                incoming.swap(m_PendingBatches);
        }

        if (!incoming.empty()) {
            std::unordered_set<std::string> currentPaths;
            std::unordered_set<std::string> currentNames;
            for (const auto& existing : m_Candidates) {
                currentPaths.insert(NormalizePathKey(existing.fullPath));
                currentNames.insert(NormalizeFilenameKey(existing.filename));
            }

            for (auto& batch : incoming) {
                if (!m_ModalOpen) {
                    m_ModalIsAutoDetected = batch.isAutoDetected;
                    m_ModalTargetBgFolder = batch.targetBgFolder;
                } else if (!batch.isAutoDetected) {
                    m_ModalIsAutoDetected = false;
                    if (!batch.targetBgFolder.empty())
                        m_ModalTargetBgFolder = batch.targetBgFolder;
                }

                for (auto& item : batch.items) {
                    const std::string pKey = NormalizePathKey(item.fullPath);
                    const std::string nKey = NormalizeFilenameKey(item.filename);
                    if (currentPaths.count(pKey) || currentNames.count(nKey))
                        continue; // evitar duplicados incluso dentro del mismo lote de seleccion
                    currentPaths.insert(pKey);
                    currentNames.insert(nKey);
                    m_Candidates.push_back(std::move(item));
                }
            }

            if (!m_Candidates.empty()) {
                RefreshCandidateDuplicateFlags();
                // Si un item ya existe en el destino preseleccionado pero NO en el otro
                // (solo para video/imagen), ajustar o desmarcar para evitar duplicados.
                for (auto& c : m_Candidates) {
                    const bool dupInTarget = (c.destination == MediaImportDest::Multimedia)
                        ? c.alreadyInMultimedia
                        : c.alreadyInBackground;
                    if (dupInTarget) {
                        if (c.kind != CandidateMediaKind::Audio &&
                            c.destination == MediaImportDest::Multimedia && !c.alreadyInBackground) {
                            // Mantener destino original pero desmarcar por seguridad si ya esta en el programa
                            c.selected = false;
                        } else {
                            c.selected = false;
                        }
                    }
                }
                m_ModalOpen = true;
            }
        }
    }

    // 3. Escaneo periodico no bloqueante de Descargas + Carpetas Vinculadas
    const auto& perf = Settings::SettingsManager::Get().GetSettings().performance;
    const bool pauseScan = perf.performanceMode || perf.pauseBackgroundScan;
    const double now = ImGui::GetTime();
    const bool forceNow = m_ForceImmediateScan.exchange(false);
    if ((forceNow || (!pauseScan && (now - m_LastScanTimeSec >= 3.2))) && !m_ScanRunning.load()) {
        m_LastScanTimeSec = now;
        StartAsyncWatcherScan();
    }
}

void MediaImportManager::StartAsyncWatcherScan() {
    if (m_ScanRunning.exchange(true))
        return;

    // Capturar snapshot en el hilo principal para cero carreras de datos
    const auto& settings = Settings::SettingsManager::Get().GetSettings();
    const bool watchDownloads = settings.storage.watchDownloadsFolder;
    const bool baselineDone   = settings.storage.downloadsBaselineInitialized;
    const auto watchedFolders = settings.storage.watchedFolders;

    std::unordered_set<std::string> knownNorm;
    for (const auto& k : settings.storage.knownWatchedFiles)
        knownNorm.insert(NormalizePathKey(k));

    // Incluir tambien lo que ya esta actualmente en el modal abierto
    for (const auto& c : m_Candidates)
        knownNorm.insert(NormalizePathKey(c.fullPath));

    std::unordered_set<std::string> mmPaths, mmNames, bgPaths, bgNames;
    CollectExistingSets(mmPaths, mmNames, bgPaths, bgNames);

    std::thread([this, watchDownloads, baselineDone, watchedFolders,
                 knownNorm = std::move(knownNorm),
                 mmPaths   = std::move(mmPaths),
                 mmNames   = std::move(mmNames),
                 bgPaths   = std::move(bgPaths),
                 bgNames   = std::move(bgNames)]() mutable {
        const std::string downloadsDir = GetSystemDownloadsFolderUtf8();
        const std::string downloadsNorm = NormalizePathKey(downloadsDir);

        std::vector<std::string> newlyKnownToSave;
        bool setBaselineFlag = false;
        std::vector<ImportCandidate> detected;
        std::unordered_set<std::string> batchNames;

        const auto nowFs = fs::file_time_type::clock::now();

        auto scanWatchedDirectory = [&](const std::string& folderUtf8, bool seedOnly) {
            if (folderUtf8.empty()) return;
            std::error_code ec;
            fs::path dir = Utf8ToFsPath(folderUtf8);
            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;

                const std::string fnameU8 = FsPathToUtf8(entry.path().filename());
                const std::string lowName = ToLowerAsciiOrFold(fnameU8);
                const std::string lowExt  = ToLowerAsciiOrFold(FsPathToUtf8(entry.path().extension()));

                if (IsTemporaryDownloadExtension(lowExt, lowName))
                    continue;

                CandidateMediaKind kind;
                if (!ClassifyExtension(lowExt, kind))
                    continue;

                const std::string fullU8  = FsPathToUtf8(entry.path());
                const std::string normKey = NormalizePathKey(fullU8);
                if (normKey.empty() || knownNorm.count(normKey))
                    continue;

                std::error_code szEc;
                const uint64_t fsize = entry.file_size(szEc);
                if (szEc || fsize == 0)
                    continue; // archivo aun vacio o bloqueado durante descarga

                // Esperar ~1.5s tras la ultima escritura en disco para no ofrecer una descarga a mitad
                std::error_code tmEc;
                const auto ftime = entry.last_write_time(tmEc);
                if (!tmEc && (nowFs - ftime) < std::chrono::milliseconds(1500))
                    continue;

                if (seedOnly) {
                    knownNorm.insert(normKey);
                    newlyKnownToSave.push_back(normKey);
                    continue;
                }

                const std::string nameKey = NormalizeFilenameKey(fnameU8);
                // Si el archivo (o uno con el exacto mismo nombre) YA existe en Multimedia o Background,
                // lo registramos como conocido y NUNCA lo duplicamos ni molestamos al usuario.
                if (mmPaths.count(normKey) || bgPaths.count(normKey) ||
                    mmNames.count(nameKey) || bgNames.count(nameKey) ||
                    batchNames.count(nameKey)) {
                    knownNorm.insert(normKey);
                    newlyKnownToSave.push_back(normKey);
                    continue;
                }

                batchNames.insert(nameKey);
                knownNorm.insert(normKey);

                ImportCandidate cand;
                cand.fullPath          = fullU8;
                cand.filename          = fnameU8;
                cand.parentFolderLabel = FriendlyParentLabel(entry.path(), downloadsNorm);
                cand.fileSizeBytes     = fsize;
                cand.kind              = kind;
                cand.destination       = GuessSmartDestination(fnameU8, kind, MediaImportDest::Multimedia);
                cand.selected          = true;
                detected.push_back(std::move(cand));
            }
        };

        if (!downloadsDir.empty()) {
            if (!baselineDone) {
                scanWatchedDirectory(downloadsDir, /*seedOnly=*/true);
                setBaselineFlag = true;
            } else if (watchDownloads) {
                scanWatchedDirectory(downloadsDir, /*seedOnly=*/false);
            }
        }

        for (const auto& wf : watchedFolders) {
            if (!wf.enabled || wf.path.empty()) continue;
            if (!downloadsNorm.empty() && NormalizePathKey(wf.path) == downloadsNorm && !watchDownloads) {
                // Si el usuario agrego Descargas explicitamente a watchedFolders, igual se escanea
            }
            scanWatchedDirectory(wf.path, /*seedOnly=*/false);
        }

        if (!newlyKnownToSave.empty() || setBaselineFlag) {
            std::lock_guard<std::mutex> lk(s_KnownQueueMutex);
            for (auto& p : newlyKnownToSave)
                s_KnownPathsToPersist.push_back(std::move(p));
            if (setBaselineFlag)
                s_MarkDownloadsBaselineDone = true;
        }

        if (!detected.empty()) {
            PendingBatch batch;
            batch.items          = std::move(detected);
            batch.isAutoDetected = true;
            batch.defaultDest    = MediaImportDest::Multimedia;
            std::lock_guard<std::mutex> lk(m_PendingMutex);
            m_PendingBatches.push_back(std::move(batch));
        }

        m_ScanRunning = false;
    }).detach();
}

void MediaImportManager::OpenFilePickerForImport(MediaImportDest defaultDest,
                                                 const std::string& targetBgFolder) {
    if (m_PickerRunning.exchange(true))
        return;

    std::thread([this, defaultDest, targetBgFolder]() {
        std::vector<UI::FileFilterItem> filters;
        if (defaultDest == MediaImportDest::Background) {
            filters.push_back({"Fondos (Video e Imagen)",
                {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.m4v",
                 "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"}});
            filters.push_back({"Videos",   {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.m4v"}});
            filters.push_back({"Imágenes", {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"}});
        } else {
            filters.push_back({"Multimedia y Fondos",
                {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.m4v",
                 "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp",
                 "*.mp3", "*.flac", "*.wav", "*.ogg", "*.aac", "*.m4a", "*.wma", "*.opus", "*.aiff"}});
            filters.push_back({"Videos",   {"*.mp4", "*.mkv", "*.avi", "*.mov", "*.webm", "*.m4v"}});
            filters.push_back({"Imágenes", {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp"}});
            filters.push_back({"Audio",    {"*.mp3", "*.flac", "*.wav", "*.ogg", "*.aac", "*.m4a", "*.wma", "*.opus", "*.aiff"}});
        }
        filters.push_back({"Todos los archivos", {"*"}});

        const std::string title = (defaultDest == MediaImportDest::Background)
            ? "Seleccionar Fondos o Videos"
            : "Seleccionar Archivos Multimedia o Fondos";

        std::vector<std::string> chosen = UI::PickMultipleFiles(title, filters);
        if (!chosen.empty()) {
            EnqueueManualFiles(chosen, defaultDest, targetBgFolder);
        }
        m_PickerRunning = false;
    }).detach();
}

void MediaImportManager::EnqueueManualFiles(const std::vector<std::string>& rawPaths,
                                            MediaImportDest defaultDest,
                                            const std::string& targetBgFolder) {
    if (rawPaths.empty()) return;

    const std::string downloadsNorm = NormalizePathKey(GetSystemDownloadsFolderUtf8());
    std::vector<ImportCandidate> items;
    std::unordered_set<std::string> seenNames;

    auto processSingleFile = [&](const fs::path& p) {
        std::error_code ec;
        if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) return;

        const std::string fnameU8 = FsPathToUtf8(p.filename());
        const std::string lowExt  = ToLowerAsciiOrFold(FsPathToUtf8(p.extension()));
        CandidateMediaKind kind;
        if (!ClassifyExtension(lowExt, kind)) return;

        const std::string nameKey = NormalizeFilenameKey(fnameU8);
        if (!seenNames.insert(nameKey).second) return; // deduplicar mismo nombre en la misma seleccion

        std::error_code szEc;
        uint64_t fsize = fs::file_size(p, szEc);

        ImportCandidate cand;
        cand.fullPath          = FsPathToUtf8(p);
        cand.filename          = fnameU8;
        cand.parentFolderLabel = FriendlyParentLabel(p, downloadsNorm);
        cand.fileSizeBytes     = szEc ? 0 : fsize;
        cand.kind              = kind;
        cand.destination       = (kind == CandidateMediaKind::Audio)
            ? MediaImportDest::Multimedia
            : defaultDest;
        cand.selected          = true;
        items.push_back(std::move(cand));
    };

    for (const auto& raw : rawPaths) {
        if (raw.empty()) continue;
        std::error_code ec;
        fs::path p = Utf8ToFsPath(raw);
        if (!fs::exists(p, ec)) {
            ec.clear();
            p = fs::path(raw);
            if (!fs::exists(p, ec)) continue;
        }
        if (fs::is_directory(p, ec)) {
            for (const auto& entry : fs::directory_iterator(p, ec)) {
                if (ec) break;
                if (entry.is_regular_file())
                    processSingleFile(entry.path());
            }
        } else {
            processSingleFile(p);
        }
    }

    if (!items.empty()) {
        PendingBatch batch;
        batch.items          = std::move(items);
        batch.isAutoDetected = false;
        batch.defaultDest    = defaultDest;
        batch.targetBgFolder = targetBgFolder;
        std::lock_guard<std::mutex> lk(m_PendingMutex);
        m_PendingBatches.push_back(std::move(batch));
    }
}

bool MediaImportManager::LinkSingleExternalFileDirect(const std::string& rawPath, MediaImportDest dest) {
    if (rawPath.empty()) return false;
    fs::path p = Utf8ToFsPath(rawPath);
    std::error_code ec;
    if (!fs::exists(p, ec)) return false;

    const std::string fnameU8 = FsPathToUtf8(p.filename());
    const std::string lowExt  = ToLowerAsciiOrFold(FsPathToUtf8(p.extension()));
    CandidateMediaKind kind;
    if (!ClassifyExtension(lowExt, kind)) return false;
    if (kind == CandidateMediaKind::Audio) dest = MediaImportDest::Multimedia;

    std::unordered_set<std::string> mmPaths, mmNames, bgPaths, bgNames;
    CollectExistingSets(mmPaths, mmNames, bgPaths, bgNames);

    const std::string pKey = NormalizePathKey(rawPath);
    const std::string nKey = NormalizeFilenameKey(fnameU8);

    auto& mgr     = Settings::SettingsManager::Get();
    auto& storage = mgr.GetSettings().storage;
    std::unordered_set<std::string> knownSet(storage.knownWatchedFiles.begin(),
                                             storage.knownWatchedFiles.end());
    AppendUniqueKnown(storage.knownWatchedFiles, knownSet, rawPath);

    bool added = false;
    if (dest == MediaImportDest::Multimedia) {
        if (!mmPaths.count(pKey) && !mmNames.count(nKey)) {
            storage.externalMultimediaFiles.push_back(FsPathToUtf8(p));
            added = true;
        }
    } else {
        if (!bgPaths.count(pKey) && !bgNames.count(nKey)) {
            storage.externalBackgroundFiles.push_back(FsPathToUtf8(p));
            added = true;
        }
    }

    mgr.SaveSettings();
    if (added) {
        ProyecThor::Library::RefreshMultimediaLists();
        ProyecThor::Library::ForceLibraryRescan() = true;
        ProyecThor::UI::BumpBackgroundsListRevision();
    }
    return added;
}

int MediaImportManager::CommitPendingImport(bool copyToAppFolder) {
    if (m_Candidates.empty()) {
        m_ModalOpen = false;
        return 0;
    }

    std::unordered_set<std::string> mmPaths, mmNames, bgPaths, bgNames;
    CollectExistingSets(mmPaths, mmNames, bgPaths, bgNames);

    auto& mgr     = Settings::SettingsManager::Get();
    auto& storage = mgr.GetSettings().storage;
    std::unordered_set<std::string> knownSet;
    for (const auto& k : storage.knownWatchedFiles)
        knownSet.insert(NormalizePathKey(k));

    const std::string assetsBase = ProyecThor::GetAssetsPath();
    int addedCount = 0;
    bool touchedMm = false;
    bool touchedBg = false;

    for (const auto& c : m_Candidates) {
        const std::string pKey = NormalizePathKey(c.fullPath);
        const std::string nKey = NormalizeFilenameKey(c.filename);

        // Registrar todos los candidatos del lote como ya decididos por el usuario
        if (!pKey.empty() && knownSet.insert(pKey).second) {
            storage.knownWatchedFiles.push_back(pKey);
        }

        if (!c.selected)
            continue;

        MediaImportDest dest = (c.kind == CandidateMediaKind::Audio)
            ? MediaImportDest::Multimedia
            : c.destination;

        if (dest == MediaImportDest::Multimedia) {
            if (mmPaths.count(pKey) || mmNames.count(nKey))
                continue; // garantia estricta: jamas duplicar un archivo con el mismo nombre

            if (copyToAppFolder) {
                std::string targetDir = (c.kind == CandidateMediaKind::Video)
                    ? ProyecThor::Library::GetVideoLibraryPath()
                    : ((c.kind == CandidateMediaKind::Image) ? (assetsBase + "/images")
                                                             : ProyecThor::Audio::GetAudioPath());
                std::error_code ec;
                fs::path dstDir = Utf8ToFsPath(targetDir);
                fs::create_directories(dstDir, ec);
                fs::path srcP = Utf8ToFsPath(c.fullPath);
                fs::path dstP = dstDir / srcP.filename();
                if (!fs::exists(dstP, ec)) {
                    fs::copy_file(srcP, dstP, fs::copy_options::skip_existing, ec);
                }
                if (!ec) {
                    mmPaths.insert(NormalizePathKey(FsPathToUtf8(dstP)));
                    mmNames.insert(nKey);
                    ++addedCount;
                    touchedMm = true;
                }
            } else {
                // Por defecto: queda en su carpeta original (ej. Descargas) y ProyecThor lo abre directo
                storage.externalMultimediaFiles.push_back(c.fullPath);
                mmPaths.insert(pKey);
                mmNames.insert(nKey);
                ++addedCount;
                touchedMm = true;
            }
        } else {
            if (bgPaths.count(pKey) || bgNames.count(nKey))
                continue; // garantia estricta: jamas duplicar un fondo con el mismo nombre

            if (copyToAppFolder) {
                std::error_code ec;
                fs::path dstDir = Utf8ToFsPath(assetsBase + "/backgrounds");
                if (!m_ModalTargetBgFolder.empty())
                    dstDir = dstDir / Utf8ToFsPath(m_ModalTargetBgFolder);
                fs::create_directories(dstDir, ec);
                fs::path srcP = Utf8ToFsPath(c.fullPath);
                fs::path dstP = dstDir / srcP.filename();
                if (!fs::exists(dstP, ec)) {
                    fs::copy_file(srcP, dstP, fs::copy_options::skip_existing, ec);
                }
                if (!ec) {
                    bgPaths.insert(NormalizePathKey(FsPathToUtf8(dstP)));
                    bgNames.insert(nKey);
                    ++addedCount;
                    touchedBg = true;
                }
            } else {
                storage.externalBackgroundFiles.push_back(
                    FormatExternalBgEntry(m_ModalTargetBgFolder, c.fullPath));
                bgPaths.insert(pKey);
                bgNames.insert(nKey);
                ++addedCount;
                touchedBg = true;
            }
        }
    }

    mgr.SaveSettings();

    m_Candidates.clear();
    m_ModalTargetBgFolder.clear();
    m_ModalOpen = false;

    if (touchedMm) {
        ProyecThor::Library::RefreshMultimediaLists();
        ProyecThor::Library::ForceLibraryRescan() = true;
    }
    if (touchedBg) {
        ProyecThor::UI::BumpBackgroundsListRevision();
    }

    return addedCount;
}

void MediaImportManager::DismissPendingImport() {
    if (!m_Candidates.empty()) {
        auto& mgr     = Settings::SettingsManager::Get();
        auto& storage = mgr.GetSettings().storage;
        std::unordered_set<std::string> knownSet;
        for (const auto& k : storage.knownWatchedFiles)
            knownSet.insert(NormalizePathKey(k));

        bool changed = false;
        for (const auto& c : m_Candidates) {
            const std::string pKey = NormalizePathKey(c.fullPath);
            if (!pKey.empty() && knownSet.insert(pKey).second) {
                storage.knownWatchedFiles.push_back(pKey);
                changed = true;
            }
        }
        if (changed)
            mgr.SaveSettings();
    }

    m_Candidates.clear();
    m_ModalTargetBgFolder.clear();
    m_ModalOpen = false;
}

bool MediaImportManager::UnlinkExternalMultimedia(const std::string& utf8Path) {
    const std::string targetKey = NormalizePathKey(utf8Path);
    if (targetKey.empty()) return false;

    auto& mgr = Settings::SettingsManager::Get();
    auto& vec = mgr.GetSettings().storage.externalMultimediaFiles;
    auto oldSz = vec.size();
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const std::string& entry) {
        return NormalizePathKey(entry) == targetKey;
    }), vec.end());

    if (vec.size() != oldSz) {
        mgr.SaveSettings();
        return true;
    }
    return false;
}

bool MediaImportManager::UnlinkExternalBackground(const std::string& utf8Path) {
    const std::string targetKey = NormalizePathKey(utf8Path);
    if (targetKey.empty()) return false;

    auto& mgr = Settings::SettingsManager::Get();
    auto& vec = mgr.GetSettings().storage.externalBackgroundFiles;
    auto oldSz = vec.size();
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const std::string& raw) {
        std::string folder, fpath;
        ParseExternalBgEntry(raw, folder, fpath);
        return NormalizePathKey(fpath) == targetKey;
    }), vec.end());

    if (vec.size() != oldSz) {
        mgr.SaveSettings();
        ProyecThor::UI::BumpBackgroundsListRevision();
        return true;
    }
    return false;
}

bool MediaImportManager::MoveExternalToBackground(const std::string& utf8Path,
                                                  const std::string& bgFolder) {
    if (utf8Path.empty()) return false;
    fs::path p = Utf8ToFsPath(utf8Path);
    const std::string fnameU8 = FsPathToUtf8(p.filename());

    UnlinkExternalMultimedia(utf8Path);
    if (!ExistsInBackgroundByName(fnameU8) && !IsExternalBackgroundPath(utf8Path)) {
        auto& mgr = Settings::SettingsManager::Get();
        mgr.GetSettings().storage.externalBackgroundFiles.push_back(
            FormatExternalBgEntry(bgFolder, utf8Path));
        mgr.SaveSettings();
    }
    ProyecThor::Library::RefreshMultimediaLists();
    ProyecThor::Library::ForceLibraryRescan() = true;
    ProyecThor::UI::BumpBackgroundsListRevision();
    return true;
}

bool MediaImportManager::MoveExternalToMultimedia(const std::string& utf8Path) {
    if (utf8Path.empty()) return false;
    fs::path p = Utf8ToFsPath(utf8Path);
    const std::string fnameU8 = FsPathToUtf8(p.filename());

    UnlinkExternalBackground(utf8Path);
    if (!ExistsInMultimediaByName(fnameU8) && !IsExternalMultimediaPath(utf8Path)) {
        auto& mgr = Settings::SettingsManager::Get();
        mgr.GetSettings().storage.externalMultimediaFiles.push_back(utf8Path);
        mgr.SaveSettings();
    }
    ProyecThor::Library::RefreshMultimediaLists();
    ProyecThor::Library::ForceLibraryRescan() = true;
    ProyecThor::UI::BumpBackgroundsListRevision();
    return true;
}

} // namespace ProyecThor::Core
