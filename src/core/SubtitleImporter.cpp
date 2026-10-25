#include "SubtitleImporter.h"
#include "plugins/PluginManager.h"
#include <algorithm>
#include <cctype>

namespace ProyecThor::Core {

static bool IsSafeUrl(const std::string& url) {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0)
        return false;
    for (char c : url) {
        if (c == '"' || c == '`' || c == '$' || c == '\\' || c == '\n' || c == '\r' || c == '\'')
            return false;
    }
    return true;
}

static std::string TrimCopy(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

SubtitleFetchResult FetchSubtitlesAsLyrics(const std::string& url) {
    SubtitleFetchResult result;

    const std::string trimmedUrl = TrimCopy(url);
    if (!IsSafeUrl(trimmedUrl)) {
        result.error = "URL invalida: debe ser un link http(s) sin comillas ni caracteres especiales.";
        return result;
    }

    // Delegamos la extracción de subtítulos al plugin integrado 'ytdlp'
    auto lyricsRes = PluginManager::Get().FetchLyricsFromUrl(trimmedUrl);
    result.success = lyricsRes.success;
    result.title   = lyricsRes.title;
    result.lyrics  = lyricsRes.lyrics;
    result.error   = lyricsRes.error;

    return result;
}

} // namespace ProyecThor::Core
