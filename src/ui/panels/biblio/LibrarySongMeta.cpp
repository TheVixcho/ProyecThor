#include "LibrarySongMeta.h"
#include "LibraryHelpers.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <mutex>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ProyecThor::Library {

namespace {
    struct CachedSongMeta {
        SongMeta meta;
        fs::file_time_type lastWriteTime{};
        bool exists = false;
    };
    static std::unordered_map<std::string, CachedSongMeta> s_MetaCache;
    static std::mutex s_MetaMutex;
}

static std::string SongMetaPath(const std::string& filename)
{
    std::string safe = filename;
    for (char& c : safe) {
        if (c == ':') c = '_';
    }
    return GetAssetsPath() + "/songs_meta/" + safe + ".json";
}

static void EnsureSongMetaDir()
{
    try {
        fs::create_directories(U8Path(GetAssetsPath() + "/songs_meta"));
    } catch (...) {
        // Best-effort — si falla, Set/GetSongMeta simplemente no persisten.
    }
}

SongMeta GetSongMeta(const std::string& filename)
{
    SongMeta meta; // linesPerSlide=0 (centinela legacy) por default
    if (filename.empty()) return meta;

    std::string metaPathStr = SongMetaPath(filename);
    fs::path metaPath = U8Path(metaPathStr);
    std::error_code ec;
    auto lastWrite = fs::last_write_time(metaPath, ec);

    {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        auto it = s_MetaCache.find(filename);
        if (it != s_MetaCache.end()) {
            if (!ec && it->second.exists && it->second.lastWriteTime == lastWrite) {
                return it->second.meta;
            } else if (ec && !it->second.exists) {
                return meta; // Sigue sin existir el archivo, retorno rápido en memoria
            }
        }
    }

    if (ec) {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        s_MetaCache[filename] = CachedSongMeta{ meta, fs::file_time_type{}, false };
        return meta;
    }

    std::ifstream f(metaPath);
    if (!f.is_open()) {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        s_MetaCache[filename] = CachedSongMeta{ meta, fs::file_time_type{}, false };
        return meta;
    }

    json j;
    try {
        f >> j;
    } catch (...) {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        s_MetaCache[filename] = CachedSongMeta{ meta, lastWrite, true };
        return meta; // JSON corrupto/incompleto: se ignora
    }

    meta.version      = j.value("version", 1);
    meta.title        = j.value("title", "");
    meta.artistAuthor = j.value("artistAuthor", "");
    meta.note         = j.value("note", "");
    meta.copyright    = j.value("copyright", "");
    meta.extra        = j.value("extra", "");
    meta.linesPerSlide = j.value("linesPerSlide", 0);
    meta.tempoBpm      = j.value("tempoBpm", 0);

    meta.verseDurationOverrideMs.clear();
    if (j.contains("verseDurationOverrideMs") && j["verseDurationOverrideMs"].is_array()) {
        for (const auto& v : j["verseDurationOverrideMs"])
            meta.verseDurationOverrideMs.push_back(v.get<int>());
    }

    {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        s_MetaCache[filename] = CachedSongMeta{ meta, lastWrite, true };
    }

    return meta;
}

void SetSongMeta(const std::string& filename, const SongMeta& meta)
{
    EnsureSongMetaDir();

    json j;
    j["version"]       = meta.version;
    j["title"]         = meta.title;
    j["artistAuthor"]  = meta.artistAuthor;
    j["note"]          = meta.note;
    j["copyright"]     = meta.copyright;
    j["extra"]         = meta.extra;
    j["linesPerSlide"] = meta.linesPerSlide;
    j["tempoBpm"]      = meta.tempoBpm;
    j["verseDurationOverrideMs"] = meta.verseDurationOverrideMs;

    std::string metaPathStr = SongMetaPath(filename);
    fs::path metaPath = U8Path(metaPathStr);
    std::error_code ec;
    fs::create_directories(metaPath.parent_path(), ec);
    std::ofstream f(metaPath);
    if (f.is_open()) {
        f << j.dump(2);
        f.close();
    }

    auto lastWrite = fs::last_write_time(metaPath, ec);
    {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        s_MetaCache[filename] = CachedSongMeta{ meta, lastWrite, true };
    }
}

void RenameSongMeta(const std::string& oldFilename, const std::string& newFilename)
{
    std::error_code ec;
    fs::path oldPath = U8Path(SongMetaPath(oldFilename));
    if (!fs::exists(oldPath, ec)) return;

    EnsureSongMetaDir();
    fs::path newPath = U8Path(SongMetaPath(newFilename));
    fs::create_directories(newPath.parent_path(), ec);
    fs::rename(oldPath, newPath, ec);

    {
        std::lock_guard<std::mutex> lk(s_MetaMutex);
        auto it = s_MetaCache.find(oldFilename);
        if (it != s_MetaCache.end()) {
            CachedSongMeta copy = it->second;
            s_MetaCache.erase(it);
            s_MetaCache[newFilename] = copy;
        }
    }
}

void ClearSongMetaCache()
{
    std::lock_guard<std::mutex> lk(s_MetaMutex);
    s_MetaCache.clear();
}

} // namespace ProyecThor::Library
