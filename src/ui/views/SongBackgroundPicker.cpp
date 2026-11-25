#include "SongBackgroundPicker.h"
#include "SettingsManager.h"
#include "core/MediaImportManager.h"
#include "ui/panels/biblio/LibraryHelpers.h"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <unordered_set>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#endif

namespace ProyecThor::UI {

static std::filesystem::path SongBgRootDir()
{
    std::filesystem::path dir;
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf);
    dir = std::filesystem::path(buf) / "ProyecThor";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;
    if (xdgConfig && *xdgConfig)
        base = std::filesystem::path(xdgConfig);
    else
        base = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config";
    dir = base / "ProyecThor";
#endif
    return dir / "assets" / "backgrounds";
}

std::vector<SongBgEntry> ListSongBackgrounds()
{
    auto isMedia = [](const std::string& ext) {
        return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm" || ext == ".m4v"
            || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp";
    };
    auto isImageExt = [](const std::string& ext) {
        return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp";
    };

    std::unordered_set<std::string> seenPaths;
    std::unordered_set<std::string> seenNames;
    std::vector<SongBgEntry> out;

    auto addPath = [&](const std::filesystem::path& p, const std::string& folder) {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (!isMedia(ext)) return;

        std::string fullUtf8 = Library::PathToUtf8(p);
        std::string fnameUtf8 = Library::PathToUtf8(p.filename());
        std::string stemUtf8  = Library::PathToUtf8(p.stem());

        std::string pathKey = Core::MediaImportManager::NormalizePathKey(fullUtf8);
        std::string nameKey = Core::MediaImportManager::NormalizeFilenameKey(fnameUtf8);
        if (nameKey.empty()) return;
        if (!pathKey.empty() && seenPaths.count(pathKey) > 0) return;
        if (seenNames.count(nameKey) > 0) return;

        if (!pathKey.empty()) seenPaths.insert(pathKey);
        seenNames.insert(nameKey);

        SongBgEntry entry;
        entry.fullPath = fullUtf8;
        entry.label    = folder.empty() ? stemUtf8 : (folder + "/" + stemUtf8);
        entry.isImage  = isImageExt(ext);
        out.push_back(std::move(entry));
    };

    std::error_code ec;
    std::filesystem::path root = SongBgRootDir();
    if (std::filesystem::exists(root, ec))
    {
        for (const auto& e : std::filesystem::directory_iterator(root, ec))
        {
            if (e.is_directory())
            {
                std::string folder = Library::PathToUtf8(e.path().filename());
                std::error_code subEc;
                for (const auto& sub : std::filesystem::directory_iterator(e.path(), subEc))
                    if (sub.is_regular_file()) addPath(sub.path(), folder);
            }
            else if (e.is_regular_file())
            {
                addPath(e.path(), "");
            }
        }
    }

    const auto& extBgs = ProyecThor::Settings::SettingsManager::Get().GetSettings().storage.externalBackgroundFiles;
    for (const auto& rawEntry : extBgs)
    {
        std::string folder, extPath;
        Core::MediaImportManager::ParseExternalBgEntry(rawEntry, folder, extPath);
        if (extPath.empty()) continue;
        std::error_code extEc;
        std::filesystem::path p = Library::U8Path(extPath);
        if (std::filesystem::exists(p, extEc) && std::filesystem::is_regular_file(p, extEc))
        {
            addPath(p, folder);
        }
    }

    std::sort(out.begin(), out.end(), [](const SongBgEntry& a, const SongBgEntry& b) { return a.label < b.label; });
    return out;
}

}

