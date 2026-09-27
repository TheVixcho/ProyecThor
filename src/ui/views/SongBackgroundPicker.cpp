#include "SongBackgroundPicker.h"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cctype>

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
        return ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov"
            || ext == ".jpg" || ext == ".jpeg" || ext == ".png";
    };
    auto isImageExt = [](const std::string& ext) {
        return ext == ".jpg" || ext == ".jpeg" || ext == ".png";
    };
    auto addFile = [&](const std::filesystem::directory_entry& f, const std::string& folder,
                        std::vector<SongBgEntry>& out) {
        std::string ext = f.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!isMedia(ext)) return;
        SongBgEntry entry;
        entry.fullPath = f.path().string();
        entry.label    = folder.empty() ? f.path().stem().string() : (folder + "/" + f.path().stem().string());
        entry.isImage  = isImageExt(ext);
        out.push_back(std::move(entry));
    };

    std::vector<SongBgEntry> out;
    std::error_code ec;
    std::filesystem::path root = SongBgRootDir();
    if (!std::filesystem::exists(root, ec)) return out;

    for (const auto& e : std::filesystem::directory_iterator(root, ec))
    {
        if (e.is_directory())
        {
            std::string folder = e.path().filename().string();
            std::error_code subEc;
            for (const auto& sub : std::filesystem::directory_iterator(e.path(), subEc))
                if (sub.is_regular_file()) addFile(sub, folder, out);
        }
        else if (e.is_regular_file())
        {
            addFile(e, "", out);
        }
    }
    std::sort(out.begin(), out.end(), [](const SongBgEntry& a, const SongBgEntry& b) { return a.label < b.label; });
    return out;
}

}

