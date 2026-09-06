#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>
#endif

#include "backend/core/AppPaths.h"

namespace fs = std::filesystem;

namespace ProyecThor::Library {

// =============================================================================
//  Conversiones UTF-16 <-> UTF-8 / ANSI
// =============================================================================
#ifdef _WIN32
inline std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                        s.data(), n, nullptr, nullptr);
    return s;
}

inline std::wstring Utf8ToWide(const std::string& u)
{
    if (u.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u.data(), (int)u.size(), w.data(), n);
    return w;
}

inline std::string AnsiToUtf8(const std::string& ansi)
{
    if (ansi.empty()) return {};
    int wn = MultiByteToWideChar(CP_ACP, 0, ansi.data(), (int)ansi.size(), nullptr, 0);
    std::wstring w(wn, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi.data(), (int)ansi.size(), w.data(), wn);
    return WideToUtf8(w);
}
#endif
// =============================================================================
//  Conversion de fs::path a UTF-8 (cross-platform)
// =============================================================================
inline std::string PathToUtf8(const fs::path& p)
{
#ifdef _WIN32
    return WideToUtf8(p.wstring());
#else
    return p.string();
#endif
}
// =============================================================================
//  Validacion y normalizacion de UTF-8
// =============================================================================
inline bool IsValidUtf8(const char* data, size_t size)
{
    size_t i = 0;
    while (i < size) {
        unsigned char c = (unsigned char)data[i];
        int extra = 0;
        if      (c < 0x80)           extra = 0;
        else if ((c & 0xE0) == 0xC0) { extra = 1; if (c < 0xC2) return false; }
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) { extra = 3; if (c > 0xF4) return false; }
        else return false;
        for (int j = 1; j <= extra; ++j) {
            if (i + j >= size || ((unsigned char)data[i+j] & 0xC0) != 0x80)
                return false;
        }
        i += 1 + extra;
    }
    return true;
}

inline std::string NormalizeToUtf8(const std::string& raw)
{
    const unsigned char* b = (const unsigned char*)raw.data();
    size_t sz = raw.size();
    std::string content;

    if (sz >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
        content.assign(raw.data() + 3, sz - 3);
#ifdef _WIN32
    else if (sz >= 2 && b[0] == 0xFF && b[1] == 0xFE) {
        size_t wlen = (sz - 2) / 2;
        std::wstring w(reinterpret_cast<const wchar_t*>(raw.data() + 2), wlen);
        content = WideToUtf8(w);
    }
    else if (sz >= 2 && b[0] == 0xFE && b[1] == 0xFF) {
        std::wstring w;
        w.reserve((sz - 2) / 2);
        for (size_t i = 2; i + 1 < sz; i += 2)
            w.push_back((wchar_t)((b[i] << 8) | b[i+1]));
        content = WideToUtf8(w);
    }
#endif
    else if (IsValidUtf8(raw.data(), sz))
        content = raw;
#ifdef _WIN32
    else
        content = AnsiToUtf8(raw);
#else
    else
        content = raw;
#endif

    content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
    return content;
}

// =============================================================================
//  Ruta de assets -- delega en ProyecThor::GetAssetsPath() (AppPaths.h) en
//  vez de recalcular %APPDATA%/etc. por su cuenta, asi respeta la carpeta de
//  datos elegida en Ajustes > Actualizaciones > "Carpeta de datos".
// =============================================================================
inline const std::string& GetAssetsPath()
{
    return ProyecThor::GetAssetsPath();
}

inline fs::path U8Path(const std::string& utf8)
{
#ifdef _WIN32
    return fs::path(Utf8ToWide(utf8));
#else
    return fs::path(utf8);
#endif
}

// =============================================================================
//  Utilidades de nombres de archivo
// =============================================================================
inline std::string SplitExtension(const std::string& filename, std::string& outExt)
{
    auto pos = filename.rfind('.');
    if (pos != std::string::npos && pos != 0) {
        outExt = filename.substr(pos);
        return filename.substr(0, pos);
    }
    outExt.clear();
    return filename;
}

inline std::string StripExtension(const std::string& filename)
{
    auto pos = filename.rfind('.');
    return (pos != std::string::npos) ? filename.substr(0, pos) : filename;
}

inline std::string TruncURL(const std::string& url, size_t maxLen = 52)
{
    std::string d = url;
    if (d.rfind("https://", 0) == 0) d = d.substr(8);
    else if (d.rfind("http://", 0) == 0) d = d.substr(7);
    if (d.size() > maxLen) d = d.substr(0, maxLen - 3) + "...";
    return d;
}

// =============================================================================
//  Animacion — lerp escalares y colores
// =============================================================================
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

inline ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t)
{
    return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t),
             Lerp(a.z, b.z, t), Lerp(a.w, b.w, t) };
}

// =============================================================================
//  Flag global de refresco de lista
// =============================================================================
inline bool& ForceListUpdate()
{
    static bool s_flag = true;
    return s_flag;
}

// =============================================================================
//  Flag global de reescaneo completo de la lista (ctx.items) desde disco
// =============================================================================
// A diferencia de ForceListUpdate() (que solo reordena/refiltra lo YA
// cargado en ctx.items), esta pide un reescaneo real del directorio via
// LibraryPanel::RefreshList() — necesario cuando un archivo cambio de
// nombre en disco desde un lugar sin acceso directo a LibraryContext (ver
// SongEditView::FlushIfDirty / RenameNewSongToTitleIfApplicable). La
// consume LibraryPanel::Render() en cada frame.
inline bool& ForceLibraryRescan()
{
    static bool s_flag = false;
    return s_flag;
}

} // namespace ProyecThor::Library